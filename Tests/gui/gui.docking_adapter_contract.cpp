// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <array>
#include <bit>
#include <cstdint>
#include <string_view>

module gui.engine;
import epoch.gui;

namespace epochengine::gui
{
PRODUCTION_DECLARATIONS
}

namespace
{
    using namespace epochengine;
    using namespace epochengine::gui;
    using docking_adapter_detail::to_lib;
    using docking_adapter_detail::from_lib;
    std::uint32_t bits(float value) { return std::bit_cast<std::uint32_t>(value); }
    template<class T> auto vec(const T& value) { return std::array{bits(value.x),bits(value.y)}; }
    template<class T> auto rect(const T& value) { return std::array{vec(value.position),vec(value.size)}; }
    template<class T> auto floating(const T& value)
    {
        return std::array{bits(value.position.x),bits(value.position.y),bits(value.size.x),bits(value.size.y),
            bits(value.drag_offset.x),bits(value.drag_offset.y),bits(value.resize_origin_mouse.x),bits(value.resize_origin_mouse.y),
            bits(value.resize_origin_size.x),bits(value.resize_origin_size.y),std::uint32_t(value.open),std::uint32_t(value.initialized),
            std::uint32_t(value.dragging),std::uint32_t(value.resizing),std::uint32_t(value.close_pressed),value.focus_order};
    }
    template<class T> auto host(const T& value)
    { return std::array{value.active_window_id,value.next_focus_order,std::uint32_t(value.changed_this_frame)}; }
    template<class T> auto window(const T& value)
    {
        return std::array{value.id,std::uint32_t(value.mode),std::uint32_t(value.dock_slot),std::uint32_t(value.visible),
            std::uint32_t(value.initialized),std::uint32_t(value.active),std::uint32_t(value.detach_requested),
            std::uint32_t(value.close_requested),value.focus_order};
    }
    template<class T> auto chrome_flags(const T& v)
    { return std::array{v.visible,v.hovered,v.title_hovered,v.dock_hovered,v.float_hovered,v.detach_hovered,v.close_hovered,v.active}; }
    template<class T> auto result_flags(const T& v)
    { return std::array{v.changed,v.focused,v.dock_requested,v.float_requested,v.detach_requested,v.close_requested}; }
    template<class A,class B> bool chrome_same(const A& a,const B& b)
    {
        return rect(a.frame)==rect(b.frame)&&rect(a.title_bar)==rect(b.title_bar)&&rect(a.content)==rect(b.content)
            &&rect(a.dock_button)==rect(b.dock_button)&&rect(a.float_button)==rect(b.float_button)
            &&rect(a.detach_button)==rect(b.detach_button)&&rect(a.close_button)==rect(b.close_button)
            &&chrome_flags(a)==chrome_flags(b);
    }
}

#define CHECK(expression) do { if (!(expression)) return __LINE__; } while(false)
extern "C" int epoch_gui_docking_adapter_contract()
{
    using namespace epochengine;
    using namespace epochengine::gui;
    using docking_adapter_detail::to_lib;
    using docking_adapter_detail::from_lib;
    const Vec2 point{-0.0f,123.25f};
    CHECK(vec(from_lib(to_lib(point)))==vec(point));
    const WidgetBounds bounds{point,{456.75f,789.5f}};
    CHECK(rect(from_lib(to_lib(bounds)))==rect(bounds));
    FloatingWindowState floatingState{};
    floatingState.position={13,17};floatingState.size={301,207};floatingState.drag_offset={7,19};
    floatingState.resize_origin_mouse={31,41};floatingState.resize_origin_size={89,97};
    floatingState.open=true;floatingState.initialized=true;floatingState.dragging=true;
    floatingState.resizing=true;floatingState.close_pressed=true;floatingState.focus_order=1234567;
    FloatingWindowState floatingCopy{};from_lib(to_lib(floatingState),floatingCopy);
    CHECK(floating(floatingState)==floating(floatingCopy));
    for (auto value : {DockSlot::none,DockSlot::left,DockSlot::right,DockSlot::top,DockSlot::bottom,DockSlot::center})
        CHECK(from_lib(to_lib(value))==value);
    CHECK(to_lib(static_cast<DockSlot>(255))==gui_lib::DockSlot::none);
    CHECK(from_lib(static_cast<gui_lib::DockSlot>(255))==DockSlot::none);
    for(auto value : {DockGuideTarget::none,DockGuideTarget::left_tabs,DockGuideTarget::right_tabs,
        DockGuideTarget::bottom_left_tabs,DockGuideTarget::bottom_right_tabs,DockGuideTarget::left_context,
        DockGuideTarget::right_context,DockGuideTarget::float_window}) CHECK(from_lib(to_lib(value))==value);
    CHECK(to_lib(static_cast<DockGuideTarget>(255))==gui_lib::DockGuideTarget::none);
    CHECK(from_lib(static_cast<gui_lib::DockGuideTarget>(255))==DockGuideTarget::none);
    for(auto value : {DockableWindowMode::docked,DockableWindowMode::floating,DockableWindowMode::detached})
        CHECK(from_lib(to_lib(value))==value);
    CHECK(to_lib(static_cast<DockableWindowMode>(255))==gui_lib::DockableWindowMode::floating);
    CHECK(from_lib(static_cast<gui_lib::DockableWindowMode>(255))==DockableWindowMode::floating);
    for(auto value : {DockableWindowAction::none,DockableWindowAction::focus,DockableWindowAction::dock,
        DockableWindowAction::float_window,DockableWindowAction::detach,DockableWindowAction::close})
        CHECK(from_lib(to_lib(value))==value);
    CHECK(to_lib(static_cast<DockableWindowAction>(255))==gui_lib::DockableWindowAction::none);
    CHECK(from_lib(static_cast<gui_lib::DockableWindowAction>(255))==DockableWindowAction::none);

    DockGuideOptions guides{};
    guides.guide_bounds={{10,20},{800,600}};guides.left_tabs_preview={{21,31},{101,201}};
    guides.right_tabs_preview={{32,42},{202,302}};guides.bottom_left_tabs_preview={{43,53},{303,403}};
    guides.bottom_right_tabs_preview={{54,64},{404,504}};guides.left_context_preview={{65,75},{505,605}};
    guides.right_context_preview={{76,86},{606,706}};guides.floating_preview={{87,97},{707,807}};
    guides.pointer={111,222};guides.guide_extent=37;guides.guide_gap=9;
    guides.allow_contexts=true;guides.center_context_guides_in_previews=true;
    const auto converted=to_lib(guides);
    CHECK(rect(converted.guide_bounds)==rect(guides.guide_bounds));
    CHECK(rect(converted.left_tabs_preview)==rect(guides.left_tabs_preview));
    CHECK(rect(converted.right_tabs_preview)==rect(guides.right_tabs_preview));
    CHECK(rect(converted.bottom_left_tabs_preview)==rect(guides.bottom_left_tabs_preview));
    CHECK(rect(converted.bottom_right_tabs_preview)==rect(guides.bottom_right_tabs_preview));
    CHECK(rect(converted.left_context_preview)==rect(guides.left_context_preview));
    CHECK(rect(converted.right_context_preview)==rect(guides.right_context_preview));
    CHECK(rect(converted.floating_preview)==rect(guides.floating_preview));
    CHECK(vec(converted.pointer)==vec(guides.pointer)&&converted.guide_extent==37&&converted.guide_gap==9);
    CHECK(converted.allow_side_tabs&&converted.allow_bottom_tabs&&converted.allow_contexts&&converted.allow_float&&converted.center_context_guides_in_previews);
    const auto libraryLayout=gui_lib::make_dock_guide_layout(converted);
    const auto engineLayout=make_dock_guide_layout(guides);
    CHECK(engineLayout.count==libraryLayout.count&&engineLayout.hovered_target==from_lib(libraryLayout.hovered_target));
    for(std::uint32_t i=0;i<engineLayout.count;++i)
        CHECK(engineLayout.guides[i].target==from_lib(libraryLayout.guides[i].target)
            &&rect(engineLayout.guides[i].target_bounds)==rect(libraryLayout.guides[i].target_bounds)
            &&rect(engineLayout.guides[i].preview_bounds)==rect(libraryLayout.guides[i].preview_bounds)
            &&engineLayout.guides[i].hovered==libraryLayout.guides[i].hovered);
    auto excessive=libraryLayout;excessive.count=99;
    CHECK(from_lib(excessive).count==7);

    DockableWindowHostState engineHost{51,61,true};
    DockableWindowHostState hostCopy{};from_lib(to_lib(engineHost),hostCopy);CHECK(host(engineHost)==host(hostCopy));
    DockableWindowState engineState{};
    engineState.id=77;engineState.mode=DockableWindowMode::docked;engineState.dock_slot=DockSlot::bottom;
    engineState.floating=floatingState;engineState.visible=true;engineState.initialized=true;engineState.active=true;
    engineState.detach_requested=true;engineState.close_requested=true;engineState.focus_order=719;
    DockableWindowState stateCopy{};from_lib(to_lib(engineState),stateCopy);
    CHECK(window(engineState)==window(stateCopy)&&floating(engineState.floating)==floating(stateCopy.floating));
    DockableWindowOptions options{};
    options.title="Actual module contract";options.docked_frame={{10,20},{300,400}};
    options.floating.default_position={40,50};options.floating.default_size={320,240};
    options.floating.min_size={160,100};options.floating.viewport_size={1024,768};options.viewport_size={1024,768};
    options.floating.movable=false;options.floating.resizable=false;options.floating.closable=false;
    options.floating.constrain_to_viewport=true;
    options.title_bar_height=29;options.content_padding=8;options.action_button_width=63;options.action_button_gap=5;
    options.fallback_dock_slot=DockSlot::left;
    const auto co=to_lib(options);
    CHECK(co.title==options.title&&rect(co.docked_frame)==rect(options.docked_frame));
    CHECK(vec(co.floating.default_position)==vec(options.floating.default_position)&&vec(co.floating.default_size)==vec(options.floating.default_size));
    CHECK(vec(co.floating.min_size)==vec(options.floating.min_size)&&vec(co.floating.viewport_size)==vec(options.floating.viewport_size));
    CHECK(co.floating.constrain_to_viewport);
    CHECK(!co.floating.movable&&!co.floating.resizable&&!co.floating.closable&&co.title_bar_height==29&&co.content_padding==8&&co.action_button_width==63&&co.action_button_gap==5);
    for(auto action : {DockableWindowAction::none,DockableWindowAction::focus,DockableWindowAction::float_window,
        DockableWindowAction::detach,DockableWindowAction::dock,DockableWindowAction::close})
    {
        DockableWindowInput input{};input.mouse_position={43,51};input.mouse_down=true;input.mouse_pressed=true;input.mouse_released=true;
        input.requested_action=action;input.requested_dock_slot=DockSlot::left;
        auto libraryHost=to_lib(engineHost);auto libraryState=to_lib(engineState);
        const auto libraryResult=gui_lib::update_dockable_window(libraryHost,libraryState,to_lib(options),to_lib(input));
        const auto engineResult=update_dockable_window(engineHost,engineState,options,input);
        CHECK(host(engineHost)==host(libraryHost)&&window(engineState)==window(libraryState)&&floating(engineState.floating)==floating(libraryState.floating));
        CHECK(chrome_same(engineResult.chrome,libraryResult.chrome)&&result_flags(engineResult)==result_flags(libraryResult));
        CHECK(engineResult.mode==from_lib(libraryResult.mode)&&engineResult.action==from_lib(libraryResult.action)&&engineResult.dock_slot==from_lib(libraryResult.dock_slot));
    }
    auto libraryHost=to_lib(engineHost);auto libraryState=to_lib(engineState);
    gui_lib::focus_dockable_window(libraryHost,libraryState);focus_dockable_window(engineHost,engineState);
    CHECK(host(engineHost)==host(libraryHost)&&window(engineState)==window(libraryState));
    return 0;
}
