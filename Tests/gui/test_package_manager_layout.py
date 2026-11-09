"""Actual package review/catalog drawing and callbacks with a recording GUI port."""
from pathlib import Path
import hashlib
import json
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]
PANEL = ROOT / "Engine/src/editor/application/editor.frame_packages_about.inl"
LAYOUT = ROOT / "Engine/src/editor/editor.update_modal_layout.hpp"
BOOTSTRAP = ROOT / "Engine/src/editor/application/editor.frame_bootstrap.inl"


def consumer():
    panel = PANEL.read_text(encoding="utf-8")
    review = definition(panel, "            if (editor.packageSourceView)")
    row = definition(panel, "            const auto packageListRowHeight =", True)
    start = panel.index("            const float packageFooterHeight =")
    end = panel.index("            gui::set_cursor({modalPos.x + 28.0f, packageBodyLayout.footerTop});", start)
    catalog = panel[start:end]
    bootstrap = BOOTSTRAP.read_text(encoding="utf-8")
    sizing_start = bootstrap.index("        const gui::Vec2 packageManagerModalSize")
    sizing = bootstrap[sizing_start:bootstrap.index(";", sizing_start) + 1]
    return PREFIX.replace("LAYOUT_HEADER", LAYOUT.as_posix()) + r'''
gui::Vec2 modal_size(float w,float h){
 const auto fit_modal_size=[&](gui::Vec2 wanted,gui::Vec2 minimum){
  const auto size=layout::fit_modal_size({w,h},{wanted.x,wanted.y},{minimum.x,minimum.y});return gui::Vec2{size.x,size.y};};
''' + sizing + r'''
 return packageManagerModalSize;
}
void review_draw(EditorState& editor,float width,float height){
 gui::reset();const float contentWidth=width,packageContentBottom=height;
 const gui::Vec2 modalPos{};const gui::InlineButtonRowOptions packageActionLayout{
  .sizing=gui::InlineButtonSizing::FitContent,.alignment=gui::InlineButtonAlignment::Center,.wrap=true,.available_width=width};
 Catalog* selectedCatalog=nullptr;void* ctx=nullptr;
''' + review + r'''
}
auto catalog_draw(EditorState&editor,float contentWidth,float packageContentBottom){
 gui::reset();const gui::Vec2 modalPos{};const bool projectAiResources=false,selectedResource=false;
 const gui::InlineButtonRowOptions packageActionLayout{.sizing=gui::InlineButtonSizing::FitContent,
  .alignment=gui::InlineButtonAlignment::Center,.wrap=true,.available_width=contentWidth};
 const std::array packageActions{gui::InlineButtonSpec{.label="Review Extension"},gui::InlineButtonSpec{.label="Close"}};
 std::vector<Catalog>catalogEntries{{"planet","Planetary Terrain","Generate an editable planet and its ocean."}};
 std::vector<Package>resources{{"forest","Forest Factory","Paint procedural forest areas.","local-source"}};
 const Catalog*selectedCatalog=&catalogEntries.front();const Package*selectedPackage=nullptr;
 const std::string selectedName="Planetary Terrain",purpose="Generate terrain.",requirement="Review these exact files before adding them.",destination="project";
 const float packageListPaneWidth=contentWidth>=900?(std::min)(600.f,contentWidth*.44f):contentWidth;
 const float packageListTextWidth=(std::max)(1.f,packageListPaneWidth-24.f);
''' + row + r'''
 const float packageListContentHeight=packageListRowHeight(catalogEntries.front().summary)+packageListRowHeight(resources.front().summary)+12;
 const unsigned visibleCount=2;
 const auto visible_in_catalog=[](const Catalog&){return true;};const auto visible_resource=[](const Package&){return false;};
 const auto visible_local_package=[](const Package&){return true;};
 const auto select_catalog_entry=[&](const Catalog&){++editor.catalogSelections;};
 const auto select_package=[&](const Package&){++editor.catalogSelections;};
''' + catalog + r'''
 return packageBodyLayout;
}
int main(){
 for(auto viewport:std::array<gui::Vec2,4>{{{640,480},{1280,720},{1920,1080},{3840,2160}}}){
  const auto size=modal_size(viewport.x,viewport.y);CHECK(size.x<=viewport.x-64&&size.y<=viewport.y-64);
  CHECK(size.x>=1&&size.y>=1);
 }
 EditorState editor;editor.packageSourceReview=Review{};editor.packageSourceReview->files.resize(46);
 editor.packageSourceReviewCurrent=true;editor.packageSourceExpectedId="research_planetary_terrain";
 float previousHeight{};
 for(float height:{420.f,800.f,1300.f}){
  review_draw(editor,600,height);
  const auto body=gui::area("package-source-review"),files=gui::area("package-source-files");
  CHECK(body.size.y>100&&body.position.y>gui::primary.position.y+gui::primary.size.y);
  CHECK(body.position.y+body.size.y<=height&&files.size.y>previousHeight);previousHeight=files.size.y;
  CHECK(gui::primary.label=="Add to Project"&&gui::primary.enabled&&gui::primary.size.x<300);
  CHECK(gui::labels_have("Waiting for your approval")&&gui::labels_have("46 checked source files"));
  CHECK(gui::primaryRows==1&&gui::primary.centered&&gui::primary.textSized);
 }
 gui::requested="Add to Project";review_draw(editor,600,700);CHECK(editor.attachments==1&&!editor.update);
 CHECK(editor.acceptedHash=="reviewed-content"&&editor.packageSourceReview->files.size()==46);
 editor.planetaryAttachment.attached=true;editor.planetaryAttachment.registry_package_id="research_planetary_terrain";
 editor.planetaryAttachment.content_hash="old-content";gui::requested="Update Project Source";review_draw(editor,600,700);
 CHECK(editor.attachments==2&&editor.update&&editor.acceptedHash=="reviewed-content");
 editor.packageSourceReviewCurrent=false;gui::requested="Check Source Again";review_draw(editor,360,420);
 CHECK(editor.checks==1&&editor.attachments==2&&gui::primary.label=="Check Source Again");
 editor.packageSourceReviewCurrent=true;editor.busy=true;gui::requested="Update Project Source";review_draw(editor,600,700);
 CHECK(editor.attachments==2&&!gui::primary.enabled&&gui::labels_have("Checking extension files"));
 gui::requested="Cancel Operation";review_draw(editor,600,700);CHECK(editor.cancels==1);editor.busy=false;
 editor.packageSourceExpectedId="research_fft_ocean";editor.planetaryFeatureRequest=true;
 editor.packageSourceReview->registry_package_id="research_fft_ocean";gui::requested="Include Water & Continue";
 review_draw(editor,600,700);CHECK(editor.water==1);editor.planetaryFeatureRequest=false;
 editor.packageSourceExpectedId.clear();gui::requested="Approve & Stage Source";editor.packageSourceStaged.staged=false;
 review_draw(editor,600,700);CHECK(editor.stages==1);
 editor.packageSourceExpectedId="research_planetary_terrain";editor.projectRoot.clear();
 editor.packageSourceReview->registry_package_id="research_planetary_terrain";editor.packageSourceStaged.staged=true;
 gui::requested="Create Terrain Project";review_draw(editor,600,700);CHECK(editor.creations==1);
 for(float width:{360.f,600.f,950.f,1500.f})for(float height:{420.f,900.f,2000.f}){
  const auto measured=catalog_draw(editor,width,height);const auto list=gui::area("package-manager-package-list");
  CHECK(list.size.x>=1&&list.size.y>0&&list.position.x+list.size.x<=width);
  if(width>=900){const auto detail=gui::area("package-manager-detail-scroll");
   CHECK(detail.position.x>=list.position.x+list.size.x+15&&detail.position.y==list.position.y+24);
   CHECK(detail.position.x+detail.size.x<=width+.01f&&detail.position.y+detail.size.y<=measured.footerTop);
   CHECK(list.size.y>height*.5f);
  }else if(gui::has_area("package-manager-detail-scroll")){
   const auto detail=gui::area("package-manager-detail-scroll");CHECK(detail.position.y>=list.position.y+list.size.y);
  }
  if(height==2000&&width==600)CHECK(list.size.y>620);
 }
 gui::requested="Planetary Terrain";catalog_draw(editor,1000,900);CHECK(editor.catalogSelections==1);
 CHECK(layout::package_manager_layout_contract());
}
'''


PREFIX = r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "LAYOUT_HEADER"
namespace layout=epochengine::editor_update_modal;
namespace editor_update_modal=epochengine::editor_update_modal;
void check(bool result,int line){if(!result){std::fprintf(stderr,"Package layout contract line %d\n",line);std::exit(83);}}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),__LINE__)
namespace gui{
 struct Vec2{float x{},y{};};enum class SemanticTone{info,warning,success};
 enum class InlineButtonSizing{FitContent};enum class InlineButtonAlignment{Center};
 struct InlineButtonSpec{std::string_view label;float width{};bool enabled{true};};
 struct InlineButtonRowOptions{InlineButtonSizing sizing{};InlineButtonAlignment alignment{};bool wrap{};float available_width{};};
 struct ScrollAreaOptions{std::string_view id;Vec2 size;float content_height{};bool draw_background{},show_scrollbar{};};
 struct ScrollTextPanelOptions{std::string_view id;Vec2 size;std::span<const std::string>lines;unsigned max_line_chars{};bool selectable{},stick_to_bottom{},wrap_lines{};};
 struct MenuItemOptions{std::string_view label;Vec2 size;SemanticTone tone;bool selected{};};
 struct Area{std::string id;Vec2 position,size;};struct Button{std::string label;Vec2 position,size;bool enabled{},centered{},textSized{};};
 Vec2 cursor{0,50};std::vector<Area>areas,stack;std::vector<std::string>labels;Button primary;unsigned primaryRows{};std::string requested;
 void reset(){cursor={0,50};areas.clear();stack.clear();labels.clear();primary={};primaryRows=0;}
 Vec2 cursor_position(){return cursor;}void set_cursor(Vec2 v){cursor=v;}void advance_cursor(Vec2 v){cursor.x+=v.x;cursor.y+=v.y;}
 float measure_wrapped_label_height(std::string_view text,float width){return text.empty()?0.f:20.f*(std::max)(1.f,std::ceil(float(text.size())*7.f/(std::max)(1.f,width)));}
 void wrapped_label(std::string_view text,float width){labels.emplace_back(text);cursor.y+=measure_wrapped_label_height(text,width)+4;}
 void label(std::string_view text){wrapped_label(text,600);}
 void section_heading(std::string_view text,SemanticTone=SemanticTone::info){labels.emplace_back(text);cursor.y+=24;}
 float measure_section_heading_height(std::string_view,float){return 24;}float menu_item_row_height(){return 28;}
 struct Edit{bool changed{};};Edit edit_box(std::string&,Vec2,unsigned,bool){cursor.y+=34;return {};}
 template<std::size_t N>float inline_button_row_height(const std::array<InlineButtonSpec,N>&items,float height,float gap,const InlineButtonRowOptions&o){
  float used{},rows=1;for(const auto&i:items){float width=float(i.label.size())*7+24;if(used&&used+gap+width>o.available_width){++rows;used=0;}used+=width+gap;}return rows*height+(rows-1)*gap;
 }
 template<std::size_t N>std::optional<std::size_t> inline_button_row(const std::array<InlineButtonSpec,N>&items,float height,float gap,const InlineButtonRowOptions&o){
  if(N==4){++primaryRows;const auto&i=items.front();primary={std::string{i.label},cursor,{float(i.label.size())*7+24,height},i.enabled,o.alignment==InlineButtonAlignment::Center,o.sizing==InlineButtonSizing::FitContent};}
  cursor.y+=inline_button_row_height(items,height,gap,o)+4;
  for(std::size_t n=0;n<N;++n)if(items[n].enabled&&items[n].label==requested){requested.clear();return n;}return {};
 }
 bool begin_scroll_area(const ScrollAreaOptions&o){areas.push_back({std::string{o.id},cursor,o.size});stack.push_back(areas.back());cursor.x+=8;cursor.y+=8;return true;}
 void end_scroll_area(){auto p=stack.back();stack.pop_back();cursor={p.position.x,p.position.y+p.size.y+4};}
 void scroll_text_panel(const ScrollTextPanelOptions&o){areas.push_back({std::string{o.id},cursor,o.size});cursor.y+=o.size.y+4;}
 bool menu_item(const MenuItemOptions&o){labels.emplace_back(o.label);cursor.y+=o.size.y+4;if(requested==o.label){requested.clear();return true;}return false;}
 bool labels_have(std::string_view s){return std::any_of(labels.begin(),labels.end(),[&](auto&l){return l.find(s)!=std::string::npos;});}
 bool has_area(std::string_view id){return std::any_of(areas.begin(),areas.end(),[&](auto&a){return a.id==id;});}
 Area area(std::string_view id){auto found=std::find_if(areas.begin(),areas.end(),[&](auto&a){return a.id==id;});CHECK(found!=areas.end());return *found;}
}
namespace package_catalog{enum class Availability{available};}
struct Catalog{std::string id,display_name,summary,source_url,immutable_revision,license_url;package_catalog::Availability availability{};};
struct Package{std::string id,displayName,summary,externalSourceRepo;};
namespace package_registry{
 const Package*find(std::string_view){return nullptr;}
 std::string_view project_resource_purpose(std::string_view){return "Project resource";}
}
struct Review{std::string registry_package_id="research_planetary_terrain",display_name="Planetary Terrain",summary="Generate a planet",license="Epoch",declared_repository="local",content_hash="reviewed-content";
 std::filesystem::path manifest_path;std::vector<int>files;unsigned total_bytes=1234;bool ready=true;bool review_ready()const{return ready;}};
struct Staged{bool staged=true;std::filesystem::path source_root,receipt_path;bool source_staged()const{return staged;}};
struct Attachment{bool attached{};std::string registry_package_id,content_hash;bool source_attached()const{return attached;}};
struct Request{struct {std::string mode="planet";unsigned seed=1337;}settings;};
struct EditorState{
 bool packageSourceView=true,busy{},packageSourceReviewCurrent{},planetaryFeatureRequest{},projectBuildPending{},scriptBuildPending{},packageSourceTechnicalDetails{},showPackageManagerModal=true;
 bool packageTechnicalDetailsOpen{},packageModelInstall{};
 std::optional<int>planetaryFeatureReviewPending,planetaryFeatureAdmissionPending,waterAttachmentPending;
 std::optional<Request>aiPackageRequest;std::optional<Review>packageSourceReview;Staged packageSourceStaged;Attachment planetaryAttachment;
 std::string projectRoot="project",packageSourceExpectedId,packageSourcePathDraft,packageSourceStatus="Ready for exact file review",packageInstallStatus,packageCatalogStatus;
 std::vector<std::string>packageSourceFileLines;unsigned checks{},attachments{},water{},stages{},creations{},cancels{},catalogSelections{};bool update{};std::string acceptedHash;
};
bool package_source_task_busy(const EditorState&s){return s.busy;}bool planetary_project_busy(const EditorState&){return false;}
bool ai_package_generation_awaiting_attachment(const EditorState&){return false;}
void start_package_source_inspection(EditorState&s){++s.checks;}
void open_package_source_review(EditorState&s,const std::string&){++s.checks;}
void approve_and_stage_package_source(EditorState&s){++s.stages;}
void start_reviewed_water_attachment(EditorState&s){++s.water;}
void create_planetary_example_project(EditorState&s,void*){++s.creations;}
void start_planetary_project_attachment(EditorState&s,bool update){++s.attachments;s.update=update;s.acceptedHash=s.packageSourceReview->content_hash;}
void cancel_package_source_task(EditorState&s){++s.cancels;}
void cancel_planetary_project_task(EditorState&){}void browse_package_source(EditorState&){}
'''


class PackageManagerLayout(unittest.TestCase):
    def test_actual_review_actions_and_responsive_catalog_debug_release(self):
        panel = PANEL.read_text(encoding="utf-8")
        self.assertIn('.title = "Package Manager"', panel)
        self.assertNotIn('.title = "Project Package Manager"', panel)
        proof = ROOT / "Artifacts/Reports/package-manager-layout-nov06" / str(time.time_ns())
        proof.mkdir(parents=True)
        source = proof / "editor.package_layout_contract.cpp"
        source.write_text(consumer(), encoding="utf-8")
        runs = compile_and_run(self, source, proof, windows_flags=("/W3", "/WX"))
        (proof / "result.json").write_text(json.dumps({"passed": True, "nativeAcceptance": False,
            "scope": "Actual source-review draw/actions, package catalog draw and responsive body/modal measurement. GUI and source-copy callbacks are recording ports; exact source admission has its separate real-owner contract.",
            "commands": runs, "owners": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in (PANEL, LAYOUT, BOOTSTRAP)}}, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
