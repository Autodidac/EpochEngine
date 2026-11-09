# SPDX-License-Identifier: LicenseRef-MIT-NoSell
# Copyright (c) 2026 Adam Rushford

from __future__ import annotations

import json
import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
PACKAGES = ROOT / "packages"
REQUIRED = {
    "id",
    "displayName",
    "manifestLicense",
    "manifestCopyright",
    "status",
    "kind",
    "activation",
    "summary",
    "security",
    "source",
    "engineBoundary",
}
SECURITY_REQUIRED = {
    "serverOrListenerAllowed",
    "requiresExplicitNetworkApproval",
    "autoRunAllowed",
    "includeByDefault",
}
SCRIPTED_SOURCE_KINDS = {
    "code-extension",
    "downloadable-source",
}


def load_json(path: pathlib.Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def validate_script_repository(path: pathlib.Path, repository: str) -> list[str]:
    # The package API fixes sourceRepo as the fourth literal in its inert plan.
    # Check the same declaration the compiler reads, not unrelated URL strings.
    plans = re.findall(
        r'\bPackageScriptPlan\s+\w+\s*\{\s*"[^"\n]*"\s*,\s*"[^"\n]*"'
        r'\s*,\s*"[^"\n]*"\s*,\s*"([^"\n]*)"\s*,',
        path.read_text(encoding="utf-8"))
    if len(plans) != 1 or not repository or plans[0] != repository:
        return [f"{path}: script sourceRepo must match manifest source.repo"]
    return []


def validate_manifest(path: pathlib.Path) -> list[str]:
    errors: list[str] = []
    try:
        manifest = load_json(path)
    except json.JSONDecodeError as exc:
        return [f"{path}: invalid JSON: {exc}"]

    missing = sorted(REQUIRED - set(manifest))
    if missing:
        errors.append(f"{path}: missing required fields: {', '.join(missing)}")

    security = manifest.get("security")
    if not isinstance(security, dict):
        errors.append(f"{path}: security must be an object")
        return errors

    missing_security = sorted(SECURITY_REQUIRED - set(security))
    if missing_security:
        errors.append(f"{path}: missing security fields: {', '.join(missing_security)}")

    if security.get("autoRunAllowed") is True:
        errors.append(f"{path}: autoRunAllowed must stay false for package imports")

    if security.get("includeByDefault") is True:
        errors.append(f"{path}: includeByDefault must stay false for optional packages")

    if security.get("serverOrListenerAllowed") is True and security.get("requiresExplicitNetworkApproval") is not True:
        errors.append(f"{path}: server/listener package must require explicit network approval")

    package_id = manifest.get("id")
    if package_id and path.parent.name != package_id:
        errors.append(f"{path}: folder name must match id '{package_id}'")

    if manifest.get("manifestLicense") != "LicenseRef-MIT-NoSell":
        errors.append(f"{path}: manifestLicense must be LicenseRef-MIT-NoSell")

    copyright_notice = str(manifest.get("manifestCopyright", ""))
    if "2026 Adam Rushford" not in copyright_notice:
        errors.append(f"{path}: manifestCopyright must match the EpochEngine copyright year and owner")

    boundary = manifest.get("engineBoundary")
    if isinstance(boundary, dict):
        cache_root = boundary.get("expectedCacheRoot", "")
        if package_id and cache_root != f"cache/packages/{package_id}":
            errors.append(f"{path}: expectedCacheRoot should be cache/packages/{package_id}")

    build = manifest.get("build")
    if isinstance(build, dict):
        source_files = build.get("sourceFiles", [])
        if source_files is not None:
            if not isinstance(source_files, list):
                errors.append(f"{path}: build.sourceFiles must be an array when present")
            else:
                for source_file in source_files:
                    source_path = path.parent / str(source_file)
                    if not source_path.is_file():
                        errors.append(f"{path}: declared build source file is missing: {source_file}")

    script = manifest.get("script")
    if manifest.get("kind") in SCRIPTED_SOURCE_KINDS:
        if not isinstance(script, dict):
            errors.append(f"{path}: source/visual packages must declare a C++23 script object")
        else:
            script_path = str(script.get("path", ""))
            if script.get("language") != "C++23":
                errors.append(f"{path}: script.language must be C++23")
            if script.get("api") != "epoch_package_script_api":
                errors.append(f"{path}: script.api must be epoch_package_script_api")
            if not script_path.endswith(".ascript.cpp"):
                errors.append(f"{path}: script.path must point at a .ascript.cpp runner")
            elif not (path.parent / script_path).is_file():
                errors.append(f"{path}: script.path does not exist: {script_path}")
            else:
                source = manifest.get("source")
                repository = source.get("repo", "") if isinstance(source, dict) else ""
                errors.extend(validate_script_repository(path.parent / script_path, repository))
            if script.get("humanGated") is not True:
                errors.append(f"{path}: package scripts must be human-gated")
            if script.get("autoRunAllowed") is True:
                errors.append(f"{path}: package scripts must not auto-run")

            if script.get("compilesSource") is not True:
                errors.append(f"{path}: visual/source package script must compile source")
            if script.get("livePreview") is not True:
                errors.append(f"{path}: visual/source package script must register a live preview")

    return errors


def main() -> int:
    manifests = sorted(PACKAGES.glob("*/package.epoch-package.json"))
    if not manifests:
        print("No package manifests found.", file=sys.stderr)
        return 1

    errors: list[str] = []
    for manifest in manifests:
        errors.extend(validate_manifest(manifest))

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print(f"Validated {len(manifests)} package manifest(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
