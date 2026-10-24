// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        [[nodiscard]] static AiWorkspaceDomain snapshot_ai_workspace_domain(std::uint8_t value) noexcept
        {
            switch (static_cast<AiWorkspaceDomain>(value))
            {
            case AiWorkspaceDomain::Tooling:
            case AiWorkspaceDomain::Engine:
            case AiWorkspaceDomain::Software:
            case AiWorkspaceDomain::Evidence:
            case AiWorkspaceDomain::Visualizer:
            case AiWorkspaceDomain::Ops:
            case AiWorkspaceDomain::Control:
                return static_cast<AiWorkspaceDomain>(value);
            default:
                return AiWorkspaceDomain::Control;
            }
        }

        [[nodiscard]] static float finite_or(float value, float fallback) noexcept
        {
            return std::isfinite(value) ? value : fallback;
        }

        [[nodiscard]] static double finite_or(double value, double fallback) noexcept
        {
            return std::isfinite(value) ? value : fallback;
        }

        [[nodiscard]] static float snapshot_layout_split(float value, float fallback) noexcept
        {
            return (std::clamp)(finite_or(value, fallback), 0.10f, 0.90f);
        }

        [[nodiscard]] static double snapshot_frame_limit(double value, double fallback) noexcept
        {
            const double resolved = finite_or(value, fallback);
            return resolved >= 0.0 ? resolved : fallback;
        }

        [[nodiscard]] static canvas2d::ProjectSettings canvas2d_runtime_settings(
            const EditorCanvas2DProjectSnapshot& value) noexcept
        {
            canvas2d::ProjectSettings settings{};
            settings.logical_canvas = {value.logical_width, value.logical_height};
            settings.pixels_per_world_unit = value.pixels_per_world_unit;
            switch (value.scale_policy)
            {
            case 1u: settings.viewport_policy = canvas2d::ViewportPolicy::fractional_scale; break;
            case 2u: settings.viewport_policy = canvas2d::ViewportPolicy::fill_crop; break;
            case 3u: settings.viewport_policy = canvas2d::ViewportPolicy::stretch; break;
            case 0u:
            default: settings.viewport_policy = canvas2d::ViewportPolicy::integer_scale; break;
            }
            settings.presentation_filter = value.sampling_policy == 1u
                ? FilterMode::linear
                : FilterMode::nearest;
            settings.pixel_snap = value.pixel_snapping
                ? canvas2d::PixelSnapMode::camera_and_sprites
                : canvas2d::PixelSnapMode::none;
            settings.maximum_sprites_per_batch = value.maximum_sprites_per_batch;
            settings.maximum_batches = value.maximum_batches;
            settings.tile_chunk_extent = value.tile_chunk_extent;
            return settings;
        }

        [[nodiscard]] static EditorCanvas2DProjectSnapshot sanitize_canvas2d_project(
            EditorCanvas2DProjectSnapshot value) noexcept
        {
            value.logical_width = (std::clamp)(value.logical_width, 160u, 16'384u);
            value.logical_height = (std::clamp)(value.logical_height, 90u, 16'384u);
            value.pixels_per_world_unit = (std::clamp)(
                finite_or(value.pixels_per_world_unit, 100.0f), 1.0f, 2'048.0f);
            if (value.scale_policy > 3u)
                value.scale_policy = 0u;
            if (value.sampling_policy > 1u)
                value.sampling_policy = 0u;
            value.maximum_sprites_per_batch = (std::clamp)(
                value.maximum_sprites_per_batch, 1u, 65'536u);
            value.maximum_batches = (std::clamp)(value.maximum_batches, 1u, 4'096u);
            switch (value.tile_chunk_extent)
            {
            case 8u: case 16u: case 32u: case 64u: case 128u: case 256u:
                break;
            default:
                value.tile_chunk_extent = 32u;
                break;
            }
            if (canvas2d::validate(canvas2d_runtime_settings(value)) != canvas2d::ResultCode::success)
                return {};
            return value;
        }

        [[nodiscard]] static EditorTimeControl snapshot_time_control(EditorTimeControl value) noexcept
        {
            value.fixed_dt_seconds = (std::clamp)(finite_or(value.fixed_dt_seconds, 1.0 / 60.0), 1.0 / 240.0, 0.25);
            value.time_scale = (std::clamp)(finite_or(value.time_scale, 1.0), 0.0, 8.0);
            value.max_steps_per_frame = (std::clamp)(value.max_steps_per_frame, 1u, 64u);
            value.step_once = false;
            return value;
        }

        #include "../editor.update_runtime.inl"
        [[nodiscard]] static std::string format_ms(double seconds)
        {
            return epochengine::format_text("{:.2f} ms", seconds * 1000.0);
        }

        [[nodiscard]] static std::string format_seconds(double seconds)
        {
            return epochengine::format_text("{:.2f} s", seconds);
        }

        [[nodiscard]] static std::string format_rate(double dt_seconds)
        {
            if (dt_seconds <= 0.0)
                return "0 Hz";
            return epochengine::format_text("{:.0f} Hz", 1.0 / dt_seconds);
        }

        [[nodiscard]] static epochengine::core::time::simulation_stats timeline_stats_from_editor(
            const EditorState& editor) noexcept
        {
            return epochengine::core::time::simulation_stats{
                .frame_index = editor.timeSnapshot.frame_index,
                .simulated_steps = editor.timeSnapshot.simulated_steps,
                .step_budget = editor.timeSnapshot.step_budget,
                .real_dt_seconds = editor.timeSnapshot.real_dt_seconds,
                .scaled_dt_seconds = editor.timeSnapshot.scaled_dt_seconds,
                .fixed_dt_seconds = editor.timeSnapshot.fixed_dt_seconds,
                .accumulator_seconds = editor.timeSnapshot.accumulator_seconds,
                .simulated_seconds = editor.timeSnapshot.simulated_seconds,
                .time_scale = editor.timeSnapshot.time_scale,
                .paused = editor.timeSnapshot.paused,
                .max_steps_per_frame = editor.timeSnapshot.max_steps_per_frame
            };
        }

        static void ensure_timeline_defaults(EditorState& editor)
        {
            if (editor.timelineTracks.empty())
                editor.timelineTracks = epochengine::timeline::default_editor_tracks();

            if (editor.timelineEvents.empty())
            {
                editor.timelineEvents.push_back(epochengine::timeline::TimelineEvent{
                    .track_id = "save",
                    .kind = epochengine::timeline::TimelineEventKind::Checkpoint,
                    .simulated_seconds = 0.0,
                    .frame_index = 0,
                    .label = "Initial checkpoint gate",
                    .target_name = "PersistentLevel",
                    .payload = "streaming-save start"
                });
                epochengine::timeline::sort_events(editor.timelineEvents);
            }
        }

        struct SurfaceCanvas
        {
            int width = 0;
            int height = 0;
            std::vector<std::uint8_t> pixels{};

            SurfaceCanvas(int w, int h, gui::Color clear)
                : width(w)
                , height(h)
                , pixels(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u, 0)
            {
                fill_rect(0, 0, width, height, clear);
            }

            void set_pixel(int x, int y, gui::Color color) noexcept
            {
                if (x < 0 || y < 0 || x >= width || y >= height)
                    return;

                const std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                    + static_cast<std::size_t>(x)) * 4u;
                pixels[idx + 0] = color.r;
                pixels[idx + 1] = color.g;
                pixels[idx + 2] = color.b;
                pixels[idx + 3] = color.a;
            }

            void fill_rect(int x, int y, int w, int h, gui::Color color) noexcept
            {
                const int x0 = (std::max)(0, x);
                const int y0 = (std::max)(0, y);
                const int x1 = (std::min)(width, x + w);
                const int y1 = (std::min)(height, y + h);

                for (int py = y0; py < y1; ++py)
                    for (int px = x0; px < x1; ++px)
                        set_pixel(px, py, color);
            }

            void stroke_rect(int x, int y, int w, int h, gui::Color color) noexcept
            {
                fill_rect(x, y, w, 1, color);
                fill_rect(x, y + h - 1, w, 1, color);
                fill_rect(x, y, 1, h, color);
                fill_rect(x + w - 1, y, 1, h, color);
            }

            void hline(int x, int y, int w, gui::Color color, int thickness = 1) noexcept
            {
                fill_rect(x, y, w, thickness, color);
            }

            void vline(int x, int y, int h, gui::Color color, int thickness = 1) noexcept
            {
                fill_rect(x, y, thickness, h, color);
            }
        };

        [[nodiscard]] static std::array<std::string_view, 7> tiny_glyph(char ch) noexcept
        {
            switch (static_cast<char>(std::toupper(static_cast<unsigned char>(ch))))
            {
            case '0': return { " ### ", "#   #", "#  ##", "# # #", "##  #", "#   #", " ### " };
            case '1': return { "  #  ", " ##  ", "# #  ", "  #  ", "  #  ", "  #  ", "#####" };
            case '2': return { " ### ", "#   #", "    #", "   # ", "  #  ", " #   ", "#####" };
            case '3': return { "#### ", "    #", "    #", " ### ", "    #", "    #", "#### " };
            case '4': return { "#   #", "#   #", "#   #", "#####", "    #", "    #", "    #" };
            case '5': return { "#####", "#    ", "#    ", "#### ", "    #", "#   #", " ### " };
            case '6': return { " ### ", "#    ", "#    ", "#### ", "#   #", "#   #", " ### " };
            case '7': return { "#####", "    #", "   # ", "  #  ", " #   ", " #   ", " #   " };
            case '8': return { " ### ", "#   #", "#   #", " ### ", "#   #", "#   #", " ### " };
            case '9': return { " ### ", "#   #", "#   #", " ####", "    #", "    #", " ### " };
            case 'A': return { " ### ", "#   #", "#   #", "#####", "#   #", "#   #", "#   #" };
            case 'B': return { "#### ", "#   #", "#   #", "#### ", "#   #", "#   #", "#### " };
            case 'C': return { " ### ", "#   #", "#    ", "#    ", "#    ", "#   #", " ### " };
            case 'D': return { "#### ", "#   #", "#   #", "#   #", "#   #", "#   #", "#### " };
            case 'E': return { "#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#####" };
            case 'F': return { "#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#    " };
            case 'G': return { " ### ", "#   #", "#    ", "#  ##", "#   #", "#   #", " ### " };
            case 'H': return { "#   #", "#   #", "#   #", "#####", "#   #", "#   #", "#   #" };
            case 'I': return { "#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "#####" };
            case 'J': return { "#####", "    #", "    #", "    #", "#   #", "#   #", " ### " };
            case 'K': return { "#   #", "#  # ", "# #  ", "##   ", "# #  ", "#  # ", "#   #" };
            case 'L': return { "#    ", "#    ", "#    ", "#    ", "#    ", "#    ", "#####" };
            case 'M': return { "#   #", "## ##", "# # #", "#   #", "#   #", "#   #", "#   #" };
            case 'N': return { "#   #", "##  #", "# # #", "#  ##", "#   #", "#   #", "#   #" };
            case 'O': return { " ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### " };
            case 'P': return { "#### ", "#   #", "#   #", "#### ", "#    ", "#    ", "#    " };
            case 'Q': return { " ### ", "#   #", "#   #", "#   #", "# # #", "#  # ", " ## #" };
            case 'R': return { "#### ", "#   #", "#   #", "#### ", "# #  ", "#  # ", "#   #" };
            case 'S': return { " ####", "#    ", "#    ", " ### ", "    #", "    #", "#### " };
            case 'T': return { "#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  " };
            case 'U': return { "#   #", "#   #", "#   #", "#   #", "#   #", "#   #", " ### " };
            case 'V': return { "#   #", "#   #", "#   #", "#   #", "#   #", " # # ", "  #  " };
            case 'W': return { "#   #", "#   #", "#   #", "# # #", "# # #", "## ##", "#   #" };
            case 'X': return { "#   #", "#   #", " # # ", "  #  ", " # # ", "#   #", "#   #" };
            case 'Y': return { "#   #", "#   #", " # # ", "  #  ", "  #  ", "  #  ", "  #  " };
            case 'Z': return { "#####", "    #", "   # ", "  #  ", " #   ", "#    ", "#####" };
            case '+': return { "     ", "  #  ", "  #  ", "#####", "  #  ", "  #  ", "     " };
            case '-': return { "     ", "     ", "     ", "#####", "     ", "     ", "     " };
            case '.': return { "     ", "     ", "     ", "     ", "     ", "     ", "  #  " };
            case '/': return { "    #", "    #", "   # ", "  #  ", " #   ", "#    ", "#    " };
            case ':': return { "     ", "  #  ", "     ", "     ", "  #  ", "     ", "     " };
            case '>': return { "#    ", " #   ", "  #  ", "   # ", "  #  ", " #   ", "#    " };
            default: return { "     ", "     ", "     ", "     ", "     ", "     ", "     " };
            }
        }

        static void draw_tiny_text(
            SurfaceCanvas& canvas,
            std::string_view text,
            int x,
            int y,
            gui::Color color,
            int scale = 2) noexcept
        {
            int penX = x;
            const int safeScale = (std::max)(1, scale);
            for (const char ch : text)
            {
                if (ch == ' ')
                {
                    penX += 4 * safeScale;
                    continue;
                }

                const auto glyph = tiny_glyph(ch);
                for (int row = 0; row < 7; ++row)
                {
                    for (int col = 0; col < 5; ++col)
                    {
                        if (glyph[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] != ' ')
                            canvas.fill_rect(penX + col * safeScale, y + row * safeScale, safeScale, safeScale, color);
                    }
                }
                penX += 6 * safeScale;
            }
        }

        static void draw_tiny_text_clipped(
            SurfaceCanvas& canvas,
            std::string_view text,
            int x,
            int y,
            int maxWidth,
            gui::Color color,
            int scale = 2) noexcept
        {
            int penX = x;
            const int safeScale = (std::max)(1, scale);
            const int endX = x + (std::max)(0, maxWidth);
            for (const char ch : text)
            {
                const int advance = (ch == ' ') ? (4 * safeScale) : (6 * safeScale);
                if (penX + advance > endX)
                    break;

                if (ch == ' ')
                {
                    penX += advance;
                    continue;
                }

                const auto glyph = tiny_glyph(ch);
                for (int row = 0; row < 7; ++row)
                {
                    for (int col = 0; col < 5; ++col)
                    {
                        if (glyph[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] != ' ')
                            canvas.fill_rect(penX + col * safeScale, y + row * safeScale, safeScale, safeScale, color);
                    }
                }
                penX += advance;
            }
        }

        [[nodiscard]] static std::string recommended_support_tier(
            const std::shared_ptr<core::Context>& ctx,
            std::size_t workerCount)
        {
            if (!ctx)
                return "Baseline";

            if (workerCount >= 10
                && (ctx->type == core::ContextType::Vulkan
                    || ctx->type == core::ContextType::OpenGL))
            {
                return "Extended";
            }

            switch (ctx->type)
            {
            case core::ContextType::Vulkan:
                return workerCount >= 8 ? "Standard" : "Baseline";
            case core::ContextType::OpenGL:
            case core::ContextType::RayLib:
            case core::ContextType::SFML:
            case core::ContextType::SDL:
                return workerCount >= 6 ? "Standard" : "Baseline";
            case core::ContextType::Software:
            default:
                return "Baseline";
            }
        }

        [[nodiscard]] static std::string pacing_health_summary(const EditorTimeSnapshot& snapshot)
        {
            if (snapshot.paused)
                return "Paused by editor control";

            if (snapshot.fixed_dt_seconds <= 0.0)
                return "No fixed-step pacing configured";

            const double frameBudgetSeconds = snapshot.fixed_dt_seconds
                * static_cast<double>((std::max)(1u, snapshot.max_steps_per_frame));

            if (snapshot.step_budget >= snapshot.max_steps_per_frame
                && snapshot.max_steps_per_frame > 0)
            {
                return "At frame-step cap; simulation debt is being clamped";
            }

            if (snapshot.real_dt_seconds > frameBudgetSeconds * 1.15)
            {
                return "Over budget; frame time is outrunning configured simulation pacing";
            }

            if (snapshot.accumulator_seconds > snapshot.fixed_dt_seconds * 0.5)
            {
                return "Recovering accumulated step debt";
            }

            return "Healthy; frame pacing is inside the configured budget";
        }

        [[nodiscard]] static std::string compiler_identity()
        {
#if defined(__clang__)
            return epochengine::format_text("Clang {}.{}.{}", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(_MSC_VER)
            return epochengine::format_text("MSVC {}", _MSC_VER);
#elif defined(__GNUC__)
            return epochengine::format_text("GCC {}.{}.{}", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
            return "Unknown compiler";
#endif
        }

        [[nodiscard]] static std::string language_mode_summary()
        {
            return epochengine::format_text("__cplusplus={} (C++23 baseline; latest validation is optional)",
                static_cast<long long>(__cplusplus));
        }

        [[nodiscard]] static constexpr std::string_view build_configuration_label() noexcept
        {
#if defined(NDEBUG)
            return "Release";
#else
            return "Debug";
#endif
        }

        [[nodiscard]] static std::string feature_probe_summary()
        {
            return epochengine::format_text("expected={} stacktrace={} execution={} contracts={} reflection={}",
                epochengine::core::has_expected ? "yes" : "no",
                epochengine::core::has_stacktrace ? "yes" : "no",
                epochengine::core::has_std_execution ? "yes" : "no",
                epochengine::core::has_contracts ? "yes" : "no",
                epochengine::core::has_static_reflection ? "yes" : "no");
        }

        [[nodiscard]] static constexpr std::string_view hosted_ci_contract() noexcept
        {
            return "C++23 headless plus MSVC C++latest headless; full graphics stays local/release until runner-safe.";
        }

        [[nodiscard]] static std::string backend_runtime_guidance(
            const std::shared_ptr<core::Context>& ctx,
            std::string_view supportTier)
        {
            if (!ctx)
                return "No active backend";

            switch (ctx->type)
            {
            case core::ContextType::OpenGL:
                return std::string("Current editor priority path; keep startup presentation clean and stay within ")
                    + std::string(supportTier) + " tier expectations.";
            case core::ContextType::Vulkan:
                return "Best fit for stronger desktop tiers; keep shader/package validation honest before treating it as default.";
            case core::ContextType::Software:
                return "Correctness and capture fallback; prioritize clarity and deterministic tooling over throughput.";
            case core::ContextType::SDL:
                return "Proxy-host backend; keep detach, input ownership, and redock behavior stable before polishing extras.";
            case core::ContextType::SFML:
                return "Proxy-host backend; preserve truthful promoted-window behavior while converging input and resize parity.";
            case core::ContextType::RayLib:
                return "Useful backend baseline for detached-window truth; protect the working ownership contract while converging behavior.";
            default:
                return "Keep runtime ownership explicit and avoid backend-specific drift.";
            }
        }

        [[nodiscard]] static std::string backend_convergence_focus(const std::shared_ptr<core::Context>& ctx)
        {
            if (!ctx)
                return "No active backend";

            switch (ctx->type)
            {
            case core::ContextType::OpenGL:
                return "Focus: first-present stability, startup cosmetics, and single-context editor convergence.";
            case core::ContextType::Vulkan:
                return "Focus: packaging/shader validation and support-tier honesty.";
            case core::ContextType::Software:
                return "Focus: capture fidelity, deterministic output, and launcher-shell baseline behavior.";
            case core::ContextType::SDL:
                return "Focus: promoted-window detach/redock stability and input ownership.";
            case core::ContextType::SFML:
                return "Focus: promoted-window detach/redock parity and resize/input cleanup.";
            case core::ContextType::RayLib:
                return "Focus: keep the working detach contract as the runtime truth reference for the other proxy-host backends.";
            default:
                return "Focus: converge backend behavior without adding another special-case path.";
            }
        }

        [[nodiscard]] static SurfaceCanvas build_ai_loop_surface(
            const std::array<bool, 5>& readiness,
            bool gateReady,
            bool buildPending)
        {
            constexpr int kSurfaceWidth = 960;
            constexpr int kSurfaceHeight = 124;
            SurfaceCanvas canvas(kSurfaceWidth, kSurfaceHeight, gui::Color{ 12, 15, 21, 255 });

            for (int x = 24; x < kSurfaceWidth; x += 48)
                canvas.vline(x, 0, kSurfaceHeight, gui::Color{ 20, 25, 34, 255 });
            for (int y = 20; y < kSurfaceHeight; y += 28)
                canvas.hline(0, y, kSurfaceWidth, gui::Color{ 18, 23, 31, 255 });

            const std::array<gui::Color, 5> accents{{
                { 91, 151, 255, 255 },
                { 255, 196, 88, 255 },
                { 88, 211, 159, 255 },
                { 190, 128, 255, 255 },
                { 255, 132, 155, 255 }
            }};
            constexpr std::array<std::string_view, 5> stageNames{
                "PLAN",
                "EXEC",
                "BUILD",
                "VERIFY",
                "GATE"
            };
            draw_tiny_text(canvas, "OS AI EVIDENCE LOOP", 34, 12, gui::Color{ 214, 224, 238, 255 }, 1);

            int active = 4;
            for (int i = 0; i < 5; ++i)
            {
                if (!readiness[static_cast<std::size_t>(i)])
                {
                    active = i;
                    break;
                }
            }

            const int cardW = 136;
            const int cardH = 54;
            const int gap = 44;
            const int baseX = 34;
            const int y = 38;

            for (int i = 0; i < 5; ++i)
            {
                const int x = baseX + i * (cardW + gap);
                const bool ready = readiness[static_cast<std::size_t>(i)];
                const bool current = i == active;
                const auto accent = accents[static_cast<std::size_t>(i)];
                const gui::Color fill = ready
                    ? gui::Color{ 38, 82, 66, 255 }
                    : (current ? gui::Color{ 92, 73, 38, 255 } : gui::Color{ 42, 48, 60, 255 });
                const gui::Color stroke = ready ? gui::Color{ 102, 244, 168, 255 } : (current ? accent : gui::Color{ 255, 255, 255, 36 });

                if (i != 0)
                    canvas.fill_rect(x - gap + 8, y + cardH / 2 - 3, gap - 16, 6, gui::Color{ 54, 62, 78, 255 });

                canvas.fill_rect(x, y, cardW, cardH, fill);
                canvas.stroke_rect(x, y, cardW, cardH, stroke);
                canvas.fill_rect(x + 12, y + 12, 28, cardH - 24, gui::Color{ 255, 255, 255, static_cast<std::uint8_t>(ready ? 52 : 28) });
                canvas.fill_rect(x + cardW - 14, y + 10, 6, cardH - 20, accent);
                draw_tiny_text(canvas, stageNames[static_cast<std::size_t>(i)], x + 48, y + 20, gui::Color{ 232, 238, 248, 255 }, 2);
                if (current)
                    canvas.fill_rect(x + 8, y - 10, cardW - 16, 5, accent);
            }

            const int progressW = 820;
            canvas.fill_rect(34, 104, progressW, 8, gui::Color{ 30, 35, 45, 255 });
            const int readyCount = static_cast<int>(std::count(readiness.begin(), readiness.end(), true));
            canvas.fill_rect(34, 104, (progressW * readyCount) / 5, 8, gui::Color{ 94, 201, 142, 255 });

            if (gateReady)
                canvas.fill_rect(866, 20, 68, 16, gui::Color{ 80, 167, 115, 255 });
            else
                canvas.fill_rect(866, 20, 68, 16, gui::Color{ 97, 80, 70, 255 });

            if (buildPending)
                canvas.fill_rect(866, 44, 68, 16, gui::Color{ 223, 174, 77, 255 });
            else
                canvas.fill_rect(866, 44, 68, 16, gui::Color{ 58, 68, 84, 255 });
            draw_tiny_text(canvas, gateReady ? "GATE OK" : "GATE WAIT", 764, 23, gui::Color{ 238, 242, 248, 255 }, 1);
            draw_tiny_text(canvas, buildPending ? "BUILD RUN" : "BUILD IDLE", 764, 47, gui::Color{ 238, 242, 248, 255 }, 1);

            return canvas;
        }

        [[nodiscard]] EditorAutomationCommand read_editor_automation_command() noexcept
        {
            std::string value;

#if defined(_WIN32)
            char* raw = nullptr;
            std::size_t raw_size = 0;
            if (_dupenv_s(&raw, &raw_size, "EPOCH_EDITOR_AUTO_COMMAND") != 0 || raw == nullptr)
                return EditorAutomationCommand::None;

            value.assign(raw, raw_size > 0 ? raw_size - 1 : 0);
            free(raw);
#else
            if (const char* const raw = std::getenv("EPOCH_EDITOR_AUTO_COMMAND"))
                value = raw;
            else
                return EditorAutomationCommand::None;
#endif

            const auto consume = [](
                const EditorAutomationCommand command) noexcept
                {
#if defined(_WIN32)
                    (void)_putenv_s("EPOCH_EDITOR_AUTO_COMMAND", "");
#else
                    (void)::unsetenv("EPOCH_EDITOR_AUTO_COMMAND");
#endif
                    return command;
                };

            if (value == "smart-update")
            {
                return consume(EditorAutomationCommand::SmartUpdate);
            }
            if (value == "source-update")
            {
                return consume(EditorAutomationCommand::SourceUpdate);
            }
            if (value == "assets-interaction-proof")
            {
                return consume(EditorAutomationCommand::AssetsInteractionProof);
            }
            if (value == "candidate-lab-smoke-choose")
            {
                return consume(EditorAutomationCommand::CandidateLabSmokeChoose);
            }
            if (value == "candidate-lab-smoke-keep")
            {
                return consume(EditorAutomationCommand::CandidateLabSmokeKeep);
            }
            if (value == "self-coding-local-smoke")
            {
                return consume(EditorAutomationCommand::SelfCodingLocalSmoke);
            }

            return EditorAutomationCommand::None;
        }

        void append_editor_automation_trace(const std::string_view message)
        {
            const auto data = core::path::candidate_data_root();
            const auto path = data.empty()
                ? epochengine::logger::config().root_dir / "epoch_editor_auto_command.log"
                : data / "logs" / "epoch_editor_auto_command.log";
            std::error_code logError{};
            std::filesystem::create_directories(path.parent_path(), logError);
            if (logError) return;
            std::ofstream trace(path, std::ios::app | std::ios::binary);
            if (!trace)
                return;

            trace << message << '\n';
        }

        [[nodiscard]] std::string read_editor_automation_text(
            const char* const name) noexcept
        {
#if defined(_WIN32)
            char* raw = nullptr;
            std::size_t rawSize = 0u;
            if (_dupenv_s(&raw, &rawSize, name) != 0 || raw == nullptr)
                return {};
            std::string value{raw, rawSize > 0u ? rawSize - 1u : 0u};
            std::free(raw);
            return value;
#else
            if (const char* const raw = std::getenv(name))
                return std::string{raw};
            return {};
#endif
        }

        [[nodiscard]] std::string read_requested_editor_project_id() noexcept
        {
#if defined(_WIN32)
            char* raw = nullptr;
            std::size_t raw_size = 0;
            if (_dupenv_s(&raw, &raw_size, "EPOCH_EDITOR_PROJECT_ID") != 0 || raw == nullptr)
                return {};

            std::string value{ raw };
            std::free(raw);
            return value;
#else
            if (const char* const raw = std::getenv("EPOCH_EDITOR_PROJECT_ID"))
                return std::string{ raw };
            return {};
#endif
        }

        [[nodiscard]] bool try_claim_editor_automation_command(EditorAutomationCommand command) noexcept
        {
            if (command == EditorAutomationCommand::None)
                return false;

            static std::atomic<bool> claimed{ false };
            bool expected = false;
            return claimed.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
        }

