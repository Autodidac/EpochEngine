"""Run actual manager text/row decisions without a native GUI."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]
class ManagerReadability(unittest.TestCase):
 def test_wrapped_information_measured_rows_and_truthful_availability(self):
  model=(ROOT/'Engine/src/editor/application/editor.frame_model_manager.inl').read_text(encoding='utf-8')
  package=(ROOT/'Engine/src/editor/application/editor.frame_packages_about.inl').read_text(encoding='utf-8')
  info=definition(model,'            const auto modelInfo =',True)
  row=definition(package,'            const auto packageListRowHeight =',True)
  start=package.index('            const std::string requirement =')
  requirement=package[start:package.index(';\n',start)+1]
  start=package.index('            const float packageFooterHeight =')
  footer=package[start:package.index(';\n',start)+1]
  code=r'''
#include <algorithm>
#include <cassert>
#include <string>
#include <string_view>
#include "LAYOUT_HEADER"
namespace gui {
 inline std::string rendered;inline float renderedWidth{},measuredHeight{},measuredButtons{};
 void wrapped_label(std::string_view text,float width){rendered=text;renderedWidth=width;}
 float measure_wrapped_label_height(std::string_view,float){return measuredHeight;}
 float inline_button_row_height(int,float,float,int){return measuredButtons;}
}
namespace package_catalog {enum class Availability{available,descriptor_only};}
struct Catalog {package_catalog::Availability availability;};
struct Package {bool requiresExplicitNetworkApproval;};
std::string availability(bool selectedResource,bool selectedPlan,bool sourceInstallUnavailable,
 const Catalog* selectedCatalog,const Package* selectedPackage,bool selectedInstalled,bool projectOpen){
 struct {std::string projectRoot;} editor{projectOpen?"project":""};
 REQUIREMENT
 return requirement;
}
int main(){
 for(float managerWidth:{260.0f,720.0f,942.0f}){
  MODEL_INFO
  const std::string full="Qwen-3.5-27B-Derestricted-Q3_K_S-with-vision-projector";
  modelInfo("Selected model",full,150);
  assert(gui::rendered=="Selected model: "+full&&gui::renderedWidth==managerWidth);
 }
 const float packageListTextWidth=260;
 ROW_HEIGHT
 gui::measuredHeight=20;assert(packageListRowHeight("short")==60);
 gui::measuredHeight=100;assert(packageListRowHeight("wrapped summary")==140);
 const Catalog ready{package_catalog::Availability::available},listed{package_catalog::Availability::descriptor_only};
 const Package ordinary{false},network{true};
 assert(availability(false,false,false,nullptr,nullptr,false,false)=="Choose a package.");
 assert(availability(false,false,false,&ready,nullptr,false,true).find("cannot be added yet")!=std::string::npos);
 assert(availability(false,false,true,&ready,&ordinary,false,true).find("not available")!=std::string::npos);
 assert(availability(false,false,false,&listed,&ordinary,false,true).find("not available")!=std::string::npos);
 assert(availability(false,false,false,&ready,&network,false,true).find("needs permission")!=std::string::npos);
 assert(availability(false,false,false,&ready,&ordinary,false,false).find("Open a project")!=std::string::npos);
 assert(availability(false,false,false,&ready,&ordinary,false,true).find("Ready to add")!=std::string::npos);
 assert(availability(false,false,false,&ready,&ordinary,true,true)=="Added to this project.");
 assert(availability(true,false,false,&ready,&ordinary,false,true).find("not available")!=std::string::npos);
 assert(availability(true,true,false,&ready,&ordinary,false,true).find("Download available")!=std::string::npos);
 assert(availability(true,true,false,&ready,&ordinary,false,false).find("Open a project")!=std::string::npos);
 assert(epochengine::editor_update_modal::package_manager_layout_contract());
 for(float statusHeight:{20.0f,60.0f})for(float buttonsHeight:{30.0f,66.0f})for(bool download:{false,true}){
  const float contentWidth=400;const std::string requirement="availability";
  const int packageActions=0,packageActionLayout=0;struct {bool packageModelInstall;} editor{download};
  gui::measuredHeight=statusHeight;gui::measuredButtons=buttonsHeight;
  FOOTER
  const auto layout=epochengine::editor_update_modal::measure_package_manager_body(100,700,20,packageFooterHeight);
  assert(layout.footerTop+packageFooterHeight<=700.01f);
  assert(layout.listHeight+layout.detailHeight+32<=layout.footerTop-100.0f+0.01f);
 }
}
'''.replace('MODEL_INFO',info).replace('ROW_HEIGHT',row).replace('REQUIREMENT',requirement).replace('FOOTER',footer).replace('LAYOUT_HEADER',(ROOT/'Engine/src/editor/editor.update_modal_layout.hpp').as_posix())
  parent=ROOT/'build/contract-tests';parent.mkdir(parents=True,exist_ok=True);scratch=Path(tempfile.mkdtemp(prefix='manager-readability-',dir=parent))
  try:
   cpp=scratch/'contract.cpp';binary=scratch/'contract';cpp.write_text(code,encoding='utf-8')
   if os.name=='nt':
    tools=sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'));sdk=Path('C:/Program Files (x86)/Windows Kits/10');kits=sorted((sdk/'Include').glob('*'))
    if not tools or not kits:self.skipTest('MSVC/SDK required')
    tool,kit=tools[-1],kits[-1];binary=scratch/'contract.exe'
    args=[str(tool/'bin/Hostx64/x64/cl.exe'),'/nologo','/utf-8','/std:c++latest','/EHsc','/W4','/WX','/I'+str(tool/'include'),'/I'+str(kit/'ucrt'),'/Fo'+str(scratch/'contract.obj'),'/Fe'+str(binary),str(cpp),'/link','/LIBPATH:'+str(tool/'lib/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'um/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'ucrt/x64')]
   else:
    compiler=shutil.which('clang++') or shutil.which('g++')
    if not compiler:self.skipTest('C++23 compiler required')
    args=[compiler,'-std=c++23',str(cpp),'-o',str(binary)]
   built=subprocess.run(args,cwd=scratch,capture_output=True,text=True,timeout=45);self.assertEqual(built.returncode,0,built.stdout[-6000:]+built.stderr)
   result=subprocess.run([str(binary)],cwd=scratch,capture_output=True,text=True,timeout=10);self.assertEqual(result.returncode,0,result.stdout+result.stderr)
  finally:
   self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()));shutil.rmtree(scratch)
