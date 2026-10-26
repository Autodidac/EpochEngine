#!/usr/bin/env python3
"""Run one bounded real Linux Editor graphics startup check; no pixel-parity claim."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import signal
import subprocess
import sys
import time
import uuid


ROOT = Path(__file__).resolve().parents[2]
READY = re.compile(r"Backend (\w+) reached render-ready state for hwnd=\S+\.")
COMPLETE = "Context session loop ended after the bounded smoke frame budget."
RENDERERS = ("opengl", "sdl", "sfml", "raylib", "vulkan", "software")


def sha256_file(path):
    # Ubuntu22.04 ships Python3.10, before hashlib.file_digest was introduced.
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def classify(log, returncode, timed_out, backend="opengl"):
    """A successful process alone is insufficient: require the selected renderer."""
    if backend not in RENDERERS:
        raise ValueError("Unsupported Linux smoke renderer")
    providers = [match.group(1).casefold() for match in READY.finditer(log)]
    ready = providers == [backend]
    complete = COMPLETE in log
    ordered = ready and complete and log.index(COMPLETE) > READY.search(log).start()
    passed = returncode == 0 and not timed_out and ordered
    return {
        "result": "pass" if passed else "fail",
        "requested_backend": backend,
        "observed_backends": providers,
        "render_ready": ready,
        "bounded_frame_completion": complete,
        "ready_before_completion": bool(ordered),
        "normal_exit": returncode == 0 and not timed_out,
        "timed_out": timed_out,
        "exit_code": returncode,
        "pixel_parity": False,
        "hardware_parity": False,
        "input_parity": False,
    }


def lavapipe_icd(search_roots=(Path("/usr/share/vulkan/icd.d"), Path("/etc/vulkan/icd.d"))):
    """Select only the installed CPU Vulkan provider for this process architecture."""
    architecture = {"AMD64": "x86_64", "arm64": "aarch64"}.get(platform.machine(), platform.machine())
    for root in search_roots:
        for name in (f"lvp_icd.{architecture}.json", "lvp_icd.json"):
            candidate = Path(root) / name
            if candidate.is_file():
                document = json.loads(candidate.read_text(encoding="utf-8"))
                library = document.get("ICD", {}).get("library_path", "")
                if not isinstance(library, str) or "lvp" not in library.casefold():
                    raise ValueError("Installed lavapipe descriptor does not identify the CPU provider")
                return candidate.resolve(), library
    raise ValueError("CPU Vulkan smoke requires Mesa lavapipe (mesa-vulkan-drivers)")


def _sample_process(pid):
    """Linux process observations; unavailable fields remain null, never zero."""
    result = {"resident_bytes": None, "high_water_resident_bytes": None,
              "cpu_seconds": None}
    try:
        status = Path(f"/proc/{pid}/status").read_text()
        for field, key in (("VmRSS", "resident_bytes"), ("VmHWM", "high_water_resident_bytes")):
            match = re.search(rf"^{field}:\s+(\d+) kB$", status, re.MULTILINE)
            if match:
                result[key] = int(match[1]) * 1024
        # comm may contain spaces and parentheses; fields follow the final ')'.
        fields = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()
        result["cpu_seconds"] = (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")
    except (OSError, ValueError, IndexError):
        pass
    return result


def _run_probe(argv, cwd, evidence, timeout_seconds, environment):
    """Exact argv, no shell; retire only this probe's process group on timeout."""
    log_path = evidence / "editor-output.log"
    started = time.monotonic()
    samples = []
    timed_out = False
    with log_path.open("xb") as log:
        process = subprocess.Popen(argv, cwd=cwd, env=environment,
                                   stdin=subprocess.DEVNULL, stdout=log,
                                   stderr=subprocess.STDOUT,
                                   start_new_session=os.name == "posix")
        try:
            while process.poll() is None:
                samples.append({"elapsed_seconds": time.monotonic() - started,
                                **_sample_process(process.pid)})
                if time.monotonic() - started >= timeout_seconds:
                    timed_out = True
                    break
                time.sleep(0.05)
        finally:
            # Also retire the owned session if the runner is cancelled or errors.
            if process.poll() is None:
                if os.name == "posix":
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                else:
                    process.kill()
            process.wait(timeout=5)
    return log_path, process.returncode, timed_out, time.monotonic() - started, samples


def run_smoke(binary, output_root, timeout_seconds=30, software_opengl=False,
              renderer="opengl", software_vulkan=False):
    binary = Path(binary).resolve(strict=True)
    output_root = Path(output_root).resolve()
    if not 1 <= timeout_seconds <= 120:
        raise ValueError("Smoke timeout must be between 1 and 120 seconds")
    if renderer not in RENDERERS:
        raise ValueError("Unsupported Linux smoke renderer")
    if software_vulkan and renderer != "vulkan":
        raise ValueError("The CPU Vulkan provider applies only to the Vulkan smoke")
    if not output_root.is_relative_to((ROOT / "build").resolve()):
        raise ValueError("Smoke evidence must stay under this checkout's build directory")
    if output_root.is_relative_to(binary.parent):
        raise ValueError("Smoke evidence must be outside the runtime directory")
    with binary.open("rb") as source:
        if source.read(6) != b"\x7fELF\x02\x01":
            raise ValueError("Expected a Linux 64-bit little-endian ELF runtime")
    binary_digest = sha256_file(binary)
    if not (binary.parent / "assets/fonts/Roboto-Regular.ttf").is_file():
        raise ValueError("Run the smoke from an asset-bearing runtime directory")
    selected_icd, icd_library = lavapipe_icd() if software_vulkan else (None, None)
    evidence = output_root / (renderer + "-" + uuid.uuid4().hex)
    evidence.mkdir(parents=True, exist_ok=False)
    # Existing trusted startup admission pins this absolute, precreated directory
    # before any saved Editor/model/project state is loaded. Assets still belong
    # to the exact executable above. This is data routing, not OS confinement.
    runtime_data = evidence / "runtime-data"
    runtime_data.mkdir(exist_ok=False)
    environment = dict(os.environ)
    environment["EPOCH_LOG_DIR"] = str(runtime_data / "logs")
    environment["TMPDIR"] = str(evidence / "scratch")
    Path(environment["EPOCH_LOG_DIR"]).mkdir()
    Path(environment["TMPDIR"]).mkdir()
    if software_opengl:
        environment["LIBGL_ALWAYS_SOFTWARE"] = "1"
    if selected_icd:
        environment["VK_DRIVER_FILES"] = str(selected_icd)
        environment["VK_ICD_FILENAMES"] = str(selected_icd)
    argv = [str(binary), "--editor", "--renderer", renderer, "--smoke",
            "--candidate-data-root", str(runtime_data)]
    receipt = {
        "schema": "epoch.linux-graphics-smoke.v1",
        "argv": argv,
        "working_directory": str(binary.parent),
        "runtime_data_root": str(runtime_data),
        "data_boundary": "fresh mutable-data root; not OS confinement",
        "binary_sha256": binary_digest,
        "software_opengl_requested": software_opengl,
        "software_vulkan_requested": software_vulkan,
        "vulkan_icd": str(selected_icd) if selected_icd else None,
        "vulkan_icd_sha256": sha256_file(selected_icd) if selected_icd else None,
        "vulkan_library": icd_library,
        "timeout_seconds": timeout_seconds,
        "resource_scope": "sampled editor process only; excludes children and GPU",
        "hardware_parity": False,
        "pixel_parity": False,
        "input_parity": False,
    }
    try:
        log, code, timed_out, elapsed, samples = _run_probe(
            argv, binary.parent, evidence, timeout_seconds, environment)
        receipt.update(classify(log.read_text(encoding="utf-8", errors="replace"), code, timed_out, renderer))
        receipt.update(elapsed_seconds=elapsed, resource_samples=samples,
                       output_sha256=sha256_file(log))
        for sample_key, receipt_key in (("resident_bytes", "sampled_peak_resident_bytes"),
                                        ("high_water_resident_bytes", "observed_high_water_resident_bytes"),
                                        ("cpu_seconds", "last_observed_cpu_seconds")):
            observed = [s[sample_key] for s in samples if s[sample_key] is not None]
            receipt[receipt_key] = max(observed) if observed else None
    except (OSError, subprocess.SubprocessError) as error:
        receipt.update(result="fail", launch_error=str(error))
    (evidence / "result.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    return receipt, evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--output-root", required=True, type=Path)
    parser.add_argument("--timeout-seconds", type=float, default=30)
    parser.add_argument("--software-opengl", action="store_true")
    parser.add_argument("--renderer", choices=RENDERERS, default="opengl")
    parser.add_argument("--software-vulkan", action="store_true")
    args = parser.parse_args()
    if sys.platform != "linux":
        parser.error("This production graphics lane requires Linux")
    try:
        receipt, evidence = run_smoke(args.binary, args.output_root, args.timeout_seconds,
                                      args.software_opengl, args.renderer, args.software_vulkan)
    except (OSError, ValueError) as error:
        parser.exit(1, f"linux_graphics_smoke.result=fail {error}\n")
    print(f"linux_graphics_smoke.result={receipt['result']} evidence={evidence}")
    return 0 if receipt["result"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
