/************************************************
 * Epoch Engine - Neuromorphic Contract
 *
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 * Provided "AS IS", without warranty of any kind.
 * See LICENSE for full terms.
 ***********************************************/
import render.neuromorphic_camera;
import render.neuromorphic_invalidation;
import simulation.neuromorphic;
import simulation.neuromorphic_adapters;

int main()
{
    const auto runtime = epochengine::simulation::neuromorphic::run_contract_checks();
    const auto camera = epochengine::render::neuromorphic_camera::run_contract_checks();
    const auto adapters = epochengine::simulation::neuromorphic::adapters::run_contract_checks();
    const auto invalidation = epochengine::render::neuromorphic_invalidation::run_contract_checks();
    return runtime.passed() && camera.passed() && adapters.passed() && invalidation.passed() ? 0 : 1;
}
