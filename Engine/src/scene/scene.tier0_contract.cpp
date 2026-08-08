// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <array>
#include <cstddef>
#include <limits>

import scene.tier0;
import render.lighting;
import terrain.foundation;

namespace epochengine::scene_tier0::contract
{
    [[nodiscard]] int run_tier0_contract()
    {
        const Tier0BuildResult first = make_default_scene();
        const Tier0BuildResult second = make_default_scene();
        if (!first || !second)
        {
            return 1;
        }

        const Tier0Validation validation = validate(first.scene);
        if (!validation ||
            first.scene.objects.size() != kTier0ObjectCount ||
            descriptor_fingerprint(first.scene) == 0 ||
            descriptor_fingerprint(first.scene) !=
                descriptor_fingerprint(second.scene))
        {
            return 2;
        }

        const auto meshPlan = epochengine::terrain::make_mesh_plan(
            first.scene.groundTerrain);
        if (!meshPlan ||
            meshPlan->vertexCount != 4 ||
            meshPlan->triangleCount != 2 ||
            meshPlan->indexCount != 6 ||
            !meshPlan->requiresNativeUpload)
        {
            return 3;
        }

        const auto groundSurface =
            epochengine::terrain::sample_surface(
                first.scene.groundTerrain,
                0.0f,
                0.0f);
        if (!groundSurface ||
            groundSurface->position.y != 0.0f ||
            groundSurface->normal != Float3{0.0f, 1.0f, 0.0f})
        {
            return 4;
        }

        const auto cameraFocus =
            focus_target(first.scene, kDefaultCameraId);
        const auto groundFocus =
            focus_target(first.scene, kDefaultGroundId);
        if (!cameraFocus ||
            !groundFocus ||
            cameraFocus->object != kDefaultCameraId ||
            groundFocus->object != kDefaultGroundId ||
            groundFocus->radius <= cameraFocus->radius)
        {
            return 5;
        }

        epochengine::lighting::LightManager lightManager{1};
        const LightingRegistration lighting =
            register_lighting(first.scene, lightManager);
        const epochengine::lighting::LightingFrame frame =
            lightManager.build_frame();
        if (!lighting ||
            frame.lights.size() != 1 ||
            frame.lights[0].desc.kind !=
                epochengine::lighting::LightKind::Directional)
        {
            return 6;
        }

        const auto runRequest = make_run_request(first.scene);
        if (!runRequest ||
            runRequest->project != kDefaultProjectId ||
            runRequest->scene != kDefaultSceneId ||
            runRequest->camera != kDefaultCameraId ||
            runRequest->spawn != kDefaultSpawnId ||
            runRequest->sceneRevision != first.scene.revision)
        {
            return 7;
        }

        Tier0Limits insufficient{};
        insufficient.maximumObjects = kTier0ObjectCount - 1;
        if (make_default_scene(insufficient).status !=
            Tier0BuildStatus::invalid_limits)
        {
            return 8;
        }

        epochengine::terrain::HeightfieldDescriptor localTerrain{};
        localTerrain.asset = epochengine::terrain::stable_asset_id(
            "epoch.terrain.contract.local");
        localTerrain.kind =
            epochengine::terrain::FoundationKind::local_heightfield;
        localTerrain.origin = {-1.0f, 0.0f, -1.0f};
        localTerrain.widthSamples = 3;
        localTerrain.depthSamples = 3;
        localTerrain.sampleSpacing = 1.0f;
        localTerrain.baseHeight = 1.0f;
        localTerrain.heightScale = 2.0f;
        constexpr std::array<float, 9> samples{
            0.0f, 0.0f, 0.0f,
            0.0f, 0.5f, 0.0f,
            0.0f, 0.0f, 0.0f
        };
        auto localBuild = epochengine::terrain::make_heightfield(
            localTerrain,
            samples);
        if (!localBuild)
        {
            return 9;
        }

        const std::uint64_t initialRevision =
            localBuild.heightfield.revision;
        if (epochengine::terrain::set_sample(
                localBuild.heightfield,
                1,
                1,
                0.5f) != epochengine::terrain::TerrainStatus::ready ||
            localBuild.heightfield.revision != initialRevision)
        {
            return 10;
        }

        const auto peak = epochengine::terrain::sample_surface(
            localBuild.heightfield,
            0.0f,
            0.0f);
        if (!peak || peak->position.y != 2.0f)
        {
            return 11;
        }

        if (!epochengine::terrain::requires_extension(
                epochengine::terrain::TerrainScaleClass::
                    planetary_extension) ||
            epochengine::terrain::kFoundationCapabilities.nativeRendering ||
            epochengine::terrain::kFoundationCapabilities.planetaryTerrain)
        {
            return 12;
        }

        Tier0Scene tampered = first.scene;
        tampered.groundTerrain.normalizedSamples[0] =
            (std::numeric_limits<float>::quiet_NaN)();
        if (tampered.groundTerrain.valid() || make_run_request(tampered))
        {
            return 13;
        }

        return 0;
    }
}

#if defined(EPOCH_SCENE_TIER0_CONTRACT_MAIN)
int main()
{
    return epochengine::scene_tier0::contract::run_tier0_contract();
}
#endif
