"""Explicit Windows9B lifecycle acceptance, never auto-discovered or CTest.

Default prepares one private metadata fixture. --build requires a freshly verified
Editor SHA and faithful actual Editor tlog libraries. --run is explicit native
model authorization. No downloads, profile retuning, GUI or global environment
changes. Python is only this manual test driver, not an Engine/runtime dependency.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import re
import subprocess
import time

ROOT=Path(__file__).resolve().parents[2]
KEYS=set("auto_compact batch_size batch_threads compact_target_ratio compact_trigger_ratio context_reserve_tokens context_shift context_tokens display_name early_router effective_context_tokens enabled estimated_bytes expert_cache_mib expert_cache_mode flash_attention format gpu_layer_count_hint gpu_layers gpu_offload_percent id kind kv_cpu_pinned kv_gpu_layers kv_offload kv_type_k kv_type_v lazy_mode load_mode local_path parallel_slots phase_aware_workspace pinned_host_mib profile_revision prompt_cache_mib provider_model quant reasoning_preserve repo runtime runtime_policy source speculative_enabled speculative_model speculative_repo speculative_tokens speculative_type tensor_override threads ubatch_size idle_timeout_minutes".split())

def sha(path):
    h=hashlib.sha256()
    with path.open("rb") as stream:
        for data in iter(lambda:stream.read(1024*1024),b""):h.update(data)
    return h.hexdigest()

def write(path,value):path.write_text(json.dumps(value,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")

def canonical_input(path,directory):
    """Read-only counterpart of the native immutable-input path admission."""
    path=Path(path)
    text=str(path)
    if not path.is_absolute() or ".." in path.parts or not text or len(text)>4096 or any(c in text for c in "\0\r\n;"):
        raise RuntimeError("Invalid explicit native input path")
    resolved=path.resolve(strict=True)
    if os.path.normcase(str(resolved))!=os.path.normcase(str(path)) or path.is_symlink():
        raise RuntimeError("Redirected native input path refused")
    if not (path.is_dir() if directory else path.is_file() and path.stat().st_size>0):
        raise RuntimeError("Native input has wrong file type")
    return resolved

def shared_weights_root(explicit):
    # Capture before LOCALAPPDATA/HOME are replaced for the child. This manual
    # runtime proof needs an existing user store; it must never initialize one.
    if explicit is not None:return canonical_input(explicit,True)
    configured=os.environ.get("EPOCH_AI_SHARED_MODEL_CACHE_ROOT")
    if configured is not None:return canonical_input(configured,True)
    local=os.environ.get("LOCALAPPDATA")
    if local:return canonical_input(Path(local)/"EpochEngine/AI/model-cache",True)
    home=os.environ.get("USERPROFILE")
    if home:return canonical_input(Path(home)/"AppData/Local/EpochEngine/AI/model-cache",True)
    raise RuntimeError("Existing parent native model cache must be supplied explicitly")

def checked(command,log,cwd,timeout=180,env=None):
    if log.exists():log=log.with_stem(log.stem+"-"+str(time.time_ns()))
    began=time.monotonic();result=subprocess.run(command,cwd=cwd,env=env,capture_output=True,text=True,timeout=timeout)
    log.write_text(result.stdout+result.stderr,encoding="utf-8")
    receipt={"command":command,"exit":result.returncode,"seconds":time.monotonic()-began,"log":str(log)}
    if result.returncode:raise RuntimeError("Command failed: "+str(log))
    return receipt

def prepare(args):
    report=args.report_root.absolute()
    if report.resolve()!=report or not report.is_relative_to(ROOT/"Artifacts/Reports"):
        raise RuntimeError("Use a normalized checkout-local Artifacts/Reports directory")
    if not re.fullmatch(r"[A-Za-z0-9_.-]{1,96}",args.model_id):raise RuntimeError("Invalid explicit model ID")
    source=args.profile_manifest.resolve(strict=True);weights=args.weights.resolve(strict=True)
    server=canonical_input(args.server_executable,False)
    shared_cache=shared_weights_root(args.shared_model_cache_root)
    dependency_directories=[]
    for path in (server.parent,*args.runtime_dll_directory):
        admitted=canonical_input(path,True)
        if os.path.normcase(str(admitted)) not in [os.path.normcase(str(p)) for p in dependency_directories]:dependency_directories.append(admitted)
    if len(dependency_directories)>8:raise RuntimeError("Too many native SDK directories")
    shared_environment={"EPOCH_AI_SHARED_MODEL_CACHE_ROOT":str(shared_cache),
        "EPOCH_AI_SHARED_NATIVE_INPUTS_DISABLED":"0","EPOCH_AI_QWEN_FLASH_SERVER":str(server),
        "EPOCH_AI_SHARED_RUNTIME_DLL_DIRS":";".join(str(p) for p in dependency_directories)}
    raw=source.read_bytes()
    if not raw or len(raw)>128*1024:raise RuntimeError("Profile metadata exceeds bounded size")
    data=json.loads(raw)
    if set(data)-KEYS:raise RuntimeError("Unknown profile fields need review; no settings were omitted")
    if data.get("id")!=args.model_id or data.get("runtime")!="qwen-flash" or data.get("source")!="local-gguf":raise RuntimeError("Profile identity differs")
    if Path(data["local_path"]).resolve()!=weights:raise RuntimeError("Profile weights differ from explicit argument")
    if weights.suffix.lower()!=".gguf" or weights.stat().st_size<1024*1024:raise RuntimeError("Expected installed GGUF input")
    with weights.open("rb") as stream:
        if stream.read(4)!=b"GGUF":raise RuntimeError("Weights lack GGUF magic")
    if server.name.lower()!="llama-server.exe":raise RuntimeError("Expected installed native backend executable")
    report.mkdir(parents=True,exist_ok=True)
    private=report/"local-app-data";fixture=private/"EpochEngine/AI/native/models/runtimes"/args.model_id/"manifest.json"
    fixture.parent.mkdir(parents=True,exist_ok=True)
    if fixture.exists() and fixture.read_bytes()!=raw:raise RuntimeError("Existing fixture differs; use a fresh report root")
    if not fixture.exists():fixture.write_bytes(raw)
    for path in (report/"tmp",report/"roaming-app-data",report/"user-profile",report/"cuda-cache"):path.mkdir(exist_ok=True)
    copied_files=[str(fixture)]
    child_fixture=None
    if args.child_roundtrip:
        child=report/"child"
        child_fixture=child/"local-app-data/EpochEngine/AI/native/models/runtimes"/args.model_id/"manifest.json"
        child_fixture.parent.mkdir(parents=True,exist_ok=True)
        if child_fixture.exists() and child_fixture.read_bytes()!=raw:raise RuntimeError("Child fixture differs; use a fresh report root")
        if not child_fixture.exists():child_fixture.write_bytes(raw)
        for path in (child/"tmp",child/"roaming-app-data",child/"user-profile",child/"cuda-cache"):path.mkdir(exist_ok=True)
        copied_files.append(str(child_fixture))
    item={"schema":"epoch.explicit_asset_handoff_preparation.v2","model_id":args.model_id,"profile_source":str(source),
        "source_profile_sha256":sha(source),"fixture_profile":str(fixture),"fixture_profile_sha256":sha(fixture),
        "weights":str(weights),"weights_bytes":weights.stat().st_size,"weights_mtime_ns":weights.stat().st_mtime_ns,
        "server":str(server),"server_bytes":server.stat().st_size,"server_sha256":sha(server),
        "private_local_app_data":str(private),"copied_files":copied_files,"profile_changed":False,
        "child_roundtrip_requested":args.child_roundtrip,"child_fixture_profile":str(child_fixture) if child_fixture else None,
        "child_fixture_profile_sha256":sha(child_fixture) if child_fixture else None,
        "runtime_dll_directories":[str(path) for path in dependency_directories],
        "shared_model_cache_root":str(shared_cache),"shared_inputs_captured_before_isolation":True,
        "shared_native_inputs_environment":shared_environment,
        "model_runtime_started":False,"scope":"Only exact whitelisted native9B profile metadata copied; no weights/runtime/conversation copies."}
    plan=report/"preparation.json"
    if plan.exists():
        old=json.loads(plan.read_text(encoding="utf-8"))
        for key in ("model_id","profile_source","source_profile_sha256","weights","weights_bytes","server","server_sha256",
                "shared_model_cache_root","runtime_dll_directories","shared_native_inputs_environment",
                "child_roundtrip_requested","child_fixture_profile","child_fixture_profile_sha256"):
            if old.get(key)!=item[key]:raise RuntimeError("Retained preparation identity changed: "+key+"; use a fresh report root")
    else:write(plan,item)
    return report,item

def build(args,report,prep):
    if not args.expected_editor_sha256 or not re.fullmatch("[0-9a-f]{64}",args.expected_editor_sha256):raise RuntimeError("Build/run requires explicit accepted Editor SHA256")
    runtime=ROOT/"Artifacts/Runtime/windows/development/x64/Release";editor=runtime/"EpochEditor.exe"
    if sha(editor)!=args.expected_editor_sha256:raise RuntimeError("Editor bytes differ from approved matching build")
    tool=sorted(Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC").glob("*"))[-1]
    sdk=Path("C:/Program Files (x86)/Windows Kits/10");kit=sorted((sdk/"Include").glob("*"))[-1]
    cl=tool/"bin/Hostx64/x64/cl.exe";link=tool/"bin/Hostx64/x64/link.exe"
    source=ROOT/"Tests/ai/ai.asset_handoff_runtime.cpp";obj=report/"handoff.obj";exe=report/"asset-handoff-runtime.exe"
    tlog=ROOT/"Artifacts/Intermediate/msvc/development/x64/Release/EpochEditor/EpochEditor.tlog/link.command.1.tlog"
    lines=tlog.read_text(encoding="utf-16").splitlines()
    header=next(line for line in lines if line.startswith("^"))
    options=next(line for line in lines if line.upper().startswith("/OUT:"))
    # Preserve all ordinary linker/library options, replace only artifact/entry
    # identity and exclude Editor entry/resource objects from this new main.
    options=re.sub(r'/(?:OUT|PDB|IMPLIB):(?:"[^"]*"|\S+)','',options,flags=re.I)
    options=re.sub(r'/(?:SUBSYSTEM|ENTRY):(?:"[^"]*"|\S+)','',options,flags=re.I)
    options=re.sub(r'(?:"[^"]+\.(?:OBJ|RES)"|\S+\.(?:OBJ|RES))(?=\s|$)','',options,flags=re.I)
    project_libraries=[]
    for path in header[1:].split("|"):
        if path.upper().endswith(".LIB") and path.lower() not in [p.lower() for p in project_libraries]:project_libraries.append(path)
    for line in lines:
        if line.upper().endswith(".LIB") and not line.startswith(("^","/")) and line.lower() not in [p.lower() for p in project_libraries]:project_libraries.append(line)
    response=options+'\n'+f'/OUT:"{exe}" /PDB:"{report/"handoff.pdb"}" /IMPLIB:"{report/"handoff.lib"}" /ENTRY:mainCRTStartup /SUBSYSTEM:CONSOLE\n"{obj}"\n'
    response+='\n'.join('"'+p+'"' for p in project_libraries)+'\n'
    for directory in (tool/"lib/x64",sdk/"Lib"/kit.name/"ucrt/x64",sdk/"Lib"/kit.name/"um/x64"):response+=f'/LIBPATH:"{directory}"\n'
    rsp=report/"link.rsp";rsp.write_text(response,encoding="utf-8")
    # Match the imported production IFC's C++/CRT/security/floating environment;
    # /utf-8 would introduce _UTF8 absent from that module's actual command.
    command=[str(cl),"/nologo","/c","/MD","/DNDEBUG","/O2","/std:c++23preview","/permissive-","/fp:precise","/EHsc","/sdl","/W3","/WX","/Fo"+str(obj),
        "/I"+str(ROOT/"Artifacts/Dependencies/vcpkg_installed/x64-windows/include"),"/I"+str(tool/"include")]
    command += ["/I"+str(kit/name) for name in ("ucrt","um","shared","winrt")]
    ifc=ROOT/"Artifacts/Intermediate/msvc/development/x64/Release/EpochEngine/platform.filesystem.ixx.ifc"
    if not ifc.is_file():raise RuntimeError("Matching Release platform.filesystem IFC is required for the real OS-lease check")
    ifc_sha=sha(ifc)
    command += ["/reference","platform.filesystem="+str(ifc),"/ifcSearchDir",str(ifc.parent)]
    command.append(str(source))
    build_env=dict(os.environ)
    build_env["PATH"]=os.pathsep.join((str(tool/"bin/Hostx64/x64"),str(sdk/"bin"/kit.name/"x64"),build_env.get("PATH","")))
    commands=[checked(command,report/"compile.log",ROOT,env=build_env),checked([str(link),"@"+str(rsp)],report/"link.log",ROOT,env=build_env)]
    if sha(editor)!=args.expected_editor_sha256:raise RuntimeError("Editor changed during consumer build")
    if sha(ifc)!=ifc_sha:raise RuntimeError("Imported Release module changed during consumer build")
    libraries={p:sha(Path(p)) for p in project_libraries}
    libraries[str(runtime/"EpochEngine.lib")]=sha(runtime/"EpochEngine.lib")
    write(report/"build-result.json",{"passed":True,"commands":commands,"editor_sha256":args.expected_editor_sha256,
        "link_tlog_sha256":sha(tlog),"source_sha256":sha(source),"link_response_sha256":sha(rsp),
        "module":str(ifc),"module_sha256":ifc_sha,
        "executable":str(exe),"executable_sha256":sha(exe),"libraries":libraries,"runtime_run":False})
    return exe,runtime

def run(args,report,prep,exe,runtime):
    if (report/"consumer-result.json").exists():raise RuntimeError("Refuse repeating an already attempted runtime probe")
    if canonical_input(Path(prep["shared_model_cache_root"]),True)!=Path(prep["shared_model_cache_root"]):
        raise RuntimeError("Shared weight store changed after preparation")
    for directory in prep["runtime_dll_directories"]:canonical_input(directory,True)
    canonical_input(prep["server"],False)
    # No inherited API keys/tokens/provider or project settings. Only owned
    # process environment changes; the user's environment/profile is untouched.
    allowed=("SystemRoot","WINDIR","COMSPEC","PATHEXT","OS","PROCESSOR_ARCHITECTURE","NUMBER_OF_PROCESSORS","PROGRAMDATA","PROGRAMFILES","PROGRAMFILES(X86)","ProgramW6432")
    env={k:os.environ[k] for k in allowed if k in os.environ}
    env.update({"PATH":os.pathsep.join((str(runtime),str(Path(prep["server"]).parent),*prep["runtime_dll_directories"],str(Path(os.environ["SystemRoot"])/"System32"))),
        "LOCALAPPDATA":prep["private_local_app_data"],"APPDATA":str(report/"roaming-app-data"),"USERPROFILE":str(report/"user-profile"),
        "HOME":str(report/"user-profile"),"TMP":str(report/"tmp"),"TEMP":str(report/"tmp"),"CUDA_CACHE_PATH":str(report/"cuda-cache"),
        "EPOCH_AI_QWEN_FLASH_SERVER":prep["server"],"EPOCH_AI_QWEN_FLASH_ENABLED":"true","EPOCH_AI_MODEL_AUTO_LOAD":"false"})
    env.update(prep["shared_native_inputs_environment"])
    command=[str(exe),str(report),args.model_id,prep["fixture_profile"],prep["weights"],prep["server"]]
    if args.child_roundtrip:command.append("--child-roundtrip")
    began=time.monotonic();forced=False
    with (report/"stdout.log").open("w",encoding="utf-8") as out,(report/"stderr.log").open("w",encoding="utf-8") as err:
        process=subprocess.Popen(command,cwd=runtime,env=env,stdout=out,stderr=err,creationflags=subprocess.CREATE_NO_WINDOW)
        write(report/"start.json",{"pid":process.pid,"command":command,"executable_sha256":sha(exe),"started_utc":datetime.datetime.now(datetime.timezone.utc).isoformat(),"deadline_seconds":300,"environment_keys":sorted(env)})
        try:code=process.wait(timeout=330)
        except subprocess.TimeoutExpired:
            # Only this exact held test process. Its native children use owned
            # kill-on-close process jobs; never enumerate/kill unrelated servers.
            if (report/"consumer-result.json").exists():(report/"deadline-last-evidence.json").write_bytes((report/"consumer-result.json").read_bytes())
            forced=True;process.terminate();code=process.wait(timeout=15)
    original_same=sha(Path(prep["profile_source"]))==prep["source_profile_sha256"]
    fixture_same=sha(Path(prep["fixture_profile"]))==prep["fixture_profile_sha256"]
    child_same=not args.child_roundtrip or sha(Path(prep["child_fixture_profile"]))==prep["child_fixture_profile_sha256"]
    write(report/"runtime-result.json",{"passed":code==0 and not forced and original_same and fixture_same and child_same,"pid":process.pid,"exit":code,"process_retired":process.poll() is not None,
        "forced_retirement":forced,"elapsed_seconds":time.monotonic()-began,"original_profile_unchanged":original_same,
        "child_roundtrip_requested":args.child_roundtrip,"parent_fixture_profile_unchanged":fixture_same,"child_fixture_profile_unchanged":child_same,
        "profile_hashes":{"source":sha(Path(prep["profile_source"])),"parent":sha(Path(prep["fixture_profile"])),
            "child":sha(Path(prep["child_fixture_profile"])) if args.child_roundtrip else None},
        "fixture_profile_sha256_after":sha(Path(prep["fixture_profile"])),"scope":
            "Optional actual parent9B/owned-child9B/parent restoration; CPU lease exclusion, separate private metadata, two fixed tiny completions. Not Editor project auto-swap/image/3D/fullpipeline acceptance."
            if args.child_roundtrip else "One explicit real9B stop/restore and one bounded post-restore completion; no-child stage, no image/3D/fullpipeline acceptance."})
    if code!=0 or forced or not original_same or not fixture_same or not child_same:raise RuntimeError("Native acceptance failed; retained runtime-result/consumer-result logs")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--report-root",type=Path,required=True);p.add_argument("--model-id",required=True)
    p.add_argument("--profile-manifest",type=Path,required=True);p.add_argument("--weights",type=Path,required=True)
    p.add_argument("--server-executable",type=Path,required=True);p.add_argument("--expected-editor-sha256")
    p.add_argument("--shared-model-cache-root",type=Path,help="Existing canonical user weight store, captured before child profile isolation; default is parent's native storage root")
    p.add_argument("--runtime-dll-directory",type=Path,action="append",default=[],help="Explicit already-installed SDK dependency directory; no copying or installation")
    p.add_argument("--child-roundtrip",action="store_true",help="Explicit parent9B to owned headless child9B and parent restore; two tiny completions total")
    p.add_argument("--build",action="store_true");p.add_argument("--run",action="store_true")
    args=p.parse_args()
    if os.name!="nt":raise RuntimeError("This explicit PID-handle runtime acceptance is Windows-only")
    report,prep=prepare(args)
    if args.build or args.run:
        exe,runtime=build(args,report,prep)
        if args.run:run(args,report,prep,exe,runtime)
    print(str(report))

if __name__=="__main__":main()
