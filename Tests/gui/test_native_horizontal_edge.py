"""Production edge geometry, independent monitor bounds and native dispatch scope."""
from pathlib import Path
import json
import tempfile
import unittest
from Tests.support.cpp import compile_and_run
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]
class NativeHorizontalEdge(unittest.TestCase):
 def test_actual_edge_geometry(self):
  owner=ROOT/'Engine/dep/EpochPlatformEngine/src/platform/platform.window.cpp'
  function=definition(owner.read_text(encoding='utf-8'),'        [[nodiscard]] bool horizontal_edge_bounds(')
  code=r"""
#include <algorithm>
// Win32 defines near as an empty compatibility macro.
#define near
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
FUNCTION
int main(){long a{},b{};
 CHECK(horizontal_edge_bounds(300,900,0,1920,2,true,a,b)&&a==0&&b==900);
 CHECK(horizontal_edge_bounds(a,b,0,1920,2,true,a,b)&&a==0&&b==1920);
 CHECK(!horizontal_edge_bounds(a,b,0,1920,2,true,a,b));
 CHECK(horizontal_edge_bounds(300,900,0,1920,2,false,a,b)&&a==300&&b==1920);
 CHECK(horizontal_edge_bounds(a,b,0,1920,2,false,a,b)&&a==0&&b==1920);
 CHECK(horizontal_edge_bounds(-1700,-900,-1920,0,2,true,a,b)&&a==-1920&&b==-900);
 CHECK(horizontal_edge_bounds(2200,2900,1920,3840,2,false,a,b)&&a==2200&&b==3840);
 CHECK(horizontal_edge_bounds(-200,800,0,1920,2,true,a,b)&&a==0&&b==800);
 CHECK(horizontal_edge_bounds(a,b,0,1920,2,true,a,b)&&a==0&&b==1920);
 CHECK(horizontal_edge_bounds(300,2100,0,1920,2,false,a,b)&&a==300&&b==1920);
 CHECK(horizontal_edge_bounds(a,b,0,1920,2,false,a,b)&&a==0&&b==1920);
 CHECK(horizontal_edge_bounds(-100,900,0,1920,2,false,a,b)&&a==0&&b==1920);
 CHECK(horizontal_edge_bounds(100,2100,0,1920,2,true,a,b)&&a==0&&b==1920);
 CHECK(horizontal_edge_bounds(-400,2500,0,1920,2,true,a,b)&&a==0&&b==1920);
 CHECK(horizontal_edge_bounds(3,900,0,1920,4,true,a,b)&&a==3&&b==1920);
 CHECK(horizontal_edge_bounds(3,900,0,1920,2,true,a,b)&&a==0&&b==900);
 CHECK(!horizontal_edge_bounds(900,300,0,1920,2,true,a,b));
 CHECK(!horizontal_edge_bounds(300,900,1920,1920,2,true,a,b));
}
""".replace('FUNCTION',function)
  parent=ROOT/'Artifacts/Reports/native-horizontal-edge';parent.mkdir(parents=True,exist_ok=True)
  report=Path(tempfile.mkdtemp(prefix='contract-',dir=parent));source=report/'contract.cpp';source.write_text(code,encoding='utf-8')
  results=compile_and_run(self,source,report)
  (report/'result.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
if __name__=='__main__':unittest.main()
