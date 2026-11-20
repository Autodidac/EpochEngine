"""Actual palette placement with the GUI document and runtime, using scene ports."""
from pathlib import Path
import os
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.gui_contract import run_gui_contract


ROOT = Path(__file__).resolve().parents[2]


class GuiPalettePlacement(unittest.TestCase):
    def test_container_selection_and_canvas_fallback(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC GUI module consumer')
        editor = ROOT / 'Engine/src/editor/application'
        documents = editor / 'editor.application_project_documents.inl'
        commands = editor / 'editor.application_assistant_authoring.inl'
        source = documents.read_text(encoding='utf-8')
        owners = '\n'.join(definition(source, marker) for marker in (
            '        [[nodiscard]] authoring::gui::WidgetDescriptor\n        gui_widget_descriptor_from_entity(',
            '        [[nodiscard]] authoring::gui::MutationResult create_authored_gui_widget(',
            '        [[nodiscard]] std::string_view gui_entity_type_for_widget(\n            authoring::gui::WidgetKind kind) noexcept\n',
            '        [[nodiscard]] std::string_view gui_widget_asset_path(',
            '        [[nodiscard]] EditorEntity gui_projection_entity(\n            const authoring::gui::WidgetView& widget,\n            std::size_t order)\n',
        ))
        owners += '\n' + definition(commands.read_text(encoding='utf-8'),
                                    '        [[nodiscard]] bool add_gui_palette_widget(')
        fixture = r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
import authoring.gui_document;
import authoring.gui_compiler;
import project.gui_runtime;
namespace authoring = epochengine::authoring;
using namespace authoring::gui;
namespace scene { struct SceneTextureMaterialSnapshot {std::string logical_path;}; }
struct EditorEntity {
 std::string name, type, category;
 std::array<float,3> position{},scale{1,1,1};
 bool visible{};
 std::optional<scene::SceneTextureMaterialSnapshot> textureMaterial;
};
struct EditorState {
 std::unique_ptr<GuiDocument> guiDocument;
 std::vector<WidgetHandle> guiDocumentWidgets;
 std::optional<WidgetHandle> selected;
 std::optional<EditorEntity> projection;
 std::string guiDocumentStatus;
 unsigned canvasCommands{};
 bool allowProjection{true};
};
std::optional<WidgetHandle> selected_gui_widget(const EditorState& state){
 return state.selected;
}
bool select_gui_widget(EditorState& state,WidgetHandle handle){
 if(!state.guiDocument->widget(handle))return false;
 state.selected=handle;return true;
}
bool add_entity(EditorState& state,std::string_view){++state.canvasCommands;return true;}
std::optional<EditorEntity> make_editor_entity(const EditorState&,std::string_view type){
 if(type=="gui_input")return EditorEntity{.name="Input",.type="GuiTextInput",.category="UI",.scale={2,.4f,1}};
 if(type=="gui_tabs")return EditorEntity{.name="Nested tabs",.type="GuiTabSet",.category="UI",.scale={2,1,1}};
 return std::nullopt;
}
bool is_gui_authoring_entity(const EditorEntity& entity){return entity.category=="UI";}
bool create_gui_widget_projection(EditorState& state,EditorEntity entity,WidgetHandle handle,std::string_view){
 if(!state.allowProjection)return false;
 state.projection=std::move(entity);state.guiDocumentWidgets.push_back(handle);return true;
}
OWNERS
#define CHECK(value) do {if(!(value)){std::fprintf(stderr,"Palette contract failed line %d\n",__LINE__);return 1;}}while(false)
int main(){
 EditorState state;
 state.guiDocument=std::make_unique<GuiDocument>(DocumentHandle{6,1},BranchIdentity{8,1});
 auto& document=*state.guiDocument;
 WidgetDescriptor canvas;
 canvas.kind=WidgetKind::canvas;canvas.content=CanvasContent{600,420,true};canvas.name="Canvas";
 canvas.layout.width=600;canvas.layout.height=420;
 const auto root=document.create_widget(canvas).widget;CHECK(root);
 WidgetDescriptor tabs;
 tabs.kind=WidgetKind::tab_set;tabs.content=TabSetContent{};tabs.name="Pages";
 tabs.layout.x=32;tabs.layout.y=40;tabs.layout.width=400;tabs.layout.height=240;
 const auto tabSet=create_authored_gui_widget(document,tabs,root).widget;CHECK(tabSet);
 const auto pages=document.widget(tabSet)->children;
 CHECK(pages.size()==3&&std::get<TabSetContent>(document.widget(tabSet)->descriptor.content).selected_page==pages.front());
 CHECK(std::get<TabPageContent>(document.widget(pages[1])->descriptor.content).label=="Details");
 CHECK(add_gui_palette_widget(state,"gui_input")&&state.canvasCommands==1);
 state.selected=tabSet;
 CHECK(add_gui_palette_widget(state,"gui_input")&&state.canvasCommands==2);
 state.selected=WidgetHandle{pages[1].index,pages[1].generation+1};
 CHECK(add_gui_palette_widget(state,"gui_input")&&state.canvasCommands==3);
 state.selected=pages.front();
 const auto before=document.revision();
 state.allowProjection=false;
 CHECK(!add_gui_palette_widget(state,"gui_input"));
 CHECK(document.revision()==before&&document.widget(pages.front())->children.empty()&&state.guiDocumentWidgets.empty());
 state.allowProjection=true;
 CHECK(add_gui_palette_widget(state,"gui_input"));
 const auto input=state.selected.value();
 CHECK(input==state.guiDocumentWidgets.back()&&document.widget(input)->parent==pages.front());
 const auto layout=document.widget(input)->descriptor.layout;
 CHECK(layout.x==12&&layout.y==12&&state.projection&&state.projection->type=="GuiTextInput");
 CHECK(std::abs((state.projection->position[0]+3.2f)*100-layout.x)<.001f);
 CHECK(add_gui_palette_widget(state,"gui_input")&&state.canvasCommands==4); // Leaf selection uses the canvas.
 CHECK(document.undo()&&!document.widget(input));
 CHECK(document.redo()&&document.widget(input)->parent==pages.front());
 constexpr auto texture="Assets/Textures/shared.epoch_texture";
 WidgetDescriptor image;
 image.kind=WidgetKind::image;image.content=ImageContent{texture,"Image",true};image.name="Picture";
 const auto picture=document.create_widget(image,root);CHECK(picture);
 const auto imageView=document.widget(picture.widget);CHECK(imageView);
 const auto projectedImage=gui_projection_entity(*imageView,0u);
 CHECK(projectedImage.type=="GuiImage"&&!projectedImage.textureMaterial);
 CHECK(gui_widget_asset_path(*imageView)==texture);
 WidgetDescriptor button;
 button.kind=WidgetKind::image_button;button.content=ImageButtonContent{texture,"Button",true};button.name="PictureButton";
 const auto imageButton=document.create_widget(button,root);CHECK(imageButton);
 const auto buttonView=document.widget(imageButton.widget);CHECK(buttonView);
 const auto projectedButton=gui_projection_entity(*buttonView,1u);
 CHECK(projectedButton.type=="GuiImageButton"&&!projectedButton.textureMaterial);
 CHECK(gui_widget_asset_path(*buttonView)==texture);
 const auto saved=serialize_gui_document(document.snapshot());CHECK(saved);
 const auto reopened=deserialize_gui_document(saved.bytes);CHECK(reopened);
 const auto compiled=compile_gui_document(*reopened.snapshot);CHECK(compiled);
 CHECK(std::ranges::count_if(compiled.artifact.widgets,[&](const auto& widget){
  return widget.asset_path==texture;
 })==2);
 using epochengine::project_gui_runtime::RuntimeSession;
 RuntimeSession runtime(compiled.artifact);
 const auto frame=runtime.build_frame({600,420});
 const auto rendered=std::ranges::find_if(frame.widgets,[&](const auto& row){
  return compiled.artifact.widgets[row.index].name=="Input";
 });
 CHECK(rendered!=frame.widgets.end()&&rendered->visible&&rendered->bounds.x==44&&rendered->bounds.y==84);
 state.selected=pages[1];
 CHECK(add_gui_palette_widget(state,"gui_tabs"));
 const auto nested=document.widget(state.selected.value());
 CHECK(nested&&nested->parent==pages[1]&&nested->children.size()==3);
}
'''.replace('OWNERS', owners)
        run_gui_contract(self, 'gui-palette-placement', fixture, [documents, commands, Path(__file__)],
                         'Actual palette action, tab definitions, GuiDocument and compiled GUI runtime with recorded scene projection')


if __name__ == '__main__':
    unittest.main()
