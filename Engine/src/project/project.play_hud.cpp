// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include "../../include/core.format_text.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <source_location>
#include <span>
#include <string>
#include <unordered_map>

import context.window;
import core.commandline;
import core.context;
import core.logger;
import core.timer;
import gui.engine;
import gui.frame_limit_control;
import perf.tier;
import project.planetary_runtime;
import scene.runtime;

#include "project.play_hud.hpp"

namespace epochengine::project_play_hud
{
    namespace cli = core::cli;

    void record_runtime_frame_evidence(RuntimeFrameEvidence& evidence, timing::Clock::time_point started,double interval,
        const std::array<double,6>& phases,gui::Vec2 mouse,bool looking,bool lodPending,bool displayPending) noexcept
    {
        if(evidence.reports>=24u)return;
        if(evidence.frames==0u)evidence.began=started;
        const double delta=evidence.has_mouse?std::hypot(double(mouse.x-evidence.last_mouse.x),
            double(mouse.y-evidence.last_mouse.y)):0.0;
        evidence.last_mouse=mouse;evidence.has_mouse=true;
        const bool moving=delta>0.0;
        ++evidence.frames;
        if(interval>0.0&&std::isfinite(interval))
        {
            ++evidence.intervals;evidence.interval_sum+=interval;
            if(interval>evidence.interval_maximum)
            {evidence.interval_maximum=interval;evidence.longest_interval_moving=moving;evidence.longest_interval_looking=looking;}
        }
        evidence.moving_frames+=moving;evidence.looking_frames+=looking;
        evidence.lod_pending_frames+=lodPending;evidence.display_pending_frames+=displayPending;
        evidence.mouse_delta_maximum=(std::max)(evidence.mouse_delta_maximum,delta);
        double work{};
        for(std::size_t phase=0u;phase<phases.size();++phase)
        {work+=phases[phase];evidence.phase_maximum[phase]=(std::max)(evidence.phase_maximum[phase],phases[phase]);}
        evidence.work_sum+=work;
        if(work>evidence.work_maximum)
        {evidence.work_maximum=work;evidence.slowest_moving=moving;evidence.slowest_looking=looking;}
        const auto finished=timing::Clock::now();
        if(finished-evidence.began<std::chrono::seconds{5})return;
        // Keep the bounded evidence budget for interaction or stalls;
        // an idle project must not exhaust it before the flight test.
        const bool report=evidence.reports==0u||evidence.moving_frames||evidence.looking_frames
            ||evidence.interval_maximum>=50.0||evidence.work_maximum>=50.0;
        if(report)try
        {
            // Owner work is elapsed time inside this update, including any
            // waits. It is distinct from renderer FPS and OS CPU accounting.
            auto& log=logger::get("Engine.Project");
            log.logf(logger::LogLevel::INFO,std::source_location::current(),
                "Scene owner #{}: {} turns; interval avg/max {:.2f}/{:.2f} ms; work avg/max {:.2f}/{:.2f} ms; prior log {:.2f} ms.",
                evidence.reports+1u,evidence.frames,evidence.interval_sum/(std::max)(1u,evidence.intervals),
                evidence.interval_maximum,evidence.work_sum/evidence.frames,evidence.work_maximum,evidence.previous_log_milliseconds);
            log.logf(logger::LogLevel::INFO,std::source_location::current(),
                "Scene owner phase max ms: input/gameplay {:.2f}, exploration {:.2f}, LOD/water {:.2f}, display {:.2f}, GUI/navigation {:.2f}, present {:.2f}.",
                evidence.phase_maximum[0],evidence.phase_maximum[1],evidence.phase_maximum[2],
                evidence.phase_maximum[3],evidence.phase_maximum[4],evidence.phase_maximum[5]);
            log.logf(logger::LogLevel::INFO,std::source_location::current(),
                "Scene owner input: moved/look {}/{}; max delta {:.1f}px; peak work moved/look={}/{}; peak interval moved/look={}/{}; pending LOD/display={}/{}.",
                evidence.moving_frames,evidence.looking_frames,evidence.mouse_delta_maximum,
                evidence.slowest_moving,evidence.slowest_looking,evidence.longest_interval_moving,
                evidence.longest_interval_looking,evidence.lod_pending_frames,evidence.display_pending_frames);
        }
        catch(...){} // Evidence must not interrupt input or publication.
        const double logMilliseconds=report
            ?std::chrono::duration<double,std::milli>(timing::Clock::now()-finished).count():evidence.previous_log_milliseconds;
        const auto reports=evidence.reports+static_cast<std::uint32_t>(report);
        evidence={};evidence.reports=reports;evidence.began=finished;
        evidence.previous_log_milliseconds=logMilliseconds;
        evidence.last_mouse=mouse;evidence.has_mouse=true;
    }

    void draw_reach_objective(const gui::WidgetBounds& safe,
        const scene_runtime::ReachObjectiveSession& objective,const std::string& targetName,bool travelBeforeWalk)
    {
        if (!objective.objective()) return;
        std::string text;
        if (targetName.empty())
            text = "Objective target unavailable";
        else if (objective.completed())
            text = "Objective complete · " + targetName;
        else
        {
            text = "Reach " + targetName;
            if (const auto distance = objective.distance(); distance)
                text += epochengine::format_text("\n{:.1f} m away", *distance);
            else if (travelBeforeWalk)
                text += "\nLand and switch to Walk";
        }
        const float width = (std::max)(1.0f, (std::min)(400.0f, safe.size.x - 32.0f));
        const float textWidth = (std::max)(1.0f, width - 20.0f);
        const float height = gui::measure_wrapped_label_height(text, textWidth) + 16.0f;
        const gui::Vec2 origin{safe.position.x + (std::max)(0.0f, safe.size.x - width - 16.0f),
            safe.position.y + (std::max)(0.0f, safe.size.y - height - 16.0f)};
        const auto cursor = gui::cursor_position();
        gui::panel_rect(origin, {width, height});
        gui::set_cursor({origin.x + 10.0f, origin.y + 8.0f});
        gui::wrapped_label(text, textWidth);
        gui::set_cursor(cursor);
    }

    void draw_flight_touch(const gui::WidgetBounds& bounds)
    {
        gui::begin_window("", bounds.position, bounds.size, false, false);
        {
            gui::ContentClipScope clip(bounds.position,
                {bounds.position.x + bounds.size.x, bounds.position.y + bounds.size.y});
            gui::set_cursor(bounds.position);
            (void)gui::button("Ascend", {108.0f, bounds.size.y});
            gui::set_cursor({bounds.position.x + 112.0f, bounds.position.y});
            (void)gui::button("Descend", {108.0f, bounds.size.y});
        }
        gui::end_window();
    }

    [[nodiscard]] gui::WidgetBounds runtime_panel_bounds(const gui::WidgetBounds& safe,
        bool controlsVisible,bool exploring,float panelHeight) noexcept
    {
        const float width = std::isfinite(safe.size.x) ? (std::max)(1.0f, safe.size.x) : 1.0f;
        const float height = std::isfinite(safe.size.y) ? (std::max)(1.0f, safe.size.y) : 1.0f;
        const float inset = (std::min)(24.0f,((std::min)(width,height)-1.0f)*0.5f);
        const float availableWidth = (std::max)(1.0f,width-2.0f*inset);
        const float availableHeight = (std::max)(1.0f,height-2.0f*inset);
        if (controlsVisible)
            return {{safe.position.x + inset, safe.position.y + inset},{(std::min)(430.0f,availableWidth),
                (std::min)(panelHeight,availableHeight)}};
        const float toggleWidth = (std::min)(exploring ? 108.0f : 160.0f, availableWidth);
        const float toggleHeight = (std::min)(exploring ? 28.0f : 32.0f, availableHeight);
        return {{safe.position.x + width - inset - toggleWidth, safe.position.y + inset},
            {toggleWidth,toggleHeight}};
    }

    void draw_planetary_flight_status(const gui::WidgetBounds& safe,
        const project_planetary_runtime::Frame& frame,bool hasObjective,bool preparingTerrain)
    {
        if (!frame.valid
            || !std::isfinite(safe.size.x) || !std::isfinite(safe.size.y)) return;

        constexpr float margin = 16.0f;
        float availableWidth = safe.size.x - 2.0f * margin;
        if (hasObjective)
            availableWidth -= (std::min)(400.0f, availableWidth) + margin;
        const float width = (std::min)(340.0f, availableWidth);
        if (width < 128.0f) return;

        // Exploration publishes validated frames, including the active body and motion.
        const auto bodies = std::span{frame.bodies}.first(frame.body_count);
        const auto body = std::ranges::find_if(bodies, [&](const auto& entry) { return entry.id == frame.anchor; });
        if (body == bodies.end()) return;
        const auto mode = frame.mode == project_planetary_runtime::TravelMode::walk ? "Walk"
            : frame.mode == project_planetary_runtime::TravelMode::cruise ? "Cruise" : "Flight";
        const auto altitude = std::abs(frame.altitude) >= 1000.0
            ? epochengine::format_text("{:.1f} km", frame.altitude / 1000.0)
            : epochengine::format_text("{:.0f} m", frame.altitude);
        const auto text = preparingTerrain ? std::string{"Preparing nearby terrain"}
            : epochengine::format_text("{} · {}\nAltitude {}  |  Speed {:.0f} m/s",mode,body->name,altitude,frame.speed);
        const float textWidth = width - 24.0f;
        const float height = gui::measure_wrapped_label_height(text, textWidth) + 16.0f;
        if (height + 2.0f * margin > safe.size.y) return;

        gui::begin_overlay_window("Flight status",
            {safe.position.x + margin, safe.position.y + safe.size.y - height - margin},
            {width, height}, false);
        gui::wrapped_label(text, textWidth);
        gui::end_window();
    }

    void draw_runtime_controls_restore(const gui::WidgetBounds& bounds,
        bool& controlsVisible)
    {
        gui::begin_window("",bounds.position,bounds.size,false,false);
        {
            gui::ContentClipScope clip(bounds.position,
                {bounds.position.x+bounds.size.x,bounds.position.y+bounds.size.y});
            gui::set_cursor(bounds.position);
            if (gui::button("Controls",bounds.size))
                controlsVisible = true;
        }
        gui::end_window();
    }
    void draw_runtime_controls_hide(float contentWidth,bool& controlsVisible)
    {
        if (gui::button("Hide Controls", {(std::min)(120.0f,contentWidth),24.0f}))
            controlsVisible = false;
    }

    void draw_runtime_frame_limit(const std::shared_ptr<core::Context>& ctx, float contentWidth,
        std::unordered_map<const void*,gui::FrameLimitEditState>& edits)
    {
        const auto requestedMode = cli::frame_vsync_requested ? perf::frame_pacing_mode::vsync
            : cli::frame_limit_fps > 0.0 ? perf::frame_pacing_mode::target_hz
            : perf::frame_pacing_mode::uncapped;
        const auto startup = perf::select_frame_pacing_policy(requestedMode,
            cli::frame_limit_fps, cli::frame_limit_explicit,
            !cli::parented_mode && !cli::editor_requested && !cli::run_menu_loop);
        auto* pacing = ctx && ctx->windowData ? &ctx->windowData->editorFramePacingOverride : nullptr;
        gui::label("Foreground FPS (this run)");
        (void)gui::live_frame_limit_control("project-runtime-frame-limit", startup, pacing,
            edits[ctx.get()], contentWidth);
    }
}
