/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module canvas2d.scene_contracts;

import editor.canvas2d_scene;
import render.canvas2d_scene;

namespace epochengine::canvas2d_scene_contracts
{
    ContractResult run() noexcept
    {
        const auto resources =
            canvas2d::scene_content::run_resource_closure_contract();
        if (resources
            != canvas2d::scene_content::ResourceClosureContractFailure::none)
        {
            return {false,
                canvas2d::scene_content::resource_closure_contract_failure_name(
                    resources)};
        }
        const auto scene = canvas2d::scene_content::run_scene_content_contract();
        if (scene != canvas2d::scene_content::SceneContractFailure::none)
        {
            return {
                false,
                canvas2d::scene_content::scene_contract_failure_name(scene)};
        }
        const auto editor = editor_canvas2d::run_contract();
        if (editor != editor_canvas2d::ContractFailure::none)
            return {false, editor_canvas2d::contract_failure_name(editor)};
        return {true, "pass"};
    }
}
