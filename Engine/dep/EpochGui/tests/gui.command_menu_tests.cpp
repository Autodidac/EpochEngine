// SPDX-License-Identifier: MPL-2.0
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <vector>
import epoch.gui.command_menu;

namespace gui = epochengine::gui_lib;
#define CHECK(value) do { if (!(value)) return __LINE__; } while (false)

namespace
{
    auto items()
    {
        return std::array{
            gui::CommandMenuItemSpec{.id=1, .label="Open Asset Browser", .preferred_width=184},
            gui::CommandMenuItemSpec{.id=2, .label="Package Manager...", .preferred_width=184, .enabled=false},
            gui::CommandMenuItemSpec{.id=3, .label="Import Mesh...", .preferred_width=220, .separator_before=true},
            gui::CommandMenuItemSpec{.id=4, .label="Reload Meshes", .preferred_width=172},
            gui::CommandMenuItemSpec{.id=5, .label="Add Static Mesh", .preferred_width=180, .separator_before=true},
            gui::CommandMenuItemSpec{.id=6, .label="Add Light", .preferred_width=120,
                .indicator=gui::CommandMenuIndicator::check, .checked=true},
            gui::CommandMenuItemSpec{.id=7, .label="Add Spawn", .preferred_width=120,
                .indicator=gui::CommandMenuIndicator::radio, .checked=true},
            gui::CommandMenuItemSpec{.id=8, .label="Duplicate Selected", .preferred_width=184, .separator_before=true},
            gui::CommandMenuItemSpec{.id=9, .label="Delete Selected", .shortcut="Del", .preferred_width=210}};
    }

    gui::CommandMenuOptions options()
    {
        return {.owner={{40,30},{100,30}}, .viewport_size={600,500}, .row_height=30};
    }

    gui::CommandMenuInput open()
    { return {.popup={.open_requested=true}}; }

    gui::Vec2 center(gui::Rect rect)
    { return {rect.position.x+rect.size.x/2,rect.position.y+rect.size.y/2}; }

    bool near(float left,float right)
    { return std::abs(left-right)<0.01f; }

    bool bounded(gui::Rect rect,gui::Vec2 viewport)
    {
        return std::isfinite(rect.position.x)&&std::isfinite(rect.position.y)
            &&rect.position.x>=0&&rect.position.y>=0
            &&rect.position.x+rect.size.x<=viewport.x+0.01f
            &&rect.position.y+rect.size.y<=viewport.y+0.01f;
    }
}

int main()
{
    auto commands=items();auto config=options();gui::CommandMenuState state;
    const auto measurement=gui::measure_command_menu(commands,config);
    CHECK(measurement.valid&&near(measurement.content_height,270));
    CHECK(near(measurement.preferred_size.x,232)&&near(measurement.preferred_size.y,282));
    auto layout=gui::update_command_menu(state,commands,config,open());
    CHECK(layout.valid&&layout.popup.visible&&layout.popup.opened);
    // The consumer supplies hit testing for its headers, connecting gap and
    // nested panes. Continuous absence closes after the delay, never on entry
    // into another part of the same menu.
    CHECK(!gui::command_menu_hover_exit_due(state,false,0.5f));
    CHECK(!gui::command_menu_hover_exit_due(state,true,0.0f));
    CHECK(state.hover_exit_seconds==0.0f);
    CHECK(!gui::command_menu_hover_exit_due(state,false,0.5f));
    CHECK(!gui::command_menu_hover_exit_due(state,false,std::numeric_limits<float>::quiet_NaN()));
    CHECK(gui::command_menu_hover_exit_due(state,false,0.25f));
    CHECK(!gui::command_menu_hover_exit_due(state,true,0.0f));
    gui::close_command_menu(state);
    CHECK(state.hover_exit_seconds==0.0f);
    CHECK(!gui::command_menu_hover_exit_due(state,false,1.0f));
    layout=gui::update_command_menu(state,commands,config,open());
    CHECK(layout.rows.size()==commands.size()&&!layout.scrollbar&&layout.scroll_offset==0);
    CHECK(layout.maximum_scroll_offset==0&&bounded(layout.popup.popup,config.viewport_size));
    CHECK(layout.rows.back().visible&&near(layout.rows.back().rect.position.y+30,
        layout.content_clip.position.y+layout.content_clip.size.y));
    for(int frame=0;frame<20;++frame)
    {
        layout=gui::update_command_menu(state,commands,config,{});
        CHECK(!layout.popup.opened&&!layout.scrollbar&&layout.scroll_offset==0);
    }
    // Visible rows come from the actual descriptor span, including all disabled
    // entries and separators; there is no second declared count to drift.
    for(std::size_t count=1;count<=commands.size();++count)
    {
        gui::close_command_menu(state);
        layout=gui::update_command_menu(state,std::span{commands}.first(count),config,open());
        CHECK(layout.rows.size()==count&&!layout.scrollbar);
        CHECK(near(layout.content_height,static_cast<float>(count)*config.row_height));
    }
    // Keyboard navigation skips disabled commands and keeps its focus when the
    // pointer has not moved. Marks/shortcuts remain caller-owned metadata.
    gui::close_command_menu(state);layout=gui::update_command_menu(state,commands,config,open());
    CHECK(state.focused_id==1);
    layout=gui::update_command_menu(state,commands,config,{.down_pressed=true});
    CHECK(state.focused_id==3&&layout.focused_index==2&&layout.keyboard_captured);
    layout=gui::update_command_menu(state,commands,config,{});CHECK(state.focused_id==3);
    layout=gui::update_command_menu(state,commands,config,{.accept_pressed=true});
    CHECK(layout.activated_id==3&&!state.popup.open&&layout.popup.closed&&!layout.popup.visible);
    CHECK(commands[5].checked&&commands[6].checked&&commands.back().shortcut=="Del");
    layout=gui::update_command_menu(state,commands,config,{.shortcut_id=2});
    CHECK(!layout.activated_id&&!state.popup.open);
    layout=gui::update_command_menu(state,commands,config,{.shortcut_id=9});
    CHECK(layout.activated_id==9&&layout.keyboard_captured);

    // Pointer activation requires an enabled command and a press/release on the
    // same current row, both clipped to the popup viewport.
    layout=gui::update_command_menu(state,commands,config,open());
    auto first=center(layout.rows[0].rect),third=center(layout.rows[2].rect);
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=first,.mouse_pressed=true},.pointer_down=true});
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=third,.mouse_released=true}});
    CHECK(!layout.activated_id&&state.popup.open&&layout.pointer_captured);
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=third,.mouse_pressed=true,.mouse_released=true},.press_position=first});
    CHECK(!layout.activated_id&&state.popup.open&&layout.pointer_captured);
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=first,.mouse_pressed=true,.mouse_released=true},
            .press_position=gui::Vec2{590,490}});
    CHECK(!layout.activated_id&&!state.popup.open&&layout.popup.closed&&layout.pointer_captured);
    layout=gui::update_command_menu(state,commands,config,open());
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=first,.mouse_pressed=true},.pointer_down=true});
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=first,.mouse_released=true}});
    CHECK(layout.activated_id==1&&!state.popup.open&&layout.pointer_captured);
    layout=gui::update_command_menu(state,commands,config,open());
    auto disabled=center(layout.rows[1].rect);
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=disabled,.mouse_pressed=true},.pointer_down=true});
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=disabled,.mouse_released=true}});
    CHECK(!layout.activated_id&&state.popup.open&&layout.pointer_captured);

    // Scrollbars exist only after real viewport clipping. Wheel and thumb drag
    // use the existing compiled scrollbar owner and retain bounded scroll state.
    config.viewport_size.y=150;gui::close_command_menu(state);
    layout=gui::update_command_menu(state,commands,config,open());
    CHECK(layout.scrollbar&&layout.maximum_scroll_offset>0&&layout.scroll_offset==0);
    CHECK(bounded(layout.popup.popup,config.viewport_size));
    auto pointer=center(layout.content_clip);
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=pointer},.wheel_delta=-30});
    CHECK(layout.wheel_scrolled&&near(layout.scroll_offset,30));
    layout=gui::update_command_menu(state,commands,config,{.popup={.mouse_position=pointer}});
    CHECK(near(layout.scroll_offset,30));
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=pointer},.wheel_delta=-1e6f});
    CHECK(near(layout.scroll_offset,layout.maximum_scroll_offset));
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=pointer},.wheel_delta=1e6f});
    CHECK(layout.scroll_offset==0);
    const auto thumb=center(layout.scrollbar->thumb);
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=thumb,.mouse_pressed=true},.pointer_down=true});
    CHECK(state.dragging_scrollbar&&layout.pointer_captured&&!state.pressed_id);
    const gui::Vec2 bottom{thumb.x,layout.scrollbar->track.position.y+layout.scrollbar->track.size.y};
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=bottom},.pointer_down=true});
    CHECK(near(layout.scroll_offset,layout.maximum_scroll_offset));
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=bottom,.mouse_released=true}});
    CHECK(!state.dragging_scrollbar&&!layout.activated_id);
    layout=gui::update_command_menu(state,commands,config,{.home_pressed=true});
    CHECK(state.focused_id==1&&layout.scroll_offset==0);
    layout=gui::update_command_menu(state,commands,config,{.end_pressed=true});
    CHECK(state.focused_id==9&&near(layout.scroll_offset,layout.maximum_scroll_offset));
    layout=gui::update_command_menu(state,commands,config,{.up_pressed=true});CHECK(state.focused_id==8);
    layout=gui::update_command_menu(state,commands,config,{.page_up_pressed=true});
    CHECK(state.focused_id&&*state.focused_id<8);
    layout=gui::update_command_menu(state,commands,config,{.left_pressed=true});
    CHECK(layout.owner_navigation==-1&&layout.keyboard_captured);
    config.viewport_size.y=500;
    layout=gui::update_command_menu(state,commands,config,{});
    CHECK(!layout.scrollbar&&state.scroll_offset==0);

    // Outside presses, Escape and focus loss close without releasing a command
    // into the software underneath the menu.
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position={590,490},.mouse_pressed=true}});
    CHECK(!state.popup.open&&layout.popup.closed&&layout.pointer_captured&&!layout.activated_id);
    layout=gui::update_command_menu(state,commands,config,open());
    layout=gui::update_command_menu(state,commands,config,{.popup={.escape_pressed=true}});
    CHECK(!state.popup.open&&layout.popup.closed&&layout.keyboard_captured);
    layout=gui::update_command_menu(state,commands,config,open());
    layout=gui::update_command_menu(state,commands,config,{.focus_lost=true,.shortcut_id=1});
    CHECK(!state.popup.open&&!layout.activated_id&&layout.keyboard_captured);
    layout=gui::update_command_menu(state,commands,config,{.popup={.toggle_requested=true}});
    CHECK(state.popup.open&&layout.popup.opened);
    layout=gui::update_command_menu(state,commands,config,{.popup={.toggle_requested=true}});
    CHECK(!state.popup.open&&layout.popup.closed);

    // Invalid dimensions/identities are refused; nonfinite pointer/wheel values
    // cannot create an activation or corrupt the retained scroll offset.
    layout=gui::update_command_menu(state,commands,config,open());
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position={std::numeric_limits<float>::quiet_NaN(),0},.mouse_pressed=true},
            .wheel_delta=std::numeric_limits<float>::infinity()});
    CHECK(!layout.activated_id&&std::isfinite(state.scroll_offset));
    commands[1].id=commands[0].id;
    layout=gui::update_command_menu(state,commands,config,open());
    CHECK(!layout.valid&&!state.popup.open);
    commands=items();config.row_height=std::numeric_limits<float>::quiet_NaN();
    CHECK(!gui::measure_command_menu(commands,config).valid);
    config=options();commands[0].preferred_width=-1;
    CHECK(!gui::measure_command_menu(commands,config).valid);
    commands=items();for(auto& command:commands)command.enabled=false;
    layout=gui::update_command_menu(state,commands,config,open());
    CHECK(layout.valid&&state.popup.open&&!layout.focused_index);
    layout=gui::update_command_menu(state,commands,config,{.down_pressed=true,.accept_pressed=true});
    CHECK(!layout.activated_id&&!layout.focused_index);
    // Tiny viewports still open at the first row without a tail-scroll jump.
    commands=items();config=options();config.viewport_size={20,20};
    gui::close_command_menu(state);
    layout=gui::update_command_menu(state,commands,config,open());
    CHECK(layout.valid&&layout.scrollbar&&layout.scroll_offset==0);
    CHECK(bounded(layout.popup.popup,config.viewport_size));
    for(std::size_t index=0;index<5;++index)commands[index].enabled=false;
    gui::close_command_menu(state);
    layout=gui::update_command_menu(state,commands,config,open());
    CHECK(layout.focused_index==5&&layout.scroll_offset==0);
    // A descriptor update cannot keep a disabled/stale pressed command alive.
    commands=items();config=options();gui::close_command_menu(state);
    layout=gui::update_command_menu(state,commands,config,open());
    first=center(layout.rows[0].rect);
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=first,.mouse_pressed=true},.pointer_down=true});
    commands[0].enabled=false;
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=first,.mouse_released=true}});
    CHECK(!layout.activated_id&&state.popup.open);
    commands=items();
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=first,.mouse_pressed=true},.pointer_down=true});
    const gui::Vec2 padding{layout.popup.popup.position.x+1,layout.popup.popup.position.y+1};
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=padding,.mouse_pressed=true},.pointer_down=true});
    layout=gui::update_command_menu(state,commands,config,
        {.popup={.mouse_position=first,.mouse_released=true}});
    CHECK(!layout.activated_id&&state.popup.open);
    // Real flyouts share the same row, placement, scrollbar and activation
    // owners. A leaf closes the whole chain before the consumer may invoke it.
    std::array tools{
        gui::CommandMenuItemSpec{.id=101,.label="World Outliner",.preferred_width=170},
        gui::CommandMenuItemSpec{.id=102,.label="Assistant Chat",.preferred_width=170,.enabled=false}};
    std::array workspaces{
        gui::CommandMenuItemSpec{.id=201,.label="World",.preferred_width=180},
        gui::CommandMenuItemSpec{.id=202,.label="Forest Factory",.preferred_width=180}};
    std::array groups{
        gui::CommandMenuItemSpec{.id=1001,.label="Tool Windows",.preferred_width=180,.children=tools},
        gui::CommandMenuItemSpec{.id=1002,.label="Workspace Windows",.preferred_width=180,.children=workspaces},
        gui::CommandMenuItemSpec{.id=1003,.label="Reset Layout",.preferred_width=180}};
    config=options();gui::close_command_menu(state);
    layout=gui::update_command_menu(state,groups,config,open());
    const auto toolRow=center(layout.rows[0].rect),workspaceRow=center(layout.rows[1].rect);
    layout=gui::update_command_menu(state,groups,config,{.popup={.mouse_position=toolRow}});
    CHECK(layout.submenus.size()==1&&state.submenu.size()==1&&state.open_submenu_id==1001);
    CHECK(bounded(layout.submenus[0].popup.popup,config.viewport_size));
    auto childPoint=center(layout.submenus[0].rows[0].rect);
    layout=gui::update_command_menu(state,groups,config,
        {.popup={.mouse_position=childPoint,.mouse_pressed=true,.mouse_released=true}});
    CHECK(layout.activated_id==101&&!layout.activated_index&&!state.popup.open&&state.submenu.empty());
    CHECK(layout.pointer_captured&&layout.popup.closed);
    layout=gui::update_command_menu(state,groups,config,open());
    layout=gui::update_command_menu(state,groups,config,{.right_pressed=true});
    CHECK(layout.submenus.size()==1&&state.submenu[0].focused_id==101&&layout.owner_navigation==0);
    layout=gui::update_command_menu(state,groups,config,{.left_pressed=true});
    CHECK(state.popup.open&&state.submenu.empty()&&layout.owner_navigation==0);
    layout=gui::update_command_menu(state,groups,config,{.down_pressed=true,.right_pressed=true});
    CHECK(state.open_submenu_id==1002&&layout.submenus.size()==1&&state.submenu[0].focused_id==201);
    layout=gui::update_command_menu(state,groups,config,{.down_pressed=true});
    CHECK(state.submenu[0].focused_id==202&&state.focused_id==1002);
    layout=gui::update_command_menu(state,groups,config,{.accept_pressed=true});
    CHECK(layout.activated_id==202&&!state.popup.open&&state.submenu.empty());
    layout=gui::update_command_menu(state,groups,config,open());
    layout=gui::update_command_menu(state,groups,config,{.popup={.mouse_position=toolRow}});
    layout=gui::update_command_menu(state,groups,config,{.popup={.mouse_position=workspaceRow}});
    CHECK(state.open_submenu_id==1002&&state.submenu.size()==1&&state.submenu[0].focused_id==201);
    childPoint=center(layout.submenus[0].rows[1].rect);
    const auto otherPoint=center(layout.submenus[0].rows[0].rect);
    layout=gui::update_command_menu(state,groups,config,
        {.popup={.mouse_position=childPoint,.mouse_pressed=true,.mouse_released=true},.press_position=otherPoint});
    CHECK(!layout.activated_id&&state.popup.open&&state.submenu[0].popup.open);
    layout=gui::update_command_menu(state,groups,config,{.popup={.escape_pressed=true}});
    CHECK(!state.popup.open&&state.submenu.empty()&&layout.keyboard_captured);
    layout=gui::update_command_menu(state,groups,config,open());
    layout=gui::update_command_menu(state,groups,config,{.right_pressed=true});
    layout=gui::update_command_menu(state,groups,config,
        {.popup={.mouse_position={590,490},.mouse_pressed=true}});
    CHECK(!state.popup.open&&state.submenu.empty()&&layout.pointer_captured);
    layout=gui::update_command_menu(state,groups,config,open());
    layout=gui::update_command_menu(state,groups,config,{.right_pressed=true});
    layout=gui::update_command_menu(state,groups,config,{.focus_lost=true,.shortcut_id=101});
    CHECK(!layout.activated_id&&!state.popup.open&&state.submenu.empty());
    groups[0].enabled=false;
    layout=gui::update_command_menu(state,groups,config,{.shortcut_id=101});
    CHECK(!layout.activated_id&&!state.popup.open);
    groups[0].enabled=true;
    layout=gui::update_command_menu(state,groups,config,{.shortcut_id=202});
    CHECK(layout.activated_id==202&&!state.popup.open);
    // Right-edge menus flip to the left, preserving the same leaf identity.
    config.owner={{520,30},{70,30}};
    layout=gui::update_command_menu(state,groups,config,open());
    layout=gui::update_command_menu(state,groups,config,{.right_pressed=true});
    CHECK(layout.submenus.size()==1&&bounded(layout.submenus[0].popup.popup,config.viewport_size));
    CHECK(layout.submenus[0].popup.placement==gui::PopupPlacement::left);
    // Stale group removal, duplicate identities, cycles/depth and item bounds
    // refuse without dispatching the prior branch or allocating unbounded data.
    groups[0].children={};
    layout=gui::update_command_menu(state,groups,config,{});
    CHECK(state.submenu.empty()&&!layout.activated_id);
    groups[0].children=tools;tools[0].id=groups[0].id;
    layout=gui::update_command_menu(state,groups,config,open());
    CHECK(!layout.valid&&!state.popup.open);
    tools[0].id=101;
    groups[0].children={nullptr,1};
    CHECK(!gui::update_command_menu(state,groups,config,open()).valid);
    std::array<gui::CommandMenuItemSpec,1>cycle{{{.id=700,.label="Cycle",.preferred_width=100}}};
    cycle[0].children=cycle;
    CHECK(!gui::update_command_menu(state,cycle,config,open()).valid);
    const std::array nestedLeaves{gui::CommandMenuItemSpec{.id=9001,.label="Nested leaf",.preferred_width=120}};
    const std::array nestedGroup{gui::CommandMenuItemSpec{.id=9002,.label="Nested group",.preferred_width=120,.children=nestedLeaves}};
    const std::array nestedRoot{gui::CommandMenuItemSpec{.id=9003,.label="Root group",.preferred_width=120,.children=nestedGroup}};
    config=options();layout=gui::update_command_menu(state,nestedRoot,config,open());
    layout=gui::update_command_menu(state,nestedRoot,config,{.right_pressed=true});
    layout=gui::update_command_menu(state,nestedRoot,config,{.right_pressed=true});
    CHECK(state.submenu.size()==1&&state.submenu[0].submenu.size()==1);
    layout=gui::update_command_menu(state,nestedRoot,config,{.left_pressed=true});
    CHECK(state.submenu.size()==1&&state.submenu[0].submenu.empty());
    layout=gui::update_command_menu(state,nestedRoot,config,{.right_pressed=true});
    layout=gui::update_command_menu(state,nestedRoot,config,{.accept_pressed=true});
    CHECK(layout.activated_id==9001&&!state.popup.open&&state.submenu.empty());
    std::array<gui::CommandMenuItemSpec,9>deep{};
    for(std::size_t index=0;index<deep.size();++index){deep[index].id=800+index;deep[index].label="Level";deep[index].preferred_width=100;
        if(index+1<deep.size())deep[index].children={deep.data()+index+1,1};}
    CHECK(!gui::update_command_menu(state,std::span{deep}.first(1),config,open()).valid);
    std::vector<gui::CommandMenuItemSpec>many(1024);
    for(std::size_t index=0;index<many.size();++index)many[index]={.id=2000+index,.label="Large",.preferred_width=100};
    groups[0].children=many;
    CHECK(!gui::update_command_menu(state,groups,config,open()).valid);
    return 0;
}
