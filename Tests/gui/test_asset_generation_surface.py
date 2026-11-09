"""Actual workspace draw + GUI panel owner: CPU coverage of owned/preview regions."""
from pathlib import Path
from Tests.support.cpp import compile_and_run
import hashlib,json,subprocess,unittest
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]

class GenerationSurface(unittest.TestCase):
 def test_actual_paint_covers_old_world_except_real_mesh_view(self):
  source=(ROOT/'Engine/src/editor/editor.asset_generation.cpp').read_text(encoding='utf-8')
  viewport=(ROOT/'Engine/src/editor/application/editor.frame_viewport.inl').read_text(encoding='utf-8')
  windows=(ROOT/'Engine/src/epochgui/controls/gui.window_controls.inl').read_text(encoding='utf-8')
  draw=definition(source,'    DrawResult draw(')
  readiness=definition(source,'    std::string readiness_diagnostic(')
  image=(ROOT/'Engine/src/ai/ai.image_generation.cpp').read_text(encoding='utf-8')
  model=(ROOT/'Engine/src/ai/ai.model_generation.cpp').read_text(encoding='utf-8')
  image_validation=definition(image,'        [[nodiscard]] bool valid_text(')+definition(image,'    std::string request_diagnostic(')
  model_validation=definition(model,'        [[nodiscard]] bool digest_valid(')+definition(model,'    std::string request_diagnostic(')
  panel=definition(windows,'    void panel_rect(')
  branch=definition(viewport,'        else if(editor.mainSurface == EditorMainSurface::AssetGeneration)')
  self.assertNotIn('render_timeline_time_controls',branch)
  self.assertNotIn('Run Debug',branch)
  self.assertNotIn('main_surface_title',branch)
  self.assertIn('set_scene_preview_mode(core::ScenePreviewMode::None)',branch)
  self.assertIn('clear_scene_viewport()',branch)
  self.assertIn('clear_object_markers',branch)
  self.assertIn('gui::panel_rect(generated.preview.position,generated.preview.size)',branch)
  code=r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdlib>
#undef assert
// Failed CPU checks must never open an interactive Windows CRT dialog.
#define assert(condition) do { if (!(condition)) std::_Exit(__LINE__); } while (false)
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
UTF8_INCLUDE
struct Sprite{bool is_valid()const{return true;}};
namespace gui {
struct Vec2{float x{},y{};};struct WidgetBounds{Vec2 position{},size{};};
struct Paint{Vec2 position,size;};std::vector<Paint> paints;std::vector<std::string>labels;
struct Frame{bool ctx{true};}g_frame;Vec2 origin{20,30},cursor=origin;
struct Button{std::string label;bool enabled;Vec2 position;};std::vector<Button> buttons;std::string clicked;bool consumed{};
struct ScrollRecord{std::string id;Vec2 position,size;};std::vector<ScrollRecord>scrolls;
std::vector<WidgetBounds>images;
void ensure_resources(){} struct Palette{int panelBackground{1};};const Palette&active_palette(){static Palette p;return p;}
void draw_sprite(int,float x,float y,float w,float h){paints.push_back({{x,y},{w,h}});}
PANEL
Vec2 cursor_position(){return cursor;}void set_cursor(Vec2 p){cursor=p;}
enum class InlineButtonSizing{FitContent};enum class InlineButtonAlignment{Center};
struct InlineButtonSpec{std::string_view label;float width{};bool enabled{true};};
struct Options{InlineButtonSizing sizing{};InlineButtonAlignment alignment{};bool wrap{};float available_width{};};using InlineButtonRowOptions=Options;
float line_height(){return 18.f;}
float measure_section_heading_height(std::string_view,float){return 34.f;}
float measure_wrapped_label_height(std::string_view s,float w){return 18.f*static_cast<float>((s.size()*8u)/static_cast<std::size_t>((std::max)(1.f,w))+1u);}
template<std::size_t N>float inline_button_row_height(const std::array<InlineButtonSpec,N>&,float h,float,Options){return h;}
template<std::size_t N>std::optional<std::size_t>inline_button_row(const std::array<InlineButtonSpec,N>& items,float,float,Options options){
 assert(options.sizing==InlineButtonSizing::FitContent&&options.alignment==InlineButtonAlignment::Center&&options.wrap);
 for(std::size_t i=0;i<N;++i){buttons.push_back({std::string(items[i].label),items[i].enabled,cursor});assert(items[i].width==0.f);
 if(!consumed&&items[i].enabled&&items[i].label==clicked){consumed=true;return i;}}return {};}
struct Scroll{std::string_view id;Vec2 size;bool draw_background{};};bool begin_scroll_area(Scroll s){scrolls.push_back({std::string(s.id),cursor,s.size});return true;}void end_scroll_area(){}
void label(std::string_view s){labels.emplace_back(s);}void section_heading(std::string_view s){label(s);}void wrapped_label(std::string_view s,float){label(s);}
enum class TextInputKeyboardPolicy{focused};
struct Prompt{Vec2 size;std::string placeholder;bool stable;};std::vector<Prompt> prompts;
bool edit_box(std::string&,Vec2 size,std::size_t,bool,TextInputKeyboardPolicy,bool,std::string_view placeholder,bool stable){prompts.push_back({size,std::string(placeholder),stable});return false;}
struct Slider{std::string_view id;float minimum{},maximum{},value{},step{};Vec2 size;};struct SlideResult{bool changed{};float value{};};SlideResult slider(Slider){return {};}
bool toggle_switch(std::string_view,bool&,Vec2){return false;}
struct Select{std::string_view id,placeholder,selected;std::span<const std::string_view>options;Vec2 size;};
struct Selection{bool changed{};std::optional<std::size_t>selected_index;};Selection select_box(Select){return {};}
Sprite register_runtime_surface(std::string_view,std::span<const std::uint8_t>,std::uint32_t,std::uint32_t){return {};}
struct Image{std::string_view id;Sprite sprite;Vec2 size,source_size;std::string_view caption;};int image_box(Image i){images.push_back({cursor,i.size});return 0;}
}
enum class Stage{idle};enum class View{textured,solid,wireframe};
enum class Action{none,load_image_settings,load_model_settings,choose_reference,generate_image,approve_reference,generate_model,import_texture,review_model,import_model,cancel,save_draft,load_draft,open_completed_generation,save_image,save_model,copy_image_path,copy_model_path};
struct Mip{std::vector<std::byte>texels;std::uint32_t width{2},height{2};};struct Image{std::vector<Mip>mips;explicit operator bool()const{return !mips.empty();}};
struct Model{bool ready{};explicit operator bool()const{return ready;}};
struct Output{std::string output_sha256;std::uint64_t vertices{},faces{},textures{},elapsed_ms{};std::filesystem::path output;};
struct Completion{Output result;Image image;Model model;std::string reference_sha256;std::uint64_t wall_ms{};bool recalled_workspace{};};
struct Settings{std::string backend_name;struct Backend{std::string id;bool terms_accepted{};};std::optional<Backend>alternative_model_backend;struct{bool dinov3_terms_accepted{};}model_backend;};
struct ImageRequest{std::string prompt,negative_prompt;std::uint32_t width{512},height{512},steps{4};};
struct ModelRequest{std::uint32_t face_budget{1000},atlas{1024},resolution{512},steps{12},seed{42};std::filesystem::path reference_path;std::string approved_reference_sha256;};
struct State{
 std::optional<Settings>image_settings,model_settings;ImageRequest image_request;ModelRequest model_request;
 std::optional<Completion>reference,generated_model;bool reference_approved{},preview_expanded{},prepared{true};std::string status,image_seed{"42"},model_seed{"42"};
 std::shared_ptr<std::atomic<Stage>>phase{std::make_shared<std::atomic<Stage>>(Stage::idle)};Sprite image_sprite;View view{};
};
struct DrawPermissions{bool project_open{},reference_project_current{},model_project_current{},model_review_ready{};};
struct DrawResult{Action action{};gui::WidgetBounds preview{};bool preview_visible{};};
bool prepare_preview(State&s){return s.prepared&&s.generated_model&&s.generated_model->model;}
std::string stage_text(Stage){return "queued";}std::string path_text(const std::filesystem::path&p){return p.generic_string();}
namespace ai {
namespace asset_jobs{enum class Action{image,model};}
namespace image_generation{using Request=::ImageRequest;namespace core=epochengine::core;
IMAGE_VALIDATION
}
namespace model_generation{using Request=::ModelRequest;
MODEL_VALIDATION
}
}
namespace jobs=ai::asset_jobs;
READINESS
DRAW
bool covered(float x,float y,const DrawResult&r){
 for(const auto&p:gui::paints)if(x>=p.position.x&&y>=p.position.y&&x<p.position.x+p.size.x&&y<p.position.y+p.size.y)return true;
 return r.preview_visible&&x>=r.preview.position.x&&y>=r.preview.position.y&&x<r.preview.position.x+r.preview.size.x&&y<r.preview.position.y+r.preview.size.y;
}
int main(){
 for(const float width:{480.f,900.f})for(const float height:{80.f,500.f})for(const bool model:{false,true})for(const bool prepared:{false,true})for(const bool expanded:{false,true}){
  State s;s.prepared=prepared;s.preview_expanded=expanded;if(model){s.generated_model.emplace();s.generated_model->model.ready=true;}
  gui::paints.clear();gui::labels.clear();gui::buttons.clear();gui::scrolls.clear();gui::cursor=gui::origin;
  const auto result=draw(s,width,height,false,{true,true,true,false});
  // Old World pixels in header, gutter, right edge and timeline must all be covered.
  for(int x=0;x<static_cast<int>(width);++x)for(int y=0;y<static_cast<int>(height);++y)assert(covered(gui::origin.x+x+.5f,gui::origin.y+y+.5f,result));
  if(!model||!prepared)assert(!result.preview_visible);
  if(result.preview_visible){assert(result.preview.position.x>=gui::origin.x&&result.preview.position.y>=gui::origin.y);assert(result.preview.position.x+result.preview.size.x<=gui::origin.x+width);assert(result.preview.position.y+result.preview.size.y<=gui::origin.y+height);}
  assert(std::count(gui::labels.begin(),gui::labels.end(),"Asset Generation")==int(height>=160.f));
  if(!model&&height>=160.f){const auto controls=std::find_if(gui::scrolls.begin(),gui::scrolls.end(),[](const auto&item){return item.id=="asset-generation-controls";});assert(controls!=gui::scrolls.end()&&controls->size.x==width);}
  for(const auto&text:gui::labels)assert(text!="3D Scene"&&text!="Perspective"&&text!="Timeline"&&text!="Run Debug");
 }
 const auto reset=[](){gui::paints.clear();gui::labels.clear();gui::buttons.clear();gui::scrolls.clear();gui::images.clear();gui::prompts.clear();gui::cursor=gui::origin;gui::clicked.clear();gui::consumed=false;};
 const auto enabled=[](std::string_view label){const auto item=std::find_if(gui::buttons.begin(),gui::buttons.end(),[&](const auto&b){return b.label==label;});assert(item!=gui::buttons.end());return item->enabled;};
 State reference;reference.reference.emplace();reference.reference->image.mips.emplace_back();reference.reference_approved=true;
 reference.reference->result.output="reference.png";reference.reference->result.output_sha256=std::string(64,'a');
 reference.model_settings.emplace();reference.model_settings->alternative_model_backend=Settings::Backend{"triposr-pytorch-offline",true};
 reset();auto referenceOnly=draw(reference,900,500,false,{true,true,true,false});assert(!referenceOnly.preview_visible&&!gui::images.empty());
 assert(gui::images.back().position.x>gui::origin.x+440.f&&gui::images.back().size.x>400.f);
 assert(std::find(gui::labels.begin(),gui::labels.end(),"Use a tightly framed object with a transparent background. TripoSR does not remove backgrounds.")!=gui::labels.end());
 // The image owns its approval action. Wide layouts keep it below the image,
 // outside the independently scrolling settings, even in a short workspace.
 for(const float width:{640.f,900.f})for(const float height:{200.f,500.f})for(bool busy:{false,true})for(bool approved:{false,true}){
  State s=reference;s.reference_approved=approved;
  reset();gui::clicked="Approve Reference";const auto result=draw(s,width,height,busy,{});
  assert(gui::images.size()==1);
  const auto& image=gui::images.back();
  const auto approve=std::find_if(gui::buttons.begin(),gui::buttons.end(),[](const auto&b){return b.label=="Approve Reference";});
  assert(approve!=gui::buttons.end()&&std::count_if(gui::buttons.begin(),gui::buttons.end(),[](const auto&b){return b.label=="Approve Reference";})==1);
  assert(approve->position.x==image.position.x&&approve->position.y>=image.position.y+image.size.y);
  assert(approve->position.y+30.f<=gui::origin.y+height);
  assert(approve->enabled==(!busy&&!approved));
  assert((result.action==Action::approve_reference)==(!busy&&!approved));
  assert(s.reference_approved==approved); // Drawing never grants consent.
 }
 for(bool pending:{false,true})for(bool project:{false,true})for(bool current:{false,true})for(bool reviewed:{false,true}){
  State s=reference;s.generated_model.emplace();s.generated_model->model.ready=true;s.status=std::string(8000,'x');
  reset();const auto result=draw(s,900,500,pending,{project,current,current,reviewed});
  assert(enabled("Import as Texture")==(!pending&&project&&current));
  assert(enabled(reviewed?"Approve Project Import":"Review Project Import")==(!pending&&project&&current));
  assert(enabled("Generate 3D")==!pending);
  // Status and Cancel are outside the controls scroll, including long diagnostics.
  const auto controls=std::find_if(gui::scrolls.begin(),gui::scrolls.end(),[](const auto&i){return i.id=="asset-generation-controls";});
  const auto status=std::find_if(gui::scrolls.begin(),gui::scrolls.end(),[](const auto&i){return i.id=="asset-generation-status";});
  assert(controls!=gui::scrolls.end()&&status!=gui::scrolls.end()&&status->position.y<controls->position.y);
  assert(status->size.y<=96.f&&controls->size.y>200.f);
  if(pending)assert(enabled("Cancel Generation"));
  assert(enabled("Save Generation Draft...")==!pending&&enabled("Load Generation Draft...")==!pending);
 }
 for(bool accepted:{false,true}){
  State s=reference;s.model_settings->alternative_model_backend.reset();s.model_settings->model_backend.dinov3_terms_accepted=accepted;
  reset();gui::clicked="Generate 3D";const auto result=draw(s,900,500,false,{});assert(enabled("Generate 3D")==accepted);assert((result.action==Action::generate_model)==accepted);
 }
 for(const auto target:{"Approve Reference","Import as Texture","Generate 3D","Review Project Import","Approve Project Import","Save Generation Draft...","Load Generation Draft..."}){
  State s=reference;s.reference_approved=std::string_view(target)!="Approve Reference";s.generated_model.emplace();s.generated_model->model.ready=true;
  reset();gui::clicked=target;const bool review=std::string_view(target)=="Approve Project Import";
  const auto result=draw(s,900,500,false,{true,true,true,review});assert(result.action!=Action::none);
 }
 State pending=reference;pending.preview_expanded=true;pending.generated_model.emplace();pending.generated_model->model.ready=true;
 reset();gui::clicked="Cancel Generation";assert(draw(pending,480,80,true,{}).action==Action::cancel);
 // Missing setup is visible before clicking; setup and reference buttons are
 // real actions, not disabled no-ops. Valid input dispatches the real stage.
 State image;
 reset();gui::clicked="Set Up 2D Generation...";assert(draw(image,900,500,false,{}).action==Action::load_image_settings&&enabled("Set Up 2D Generation..."));
 reset();gui::clicked="Set Up 2D Generation...";assert(draw(image,900,500,true,{}).action==Action::none&&!enabled("Set Up 2D Generation..."));
 reset();gui::clicked="Generate 2D";assert(draw(image,900,500,false,{}).action==Action::none);
 assert(std::none_of(gui::buttons.begin(),gui::buttons.end(),[](const auto&button){return button.label=="Generate 2D";}));
 assert(std::any_of(gui::labels.begin(),gui::labels.end(),[](const auto&s){return s.find("Load 2D Backend Settings first")!=std::string::npos;}));
 reset();gui::clicked="Load 2D Backend Settings...";assert(draw(image,900,500,false,{}).action==Action::load_image_settings);
 reset();gui::clicked="Load 3D Backend Settings...";assert(draw(image,900,500,false,{}).action==Action::load_model_settings);
 reset();gui::clicked="Choose PNG Reference...";assert(draw(image,900,500,false,{}).action==Action::choose_reference);
 image.image_settings.emplace();reset();draw(image,900,500,false,{});assert(!enabled("Generate 2D"));
 assert(std::find(gui::labels.begin(),gui::labels.end(),"Enter a Main Prompt to enable Generate 2D.")!=gui::labels.end());
 assert(gui::prompts.size()==4&&gui::prompts[0].placeholder=="Main Prompt"&&gui::prompts[1].placeholder=="Avoidance Prompt (optional)");
 for(const auto& prompt:gui::prompts)assert(prompt.stable);
 const float shortPromptHeight=gui::prompts[0].size.y;
 image.image_request.prompt=std::string(1500,'W');reset();draw(image,900,900,false,{});
 const float longPromptHeight=gui::prompts[0].size.y;assert(longPromptHeight>shortPromptHeight&&longPromptHeight<420.f);
 image.image_request.prompt+=std::string(6000,'W');reset();draw(image,900,900,false,{});
 assert(gui::prompts[0].size.y==longPromptHeight); // Rest scrolls inside the field.
 image.image_request.prompt="One brass lantern";
 reset();gui::clicked="Generate 2D";assert(draw(image,900,500,false,{}).action==Action::generate_image&&enabled("Generate 2D"));
 for(const auto seed:{"","-1","1.5","4294967296"}){image.image_seed=seed;reset();gui::clicked="Generate 2D";assert(draw(image,900,500,false,{}).action==Action::none&&!enabled("Generate 2D"));}
 image.image_seed="42";image.image_request.width=2048;image.image_request.height=2048;
 reset();gui::clicked="Generate 2D";assert(draw(image,900,500,false,{}).action==Action::none&&!enabled("Generate 2D"));
 image.image_request.width=image.image_request.height=512;
 reset();gui::clicked="Generate 2D";assert(draw(image,900,500,true,{}).action==Action::none&&!enabled("Generate 2D")&&enabled("Cancel Generation"));
 reference.model_seed="2147483648";reset();gui::clicked="Generate 3D";assert(draw(reference,900,500,false,{}).action==Action::none&&!enabled("Generate 3D"));
}
'''.replace('UTF8_INCLUDE','#include "'+str(ROOT/'Engine/include/core.utf8.hpp').replace('\\','/')+'"').replace('PANEL',panel).replace('IMAGE_VALIDATION',image_validation).replace('MODEL_VALIDATION',model_validation).replace('READINESS',readiness).replace('DRAW',draw)
  proof=ROOT/'Artifacts/Reports/asset-generation-workspace-oct28/surface'/hashlib.sha256(code.encode()).hexdigest()[:12];proof.mkdir(parents=True,exist_ok=True)
  cpp=proof/'surface.cpp';cpp.write_text(code,encoding='utf-8')
  results=compile_and_run(self,cpp,proof)
  (proof/'result.json').write_text(json.dumps({'passed':True,'results':results,'cases':32,'scope':'Actual draw body and actual gui::panel_rect with recording draw/input ports; ownership coverage at normal/compressed/short sizes, valid/unavailable mesh. No GPU or native renderer acceptance.'},indent=2),encoding='utf-8')

if __name__=='__main__':unittest.main()
