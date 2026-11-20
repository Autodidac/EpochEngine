"""Exercise the actual Minesweeper input/reveal owner without graphics.

EPOCH_MINESWEEPER_SOURCE_ROOT may select a retained candidate inside this checkout.
The grid helpers, GameState and complete frame method are compiled from that
source. Mouse/key/window and rendering access use recorded dependency ports. The test is
red until every scenario passes; a compiler/build success is not game acceptance.
"""
from pathlib import Path
import hashlib
import json
import os
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]

CONTRACT = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <random>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
ACTUAL_GRID
namespace gamecore=epochengine::gamecore;
namespace input {enum class Key{Escape};enum class MouseButton{MouseLeft};}
struct TextureAtlas{};
namespace atlasmanager {
std::vector<const TextureAtlas*> get_atlas_vector_snapshot(){return {};}
}
namespace spritepool {bool is_alive(int){return true;}}
namespace core {
struct WindowData{};
struct Context {
 int x{},y{},width{160},height{160};bool down{},escape{};
 int clears{},presents{},draws{};
 bool is_key_down_safe(input::Key)const{return escape;}
 void get_mouse_position_safe(int& mx,int& my)const{mx=x;my=y;}
 bool is_mouse_button_down_safe(input::MouseButton)const{return down;}
 int get_width_safe()const{return width;}
 int get_height_safe()const{return height;}
 void clear_safe(){++clears;}
 void present_safe(){++presents;}
 template<class... Args>void draw_sprite_safe(Args&&...){++draws;}
};}
constexpr int GRID_W=16,GRID_H=16,MINES=40;
ACTUAL_STATE
struct Harness {
 GameState state;bool gameOver{},mouseWasDown{};
 std::unordered_map<std::string,int> sprites{};
 ACTUAL_INPUT
};
void board(Harness& game,std::initializer_list<std::pair<int,int>> mines={}) {
 std::fill(game.state.mine.begin(),game.state.mine.end(),false);
 std::fill(game.state.revealed.begin(),game.state.revealed.end(),false);
 std::fill(game.state.count.begin(),game.state.count.end(),0);
 game.gameOver=false;game.mouseWasDown=false;
 for(auto [x,y]:mines)game.state.mine[gamecore::idx(GRID_W,x,y)]=true;
 // Fixture counts follow the existing owner's four-neighbor grid contract.
 // This does not implement or stand in for the reveal algorithm under test.
 for(std::size_t y=0;y<GRID_H;++y)for(std::size_t x=0;x<GRID_W;++x)
  for(auto [nx,ny]:gamecore::neighbors(GRID_W,GRID_H,x,y))
   if(game.state.mine[gamecore::idx(GRID_W,nx,ny)])++game.state.count[gamecore::idx(GRID_W,x,y)];
}
bool click(Harness& game,int x,int y,bool down=true) {
 auto context=std::make_shared<core::Context>();context->x=x;context->y=y;context->down=down;
 return game.frame(context,nullptr);
}
bool shown(const Harness& game,int x,int y){return game.state.revealed[gamecore::idx(GRID_W,x,y)];}
std::size_t visible(const Harness& game){return std::count(game.state.revealed.begin(),game.state.revealed.end(),true);}
void report(const char* name,bool passed){std::printf("CASE %s %s\n",name,passed?"PASS":"FAIL");}
int main(){
 Harness game;
 board(game);const bool emptyRunning=click(game,85,85);
 report("all_empty_cascade",visible(game)==256&&!emptyRunning&&game.state.all_clear()&&!game.gameOver);
 board(game,{{8,6},{10,8},{8,10},{6,8}});click(game,85,85);
 report("isolated_zero_seed",game.state.count[gamecore::idx(GRID_W,8,8)]==0&&shown(game,8,8));
 report("isolated_zero_exact_frontier",visible(game)==5&&shown(game,8,7)&&shown(game,9,8)&&shown(game,8,9)&&shown(game,7,8));
 report("cascade_keeps_mines_covered",!shown(game,8,6)&&!shown(game,10,8)&&!shown(game,8,10)&&!shown(game,6,8)&&!game.gameOver);
 board(game,{{0,2},{2,0}});click(game,5,5);
 report("isolated_corner_zero",visible(game)==3&&shown(game,0,0)&&shown(game,1,0)&&shown(game,0,1));
 board(game,{{8,7}});const bool numberedRunning=click(game,85,85);
 report("numbered_cell",visible(game)==1&&shown(game,8,8)&&numberedRunning&&!game.gameOver);
 click(game,85,85,false);click(game,85,85);
 report("repeated_numbered_click",visible(game)==1&&shown(game,8,8)&&!game.gameOver);
 click(game,85,65);report("held_click_does_not_reveal_new_cell",visible(game)==1&&!shown(game,8,6));
 click(game,85,65,false);click(game,85,65);
 report("release_rearms_click",visible(game)==2&&shown(game,8,6));
 board(game);game.state.revealed[gamecore::idx(GRID_W,8,8)]=true;click(game,85,85);
 report("already_revealed_zero_is_unchanged",visible(game)==1&&shown(game,8,8));
 board(game,{{8,8}});const bool mineRunning=click(game,85,85);
 report("mine_stops_game",visible(game)==1&&shown(game,8,8)&&game.gameOver&&!mineRunning);
 click(game,85,75,false);click(game,85,75);
 report("game_over_ignores_further_clicks",visible(game)==1&&shown(game,8,8)&&game.gameOver);
 const std::array<std::pair<const char*,std::pair<int,int>>,8> outside{{
  {"negative_subcell_x",{-1,5}},{"negative_subcell_y",{5,-1}},
  {"negative_fullcell_x",{-10,5}},{"negative_fullcell_y",{5,-10}},
  {"right_boundary",{160,5}},{"bottom_boundary",{5,160}},
  {"far_outside",{10000,10000}},{"negative_both",{-1,-1}}}};
 for(const auto& [name,position]:outside){board(game,{{0,1}});click(game,position.first,position.second);report(name,visible(game)==0&&!game.gameOver);}
 board(game,{{0,1}});
 auto outsideContext=std::make_shared<core::Context>();
 outsideContext->x=-1;outsideContext->y=5;outsideContext->down=true;
 game.frame(outsideContext,nullptr);
 report("outside_click_keeps_frame_presentation",outsideContext->clears==1&&outsideContext->presents==1&&visible(game)==0);
 board(game);report("null_context_stops",!game.frame({},nullptr)&&visible(game)==0);
 return 0; // The Python assertion below checks ALL reported cases in both builds.
}
'''


class MinesweeperReveal(unittest.TestCase):
    def test_actual_reveal_and_input_edges(self):
        selected = Path(os.environ.get("EPOCH_MINESWEEPER_SOURCE_ROOT", ROOT)).resolve()
        self.assertTrue(selected == ROOT or ROOT in selected.parents,
                        "Candidate source must remain inside this checkout")
        owner = selected / "Engine/modules/game/game.minesweeper_like.ixx"
        grid_owner = selected / "Engine/modules/game/game.core.ixx"
        source = owner.read_text(encoding="utf-8")
        grid = grid_owner.read_text(encoding="utf-8").replace("module;", "").replace(
            "export module game.core;", "").replace("export namespace", "namespace")
        state = definition(source, "    struct GameState", True)
        frame = definition(source, "        bool frame(")
        # Keep the complete method: a model may add clear/present calls or an
        # early input return, so a first-render-call substring is not a boundary.
        actual_input = frame.replace(" override", "")
        code = CONTRACT.replace("ACTUAL_GRID", grid).replace("ACTUAL_STATE", state).replace("ACTUAL_INPUT", actual_input)
        identity = hashlib.sha256((str(selected)+code).encode()).hexdigest()[:12]
        report = ROOT / "Artifacts/Reports/minesweeper-reveal-nov01" / identity
        report.mkdir(parents=True, exist_ok=True)
        consumer = report / "consumer.cpp"
        consumer.write_text(code, encoding="utf-8")
        runs = compile_and_run(self, consumer, report, compile_timeout=90)
        results = []
        for configuration in ("Debug", "Release"):
            output = (report / (configuration+"-run.log")).read_text(encoding="utf-8")
            cases = [dict(name=line.split()[1], passed=line.split()[2]=="PASS")
                     for line in output.splitlines() if line.startswith("CASE ")]
            self.assertEqual(len(cases), 22, output)
            results.append(dict(configuration=configuration, cases=cases,
                                passed=all(case["passed"] for case in cases)))
        receipt = dict(passed=all(result["passed"] for result in results), source_root=str(selected),
                       owner_sha256={str(path.relative_to(selected)):hashlib.sha256(path.read_bytes()).hexdigest()
                                     for path in (owner,grid_owner)}, results=results, compiler_runs=runs,
                       scope="Actual production GameState and complete frame plus actual game.core helpers. Recorded mouse/key/window/render ports; no graphics, scene registration, model or native Editor launch.")
        (report / "result.json").write_text(json.dumps(receipt,indent=2)+"\n",encoding="utf-8")
        failures=[result["configuration"]+":"+case["name"] for result in results
                  for case in result["cases"] if not case["passed"]]
        self.assertFalse(failures, "Actual Minesweeper regressions: "+", ".join(failures)+"; receipt "+str(report/"result.json"))


if __name__ == "__main__":
    unittest.main()
