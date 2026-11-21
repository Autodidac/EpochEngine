"""Measure the dependency step of the Public Windows source updater on an ephemeral runner."""
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from threading import Event, Thread
import time


PARENT = "f4e8994e7d2cc5f89dc494f851dbd74bfba9ed0d"
WORKER = "619216193fb999e18528fb0d1d69d4b8c9976f8e"
MANIFEST = "bd0016a34b2c56c7074a3aec42494888986e6e2a"
BASELINE = "5f96cd15fd745122cf27e0524606d6c1efc5fd07"
REGISTRY = "23a50b64297c32cc105efc7e2efb32c79f3862ec"
TOOL_SHA = "13b8175e99a884c5ad34249218754b45541a1a63f216e92603aee57a285ac741"
BRANCH = "refs/heads/codex/measure-windows-restore-8470014e"
GIB = 2**30


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def blob(oid):
    return subprocess.check_output(["git", "cat-file", "blob", oid])


def worker_recipe(source):
    # Only literal PowerShell emitted by the pinned Windows worker is admitted.
    strings = [json.loads(m[1]) for m in re.finditer(r'^\s*<< ("(?:[^"\\]|\\.)*")\s*;?\s*$', source, re.M)]
    ps = "".join(strings)
    def section(begin, end):
        require(ps.count(begin) == ps.count(end) == 1, "Worker boundary changed")
        return ps[ps.index(begin):ps.index(end, ps.index(begin))]
    overlay = section("function Stage-CMakePolicyOverlayPort(", "New-Item -ItemType Directory -Path $workRoot -Force")
    args = section("$vcpkgArgs = @('install'", "Invoke-Tool $vcpkgExe $vcpkgArgs")
    require(overlay.endswith("}\n") and args.endswith("}\n"), "Incomplete worker recipe")
    return overlay, args


def script(overlay, args):
    # Provision the installed-registry path selected on the operator's machine.
    return r'''$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$vcpkgRoot=Join-Path $env:EPOCH_PROBE_DIR 'registry'
$manifestRoot=Join-Path $env:EPOCH_PROBE_DIR 'source/Engine'
$managedToolsRoot=Join-Path $env:EPOCH_PROBE_DIR 'tools'
$utf8NoBom=New-Object System.Text.UTF8Encoding($false)
$triplet='x64-windows'
function Write-Step($level,$message) { Write-Host "$level $message" }
function Invoke-Git { & git @args; if ($LASTEXITCODE) { throw "git failed ($LASTEXITCODE)" } }
Invoke-Git init $vcpkgRoot
Invoke-Git -C $vcpkgRoot config core.autocrlf false
Invoke-Git -C $vcpkgRoot remote add origin https://github.com/microsoft/vcpkg.git
Invoke-Git -C $vcpkgRoot fetch --depth 1 origin REGISTRY_PIN BASELINE_PIN
Invoke-Git -C $vcpkgRoot checkout --detach REGISTRY_PIN
& (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics
if ($LASTEXITCODE) { exit $LASTEXITCODE }
$vcpkgExe=Join-Path $vcpkgRoot 'vcpkg.exe'
if ((Get-FileHash -LiteralPath $vcpkgExe).Hash -ne 'TOOL_PIN') { throw 'vcpkg executable identity differs' }
$env:VCPKG_ROOT=$vcpkgRoot
'''.replace("REGISTRY_PIN", REGISTRY).replace("BASELINE_PIN", BASELINE).replace("TOOL_PIN", TOOL_SHA) + overlay + r'''
$overlayRoot=Prepare-CMakePolicyOverlay $vcpkgRoot
if (!$overlayRoot) { throw 'Missing generated overlay' }
Get-ChildItem $overlayRoot -Filter portfile.cmake -Recurse | ForEach-Object {
    Write-Host ("overlay {0} {1}" -f $_.Directory.Name,(Get-FileHash -LiteralPath $_.FullName).Hash)
}
Set-Location -LiteralPath $manifestRoot
''' + args + r'''
$driveFree=@{}
foreach($volume in ($env:EPOCH_PROBE_VOLUMES -split ';')) {
    $driveFree[$volume]=[IO.DriveInfo]::new($volume).AvailableFreeSpace
}
$marker=Join-Path $env:EPOCH_PROBE_DIR 'restore-start.json'
[IO.File]::WriteAllText(($marker+'.tmp'),($driveFree | ConvertTo-Json -Compress),$utf8NoBom)
[IO.File]::Move(($marker+'.tmp'),$marker)
Write-Host "Starting dependency restore $([DateTime]::UtcNow.ToString('o'))"
& $vcpkgExe @vcpkgArgs
exit $LASTEXITCODE
'''


def guard(free, elapsed):
    if min(free.values()) < 4 * GIB:
        return "hosted volume crossed the 4 GiB stop"
    if elapsed >= 115 * 60:
        return "restore exceeded 115 minutes"
    return ""


def sizes(paths):
    result = {}
    for key, base in paths.items():
        total = 0
        for directory, _, files in os.walk(base, followlinks=False):
            for name in files:
                try:
                    total += os.stat(os.path.join(directory, name), follow_symlinks=False).st_size
                except FileNotFoundError:
                    continue
        result[key] = total
    return result


def main():
    env = os.environ
    require(os.name == "nt" and env.get("GITHUB_ACTIONS") == "true"
            and env.get("RUNNER_ENVIRONMENT") == "github-hosted"
            and env.get("GITHUB_REPOSITORY") == "Autodidac/EpochEngine"
            and env.get("GITHUB_REF") == BRANCH and env.get("GITHUB_EVENT_NAME") == "push", "Hosted validation ref required")
    require(subprocess.check_output(["git", "rev-parse", "HEAD^"]).decode().strip() == PARENT, "Unexpected Public parent")
    worker = blob(WORKER)
    manifest = blob(MANIFEST)
    require(json.loads(manifest)["builtin-baseline"] == BASELINE, "Dependency baseline mismatch")
    recipe = script(*worker_recipe(worker.decode("utf-8-sig")))
    bases = [Path(env["RUNNER_TEMP"]), Path(env["LOCALAPPDATA"]) / "Temp"]
    bases = [p for p in bases if p.is_dir()]
    watched = {Path(env[k]).anchor for k in ("RUNNER_TEMP","GITHUB_WORKSPACE","LOCALAPPDATA","SystemRoot","TEMP")}
    require(bases and all(watched), "Missing runner volumes")
    def volumes():
        return {v: shutil.disk_usage(v).free for v in sorted(watched)}
    initial = volumes()
    base = max(bases, key=lambda p: initial[p.anchor])
    require(initial[base.anchor] >= 10*GIB and min(initial.values()) >= 5*GIB, f"Insufficient hosted starting space: {initial}")
    work = Path(tempfile.mkdtemp(prefix="epoch-restore-",dir=base))
    dirs = {n: work/n for n in ("downloads","binary-cache","tmp")}
    for p in dirs.values():
        p.mkdir()
    tree = work/"source/Engine"
    tree.mkdir(parents=True)
    (tree/"vcpkg.json").write_bytes(manifest)
    runner = work/"restore.ps1"
    runner.write_text(recipe, encoding="utf-8-sig")
    paths = {**dirs,"buildtrees":work/"registry/buildtrees","packages":work/"registry/packages",
             "installed":tree/"vcpkg_installed"}
    clean_env = {k:v for k,v in env.items() if not k.startswith(("VCPKG_","X_VCPKG_","ACTIONS_","GITHUB_","GH_"))}
    clean_env.update(EPOCH_PROBE_DIR=str(work),TEMP=str(dirs["tmp"]),TMP=str(dirs["tmp"]),
                     EPOCH_PROBE_VOLUMES=";".join(sorted(watched)),
                     VCPKG_DOWNLOADS=str(dirs["downloads"]),VCPKG_DEFAULT_BINARY_CACHE=str(dirs["binary-cache"]),
                     VCPKG_DISABLE_METRICS="1",VCPKG_MAX_CONCURRENCY="2",GIT_TERMINAL_PROMPT="0")
    peaks = {k:0 for k in paths}
    stop_samples = Event()
    def sample():
        try:
            while not stop_samples.is_set():
                observed = sizes(paths)
                for k,v in observed.items():
                    peaks[k] = max(peaks[k],v)
                stop_samples.wait(20)
        except OSError as error:
            sampler_errors.append(type(error).__name__)
    sampler_errors = []
    sampler = Thread(target=sample,daemon=True)
    start = time.monotonic()
    lowest = initial.copy()
    restore_initial = {}
    restore_lowest = {}
    restore_marker = work/"restore-start.json"
    max_total_growth = 0
    reason = ""
    print("Hosted free bytes at start", initial, "work volume", work.anchor, flush=True)
    child = subprocess.Popen(["powershell.exe","-NoProfile","-NonInteractive","-ExecutionPolicy","Bypass","-File",str(runner)],env=clean_env)
    sampler.start()
    try:
        while True:
            readings = volumes()
            if not restore_initial and restore_marker.exists():
                restore_initial = json.loads(restore_marker.read_bytes())
                require(restore_initial.keys() == initial.keys(), "Restore volumes changed")
                restore_lowest = restore_initial.copy()
            for v,n in readings.items():
                lowest[v] = min(lowest[v],n)
                if restore_lowest:
                    restore_lowest[v] = min(restore_lowest[v],n)
            max_total_growth = max(max_total_growth, sum(initial[v]-n for v,n in readings.items()))
            reason = guard(readings,time.monotonic()-start)
            if sampler_errors:
                reason = "Component measurement failed: " + sampler_errors[0]
            if reason or child.poll() is not None:
                break
            time.sleep(1)
    finally:
        if child.poll() is None:
            subprocess.run(["taskkill","/PID",str(child.pid),"/T","/F"],check=True,timeout=25)
        child.wait(timeout=25)
        stop_samples.set()
        sampler.join(timeout=45)
    require(not sampler.is_alive(),"Component inventory did not retire")
    final = sizes(paths)
    report = dict(utc=datetime.now(timezone.utc).isoformat(), exit=child.returncode, stop=reason,
                  registry=REGISTRY,baseline=BASELINE,tool_sha256=TOOL_SHA,worker_blob=WORKER,manifest_blob=MANIFEST,
                  work_volume=work.anchor,elapsed_seconds=round(time.monotonic()-start,2),
                  initial_free_bytes=initial,min_free_bytes=lowest,physical_growth_bytes={v:initial[v]-n for v,n in lowest.items()},
                  peak_simultaneous_net_growth_bytes=max_total_growth,
                  restore_initial_free_bytes=restore_initial,restore_min_free_bytes=restore_lowest,
                  sampled_component_peak_logical_bytes={k:max(peaks[k],final[k]) for k in final},final_component_logical_bytes=final)
    status = paths["installed"]/"vcpkg/status"
    if status.is_file():
        report["installed_abis"] = sorted(set(re.findall(r"^Abi: ([0-9a-f]{64})$",status.read_text(),re.M)))
    rendered = json.dumps(report,indent=2)
    print(rendered,flush=True)
    with open(env["GITHUB_STEP_SUMMARY"],"a",encoding="utf-8") as out:
        out.write("## Windows dependency restore\n\n```json\n"+rendered+"\n```\n")
    require(not reason and child.returncode == 0 and bool(report.get("installed_abis")), "Incomplete hosted restore")


if __name__ == "__main__":
    main()
