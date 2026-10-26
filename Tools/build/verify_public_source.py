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

WORKFLOW = '.github/workflows/epoch-public.yml'
WORKFLOW_SHA256 = 'a144c98d2e5c652a45a244e22e6cb6ba3897040078ad5aa5fcc31b30f57738e8'
MANIFEST = 'source-projection.json'
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
REVIEWED_SHARED = {'Engine/src/editor/ai/editor.ai_session_diagnostics.hpp': 'fb102f6ef8b39d0e8155dbdc564a7432a41d7f2f6322d976c85bcfd542e2953f', 'Engine/src/editor/ai/editor.ai_source_intent.hpp': '01001dab803008aadd19be834f4eb1e6f92a2a0748a9fbf475d2a46ae1eb71fa', 'AGENTS.md': 'a9e6a23513ee770be38d12db2baf2faf7fd28bf483bae4e80e6ba04093f16924'}
PRIVATE_API = re.compile(r'\b(?:editor_ai_development_panel|editor_ai_development_controller|aiDevelopmentPanel|aiSourceArtifacts|AiSourceTaskResult|editor_ai_approved_patch|editor_ai_engine_mission|engine_mission_requested|engine_mission_file|engine_mission_model|engine_mission_candidate|engine_mission_error|set_source_mission_model|source_mission_setup_stop_source|epoch_engine_mission_model|source_mission_model|ApprovedPatchHandoff|prepare_approved_patch_handoff|invalidate_approved_patch_handoff|epoch_propose_source_patch|epoch_select_source_context|EPOCH_SOURCE_PROPOSAL_V1|EPOCH_SOURCE_CONTEXT_REQUEST_V1|source_iteration|source_self_review|project_ai_iteration|project_ai_iteration_session|EditorProjectSelfIterationProvision|projectSelfIterationDefault|ProjectSelfIterationChoice|self_iteration_provision|includeSelfIteration|project_self_iteration_choices|project_self_iteration_label|self_iteration_orchestrator|source_patch_stager|source_patch_proposal|source_patch_bundle|curated_context_bundle|development_proposal_codec|candidate_artifacts(?=\s*::)|push_ai_development_log(?=\s*\()|log_ai_candidate_event(?=\s*\())\b')
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
    if path.parts[0].casefold() in {'changes','projects','addons','releasenotes','images','ai','cache','logs','captures','build','x64'}:
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



PUBLIC_RUNTIME_UPLOAD = b'      - name: Return audited Linux runtime only\n        uses: actions/upload-artifact@v4\n        with:\n          name: epoch-public-linux-runtime\n          if-no-files-found: error\n          retention-days: 7\n          include-hidden-files: false\n          path: |\n            build/packages/public/v${{ steps.package.outputs.version }}/epoch_linux_x64_v${{ steps.package.outputs.version }}.tar.gz\n            build/packages/public/v${{ steps.package.outputs.version }}/epoch_linux_x64_v${{ steps.package.outputs.version }}.tar.gz.sha256\n            build/packages/public/v${{ steps.package.outputs.version }}/epoch_linux_x64_v${{ steps.package.outputs.version }}.tar.gz.build-validation.json\n'


def verify_workflow(workflow: bytes) -> None:
    if source_digest(WORKFLOW, workflow) != WORKFLOW_SHA256:
        raise RuntimeError('Generated public CI changed; maintainer policy review required')
    normalized = workflow.replace(b'\r\n', b'\n')
    if (not normalized or b'contents: read' not in normalized
            or any(value in normalized.lower() for value in (b'secrets.',b'chatgpt.site',
                b'contents: write',b'id-token:',b'pull_request_target',b'--refresh-manifest'))
            or normalized.count(b'actions/upload-artifact@') != 1
            or not normalized.endswith(PUBLIC_RUNTIME_UPLOAD)):
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
    edition = files.get('Engine/include/epoch.edition.hpp', b'')
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
