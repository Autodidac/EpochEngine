"""Editor ocean uses the central timeline and rejects stale playback branches."""
from pathlib import Path
import json
import unittest
from Tests.platform.test_child_close_router import definition
from Tests.support.cpp import compile_and_run

ROOT=Path(__file__).resolve().parents[2]
OWNER=ROOT/'Engine/src/editor/application/editor.application_packages_creation.inl'

class EditorPlanetaryWaterClock(unittest.TestCase):
    def test_actual_time_policy_and_completion(self):
        owner=OWNER.read_text()
        sample=definition(owner,'        [[nodiscard]] std::optional<double> planetary_preview_sample_time(')
        reusable=definition(owner,'        [[nodiscard]] bool planetary_preview_time_reusable(')
        code=r'''#include <cmath>
#include <limits>
#include <optional>
namespace epochengine::timeline {
enum class PlaybackDirection {reverse=-1,forward=1};
struct TimelineState {double playhead_seconds{},playback_rate{1.0};bool playing{};PlaybackDirection direction{PlaybackDirection::forward};};
}
namespace project_planetary {struct StreamView {std::optional<double> simulation_seconds;};}
SAMPLE
REUSABLE
#define CHECK(x) do {if(!(x))return __LINE__;}while(false)
int main(){
 using namespace epochengine::timeline;
 TimelineState cursor;cursor.playhead_seconds=2.18;
 CHECK(planetary_preview_sample_time(true,cursor)==2.18);
 CHECK(!planetary_preview_sample_time(false,cursor));
 cursor.playing=true;
 CHECK(planetary_preview_sample_time(true,cursor)==2.0);
 cursor.playhead_seconds=2.24;CHECK(planetary_preview_sample_time(true,cursor)==2.0);
 cursor.playhead_seconds=2.26;CHECK(planetary_preview_sample_time(true,cursor)==2.25);
 project_planetary::StreamView requested{2.0},now{2.25};
 CHECK(planetary_preview_time_reusable(now,requested,cursor,true,PlaybackDirection::forward,1.0));
 CHECK(!planetary_preview_time_reusable(now,requested,cursor,false,PlaybackDirection::forward,1.0));
 cursor.playing=false;
 CHECK(!planetary_preview_time_reusable(now,requested,cursor,true,PlaybackDirection::forward,1.0));
 CHECK(!planetary_preview_time_reusable(now,requested,cursor,false,PlaybackDirection::forward,1.0));
 now.simulation_seconds=2.0;CHECK(planetary_preview_time_reusable(now,requested,cursor,false,PlaybackDirection::forward,1.0));
 cursor.playing=true;cursor.direction=PlaybackDirection::reverse;
 CHECK(!planetary_preview_time_reusable(now,requested,cursor,true,PlaybackDirection::forward,1.0));
 now.simulation_seconds=1.75;
 CHECK(planetary_preview_time_reusable(now,requested,cursor,true,PlaybackDirection::reverse,1.0));
 now.simulation_seconds=2.25;CHECK(!planetary_preview_time_reusable(now,requested,cursor,true,PlaybackDirection::reverse,1.0));
 now.simulation_seconds=1.75;cursor.playback_rate=2.0;
 CHECK(!planetary_preview_time_reusable(now,requested,cursor,true,PlaybackDirection::reverse,1.0));
 for(auto bad:{-1.0,1e12,std::numeric_limits<double>::quiet_NaN()}){
  cursor.playhead_seconds=bad;CHECK(!planetary_preview_sample_time(true,cursor));
 }
 project_planetary::StreamView dry;
 CHECK(planetary_preview_time_reusable(dry,dry,cursor,false,PlaybackDirection::forward,1.0));
 CHECK(!planetary_preview_time_reusable(dry,requested,cursor,false,PlaybackDirection::forward,1.0));
}
'''.replace('SAMPLE',sample).replace('REUSABLE',reusable)
        report=ROOT/'Artifacts/Reports/editor-planetary-water-clock-nov06'
        report.mkdir(parents=True,exist_ok=True)
        cpp=report/'clock.contract.cpp';cpp.write_text(code)
        runs=compile_and_run(self,cpp,report,windows_flags=('/W4','/WX'))
        self.assertIn('desiredView.simulation_seconds = planetary_preview_sample_time',owner)
        self.assertIn('&& planetary_preview_time_reusable(desiredView',owner)
        (report/'result.json').write_text(json.dumps({'passed':True,'scope':'Actual Editor timeline sample/coalescing and late-completion branch guards; no GUI/model/second animation clock','runs':runs},indent=2))

if __name__=='__main__':unittest.main()
