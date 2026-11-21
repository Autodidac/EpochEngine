#!/usr/bin/env python3
"""Record bounded checkout source and existing delivery sizes; launches no engine."""
from __future__ import annotations

import argparse
from collections import defaultdict
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

from validate_source_size import CPP_EXTENSIONS, linked

ROOTS = {"Engine", "Tools", "Tests", "Changes", "ReleaseNotes", "Images", ".github"}
SOURCE_BUILD_DIRS = {"Engine/docs/build", "Engine/src/build", "Tests/build", "Tools/build"}
GENERATED = {".git", ".codex", ".agents", "artifacts", "cache", "caches", "logs",
             "captures", "projects", "node_modules", "vcpkg_installed", "__pycache__",
             "bin", "built", "x64", "x86", "debug", "release", "releasewithdebuginfo"}
TEXT = {".md": "Documentation", ".txt": "Documentation", ".py": "Python",
        ".ps1": "PowerShell", ".sh": "Shell", ".bat": "Shell", ".cmd": "Shell",
        ".cmake": "Build configuration", ".json": "Data/configuration",
        ".yaml": "Data/configuration", ".yml": "Data/configuration",
        ".xml": "Build configuration", ".props": "Build configuration",
        ".targets": "Build configuration", ".vcxproj": "Build configuration",
        ".vcxitems": "Build configuration", ".filters": "Build configuration",
        ".glsl": "Shader", ".vert": "Shader", ".frag": "Shader", ".comp": "Shader",
        ".hlsl": "Shader", ".js": "JavaScript", ".html": "HTML", ".css": "CSS"}


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args], timeout=45)


def dirty_count(root):
    records = git(root, "status", "--porcelain=v1", "-z", "--untracked-files=all").split(b"\0")
    count, index = 0, 0
    while index < len(records):
        row = records[index]
        if row:
            count += 1
            if b"R" in row[:2] or b"C" in row[:2]:
                index += 1  # A rename/copy has a second pathname, not another file.
        index += 1
    return count


def in_scope(relative):
    parts = relative.parts
    if len(parts) > 1 and parts[0] not in ROOTS:
        return False
    for index, part in enumerate(parts[:-1]):
        if part.lower() in GENERATED:
            return False
        if part.lower() == "build" and "/".join(parts[:index + 1]) not in SOURCE_BUILD_DIRS:
            return False
    return True


def metadata(path, root, digest=False):
    stat = path.stat()
    row = {"path": path.relative_to(root).as_posix(), "bytes": stat.st_size,
           "modified_utc": datetime.fromtimestamp(stat.st_mtime, timezone.utc).isoformat()}
    if digest:
        with path.open("rb") as stream:
            row["sha256"] = hashlib.file_digest(stream, "sha256").hexdigest()
        row["stable_during_read"] = stat.st_size == path.stat().st_size and stat.st_mtime_ns == path.stat().st_mtime_ns
    return row


def collect(root, additional_archives=(), release_directory="Artifacts/Runtime/windows/development/x64/Release"):
    started = datetime.now(timezone.utc).isoformat()
    head = git(root, "rev-parse", "HEAD").decode().strip()
    dirty_before = dirty_count(root)
    dependencies = json.loads((root / "Engine/dep/dependencies.json").read_text(encoding="utf-8-sig"))["dependencies"]
    owners = {item["path"]: item for item in dependencies}
    files, skipped = [], []
    paths = set(git(root, "ls-files", "-z", "--cached", "--others", "--exclude-standard").split(b"\0"))
    for raw in sorted(paths):
        if not raw:
            continue
        relative = Path(raw.decode("utf-8"))
        if not in_scope(relative):
            continue
        path = root / relative
        if not path.is_file():
            continue  # Deleted tracked files do not count as current source.
        if any(linked(parent) for parent in [path, *path.parents] if parent != root and parent.is_relative_to(root)):
            skipped.append({"path": relative.as_posix(), "reason": "linked path"})
            continue
        name = relative.as_posix()
        owner, ownership = "Engine repository", "first_party"
        for prefix, item in owners.items():
            if name.startswith(prefix + "/"):
                owner = item["name"]
                ownership = "private_first_party" if item["mode"] == "development_only_self_coding" else (
                    "first_party_dependency" if item["mode"].startswith("engine_owned_") else "third_party_dependency")
                break
        if ownership == "first_party" and name.startswith("Engine/dep/"):
            owner, ownership = "Unclassified dependency", "unclassified_dependency"
        if any(part.lower() in {"third_party", "vendor", "stb"} for part in relative.parts):
            ownership = "third_party_dependency"
        extension = path.suffix.lower()
        language = "C/C++" if extension in CPP_EXTENSIONS else TEXT.get(extension, "Asset/other")
        if path.name == "CMakeLists.txt":
            language = "Build configuration"
        role = "tests" if any(part.lower() in {"tests", "test"} for part in relative.parts[:-1]) else (
            "documentation" if language == "Documentation" else "tooling" if relative.parts[0] == "Tools" else "implementation/assets")
        row = metadata(path, root)
        row.update(owner=owner, ownership=ownership, role=role, language=language)
        if language != "Asset/other":
            before = path.stat()
            if before.st_size > 32 * 1024 * 1024:
                skipped.append({"path": name, "reason": "text exceeds 32 MiB read bound"})
            else:
                # Physical lines include comments/directives. Nonblank is NOT executable LOC.
                with path.open("rb") as stream:
                    lines = list(stream)
                row.update(physical_lines=len(lines), nonblank_lines=sum(bool(line.strip()) for line in lines))
                after = path.stat()
                if before.st_size != after.st_size or before.st_mtime_ns != after.st_mtime_ns:
                    skipped.append({"path": name, "reason": "changed during count; rerun for coherent snapshot"})
        files.append(row)
    groups = defaultdict(lambda: dict(files=0, bytes=0, physical_lines=0, nonblank_lines=0))
    for row in files:
        key = (row["owner"], row["ownership"], row["role"], row["language"])
        for field in groups[key]:
            groups[key][field] += 1 if field == "files" else row.get(field, 0)
    cpp = [row for row in files if row["language"] == "C/C++" and row["ownership"].startswith(("first_party", "private_first_party"))]
    def totals(rows):
        return {"files": len(rows), **{key: sum(row.get(key, 0) for row in rows) for key in ("bytes", "physical_lines", "nonblank_lines")}}
    version_tool = root / "Tools/build/get_epoch_version.ps1"
    shell = shutil.which("pwsh") or shutil.which("powershell.exe")
    version, version_error = None, None
    if shell:
        try:
            version = subprocess.check_output([shell, "-NoProfile", "-ExecutionPolicy", "Bypass",
                "-File", str(version_tool)], stderr=subprocess.STDOUT, timeout=20).decode().strip()
        except (OSError, subprocess.SubprocessError) as error:
            version_error = str(error)
    release = root / release_directory
    binaries = [metadata(path, root, path.suffix.lower() == ".exe") for path in sorted(release.glob("*"))
                if path.is_file() and path.suffix.lower() in {".exe", ".dll", ".lib", ".pdb"}]
    archive_dirs = ["Artifacts/FullReleaseNov12", "Artifacts/FullReleaseNov12/dev-runtime",
                    "Artifacts/FullReleaseNov12/pub-runtime", "Artifacts/FullReleaseNov12/dev-runtime-reviewed",
                    "Artifacts/FullReleaseNov12/pub-runtime-reviewed", "Artifacts/FullReleaseNov12/hosted-linux-final",
                    "Artifacts/FullReleaseNov12/hosted-windows-final-43635925288", "Artifacts/FullReleaseNov12/dev-source-nov14",
                    "Artifacts/FullReleaseNov12/dev-ci/Artifacts/Packages/development/v0.91.08",
                    "Artifacts/Packages/public", "Artifacts/Packages/development", "Artifacts/Packages/extensions"]
    archive_dirs = list(dict.fromkeys([*archive_dirs, *additional_archives]))
    archives = [metadata(path, root) for directory in archive_dirs for path in sorted((root / directory).glob("*"))
                if path.is_file() and path.name.lower().endswith((".zip", ".tar.gz", ".tar.xz", ".7z"))]
    return {"schema": "epoch.engine_stats.v1", "started_utc": started, "finished_utc": datetime.now(timezone.utc).isoformat(),
            "source_version": version, "version_error": version_error, "git_head": head, "branch": git(root, "branch", "--show-current").decode().strip(),
            "dirty_files_before": dirty_before, "dirty_files_after": dirty_count(root),
            "scope": {"roots": sorted(ROOTS), "include_root_files": True, "file_selection": "git cached + nonignored untracked; existing physical files only",
                      "generated_directory_exclusions": sorted(GENERATED), "build_directory_exceptions": sorted(SOURCE_BUILD_DIRS),
                      "dependency_authority": "Engine/dep/dependencies.json", "line_definition": "Physical lines (LF-delimited, final unterminated line counts); nonblank excludes whitespace-only lines. Comments, directives and embedded templates count; this is not executable/logical LOC. No include expansion or content deduplication.",
                      "archive_directories_nonrecursive": archive_dirs, "release_directory": release_directory},
            "repository_files": totals(files), "first_party_cpp_including_tests": totals(cpp),
            "first_party_cpp_excluding_tests": totals([row for row in cpp if row["role"] != "tests"]),
            "groups": [dict(owner=key[0], ownership=key[1], role=key[2], language=key[3], **value) for key, value in sorted(groups.items())],
            "existing_release_files": binaries, "staged_archives": archives,
            "delivery_note": "Observed files only; package names are not proof of publication. Existing Release binary may precede current source; pending integration/release is not claimed. Archives are stat-only, not rehashed or extracted.",
            "acceptance_note": "Source volume and dirty-file counts do not measure accepted fixes. Compare future receipts by commit plus exact files; use named build/test/runtime receipts for correctness and model reports for throughput.",
            "skipped_or_unstable": skipped, "files": files}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, required=True, help="Checkout-local JSON receipt")
    parser.add_argument("--archive-dir", action="append", default=[], help="Additional checkout-relative archive directory; no recursive scan")
    parser.add_argument("--release-dir", default="Artifacts/Runtime/windows/development/x64/Release",
                        help="Checkout-relative directory containing the exact Release build")
    args = parser.parse_args()
    root = args.repo_root.resolve(strict=True)
    output = args.output.resolve()
    if not output.is_relative_to(root / "Artifacts/Reports"):
        parser.error("Output must stay under this checkout's Artifacts/Reports")
    for directory in args.archive_dir:
        if not (root / directory).resolve().is_relative_to(root):
            parser.error("Archive directories must stay inside this checkout")
    release = Path(args.release_dir)
    if release.is_absolute() or not (root / release).resolve().is_relative_to(root):
        parser.error("Release directory must stay inside this checkout")
    report = collect(root, args.archive_dir, release.as_posix())
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: report[key] for key in ("source_version", "git_head", "dirty_files_after", "first_party_cpp_including_tests", "first_party_cpp_excluding_tests", "skipped_or_unstable")}, indent=2))
    print(f"Receipt: {output}")


if __name__ == "__main__":
    main()
