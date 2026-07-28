// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

export module scene.tier0;

import render.lighting;
import render.math;
import terrain.foundation;

export namespace epochengine::scene_tier0
{
    using Float3 = epochengine::render_math::Float3;
    using LinearRgb = epochengine::render_math::LinearRgb;

    template<typename Tag>
    struct StableId final
    {
        std::uint64_t value{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return value != 0;
        }

        friend constexpr bool operator==(
            const StableId&,
            const StableId&) noexcept = default;
    };

    struct SceneIdTag final {};
    struct ObjectIdTag final {};
    struct ProjectIdTag final {};

    using SceneId = StableId<SceneIdTag>;
    using ObjectId = StableId<ObjectIdTag>;
    using ProjectId = StableId<ProjectIdTag>;

    [[nodiscard]] constexpr std::uint64_t stable_hash(
        std::string_view text) noexcept
    {
        std::uint64_t hash = 14695981039346656037ull;
        for (const unsigned char character : text)
        {
            hash ^= character;
            hash *= 1099511628211ull;
        }
        return hash == 0 ? 1 : hash;
    }

    template<typename Id>
    [[nodiscard]] constexpr Id stable_id(
        std::string_view canonicalName) noexcept
    {
        return {stable_hash(canonicalName)};
    }

    inline constexpr SceneId kDefaultSceneId =
        stable_id<SceneId>("epoch.scene.tier0.default");
    inline constexpr ProjectId kDefaultProjectId =
        stable_id<ProjectId>("epoch.project.tier0.default");
    inline constexpr ObjectId kDefaultCameraId =
        stable_id<ObjectId>("epoch.scene.tier0.default.camera");
    inline constexpr ObjectId kDefaultGroundId =
        stable_id<ObjectId>("epoch.scene.tier0.default.ground");
    inline constexpr ObjectId kDefaultDirectionalLightId =
        stable_id<ObjectId>("epoch.scene.tier0.default.directional_light");
    inline constexpr ObjectId kDefaultSpawnId =
        stable_id<ObjectId>("epoch.scene.tier0.default.spawn");

    inline constexpr std::size_t kTier0ObjectCount = 4;
    inline constexpr std::size_t kTier0DirectionalLightCount = 1;

    struct TransformDescriptor final
    {
        Float3 position{};
        Float3 rotationDegrees{};
        Float3 scale{1.0f, 1.0f, 1.0f};

        friend constexpr bool operator==(
            const TransformDescriptor&,
            const TransformDescriptor&) noexcept = default;
    };

    [[nodiscard]] inline bool valid(
        const TransformDescriptor& transform) noexcept
    {
        return
            epochengine::render_math::finite(transform.position) &&
            epochengine::render_math::finite(transform.rotationDegrees) &&
            epochengine::render_math::finite(transform.scale) &&
            transform.scale.x > 0.0f &&
            transform.scale.y > 0.0f &&
            transform.scale.z > 0.0f;
    }

    enum class ObjectKind : std::uint8_t
    {
        camera,
        ground,
        directional_light,
        spawn_point
    };

    struct SceneObjectDescriptor final
    {
        ObjectId id{};
        ObjectKind kind{ObjectKind::camera};
        std::string_view canonicalName{};
        TransformDescriptor transform{};
        bool runtimeVisible{true};
        bool editorVisible{true};
    };

    struct CameraDescriptor final
    {
        ObjectId object{};
        Float3 lookTarget{};
        Float3 up{0.0f, 1.0f, 0.0f};
        float verticalFieldOfViewDegrees{60.0f};
        float nearPlane{0.05f};
        float farPlane{1000.0f};
        bool primary{true};
    };

    struct GroundDescriptor final
    {
        ObjectId object{};
        epochengine::terrain::TerrainAssetId terrain{};
        LinearRgb baseColor{0.32f, 0.38f, 0.28f};
        float roughness{0.88f};
        bool receivesLighting{true};
        bool receivesShadows{true};
        bool castsShadows{};
    };

    struct DirectionalLightDescriptor final
    {
        ObjectId object{};
        epochengine::lighting::LightDesc light{};
        bool castsShadows{true};
    };

    struct SpawnPointDescriptor final
    {
        ObjectId object{};
        std::string_view role{"primary"};
        float clearanceAboveGround{0.05f};
        bool enabled{true};
    };

    enum class ProjectRunState : std::uint8_t
    {
        blocked,
        ready_to_launch
    };

    struct RunnableProjectDescriptor final
    {
        ProjectId project{};
        SceneId entryScene{};
        ObjectId primaryCamera{};
        ObjectId primarySpawn{};
        ProjectRunState state{ProjectRunState::blocked};
        double fixedStepSeconds{1.0 / 60.0};
        bool startPaused{};
    };

    struct Tier0Scene final
    {
        SceneId id{};
        std::uint64_t revision{};
        std::string_view canonicalName{};
        std::array<SceneObjectDescriptor, kTier0ObjectCount> objects{};
        CameraDescriptor camera{};
        GroundDescriptor ground{};
        DirectionalLightDescriptor directionalLight{};
        SpawnPointDescriptor spawn{};
        RunnableProjectDescriptor project{};
        epochengine::terrain::Heightfield groundTerrain{};
    };

    struct Tier0Limits final
    {
        std::size_t maximumObjects{32};
        std::size_t maximumDirectionalLights{4};
        epochengine::terrain::TerrainLimits terrain{};
    };

    enum class Tier0BuildStatus : std::uint8_t
    {
        ready,
        invalid_limits,
        terrain_failed,
        invalid_scene
    };

    struct Tier0Validation final
    {
        bool validIdentities{};
        bool uniqueObjectIdentities{};
        bool requiredObjectsPresent{};
        bool referencesResolve{};
        bool cameraValid{};
        bool groundValid{};
        bool lightingValid{};
        bool spawnValid{};
        bool projectRunnable{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return
                validIdentities &&
                uniqueObjectIdentities &&
                requiredObjectsPresent &&
                referencesResolve &&
                cameraValid &&
                groundValid &&
                lightingValid &&
                spawnValid &&
                projectRunnable;
        }
    };

    [[nodiscard]] inline const SceneObjectDescriptor* find_object(
        const Tier0Scene& scene,
        ObjectId object) noexcept
    {
        for (const SceneObjectDescriptor& candidate : scene.objects)
        {
            if (candidate.id == object)
            {
                return &candidate;
            }
        }
        return nullptr;
    }

    [[nodiscard]] inline Tier0Validation validate(
        const Tier0Scene& scene,
        const Tier0Limits& limits = {}) noexcept
    {
        Tier0Validation result{};
        result.validIdentities =
            scene.id.valid() &&
            scene.project.project.valid() &&
            scene.groundTerrain.descriptor.asset.valid();

        result.uniqueObjectIdentities = true;
        for (std::size_t index = 0; index < scene.objects.size(); ++index)
        {
            const SceneObjectDescriptor& object = scene.objects[index];
            if (!object.id.valid() ||
                object.canonicalName.empty() ||
                !valid(object.transform))
            {
                result.validIdentities = false;
            }
            for (std::size_t other = index + 1;
                 other < scene.objects.size();
                 ++other)
            {
                if (object.id == scene.objects[other].id)
                {
                    result.uniqueObjectIdentities = false;
                }
            }
        }

        const SceneObjectDescriptor* cameraObject =
            find_object(scene, scene.camera.object);
        const SceneObjectDescriptor* groundObject =
            find_object(scene, scene.ground.object);
        const SceneObjectDescriptor* lightObject =
            find_object(scene, scene.directionalLight.object);
        const SceneObjectDescriptor* spawnObject =
            find_object(scene, scene.spawn.object);
        result.requiredObjectsPresent =
            cameraObject != nullptr &&
            cameraObject->kind == ObjectKind::camera &&
            groundObject != nullptr &&
            groundObject->kind == ObjectKind::ground &&
            lightObject != nullptr &&
            lightObject->kind == ObjectKind::directional_light &&
            spawnObject != nullptr &&
            spawnObject->kind == ObjectKind::spawn_point;

        result.referencesResolve =
            scene.project.entryScene == scene.id &&
            scene.project.primaryCamera == scene.camera.object &&
            scene.project.primarySpawn == scene.spawn.object &&
            scene.ground.terrain == scene.groundTerrain.descriptor.asset;

        result.cameraValid =
            cameraObject != nullptr &&
            scene.camera.primary &&
            epochengine::render_math::finite(scene.camera.lookTarget) &&
            epochengine::render_math::finite(scene.camera.up) &&
            epochengine::render_math::length_squared(scene.camera.up) > 0.0f &&
            std::isfinite(scene.camera.verticalFieldOfViewDegrees) &&
            scene.camera.verticalFieldOfViewDegrees > 1.0f &&
            scene.camera.verticalFieldOfViewDegrees < 179.0f &&
            std::isfinite(scene.camera.nearPlane) &&
            std::isfinite(scene.camera.farPlane) &&
            scene.camera.nearPlane > 0.0f &&
            scene.camera.farPlane > scene.camera.nearPlane &&
            epochengine::render_math::length_squared(
                epochengine::render_math::subtract(
                    cameraObject != nullptr
                        ? cameraObject->transform.position
                        : Float3{},
                    scene.camera.lookTarget)) > 0.0001f;

        const epochengine::terrain::TerrainValidation terrainValidation =
            epochengine::terrain::validate(
                scene.groundTerrain.descriptor,
                limits.terrain);
        result.groundValid =
            groundObject != nullptr &&
            scene.groundTerrain.valid() &&
            terrainValidation &&
            scene.ground.receivesLighting &&
            std::isfinite(scene.ground.roughness) &&
            scene.ground.roughness >= 0.0f &&
            scene.ground.roughness <= 1.0f &&
            std::isfinite(scene.ground.baseColor.r) &&
            std::isfinite(scene.ground.baseColor.g) &&
            std::isfinite(scene.ground.baseColor.b) &&
            scene.ground.baseColor.r >= 0.0f &&
            scene.ground.baseColor.g >= 0.0f &&
            scene.ground.baseColor.b >= 0.0f;

        const epochengine::lighting::LightValidation lightValidation =
            epochengine::lighting::validate(scene.directionalLight.light);
        result.lightingValid =
            lightObject != nullptr &&
            lightValidation.valid &&
            scene.directionalLight.light.kind ==
                epochengine::lighting::LightKind::Directional &&
            scene.directionalLight.light.enabled &&
            scene.directionalLight.light.color.r >= 0.0f &&
            scene.directionalLight.light.color.g >= 0.0f &&
            scene.directionalLight.light.color.b >= 0.0f &&
            epochengine::render_math::length_squared(
                scene.directionalLight.light.direction) > 0.0001f;

        const std::optional<epochengine::terrain::TerrainSurfaceSample>
            spawnSurface =
                spawnObject != nullptr
                    ? epochengine::terrain::sample_surface(
                        scene.groundTerrain,
                        spawnObject->transform.position.x,
                        spawnObject->transform.position.z)
                    : std::nullopt;
        result.spawnValid =
            spawnObject != nullptr &&
            scene.spawn.enabled &&
            !scene.spawn.role.empty() &&
            std::isfinite(scene.spawn.clearanceAboveGround) &&
            scene.spawn.clearanceAboveGround >= 0.0f &&
            spawnSurface.has_value() &&
            spawnObject->transform.position.y >=
                spawnSurface->position.y +
                scene.spawn.clearanceAboveGround;

        result.projectRunnable =
            limits.maximumObjects >= kTier0ObjectCount &&
            limits.maximumDirectionalLights >=
                kTier0DirectionalLightCount &&
            scene.revision != 0 &&
            scene.project.state == ProjectRunState::ready_to_launch &&
            std::isfinite(scene.project.fixedStepSeconds) &&
            scene.project.fixedStepSeconds > 0.0 &&
            result.requiredObjectsPresent &&
            result.referencesResolve &&
            result.cameraValid &&
            result.spawnValid;
        return result;
    }

    struct Tier0BuildResult final
    {
        Tier0BuildStatus status{Tier0BuildStatus::invalid_scene};
        epochengine::terrain::TerrainStatus terrainStatus{
            epochengine::terrain::TerrainStatus::invalid_descriptor};
        Tier0Scene scene{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return status == Tier0BuildStatus::ready;
        }
    };

    [[nodiscard]] inline Tier0BuildResult make_default_scene(
        const Tier0Limits& limits = {})
    {
        if (limits.maximumObjects < kTier0ObjectCount ||
            limits.maximumDirectionalLights <
                kTier0DirectionalLightCount)
        {
            return {.status = Tier0BuildStatus::invalid_limits};
        }

        epochengine::terrain::HeightfieldDescriptor terrainDescriptor{};
        terrainDescriptor.asset = epochengine::terrain::kTier0GroundAssetId;
        terrainDescriptor.kind =
            epochengine::terrain::FoundationKind::flat_platform;
        terrainDescriptor.origin = {-8.0f, 0.0f, -8.0f};
        terrainDescriptor.widthSamples = 2;
        terrainDescriptor.depthSamples = 2;
        terrainDescriptor.sampleSpacing = 16.0f;
        terrainDescriptor.baseHeight = 0.0f;
        terrainDescriptor.heightScale = 0.0f;
        terrainDescriptor.materialSlot = 0;
        terrainDescriptor.collisionQueries = true;

        epochengine::terrain::HeightfieldBuildResult terrainBuild =
            epochengine::terrain::make_heightfield(
                terrainDescriptor,
                {},
                limits.terrain);
        if (!terrainBuild)
        {
            return {
                .status = Tier0BuildStatus::terrain_failed,
                .terrainStatus = terrainBuild.status
            };
        }

        Tier0Scene scene{};
        scene.id = kDefaultSceneId;
        scene.revision = 1;
        scene.canonicalName = "Default Tier 0 Scene";
        scene.objects = {
            SceneObjectDescriptor{
                .id = kDefaultCameraId,
                .kind = ObjectKind::camera,
                .canonicalName = "PrimaryCamera",
                .transform = {
                    .position = {7.0f, 5.5f, 8.0f},
                    .rotationDegrees = {-24.0f, -139.0f, 0.0f}
                },
                .runtimeVisible = false,
                .editorVisible = true
            },
            SceneObjectDescriptor{
                .id = kDefaultGroundId,
                .kind = ObjectKind::ground,
                .canonicalName = "Ground",
                .transform = {
                    .position = {0.0f, 0.0f, 0.0f}
                },
                .runtimeVisible = true,
                .editorVisible = true
            },
            SceneObjectDescriptor{
                .id = kDefaultDirectionalLightId,
                .kind = ObjectKind::directional_light,
                .canonicalName = "Sun",
                .transform = {
                    .position = {0.0f, 6.0f, 0.0f},
                    .rotationDegrees = {-50.0f, -35.0f, 0.0f}
                },
                .runtimeVisible = false,
                .editorVisible = true
            },
            SceneObjectDescriptor{
                .id = kDefaultSpawnId,
                .kind = ObjectKind::spawn_point,
                .canonicalName = "PlayerSpawn",
                .transform = {
                    .position = {0.0f, 0.05f, 0.0f}
                },
                .runtimeVisible = false,
                .editorVisible = true
            }
        };
        scene.camera = {
            .object = kDefaultCameraId,
            .lookTarget = {0.0f, 0.0f, 0.0f},
            .up = {0.0f, 1.0f, 0.0f},
            .verticalFieldOfViewDegrees = 60.0f,
            .nearPlane = 0.05f,
            .farPlane = 1000.0f,
            .primary = true
        };
        scene.ground = {
            .object = kDefaultGroundId,
            .terrain = terrainDescriptor.asset,
            .baseColor = {0.32f, 0.38f, 0.28f},
            .roughness = 0.88f,
            .receivesLighting = true,
            .receivesShadows = true,
            .castsShadows = false
        };
        scene.directionalLight = {
            .object = kDefaultDirectionalLightId,
            .light = {
                .kind =
                    epochengine::lighting::LightKind::Directional,
                .color = {1.0f, 0.97f, 0.88f},
                .intensity = 3.0f,
                .position = {0.0f, 6.0f, 0.0f},
                .direction = {-0.45f, -1.0f, -0.35f},
                .range = 10.0f,
                .innerConeCosine = 0.90f,
                .outerConeCosine = 0.75f,
                .enabled = true
            },
            .castsShadows = true
        };
        scene.spawn = {
            .object = kDefaultSpawnId,
            .role = "primary",
            .clearanceAboveGround = 0.05f,
            .enabled = true
        };
        scene.project = {
            .project = kDefaultProjectId,
            .entryScene = kDefaultSceneId,
            .primaryCamera = kDefaultCameraId,
            .primarySpawn = kDefaultSpawnId,
            .state = ProjectRunState::ready_to_launch,
            .fixedStepSeconds = 1.0 / 60.0,
            .startPaused = false
        };
        scene.groundTerrain = std::move(terrainBuild.heightfield);

        if (!validate(scene, limits))
        {
            return {
                .status = Tier0BuildStatus::invalid_scene,
                .terrainStatus =
                    epochengine::terrain::TerrainStatus::ready
            };
        }

        return {
            .status = Tier0BuildStatus::ready,
            .terrainStatus = epochengine::terrain::TerrainStatus::ready,
            .scene = std::move(scene)
        };
    }

    struct FocusTarget final
    {
        ObjectId object{};
        Float3 center{};
        float radius{1.0f};
    };

    [[nodiscard]] inline std::optional<FocusTarget> focus_target(
        const Tier0Scene& scene,
        ObjectId object) noexcept
    {
        const SceneObjectDescriptor* descriptor =
            find_object(scene, object);
        if (descriptor == nullptr || !valid(descriptor->transform))
        {
            return std::nullopt;
        }

        if (descriptor->kind == ObjectKind::ground)
        {
            if (!scene.groundTerrain.valid())
            {
                return std::nullopt;
            }
            const epochengine::terrain::TerrainBounds& bounds =
                scene.groundTerrain.bounds;
            const Float3 extent = epochengine::render_math::subtract(
                bounds.maximum,
                bounds.minimum);
            return FocusTarget{
                .object = object,
                .center = epochengine::render_math::scale(
                    epochengine::render_math::add(
                        bounds.minimum,
                        bounds.maximum),
                    0.5f),
                .radius = (std::max)(
                    1.0f,
                    0.5f * epochengine::render_math::length(extent))
            };
        }

        float radius = 0.75f;
        if (descriptor->kind == ObjectKind::camera)
        {
            radius = 1.25f;
        }
        else if (descriptor->kind == ObjectKind::directional_light)
        {
            radius = 1.0f;
        }
        return FocusTarget{
            .object = object,
            .center = descriptor->transform.position,
            .radius = radius
        };
    }

    struct LightingRegistration final
    {
        std::array<
            epochengine::lighting::LightHandle,
            kTier0DirectionalLightCount> handles{};
        std::size_t registeredCount{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return registeredCount == handles.size();
        }
    };

    [[nodiscard]] inline LightingRegistration register_lighting(
        const Tier0Scene& scene,
        epochengine::lighting::LightManager& manager)
    {
        LightingRegistration result{};
        if (!validate(scene).lightingValid)
        {
            return result;
        }

        result.handles[0] =
            manager.create(scene.directionalLight.light);
        result.registeredCount =
            result.handles[0].valid() ? 1 : 0;
        return result;
    }

    struct ProjectRunRequest final
    {
        ProjectId project{};
        SceneId scene{};
        std::uint64_t sceneRevision{};
        ObjectId camera{};
        ObjectId spawn{};
        TransformDescriptor spawnTransform{};
        double fixedStepSeconds{};
        bool startPaused{};
    };

    [[nodiscard]] inline std::optional<ProjectRunRequest> make_run_request(
        const Tier0Scene& scene,
        const Tier0Limits& limits = {}) noexcept
    {
        const Tier0Validation validation = validate(scene, limits);
        const SceneObjectDescriptor* spawn =
            find_object(scene, scene.project.primarySpawn);
        if (!validation || spawn == nullptr)
        {
            return std::nullopt;
        }

        return ProjectRunRequest{
            .project = scene.project.project,
            .scene = scene.project.entryScene,
            .sceneRevision = scene.revision,
            .camera = scene.project.primaryCamera,
            .spawn = scene.project.primarySpawn,
            .spawnTransform = spawn->transform,
            .fixedStepSeconds = scene.project.fixedStepSeconds,
            .startPaused = scene.project.startPaused
        };
    }

    namespace detail
    {
        inline constexpr std::uint64_t kHashOffset =
            14695981039346656037ull;
        inline constexpr std::uint64_t kHashPrime =
            1099511628211ull;

        inline void hash_byte(
            std::uint64_t& hash,
            std::uint8_t value) noexcept
        {
            hash ^= value;
            hash *= kHashPrime;
        }

        template<typename Integer>
        inline void hash_integer(
            std::uint64_t& hash,
            Integer value) noexcept
        {
            using Unsigned = std::make_unsigned_t<Integer>;
            Unsigned bits = static_cast<Unsigned>(value);
            for (std::size_t byte = 0; byte < sizeof(Unsigned); ++byte)
            {
                hash_byte(
                    hash,
                    static_cast<std::uint8_t>(
                        (bits >> (byte * 8)) &
                        static_cast<Unsigned>(0xff)));
            }
        }

        inline void hash_float(
            std::uint64_t& hash,
            float value) noexcept
        {
            hash_integer(hash, std::bit_cast<std::uint32_t>(value));
        }

        inline void hash_double(
            std::uint64_t& hash,
            double value) noexcept
        {
            hash_integer(hash, std::bit_cast<std::uint64_t>(value));
        }

        inline void hash_transform(
            std::uint64_t& hash,
            const TransformDescriptor& transform) noexcept
        {
            hash_float(hash, transform.position.x);
            hash_float(hash, transform.position.y);
            hash_float(hash, transform.position.z);
            hash_float(hash, transform.rotationDegrees.x);
            hash_float(hash, transform.rotationDegrees.y);
            hash_float(hash, transform.rotationDegrees.z);
            hash_float(hash, transform.scale.x);
            hash_float(hash, transform.scale.y);
            hash_float(hash, transform.scale.z);
        }
        inline void hash_string(
            std::uint64_t& hash,
            std::string_view value) noexcept
        {
            hash_integer(hash, value.size());
            for (const unsigned char character : value)
            {
                hash_byte(hash, character);
            }
        }
    }

    [[nodiscard]] inline std::uint64_t descriptor_fingerprint(
        const Tier0Scene& scene) noexcept
    {
        std::uint64_t hash = detail::kHashOffset;
        detail::hash_integer(hash, scene.id.value);
        detail::hash_integer(hash, scene.revision);
        detail::hash_string(hash, scene.canonicalName);
        for (const SceneObjectDescriptor& object : scene.objects)
        {
            detail::hash_integer(hash, object.id.value);
            detail::hash_integer(
                hash,
                static_cast<std::uint8_t>(object.kind));
            detail::hash_string(hash, object.canonicalName);
            detail::hash_transform(hash, object.transform);
            detail::hash_integer(
                hash,
                static_cast<std::uint8_t>(
                    object.runtimeVisible ? 1 : 0));
            detail::hash_integer(
                hash,
                static_cast<std::uint8_t>(
                    object.editorVisible ? 1 : 0));
        }
        detail::hash_integer(hash, scene.groundTerrain.contentHash);
        detail::hash_integer(hash, scene.camera.object.value);
        detail::hash_float(hash, scene.camera.lookTarget.x);
        detail::hash_float(hash, scene.camera.lookTarget.y);
        detail::hash_float(hash, scene.camera.lookTarget.z);
        detail::hash_float(hash, scene.camera.up.x);
        detail::hash_float(hash, scene.camera.up.y);
        detail::hash_float(hash, scene.camera.up.z);
        detail::hash_float(hash, scene.camera.verticalFieldOfViewDegrees);
        detail::hash_float(hash, scene.camera.nearPlane);
        detail::hash_float(hash, scene.camera.farPlane);
        detail::hash_integer(
            hash,
            static_cast<std::uint8_t>(scene.camera.primary ? 1 : 0));
        detail::hash_integer(hash, scene.ground.object.value);
        detail::hash_integer(hash, scene.ground.terrain.value);
        detail::hash_float(hash, scene.ground.baseColor.r);
        detail::hash_float(hash, scene.ground.baseColor.g);
        detail::hash_float(hash, scene.ground.baseColor.b);
        detail::hash_float(hash, scene.ground.roughness);
        detail::hash_integer(
            hash,
            static_cast<std::uint8_t>(scene.ground.receivesLighting ? 1 : 0));
        detail::hash_integer(
            hash,
            static_cast<std::uint8_t>(scene.ground.receivesShadows ? 1 : 0));
        detail::hash_integer(
            hash,
            static_cast<std::uint8_t>(scene.ground.castsShadows ? 1 : 0));
        detail::hash_integer(hash, scene.directionalLight.object.value);
        detail::hash_integer(
            hash,
            static_cast<std::uint8_t>(scene.directionalLight.light.kind));
        detail::hash_float(hash, scene.directionalLight.light.color.r);
        detail::hash_float(hash, scene.directionalLight.light.color.g);
        detail::hash_float(hash, scene.directionalLight.light.color.b);
        detail::hash_float(hash, scene.directionalLight.light.intensity);
        detail::hash_float(hash, scene.directionalLight.light.position.x);
        detail::hash_float(hash, scene.directionalLight.light.position.y);
        detail::hash_float(hash, scene.directionalLight.light.position.z);
        detail::hash_float(hash, scene.directionalLight.light.direction.x);
        detail::hash_float(hash, scene.directionalLight.light.direction.y);
        detail::hash_float(hash, scene.directionalLight.light.direction.z);
        detail::hash_float(hash, scene.directionalLight.light.range);
        detail::hash_float(hash, scene.directionalLight.light.innerConeCosine);
        detail::hash_float(hash, scene.directionalLight.light.outerConeCosine);
        detail::hash_integer(
            hash,
            static_cast<std::uint8_t>(scene.directionalLight.light.enabled ? 1 : 0));
        detail::hash_integer(
            hash,
            static_cast<std::uint8_t>(scene.directionalLight.castsShadows ? 1 : 0));
        detail::hash_integer(hash, scene.spawn.object.value);
        detail::hash_string(hash, scene.spawn.role);
        detail::hash_float(hash, scene.spawn.clearanceAboveGround);
        detail::hash_integer(
            hash,
            static_cast<std::uint8_t>(scene.spawn.enabled ? 1 : 0));
        detail::hash_integer(hash, scene.project.project.value);
        detail::hash_integer(hash, scene.project.entryScene.value);
        detail::hash_integer(hash, scene.project.primaryCamera.value);
        detail::hash_integer(hash, scene.project.primarySpawn.value);
        detail::hash_integer(
            hash,
            static_cast<std::uint8_t>(scene.project.state));
        detail::hash_double(hash, scene.project.fixedStepSeconds);
        detail::hash_integer(
            hash,
            static_cast<std::uint8_t>(scene.project.startPaused ? 1 : 0));
        return hash == 0 ? 1 : hash;
    }
}
