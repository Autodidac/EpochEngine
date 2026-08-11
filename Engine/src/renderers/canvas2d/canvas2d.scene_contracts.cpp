/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module canvas2d.scene_contracts;

import editor.canvas2d_scene;
import render.canvas2d_evidence;
import render.canvas2d_runtime;
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
        const auto evidence =
            canvas2d::evidence::canvas2d_pixel_evidence_runtime_contract_failure();
        if (evidence != canvas2d::evidence::PixelEvidenceContractFailure::none)
        {
            return {
                false,
                canvas2d::evidence::pixel_evidence_contract_failure_name(
                    evidence)};
        }
        const auto scene = canvas2d::scene_content::run_scene_content_contract();
        if (scene != canvas2d::scene_content::SceneContractFailure::none)
        {
            return {
                false,
                canvas2d::scene_content::scene_contract_failure_name(scene)};
        }
        const auto runtime =
            canvas2d::runtime::run_scene_raster_session_contract();
        if (runtime != canvas2d::runtime::RuntimeContractFailure::none)
        {
            return {
                false,
                canvas2d::runtime::runtime_contract_failure_name(runtime)};
        }
        const auto editor = editor_canvas2d::run_contract();
        if (editor != editor_canvas2d::ContractFailure::none)
            return {false, editor_canvas2d::contract_failure_name(editor)};
        return {true, "pass"};
    }
}
