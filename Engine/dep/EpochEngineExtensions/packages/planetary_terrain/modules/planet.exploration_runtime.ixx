// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <memory>
export module planet.exploration_runtime;
export import project.planetary_runtime;

export namespace epoch::extensions::planetary_terrain::exploration_runtime
{
    [[nodiscard]] std::unique_ptr<epochengine::project_planetary_runtime::Controller>
        make_controller(const epochengine::project_planetary_runtime::Binding&);
    // The host must first bind its normal admitted project and exact package.
    [[nodiscard]] inline bool register_controller(const epochengine::project_planetary_runtime::Binding& binding)
    {
        return epochengine::project_planetary_runtime::register_factory(binding, &make_controller);
    }
}
