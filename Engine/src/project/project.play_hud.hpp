// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

// Types are supplied by the Play host's ordinary project, GUI and timing imports.
namespace epochengine::project_play_hud
{
    struct RuntimeFrameEvidence final
    {
        timing::Clock::time_point began{};
        std::array<double,6> phase_maximum{};
        double interval_sum{},interval_maximum{},work_sum{},work_maximum{},mouse_delta_maximum{};
        double previous_log_milliseconds{};
        std::uint32_t frames{},intervals{},moving_frames{},looking_frames{},lod_pending_frames{},display_pending_frames{},reports{};
        gui::Vec2 last_mouse{};
        bool has_mouse{},slowest_moving{},slowest_looking{},longest_interval_moving{},longest_interval_looking{};
    };
    void record_runtime_frame_evidence(RuntimeFrameEvidence& evidence, timing::Clock::time_point started,double interval,
        const std::array<double,6>& phases,gui::Vec2 mouse,bool looking,bool lodPending,bool displayPending) noexcept;
    void draw_reach_objective(const gui::WidgetBounds& safe,
        const scene_runtime::ReachObjectiveSession& objective,const std::string& targetName,bool travelBeforeWalk);
    void draw_flight_touch(const gui::WidgetBounds& bounds);
    [[nodiscard]] gui::WidgetBounds runtime_panel_bounds(const gui::WidgetBounds& safe,
        bool controlsVisible,bool exploring,float panelHeight) noexcept;
    void draw_planetary_flight_status(const gui::WidgetBounds& safe,
        const project_planetary_runtime::Frame& frame,bool hasObjective,bool preparingTerrain);
    void draw_runtime_controls_restore(const gui::WidgetBounds& bounds,
        bool& controlsVisible);
    void draw_runtime_controls_hide(float contentWidth,bool& controlsVisible);
    void draw_runtime_frame_limit(const std::shared_ptr<core::Context>& ctx, float contentWidth,
        std::unordered_map<const void*,gui::FrameLimitEditState>& edits);
}
