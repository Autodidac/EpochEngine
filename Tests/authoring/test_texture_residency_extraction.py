"""Real texture module implementation extraction and pre-extraction behavior parity."""
from pathlib import Path
from Tests.support.cpp import configure_arguments, executable
import hashlib
import json
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
GOLDEN = "416f2291f32ce349593ba552eb8742198a34c7a7ce443482a264454cff562f38"
CONSUMER = r'''
#include <cstdint>
#include <fstream>
#include <string>
import authoring.texture;
namespace epochengine::authoring::texture::contract {int run_texture_document_contract();}
int main(int argc,char** argv){
 if(argc!=2)return 91;
#ifdef DOCUMENT_CONTRACT
 if(const int code=epochengine::authoring::texture::contract::run_texture_document_contract())return code;
#endif
 using namespace epochengine::authoring::texture;
 std::ofstream output(argv[1],std::ios::binary);if(!output)return 92;
 unsigned count{};
 for(unsigned format=0;format<7;++format)for(unsigned size: {0u,16u,64u,4096u})
 for(unsigned preferred=0;preferred<5;++preferred)for(unsigned flags: {0u,1u,7u,15u})for(unsigned budget: {1u,65536u,536870912u}){
  TextureCompileProfile profile;profile.format=static_cast<ArtifactFormat>(format);profile.mipmaps=MipmapPolicy::generate_box_filter;
  const auto artifact=build_compiled_texture_artifact_identity({ContentHash{{1,2,3,4}},7},profile,size,size?size/2:0,1);
  PhysicalResidencyCapabilities caps;caps.standalone_images=(flags&1)!=0;caps.atlas_regions=(flags&2)!=0;
  caps.bindless_images=(flags&4)!=0;caps.sparse_images=(flags&8)!=0;caps.available_bindless_descriptors=128;
  caps.maximum_resident_bytes=budget;
  PhysicalResidencyPolicy policy;policy.preferred=static_cast<PhysicalResidencyKind>(preferred);
  policy.target_resident_bytes=budget;policy.allow_atlas_for_small_textures=true;
  const auto result=plan_physical_residency(artifact,caps,policy);
  output<<count++<<':'<<unsigned(result.status)<<':'<<result.estimated_resident_bytes<<':'<<result.descriptor_count;
  for(const auto word:result.cache_key.words)output<<':'<<word;
  for(const auto& tier:result.tiers)output<<'|'<<unsigned(tier.kind)<<','<<unsigned(tier.first_mip)<<','<<unsigned(tier.mip_count)<<','<<tier.logical_tile_count<<','<<tier.estimated_resident_bytes<<','<<tier.descriptor_count;
  output<<'\n';
 }
 output.flush();return output?0:93;
}
'''


class TextureResidencyExtraction(unittest.TestCase):
    def test_independent_residency_and_document_contracts(self):
        relative = ["Engine/modules/asset.texture_artifact.ixx", "Engine/modules/authoring/authoring.texture.ixx",
                    "Engine/modules/authoring/authoring.texture_detail.ixx",
                    "Engine/src/assets/texture/asset.texture_artifact.cpp",
                    "Engine/src/authoring/texture/authoring.texture_document.cpp",
                    "Engine/src/authoring/texture/authoring.texture_residency.cpp",
                    "Engine/src/authoring/texture/authoring.texture_contract.cpp"]
        identities = {name: hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in relative}
        identity = hashlib.sha256((CONSUMER+json.dumps(identities, sort_keys=True)).encode()).hexdigest()[:12]
        report = ROOT/"Artifacts/Reports/texture-residency-extraction-oct28"/identity
        build = ROOT/"Artifacts/Intermediate/cmake/texture-residency-extraction"/identity
        report.mkdir(parents=True, exist_ok=True)
        (report/"consumer.cpp").write_text(CONSUMER, encoding="utf-8")
        path = lambda name: '"'+(ROOT/name).as_posix()+'"'
        common = "\n".join(path(name) for name in relative[3:4]+relative[5:6])
        full = common+"\n"+"\n".join(path(name) for name in relative[4:5]+relative[6:7])
        modules = " ".join(path(name) for name in relative[:3])
        (report/"CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(TextureResidencyExtraction LANGUAGES CXX)
add_executable(texture_contract consumer.cpp {full})
add_executable(residency_only consumer.cpp {common})
target_compile_definitions(texture_contract PRIVATE DOCUMENT_CONTRACT=1)
foreach(target texture_contract residency_only)
 target_compile_features(${{target}} PRIVATE cxx_std_23)
 if(MSVC)
  target_compile_options(${{target}} PRIVATE /W3 /WX /utf-8 /EHsc /UNDEBUG)
 else()
  target_compile_options(${{target}} PRIVATE -UNDEBUG)
 endif()
 target_include_directories(${{target}} PRIVATE "{(ROOT/'Engine/include').as_posix()}")
 target_sources(${{target}} PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" FILES {modules})
endforeach()
''', encoding="utf-8")
        commands = [configure_arguments(report,build)]
        for mode in ("Debug", "Release"):
            commands.append(["cmake", "--build", str(build), "--config", mode, "--parallel", "1"])
            for target in ("texture_contract", "residency_only"):
                commands.append([str(executable(build,mode,target)), str(report/(mode+"-"+target+".txt"))])
        runs = []
        outputs = []
        for index, command in enumerate(commands):
            process = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, timeout=180)
            (report/f"lane-{index}.log").write_text(process.stdout+process.stderr, encoding="utf-8")
            runs.append({"command":command,"exit":process.returncode})
            result = {"passed":False,"runs":runs,"production_sha256":identities,"outputs":outputs,
                      "scope":"Real independent C++23 implementation unit links without TextureDocument implementation; full existing document contract links separately. Both compare all1680 plan records including cache keys against pre-extraction Debug/Release output."}
            (report/"result.json").write_text(json.dumps(result, indent=2)+"\n",encoding="utf-8")
            self.assertEqual(process.returncode, 0, process.stdout[-6500:]+process.stderr[-2000:])
            if Path(command[0]).name in {"texture_contract", "residency_only", "texture_contract.exe", "residency_only.exe"}:
                output = Path(command[1]).read_bytes()
                digest = hashlib.sha256(output).hexdigest()
                self.assertEqual(len(output.splitlines()), 1680)
                self.assertEqual(digest, GOLDEN)
                outputs.append({"path":command[1],"sha256":digest,"cases":1680})
        result.update(passed=True, outputs=outputs)
        (report/"result.json").write_text(json.dumps(result, indent=2)+"\n",encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
