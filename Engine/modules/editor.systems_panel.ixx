/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

export module editor.systems_panel;

import systems.registry;
import taskgraph.dotsystem;

export namespace epochengine::editor_systems_panel
{
    struct Snapshot final
    {
        std::string renderer{};
        std::string capability{};
        std::string editor_admission{};
        std::string project_admission{};
        std::string resource_spine{};
        std::string proof_stages{};
        std::string rtt_overall{};
        std::string rtt_presentation{};
        std::string mesh_model{};
        std::string next_gate{};
        std::string backend_guidance{};
        std::string convergence_focus{};
        std::string support_tier{};
        std::string project_build{};
        std::string script_build{};
        std::size_t live_threads{};
        std::size_t hardware_threads{};
    };

    class Panel final
    {
    public:
        Panel();
        ~Panel();

        Panel(const Panel&) = delete;
        Panel& operator=(const Panel&) = delete;
        Panel(Panel&&) noexcept;
        Panel& operator=(Panel&&) noexcept;

        void render(
            float available_width,
            systems::Registry& registry,
            const Snapshot& snapshot,
            const taskgraph::TaskGraph* live_scheduler = nullptr,
            std::string_view live_scheduler_owner = {});

        [[nodiscard]] bool diagnostics_sampling_intent() const noexcept;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation_{};
    };
}