"""Read-only bounded observation of existing PIDs; never starts or stops a model."""
import argparse
import ctypes as c
from ctypes import wintypes as w
from datetime import datetime, timezone
import json
from pathlib import Path
from types import SimpleNamespace
import time
import benchmark_windows as metrics


def observe(pids, seconds, output):
    root = Path(__file__).resolve().parents[3]
    output = output.resolve()
    if not output.is_relative_to(root / "Artifacts" / "Reports"):
        raise ValueError("Observation output must stay in checkout-local Artifacts/Reports")
    if output.exists():
        raise ValueError("Observation output already exists")
    kernel = c.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
    kernel.OpenProcess.restype = w.HANDLE
    kernel.CloseHandle.argtypes = [w.HANDLE]
    kernel.QueryFullProcessImageNameW.argtypes = [w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD)]
    nvml = c.WinDLL(r"C:\Windows\System32\nvml.dll")
    handles = {}
    counters = None
    initialized = False
    result = {"started_utc": datetime.now(timezone.utc).isoformat(), "phase": "unknown; observational samples only", "processes": {}, "samples": []}
    try:
        if nvml.nvmlInit_v2():
            raise RuntimeError("NVML unavailable")
        initialized = True
        device = c.c_void_p()
        if nvml.nvmlDeviceGetHandleByIndex_v2(0, c.byref(device)):
            raise RuntimeError("NVML adapter unavailable")
        for pid in pids:
            handle = kernel.OpenProcess(0x1000 | 0x10, False, pid)
            if not handle:
                raise c.WinError(c.get_last_error())
            handles[pid] = handle
            name = c.create_unicode_buffer(32768)
            size = w.DWORD(len(name))
            if not kernel.QueryFullProcessImageNameW(handle, 0, name, c.byref(size)):
                raise c.WinError(c.get_last_error())
            result["processes"][str(pid)] = name.value
        counters = metrics.GpuProcessCounters()
        previous = {}
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            time.sleep(min(1, max(0, deadline-time.monotonic())))
            counters.pdh.PdhCollectQueryData(counters.query)
            engines = counters._read_counter(counters.counter) or []
            memory = {k: counters._read_counter(v) for k,v in counters.memory_counters.items()}
            row = {"seconds": seconds-(deadline-time.monotonic()), "processes": {}}
            for pid, handle in handles.items():
                data = metrics.sample(kernel, nvml, device, SimpleNamespace(_handle=handle))
                if pid in previous:
                    data.update(metrics.cpu_interval(previous[pid], data))
                previous[pid] = data
                data.update(metrics.gpu_attribution(engines, pid))
                data.update(metrics.gpu_memory_attribution(memory.get("dedicated"), memory.get("shared"), pid))
                data["owned_engines"] = {n:v for n,v in engines if n.startswith(f"pid_{pid}_")}
                row["processes"][str(pid)] = data
            result["samples"].append(row)
    finally:
        if counters:
            counters.close()
        for handle in handles.values():
            kernel.CloseHandle(handle)
        if initialized:
            nvml.nvmlShutdown()
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(result, indent=2), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, action="append", required=True)
    parser.add_argument("--seconds", type=int, choices=range(1,61), default=30)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    observe(args.pid, args.seconds, args.output)
