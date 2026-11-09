#!/usr/bin/env python3
"""Verify an explicit public source snapshot; never builds, runs models or publishes.

After staging intentional public additions/removals, --refresh-manifest can update
hashes only after the same edition/content/module guards pass. CI never refreshes.
Directory/write policies are not OS read isolation. This is a source gate, not a
compiler, renderer, model, confinement or runtime acceptance result.

Source identity uses sha256-git-text-lf-v1: only the text extensions/basenames
listed below replace CRLF bytes with LF before SHA-256. All other bytes, including
BOMs and lone CR, are preserved. Other files (including PNM raster assets) use raw
SHA-256. This permits Git's text checkout conversion without normalizing payloads;
archive bytes and archive digests always remain raw.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
from urllib.parse import urlsplit
import xml.etree.ElementTree as ET
from posixpath import normpath

WORKFLOW = '.github/workflows/epoch-public.yml'
WORKFLOW_SHA256 = '4bca7ec139848de766b8ac2803d820334f440b06f50d87c55d8dc8b378f7c9f3'
MANIFEST = 'source-projection.json'
# Exact root outputs from local CPU measurements, never source or release data.
# Keep this list shared with the exporter; do not hide similarly named source.
GENERATED_ROOT_FILES = frozenset({'solid-benchmark-debug.json', 'solid-progress.txt'})
# Operator-supplied reference artwork is usable locally, but its recorded
# provenance has not cleared redistribution. Keep the native procedural fallback.
HELD_REFERENCE_ASSETS = frozenset({
    'engine/resource/forest/bark_texture_preview.png',
    'engine/resource/forest/leaf_fan_texture.png',
    'engine/resource/forest/foliage_reference_board.png',
    'engine/resource/forest/provenance.json',
})


SOLUTION_GUID = r'\{[0-9A-Fa-f]{8}(?:-[0-9A-Fa-f]{4}){3}-[0-9A-Fa-f]{12}\}'
SOLUTION_FOLDERS = frozenset({
    '{2150E333-8FDC-42A3-9474-1A3956D46DE8}',
    '{66A26720-8FB5-11D2-AA7E-00C04F688DDE}',
})


def solution_structure(name: str, data: bytes) -> tuple[list[str], list[dict], dict[int, set[str]]]:
    """Read project blocks and project-ID records without rewriting solution bytes."""
    try:
        lines = data.decode('utf-8-sig').splitlines(keepends=True)
    except UnicodeDecodeError as error:
        raise RuntimeError(f'{name}: invalid UTF-8 solution') from error
    if not lines or not lines[0].startswith('Microsoft Visual Studio Solution File, Format Version '):
        raise RuntimeError(f'{name}: missing Visual Studio solution header')
    project_pattern = re.compile(
        rf'Project\("(?P<type>{SOLUTION_GUID})"\)\s*=\s*"(?P<name>[^"\r\n]+)",\s*'
        rf'"(?P<path>[^"\r\n]+)",\s*"(?P<guid>{SOLUTION_GUID})"')
    pair_pattern = re.compile(rf'(?P<left>{SOLUTION_GUID})\s*=\s*(?P<right>{SOLUTION_GUID})')
    projects, records = [], {}
    current = None
    section = None
    global_open = global_seen = global_ended = False
    identifiers = set()
    for index, line in enumerate(lines):
        row = line.strip()
        if not row or row.startswith('#'):
            continue
        if row.startswith('Project('):
            match = project_pattern.fullmatch(row)
            if not match or current is not None or global_seen:
                raise RuntimeError(f'{name}:{index+1}: malformed solution project block')
            current = dict(match.groupdict(), start=index)
            current['guid'] = current['guid'].upper()
            if current['guid'] in identifiers:
                raise RuntimeError(f'{name}:{index+1}: duplicate solution project ID')
            identifiers.add(current['guid'])
            continue
        if row == 'EndProject':
            if current is None or section is not None:
                raise RuntimeError(f'{name}:{index+1}: unmatched solution EndProject')
            current['end'] = index
            projects.append(current)
            current = None
            continue
        if row.startswith('ProjectSection('):
            match = re.fullmatch(r'ProjectSection\(([A-Za-z0-9_.-]+)\)\s*=\s*(?:preProject|postProject)', row)
            if current is None or section is not None or not match:
                raise RuntimeError(f'{name}:{index+1}: malformed solution project section')
            section = match.group(1)
            continue
        if row == 'EndProjectSection':
            if current is None or section is None:
                raise RuntimeError(f'{name}:{index+1}: unmatched solution project section')
            section = None
            continue
        if row == 'Global':
            if current is not None or global_seen:
                raise RuntimeError(f'{name}:{index+1}: malformed solution Global block')
            global_open = global_seen = True
            continue
        if row == 'EndGlobal':
            if not global_open or section is not None:
                raise RuntimeError(f'{name}:{index+1}: unmatched solution EndGlobal')
            global_open = False
            global_ended = True
            continue
        if row.startswith('GlobalSection('):
            match = re.fullmatch(r'GlobalSection\(([A-Za-z0-9_.-]+)\)\s*=\s*(?:preSolution|postSolution)', row)
            if not global_open or section is not None or not match:
                raise RuntimeError(f'{name}:{index+1}: malformed solution global section')
            section = match.group(1)
            continue
        if row == 'EndGlobalSection':
            if not global_open or section is None:
                raise RuntimeError(f'{name}:{index+1}: unmatched solution global section')
            section = None
            continue
        if section in {'ProjectDependencies', 'NestedProjects'}:
            match = pair_pattern.fullmatch(row)
            if not match:
                raise RuntimeError(f'{name}:{index+1}: malformed solution project-ID record')
            records[index] = {match.group('left').upper(), match.group('right').upper()}
        elif section in {'ProjectConfiguration', 'ProjectConfigurationPlatforms'}:
            match = re.fullmatch(rf'({SOLUTION_GUID})\.[^=]+\s*=\s*[^=]+', row)
            if not match:
                raise RuntimeError(f'{name}:{index+1}: malformed solution project configuration')
            records[index] = {match.group(1).upper()}
        elif section == 'SharedMSBuildProjectFiles':
            match = re.fullmatch(rf'.+\*({SOLUTION_GUID})\*SharedItemsImports\s*=\s*[0-9]+', row)
            if not match:
                raise RuntimeError(f'{name}:{index+1}: malformed shared-project record')
            records[index] = {match.group(1).upper()}
        elif current is not None and section is None:
            raise RuntimeError(f'{name}:{index+1}: content outside a solution project section')
        elif global_ended or (not global_open and index != 0
                and not re.fullmatch(r'(?:VisualStudioVersion|MinimumVisualStudioVersion)\s*=.+', row)):
            raise RuntimeError(f'{name}:{index+1}: unexpected solution content')
    if current is not None or section is not None or not global_ended or global_open:
        raise RuntimeError(f'{name}: unterminated solution block')
    for index, references in records.items():
        if not references <= identifiers:
            raise RuntimeError(f'{name}:{index+1}: solution references an undeclared project ID')
    return lines, projects, records


def solution_project_path(name: str, reference: str) -> str:
    reference = reference.replace('\\', '/')
    target = normpath(str(PurePosixPath(name).parent / reference))
    if (PurePosixPath(reference).is_absolute() or ':' in reference or '$(' in reference
            or target == '..' or target.startswith('../')):
        raise RuntimeError(f'{name}: solution project path leaves its source root')
    return target


def verify_solution_references(files: dict[str, bytes]) -> None:
    names = {name.casefold() for name in files}
    for name, data in files.items():
        if PurePosixPath(name).suffix.lower() != '.sln':
            continue
        _, projects, _ = solution_structure(name, data)
        for project in projects:
            if project['type'].upper() in SOLUTION_FOLDERS:
                continue
            target = solution_project_path(name, project['path'])
            if target.casefold() not in names:
                raise RuntimeError(f'{name}: solution project reference missing from payload: {project["path"]}')


def held_msbuild_reference(name: str, data: bytes) -> str | None:
    """Reject exact held asset declarations, including mixed MSBuild item lists."""
    if PurePosixPath(name).suffix.lower() not in {'.vcxproj', '.vcxitems', '.filters', '.props', '.targets'}:
        return None
    try:
        tree = ET.fromstring(data)
    except ET.ParseError as error:
        raise RuntimeError(f'{name}: invalid MSBuild XML') from error
    for element in tree.iter():
        for member in element.attrib.get('Include', '').split(';'):
            reference = member.strip().replace('\\', '/')
            if reference.startswith('$(EpochRepositoryRoot)'):
                resolved = normpath(reference[len('$(EpochRepositoryRoot)'):])
            else:
                reference = reference.replace('$(MSBuildThisFileDirectory)', '')
                if not reference or '$(' in reference or '*' in reference:
                    continue
                resolved = normpath(str(PurePosixPath(name).parent / reference))
            if resolved.casefold() in HELD_REFERENCE_ASSETS:
                return member
    return None
CPP = {'.c','.cc','.cpp','.cxx','.h','.hh','.hpp','.hxx','.ixx','.cppm','.mpp','.inl','.inc','.ipp','.tpp','.cu','.cuh'}
SOURCE_HASH_ALGORITHM = 'sha256-git-text-lf-v1'
HASH_TEXT_SUFFIXES = CPP | {
    '.cmake','.txt','.md','.ps1','.py','.json','.jsonc','.xml','.vcxitems',
    '.vcxproj','.filters','.props','.targets','.natvis','.sln','.bat','.cmd',
    '.sh','.in','.yml','.yaml','.toml','.rc','.env','.ini','.cfg','.css','.js',
    '.ts','.tsx','.jsx','.mjs','.cjs','.glsl','.vert','.frag','.geom','.comp',
    '.hlsl','.metal','.html','.manifest','.gltf','.mm',
}
HASH_TEXT_NAMES = {'.gitattributes','.gitignore','.editorconfig','cmakelists.txt',
                   'doxyfile','dockerfile','makefile','gnumakefile','license',
                   'licence','notice','copying','readme','authors','version','release_version'}
SOURCE_HASH_NORMALIZATION = (
    'Text extensions/basenames listed in Tools/build/verify_public_source.py: '
    'replace CRLF with LF only; preserve BOMs, lone CR and all other bytes. '
    'Other files are hashed raw. Archive bytes and digests remain raw.')


def source_digest(name: str, data: bytes) -> str:
    path = PurePosixPath(name)
    if path.suffix.lower() in HASH_TEXT_SUFFIXES or path.name.lower() in HASH_TEXT_NAMES:
        data = data.replace(b'\r\n', b'\n')
    return hashlib.sha256(data).hexdigest()


PRIVATE_NATIVE_SOURCE_PREFIXES = (
    'ai.backend_agent_', 'ai.backend_tools_', 'ai.backend_workspace_',
    'ai.backend_persistence_', 'ai.backend_learning_',
    'ai.backend_memory_episode_store.', 'ai.backend_decision_decision_engine.',
    'ai.backend_core_event.', 'ai.backend_core_event_bus.',
    'ai.backend_model_internal_model_host.',
)
PRIVATE_STEMS = ('ai.development_', 'ai.iteration_', 'ai.source_', 'ai.self_iteration_',
                 'ai.curated_context_', 'ai.mcp_campaign', 'ai.mcp_child_host',
                 'ai.mcp_orchestrator_', 'ai.mcp_supervisor_', 'editor.ai_development_',
                 'editor.candidate_artifact', 'editor.ai_candidate_', 'editor.ai_source_',
                 'editor.ai_build_failure', 'editor.ai_theme_palette_shape',
                 'editor.ai_visual_capture', 'editor.ai_resource_hub',
                 'editor.ai_documentation_guide', 'editor.ai_objective_gate',
                 'editor.ai_session_retention', 'editor.ai_session_diagnostics',
                 'editor.ai_live_steering', 'project.ai_self_iteration',
                 'editor.codex_app_server', 'editor.codex_bridge') + PRIVATE_NATIVE_SOURCE_PREFIXES
PRIVATE_STEMS += ('editor.ai_approved_patch', 'editor.ai_engine_mission',
                  'editor.ai_reference_images')
REVIEWED_SHARED = {'Engine/src/editor/ai/editor.ai_session_diagnostics.hpp': '327726e230daa46b7702145559690740ed0e2250717f67e340d6c172678b0fb5', 'Engine/src/editor/ai/editor.ai_source_intent.hpp': '52c453c06b620862e2ce82dc7c7a627d3ca553a0f18443311ee4d06394118f15', 'AGENTS.md': 'a9e6a23513ee770be38d12db2baf2faf7fd28bf483bae4e80e6ba04093f16924'}
PRIVATE_API = re.compile(r'\b(?:editor_ai_development_panel|editor_ai_development_controller|aiDevelopmentPanel|aiSourceArtifacts|AiSourceTaskResult|editor_ai_approved_patch|editor_ai_engine_mission|engine_mission_requested|engine_mission_file|engine_mission_model|engine_mission_candidate|engine_mission_error|codex_task_requested|codex_task_file|codex_task_error|set_source_mission_model|source_mission_setup_stop_source|epoch_engine_mission_model|source_mission_model|ApprovedPatchHandoff|prepare_approved_patch_handoff|invalidate_approved_patch_handoff|epoch_propose_source_patch|epoch_select_source_context|EPOCH_SOURCE_PROPOSAL_V1|EPOCH_SOURCE_CONTEXT_REQUEST_V1|source_iteration|source_self_review|project_ai_iteration|project_ai_iteration_session|EditorProjectSelfIterationProvision|projectSelfIterationDefault|ProjectSelfIterationChoice|self_iteration_provision|includeSelfIteration|project_self_iteration_choices|project_self_iteration_label|self_iteration_orchestrator|source_patch_stager|source_patch_proposal|source_patch_bundle|curated_context_bundle|development_proposal_codec|candidate_artifacts(?=\s*::)|push_ai_development_log(?=\s*\()|log_ai_candidate_event(?=\s*\())\b')
PRIVATE_AUTH = re.compile(r'/api/private/epoch-engine/device/|EpochEngine\.SourceDeviceAuth\.v1|epoch-source-key-wrap-v1|EpochEngine private source DEK wrap v1')
PRIVATE_NATIVE_CODING_API = re.compile(
    r'\b(?:AgentRuntime|ToolRegistry|InternalModelHost|register_builtin_tools|training_backend)\b|'
    r'\b(?:ToolApprovalMode|PendingToolApproval|tool_approval_mode|set_tool_approval_mode|'
    r'pending_tool_approval|decide_pending_tool)\b|'
    r'\bRuntimeHost\s*::\s*(?:run_task|resolve_tool_approval|tool_catalog)\b|'
    r'\bmodel_worker\.py\b')
PRIVATE_NATIVE_OWNER_API = re.compile(
    r'\b(?:RunOutcome|RunTerminal|EventSink|run_task|resolve_tool_approval|tool_catalog)\b')
PRIVATE_SOURCE_WORKER = re.compile(rb'\bdef\s+_repository_context\s*\(')
PRIVATE_SOURCE_PROVENANCE = re.compile(
    rb'(?i)\b[a-z]:[\\/]+Users[\\/]+[^\\/\s\x00`"\']+|'
    rb'/(?:home|Users)/[^/\s<>`"\']+/|'
    rb'\.codex[\\/]+worktrees[\\/]+[a-z0-9]{2,}[\\/]|'
    rb'\bai[\\/]+s[0-9][a-z0-9]{8,}(?:[\\/]|[\s`"\']|$)')
HTTPS_REFERENCE = re.compile(rb'(?i)\bhttps://[^\s<>`"\'\\]+')
HTTPS_HOST = re.compile(r'[A-Za-z0-9](?:[A-Za-z0-9.-]*[A-Za-z0-9])?\Z')


def private_source_provenance(name: str, data: bytes):
    # Compiler source-location/PDB breadcrumbs in binary assets are a separate
    # reproducibility/privacy concern; this gate refuses source-text evidence.
    path = PurePosixPath(name)
    if path.suffix.lower() in HASH_TEXT_SUFFIXES or path.name.lower() in HASH_TEXT_NAMES:
        remote_paths = []
        for reference in HTTPS_REFERENCE.finditer(data):
            try:
                value = reference.group().decode('ascii')
                url = urlsplit(value)
                # Credentials must never gain an exception by looking like a
                # documentation URL, including empty or percent-encoded users.
                if '@' in url.netloc:
                    return reference
                host = url.hostname or ''
                if (not HTTPS_HOST.fullmatch(host)
                        or '.' not in host or host.lower().endswith('.localhost')
                        or (url.port is not None and not 1 <= url.port <= 65535)):
                    continue
                start = reference.start() + len('https://') + len(url.netloc)
                remote_paths.append((start, start + len(url.path)))
            except (UnicodeDecodeError, ValueError):
                continue
        for match in PRIVATE_SOURCE_PROVENANCE.finditer(data):
            # /users/platforms in an HTTPS documentation path is not a local
            # home directory. Keep drive paths, candidate/worktree identities,
            # file URLs, and local paths in URL queries/fragments blocked.
            unix_home = match.group().lower().startswith((b'/users/', b'/home/'))
            if data[max(0, match.start() - 7):match.start()].lower() == b'file://':
                return match
            if unix_home and any(start <= match.start() and match.end() <= end
                                 for start, end in remote_paths):
                continue
            return match
    return None


def private_native_coding_api_matches(name: str, text: str):
    yield from PRIVATE_NATIVE_CODING_API.finditer(text)
    # Engine-wide task/queue APIs retain their architecture. Only this private
    # native agent owner loses unqualified run-task/event transport identities.
    if name.casefold().startswith(('engine/src/ai/', 'engine/modules/ai.')):
        yield from PRIVATE_NATIVE_OWNER_API.finditer(text)


PRIVATE_TRAINING_MEMBERS = (
    'doctor', 'initialize', 'start', 'cancel', 'enable_explicit_training',
    'set_paused', 'run_worker', 'ensure_training_data', 'run_training', 'write_marker')
_training_member_pattern = '(?:' + '|'.join(PRIVATE_TRAINING_MEMBERS) + ')'
PRIVATE_TRAINING_API = re.compile(
    r'\b(?:prepare_training|start_training|cancel_training|pause_training|TrainingRequest|'
    r'enable_explicit_training|ensure_training_data|run_training)\b|'
    r'\bTrainingService\s*::\s*' + _training_member_pattern + r'\b|'
    r'\btraining\s*(?:\.|->)\s*' + _training_member_pattern + r'\b')
PRIVATE_TRAINING_OWNER_API = re.compile(r'\b' + _training_member_pattern + r'\s*\(')


def private_training_api_matches(name: str, text: str):
    yield from PRIVATE_TRAINING_API.finditer(text)
    # Generic start/cancel/initialize names belong to many ordinary systems;
    # inspect unqualified declarations only in this actual training owner.
    if PurePosixPath(name).name in {'ai.backend_training_training_service.hpp',
                                    'ai.backend_training_training_service.cpp'}:
        yield from PRIVATE_TRAINING_OWNER_API.finditer(text)


IMPORT = re.compile(r'^\s*(?:export\s+)?import\s+([A-Za-z_][A-Za-z0-9_.:]*)\s*;', re.M)
INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"\n]+)"', re.M)
MODULE = re.compile(rb'export\s+module\s+([A-Za-z_][A-Za-z0-9_.:]*);')


def forbidden_path(name: str) -> bool:
    path = PurePosixPath(name)
    if name.casefold() in HELD_REFERENCE_ASSETS:
        return True
    if name.casefold() in GENERATED_ROOT_FILES:
        return True
    if name.casefold().startswith('engine/dep/epochselfcoding/'):
        return True
    if any(part.casefold() == 'selfiteration' for part in path.parts):
        return True
    if path.is_absolute() or not path.parts or '..' in path.parts or '\\' in name or ':' in name:
        return True
    if name == WORKFLOW:
        return False
    if any(part.casefold() in {'.git','.codex','.agents','.github','.vs','.idea','.vscode','.aws','.ssh','.azure','.cache','.pytest_cache','node_modules','__pycache__'} for part in path.parts):
        return True
    if path.parts[0].casefold() in {'changes','projects','addons','releasenotes','images','ai','cache','logs','captures','build','artifacts','x64'}:
        return True
    if path.name.lower().startswith(('.env','credentials','secrets')) or path.suffix.lower() in {'.pem','.key','.pfx','.p12','.sqlite','.db','.jsonl','.zip','.7z','.tar','.gz','.tgz','.exe','.dll','.pdb','.obj','.gguf','.lib','.so','.dylib','.a','.pyc','.aps'}:
        return True
    if name.startswith(('Engine/docs/reference/','Engine/docs/engine/diagnostics/',
                        'Tools/ai/','Tests/ai/','Engine/Bin/','Engine/build/','Engine/built/')):
        return True
    if name.startswith('Engine/docs/engine/') and path.name in {'ai_plan.md','ai_source.md','ai_recipes.md','ai_research.md','dark_iterations.md','source_audit.md','dream.md'}:
        return True
    if name.startswith('Engine/ai/') and not name.startswith('Engine/ai/manifests/'):
        return True
    if name in {'Engine/dep/EpochSelfCoding/assets/ai/epoch.local_mcp_iteration.ps1','Tools/build/epoch_source_projection.py','Tools/build/package_epoch_source.py','Tools/build/public_ci_workflow.yml','Tools/epoch_dream_docs.cpp','Tests/build/test_public_source_export.py','Tests/build/test_approved_patch_handoff.py','Tests/build/test_owned_source_discovery.py','Tests/build/test_commandline_startup_modes.py'}:
        return True
    if name.casefold().startswith(('engine/assets/ai/native/training/',
                                   'engine/resource/ai/native/training/')):
        return True
    if path.name == 'model_worker.py':
        return True
    if name in REVIEWED_SHARED:
        return False
    return path.name.startswith(PRIVATE_STEMS) or path.name in {'editor.ai_workflow_presentation.hpp','ai.self_iteration_analytics.hpp'}



PUBLIC_WINDOWS_RUNTIME_UPLOAD = b"""      - name: Return audited Windows runtime only
        uses: actions/upload-artifact@v4
        with:
          name: epoch-public-windows-runtime
          if-no-files-found: error
          retention-days: 7
          include-hidden-files: false
          path: |
            Artifacts/Packages/public/v${{ steps.package.outputs.version }}/epoch_win10_x64_v${{ steps.package.outputs.version }}.zip
            Artifacts/Packages/public/v${{ steps.package.outputs.version }}/epoch_win10_x64_v${{ steps.package.outputs.version }}.zip.sha256
            Artifacts/Packages/public/v${{ steps.package.outputs.version }}/epoch_win10_x64_v${{ steps.package.outputs.version }}.zip.build-validation.json

  linux:
"""
PUBLIC_LINUX_RUNTIME_UPLOAD = b"""      - name: Return audited Linux runtime only
        uses: actions/upload-artifact@v4
        with:
          name: epoch-public-linux-runtime
          if-no-files-found: error
          retention-days: 7
          include-hidden-files: false
          path: |
            Artifacts/Packages/public/v${{ steps.package.outputs.version }}/epoch_linux_x64_v${{ steps.package.outputs.version }}.tar.gz
            Artifacts/Packages/public/v${{ steps.package.outputs.version }}/epoch_linux_x64_v${{ steps.package.outputs.version }}.tar.gz.sha256
            Artifacts/Packages/public/v${{ steps.package.outputs.version }}/epoch_linux_x64_v${{ steps.package.outputs.version }}.tar.gz.build-validation.json
"""


def verify_workflow(workflow: bytes) -> None:
    if source_digest(WORKFLOW, workflow) != WORKFLOW_SHA256:
        raise RuntimeError('Generated public CI changed; maintainer policy review required')
    normalized = workflow.replace(b'\r\n', b'\n')
    if (not normalized or b'contents: read' not in normalized
            or any(value in normalized.lower() for value in (b'secrets.',b'chatgpt.site',
                b'contents: write',b'id-token:',b'pull_request_target',b'--refresh-manifest'))
            or normalized.count(b'actions/upload-artifact@') != 2
            or normalized.count(PUBLIC_WINDOWS_RUNTIME_UPLOAD) != 1
            or not normalized.endswith(PUBLIC_LINUX_RUNTIME_UPLOAD)):
        raise RuntimeError('Public CI permits only the exact audited runtime artifact allowlist')


def inventory(root: Path, manifest: dict) -> set[str]:
    if (root / '.git').exists():
        result = subprocess.run(['git','ls-files','-z'], cwd=root, capture_output=True, check=True)
        return {name.decode('utf-8') for name in result.stdout.split(b'\0') if name}
    # A flat exported ZIP has no Git history. Its explicit manifest is the only
    # source inventory; never sweep up arbitrary local files during refresh.
    return set(manifest['files']) | {MANIFEST}


def check(root: Path, manifest: dict, names: set[str], verify_hashes: bool = True) -> dict[str, str]:
    if manifest.get('schema') != 'epoch.source_projection.v1' or manifest.get('edition') != 'public' or not isinstance(manifest.get('files'),dict):
        raise RuntimeError('Expected a public source projection manifest')
    if (manifest.get('file_hash_algorithm') != SOURCE_HASH_ALGORITHM or
            manifest.get('file_hash_normalization') != SOURCE_HASH_NORMALIZATION):
        raise RuntimeError('Unsupported or unspecified public source hash policy')
    if verify_hashes and names != set(manifest['files']) | {MANIFEST}:
        raise RuntimeError('Public tracked source inventory differs; review additions/removals before explicit manifest refresh')
    files = {}
    for name in sorted(names - {MANIFEST}):
        if forbidden_path(name):
            raise RuntimeError('Private/generated path refused; no manifest refresh performed')
        path = root / name
        if path.is_symlink() or not path.is_file() or not path.resolve().is_relative_to(root):
            raise RuntimeError('Source input is missing, linked or outside its root')
        files[name] = path.read_bytes()
        if private_source_provenance(name, files[name]) is not None:
            raise RuntimeError('Private operator/candidate source provenance refused')
        if held_msbuild_reference(name, files[name]) is not None:
            raise RuntimeError('Held reference-art build item refused')
    edition = files.get('Engine/include/epoch.edition.hpp', b'')
    verify_solution_references(files)
    definitions = re.findall(rb'(?m)^#define EPOCH_SOURCE_EDITION_PUBLIC ([01])[ \t]*\r?$',edition)
    if definitions != [b'1']:
        raise RuntimeError('Expected one physical public edition identity')
    for name,digest in REVIEWED_SHARED.items():
        if name in files and source_digest(name, files[name]) != digest:
            raise RuntimeError('Reviewed shared project/diagnostic policy changed; maintainer review required')
    modules = {'std','std.compat'}
    for name,data in files.items():
        if PurePosixPath(name).suffix.lower() in {'.ixx','.cppm','.mpp'}:
            match = MODULE.search(data)
            if match:
                modules.add(match.group(1).decode('ascii'))
    for name,data in files.items():
        if PurePosixPath(name).suffix.lower() == '.py' and PRIVATE_SOURCE_WORKER.search(data):
            raise RuntimeError('Private repository-source worker refused')
        if PurePosixPath(name).suffix.lower() not in CPP:
            continue
        text = data.decode('utf-8-sig')
        # Physical dependency/example ownership does not relax the Public
        # boundary. Check every retained C++ source, including inline parts.
        if PRIVATE_API.search(text) or PRIVATE_AUTH.search(text):
            raise RuntimeError('Private engine-source/authentication code refused')
        if next(private_training_api_matches(name, text), None) is not None:
            raise RuntimeError('Private training-mutation API refused')
        if next(private_native_coding_api_matches(name, text), None) is not None:
            raise RuntimeError('Private native coding/source-worker API refused')
        if ('EPOCH_'+'PRIVATE_SOURCE_') in text or ('EPOCH_'+'PUBLIC_SOURCE_') in text:
            raise RuntimeError('Unprojected edition region refused')
        for match in IMPORT.finditer(text):
            if match.group(1) not in modules:
                raise RuntimeError('Native source imports an unavailable module; source closure failed')
        for match in INCLUDE.finditer(text):
            basename = PurePosixPath(match.group(1).replace('\\','/')).name
            if basename.startswith(PRIVATE_STEMS) and not any(PurePosixPath(safe).name == basename for safe in REVIEWED_SHARED):
                raise RuntimeError('Native source includes an excluded private implementation')
    workflow = files.get(WORKFLOW,b'')
    verify_workflow(workflow)
    hashes = {name:source_digest(name,data) for name,data in files.items()}
    if verify_hashes and hashes != manifest['files']:
        raise RuntimeError('Public source content differs from the reviewed snapshot; no automatic refresh')
    return hashes


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2])
    parser.add_argument('--refresh-manifest',action='store_true',help='Explicitly refresh hashes after edition/content/module guards, never build or publish')
    args=parser.parse_args(); root=args.root.resolve(); path=root/MANIFEST
    manifest=json.loads(path.read_text(encoding='utf-8'))
    names=inventory(root,manifest)
    hashes=check(root,manifest,names,verify_hashes=not args.refresh_manifest)
    if args.refresh_manifest:
        result={'schema':'epoch.source_projection.v1','edition':'public',
                'validation':'source-boundaries-only; build/runtime acceptance required',
                'file_hash_algorithm':SOURCE_HASH_ALGORITHM,
                'file_hash_normalization':SOURCE_HASH_NORMALIZATION,
                'manifest_maintenance':'explicit local refresh; no build or publication verdict',
                'files':hashes}
        pending=path.with_name(path.name+'.partial')
        pending.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n',encoding='utf-8',newline='\n')
        pending.replace(path)
        print('Public manifest refreshed after source guards. Build/runtime/release acceptance remains required.')
    else:
        print('Public edition, source content and module closure verified. Build/runtime acceptance remains separate.')
    return 0

if __name__=='__main__':
    raise SystemExit(main())
