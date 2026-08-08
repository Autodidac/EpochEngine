// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

export module scene.document;

import authoring.document;
import render.lighting;
import scene.tier0;
import scenesnapshot;
import terrain.foundation;

export namespace epochengine::authoring::scene
{
    using SceneId = epochengine::scene_tier0::SceneId;
    using ObjectId = epochengine::scene_tier0::ObjectId;
    using ProjectId = epochengine::scene_tier0::ProjectId;
    using Float3 = epochengine::scene_tier0::Float3;
    using LinearRgb = epochengine::scene_tier0::LinearRgb;

    inline constexpr std::uint32_t kSceneDocumentSchemaVersion = 1;

    struct SceneObjectTag final {};
    using ObjectHandle = epochengine::authoring::Handle<
        SceneObjectTag,
        std::uint32_t>;

    struct TransactionId final
    {
        std::uint64_t value{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return value != 0;
        }

        friend constexpr bool operator==(
            const TransactionId&,
            const TransactionId&) noexcept = default;
    };

    using SceneContentHash = epochengine::authoring::ContentHash;
    using SceneRevision = epochengine::authoring::DocumentRevision;

    struct TransformComponent final
    {
        Float3 position{};
        Float3 rotation_degrees{};
        Float3 scale{1.0f, 1.0f, 1.0f};

        friend constexpr bool operator==(
            const TransformComponent&,
            const TransformComponent&) noexcept = default;
    };

    struct CameraComponent final
    {
        Float3 look_target{};
        Float3 up{0.0f, 1.0f, 0.0f};
        float vertical_field_of_view_degrees{60.0f};
        float near_plane{0.05f};
        float far_plane{1000.0f};
        bool primary{};

        friend constexpr bool operator==(
            const CameraComponent&,
            const CameraComponent&) noexcept = default;
    };

    struct GroundComponent final
    {
        epochengine::terrain::Heightfield terrain{};
        LinearRgb base_color{0.32f, 0.38f, 0.28f};
        float roughness{0.88f};
        bool receives_lighting{true};
        bool receives_shadows{true};
        bool casts_shadows{};

        friend bool operator==(
            const GroundComponent&,
            const GroundComponent&) noexcept = default;
    };

    struct LightComponent final
    {
        epochengine::lighting::LightKind kind{
            epochengine::lighting::LightKind::Directional};
        LinearRgb color{1.0f, 1.0f, 1.0f};
        float intensity{1.0f};
        Float3 direction{0.0f, -1.0f, 0.0f};
        float range{10.0f};
        float inner_cone_cosine{0.90f};
        float outer_cone_cosine{0.75f};
        bool enabled{true};
        bool casts_shadows{true};

        friend constexpr bool operator==(
            const LightComponent&,
            const LightComponent&) noexcept = default;
    };

    struct SpawnComponent final
    {
        std::string role{"primary"};
        float clearance_above_ground{0.05f};
        bool enabled{true};

        friend bool operator==(
            const SpawnComponent&,
            const SpawnComponent&) noexcept = default;
    };

    enum class ObjectKind : std::uint8_t
    {
        generic,
        camera,
        ground,
        light,
        spawn
    };

    struct ObjectMetadata final
    {
        std::string name{};
        std::string generic_type{"Entity"};
        std::string category{};
        std::uint32_t sort_order{};

        friend bool operator==(
            const ObjectMetadata&,
            const ObjectMetadata&) noexcept = default;
    };

    struct SceneObjectDescriptor final
    {
        ObjectId id{};
        ObjectMetadata metadata{};
        TransformComponent transform{};
        bool runtime_visible{true};
        bool editor_visible{true};
        bool editor_only{};
        std::optional<CameraComponent> camera{};
        std::optional<GroundComponent> ground{};
        std::optional<LightComponent> light{};
        std::optional<SpawnComponent> spawn{};
    };

    struct SceneObject final
    {
        ObjectHandle handle{};
        SceneObjectDescriptor descriptor{};
    };

    [[nodiscard]] inline ObjectKind object_kind(
        const SceneObjectDescriptor& object) noexcept
    {
        if (object.camera)
            return ObjectKind::camera;
        if (object.ground)
            return ObjectKind::ground;
        if (object.light)
            return ObjectKind::light;
        if (object.spawn)
            return ObjectKind::spawn;
        return ObjectKind::generic;
    }

    struct SceneIdentity final
    {
        SceneId scene{};
        ProjectId project{};
        std::string canonical_name{};
        std::uint32_t schema_version{kSceneDocumentSchemaVersion};

        friend bool operator==(
            const SceneIdentity&,
            const SceneIdentity&) noexcept = default;
    };

    struct ProjectSettings final
    {
        ProjectId project{};
        SceneId entry_scene{};
        ObjectId primary_camera{};
        ObjectId primary_spawn{};
        epochengine::scene_tier0::ProjectRunState run_state{
            epochengine::scene_tier0::ProjectRunState::blocked};
        double fixed_step_seconds{1.0 / 60.0};
        bool start_paused{};

        friend constexpr bool operator==(
            const ProjectSettings&,
            const ProjectSettings&) noexcept = default;
    };

    struct DocumentLimits final
    {
        std::uint32_t maximum_active_objects{4096};
        std::uint32_t maximum_object_slots{16384};
        std::uint32_t maximum_scene_name_bytes{256};
        std::uint32_t maximum_object_name_bytes{256};
        std::uint32_t maximum_type_bytes{128};
        std::uint32_t maximum_category_bytes{128};
        std::uint32_t maximum_spawn_role_bytes{64};
        std::uint32_t maximum_lights{256};
        std::uint32_t maximum_grounds{32};
        std::uint64_t maximum_terrain_sample_bytes{
            64ull * 1024ull * 1024ull};
        std::uint32_t maximum_primary_cameras{1};
        std::uint32_t maximum_operations_per_transaction{256};
        std::uint32_t maximum_transaction_label_bytes{256};
        std::uint32_t maximum_history_transactions{2048};
        std::uint32_t maximum_snapshot_packages{256};
        std::uint32_t maximum_snapshot_timeline_keys{65'536};
        std::uint32_t maximum_snapshot_string_bytes{4'096};
        double maximum_fixed_step_seconds{1.0};
        epochengine::terrain::TerrainLimits terrain{};

        friend constexpr bool operator==(
            const DocumentLimits&,
            const DocumentLimits&) noexcept = default;
    };

    using HistoryMode = epochengine::authoring::HistoryMode;
    using HistoryPolicy = epochengine::authoring::HistoryPolicy;

    inline constexpr HistoryPolicy kDefaultSceneHistoryPolicy{
        .mode = HistoryMode::semantic_operations,
        .checkpoint_interval_operations = 0,
        .maximum_operations = 16'384,
        .maximum_checkpoints = 0,
        .maximum_operation_bytes = 8ull * 1024ull * 1024ull,
        .maximum_checkpoint_bytes = 0,
        .maximum_memory_bytes = 64ull * 1024ull * 1024ull,
        .maximum_disk_bytes = 0,
        .compress = false,
        .deduplicate = true,
        .allow_branching = true
    };

    struct SnapshotSourceMetadata final
    {
        std::string scene_id{};
        std::string project_id{};
        std::string world_name{};
        std::string document_kind{"scene"};
        std::string support_tier{"tier0"};
        std::vector<std::string> packages{};
        std::vector<epochengine::scene::SceneTimelineKey> timeline_keys{};
    };

    [[nodiscard]] std::optional<SceneObjectDescriptor>
        descriptor_from_snapshot_object(
            const epochengine::scene::SceneObjectSnapshot& source,
            std::uint32_t sort_order,
            const DocumentLimits& limits = {});

    enum class ResultCode : std::uint8_t
    {
        success,
        unchanged,
        invalid_document,
        invalid_limits,
        invalid_identity,
        duplicate_identity,
        duplicate_name,
        invalid_handle,
        stale_handle,
        object_not_found,
        object_limit_exceeded,
        object_slot_limit_exceeded,
        string_limit_exceeded,
        invalid_transform,
        conflicting_components,
        invalid_camera,
        invalid_ground,
        invalid_light,
        invalid_spawn,
        light_limit_exceeded,
        invalid_project_settings,
        unresolved_reference,
        invalid_operation,
        transaction_operation_limit_exceeded,
        history_transaction_limit_exceeded,
        history_operation_limit_exceeded,
        history_memory_limit_exceeded,
        nothing_to_undo,
        nothing_to_redo,
        history_corrupt,
        revision_overflow,
        tier0_seed_invalid
    };

    struct DocumentValidation final
    {
        ResultCode code{ResultCode::invalid_document};
        ObjectHandle object{};
        std::uint32_t active_objects{};
        std::uint32_t cameras{};
        std::uint32_t grounds{};
        std::uint32_t lights{};
        std::uint32_t spawns{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    struct ObjectCreatedOperation final
    {
        ObjectHandle handle{};
        SceneObjectDescriptor object{};
    };

    struct ObjectDestroyedOperation final
    {
        ObjectHandle object{};
    };

    struct ObjectMetadataChangedOperation final
    {
        ObjectHandle object{};
        ObjectMetadata value{};
    };

    struct ObjectTransformChangedOperation final
    {
        ObjectHandle object{};
        TransformComponent value{};
    };

    struct ObjectVisibilityChangedOperation final
    {
        ObjectHandle object{};
        bool runtime_visible{true};
        bool editor_visible{true};
        bool editor_only{};
    };

    struct CameraComponentChangedOperation final
    {
        ObjectHandle object{};
        std::optional<CameraComponent> value{};
    };

    struct GroundComponentChangedOperation final
    {
        ObjectHandle object{};
        std::optional<GroundComponent> value{};
    };

    struct LightComponentChangedOperation final
    {
        ObjectHandle object{};
        std::optional<LightComponent> value{};
    };

    struct SpawnComponentChangedOperation final
    {
        ObjectHandle object{};
        std::optional<SpawnComponent> value{};
    };

    struct ProjectSettingsChangedOperation final
    {
        ProjectSettings value{};
    };

    using SceneOperation = std::variant<
        ObjectCreatedOperation,
        ObjectDestroyedOperation,
        ObjectMetadataChangedOperation,
        ObjectTransformChangedOperation,
        ObjectVisibilityChangedOperation,
        CameraComponentChangedOperation,
        GroundComponentChangedOperation,
        LightComponentChangedOperation,
        SpawnComponentChangedOperation,
        ProjectSettingsChangedOperation>;

    struct SceneTransaction final
    {
        std::string label{};
        std::vector<SceneOperation> operations{};
    };

    struct TransactionResult final
    {
        ResultCode code{ResultCode::invalid_operation};
        SceneRevision revision{};
        TransactionId transaction{};
        ObjectHandle primary_object{};
        std::uint32_t applied_operations{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResultCode::success ||
                code == ResultCode::unchanged;
        }
    };

    struct TransactionSummary final
    {
        TransactionId id{};
        std::string label{};
        SceneRevision before{};
        SceneRevision after{};
        std::uint32_t operation_count{};
        std::uint64_t retained_bytes{};
        bool currently_applied{};
    };

    struct DocumentMetrics final
    {
        std::uint32_t active_objects{};
        std::uint32_t allocated_slots{};
        std::uint32_t history_transactions{};
        std::uint32_t applied_transactions{};
        std::uint64_t history_operations{};
        std::uint64_t retained_history_bytes{};
        std::uint64_t revision_sequence{};
    };

    struct SceneSeed final
    {
        SceneIdentity identity{};
        ProjectSettings project{};
        std::vector<SceneObjectDescriptor> objects{};
        std::optional<SnapshotSourceMetadata> snapshot_metadata{};
        std::uint64_t initial_revision_sequence{1};
    };

    class SceneDocument;
    struct SceneDocumentBuildResult;

    struct SnapshotProjectionResult final
    {
        ResultCode code{ResultCode::invalid_document};
        epochengine::scene::SceneSnapshot snapshot{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    class SceneDocument final
    {
    public:
        SceneDocument() = default;
        SceneDocument(const SceneDocument&) = default;
        SceneDocument& operator=(const SceneDocument&) = default;
        SceneDocument(SceneDocument&&) noexcept = default;
        SceneDocument& operator=(SceneDocument&&) noexcept = default;
        ~SceneDocument() = default;

        [[nodiscard]] static SceneDocumentBuildResult create(
            SceneSeed seed,
            DocumentLimits limits = {},
            HistoryPolicy history = kDefaultSceneHistoryPolicy);

        [[nodiscard]] static SceneDocumentBuildResult from_tier0(
            const epochengine::scene_tier0::Tier0Scene& source,
            DocumentLimits limits = {},
            HistoryPolicy history = kDefaultSceneHistoryPolicy);

        [[nodiscard]] static SceneDocumentBuildResult from_snapshot(
            const epochengine::scene::SceneSnapshot& source,
            DocumentLimits limits = {},
            HistoryPolicy history = kDefaultSceneHistoryPolicy);

        [[nodiscard]] static SceneDocumentBuildResult make_default_tier0(
            DocumentLimits limits = {},
            HistoryPolicy history = kDefaultSceneHistoryPolicy);

        [[nodiscard]] bool initialized() const noexcept
        {
            return initialized_;
        }

        [[nodiscard]] const SceneIdentity& identity() const noexcept
        {
            return state_.identity;
        }

        [[nodiscard]] const ProjectSettings& project_settings() const noexcept
        {
            return state_.project;
        }

        [[nodiscard]] SceneRevision revision() const noexcept
        {
            return revision_;
        }

        [[nodiscard]] const DocumentLimits& limits() const noexcept
        {
            return limits_;
        }

        [[nodiscard]] const HistoryPolicy& history_policy() const noexcept
        {
            return history_policy_;
        }

        [[nodiscard]] DocumentValidation validate() const;
        [[nodiscard]] std::optional<SceneObject> object(
            ObjectHandle handle) const;
        [[nodiscard]] std::optional<ObjectHandle> find(
            ObjectId id) const noexcept;
        [[nodiscard]] std::vector<SceneObject> objects() const;

        [[nodiscard]] TransactionResult apply_transaction(
            const SceneTransaction& transaction);
        [[nodiscard]] TransactionResult apply_transaction(
            std::span<const SceneOperation> operations,
            std::string_view label = {});
        [[nodiscard]] bool can_undo() const noexcept;
        [[nodiscard]] bool can_redo() const noexcept;
        [[nodiscard]] TransactionResult undo();
        [[nodiscard]] TransactionResult redo();

        [[nodiscard]] std::vector<TransactionSummary> history() const;
        [[nodiscard]] DocumentMetrics metrics() const noexcept;
        [[nodiscard]] SnapshotProjectionResult project_snapshot(
            std::uint64_t frame_index = 0,
            double simulated_seconds = 0.0) const;

    private:
        struct ObjectSlot final
        {
            std::uint32_t generation{1};
            bool active{};
            SceneObjectDescriptor object{};
        };

        struct State final
        {
            SceneIdentity identity{};
            ProjectSettings project{};
            SnapshotSourceMetadata snapshot_metadata{};
            std::vector<ObjectSlot> slots{};
        };

        struct TransactionRecord final
        {
            TransactionId id{};
            std::string label{};
            SceneRevision before{};
            SceneRevision after{};
            std::vector<SceneOperation> forward{};
            std::vector<SceneOperation> inverse{};
            std::uint64_t retained_bytes{};
        };

        enum class ApplyMode : std::uint8_t
        {
            command,
            replay
        };

        [[nodiscard]] static DocumentValidation validate_state(
            const State& state,
            const DocumentLimits& limits);
        [[nodiscard]] static SceneContentHash content_hash(
            const State& state);
        [[nodiscard]] static ResultCode apply_operation(
            State& state,
            SceneOperation& operation,
            SceneOperation& inverse,
            const DocumentLimits& limits,
            ApplyMode mode);
        [[nodiscard]] static std::uint64_t estimate_operation_bytes(
            const SceneOperation& operation) noexcept;
        [[nodiscard]] static std::uint64_t estimate_record_bytes(
            const TransactionRecord& record) noexcept;
        [[nodiscard]] static ObjectSlot* resolve_slot(
            State& state,
            ObjectHandle handle) noexcept;
        [[nodiscard]] static const ObjectSlot* resolve_slot(
            const State& state,
            ObjectHandle handle) noexcept;
        [[nodiscard]] static ObjectHandle handle_for(
            const State& state,
            ObjectId id) noexcept;
        [[nodiscard]] static bool limits_valid(
            const DocumentLimits& limits,
            const HistoryPolicy& history) noexcept;
        [[nodiscard]] TransactionResult replay_history(
            bool forward);

        bool initialized_{};
        State state_{};
        DocumentLimits limits_{};
        HistoryPolicy history_policy_{};
        SceneRevision revision_{};
        std::vector<TransactionRecord> history_{};
        std::size_t history_cursor_{};
        std::uint64_t retained_history_bytes_{};
        std::uint64_t history_operation_count_{};
        std::uint64_t next_transaction_id_{1};
    };

    struct SceneDocumentBuildResult final
    {
        ResultCode code{ResultCode::invalid_document};
        DocumentValidation validation{};
        SceneDocument value{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ResultCode::success && value.initialized();
        }

        [[nodiscard]] SceneDocument* operator->() noexcept
        {
            return &value;
        }

        [[nodiscard]] const SceneDocument* operator->() const noexcept
        {
            return &value;
        }

        [[nodiscard]] SceneDocument& document() noexcept
        {
            return value;
        }

        [[nodiscard]] const SceneDocument& document() const noexcept
        {
            return value;
        }
    };

    namespace detail
    {
        inline constexpr std::uint64_t kFnvPrime = 1'099'511'628'211ull;
        [[nodiscard]] constexpr ObjectKind snapshot_object_kind(
            std::string_view type) noexcept
        {
            if (type == "Camera")
                return ObjectKind::camera;
            if (type == "Ground")
                return ObjectKind::ground;
            if (type == "Light")
                return ObjectKind::light;
            if (type == "Spawn")
                return ObjectKind::spawn;
            return ObjectKind::generic;
        }

        static_assert(snapshot_object_kind("Camera") == ObjectKind::camera);
        static_assert(snapshot_object_kind("Ground") == ObjectKind::ground);
        static_assert(snapshot_object_kind("Light") == ObjectKind::light);
        static_assert(snapshot_object_kind("Spawn") == ObjectKind::spawn);
        static_assert(snapshot_object_kind("Mesh") == ObjectKind::generic);

        template<typename Id>
        [[nodiscard]] constexpr Id internal_id_from_public_text(
            std::string_view text,
            std::string_view fallback) noexcept
        {
            std::uint64_t value{};
            bool decimal = !text.empty();
            for (const char character : text)
            {
                if (character < '0' || character > '9')
                {
                    decimal = false;
                    break;
                }
                const std::uint64_t digit =
                    static_cast<std::uint64_t>(character - '0');
                if (value >
                    ((std::numeric_limits<std::uint64_t>::max)() - digit) /
                        10u)
                {
                    decimal = false;
                    break;
                }
                value = value * 10u + digit;
            }
            if (decimal && value != 0)
                return Id{value};
            return epochengine::scene_tier0::stable_id<Id>(
                text.empty() ? fallback : text);
        }

        [[nodiscard]] constexpr std::size_t snapshot_string_limit(
            const DocumentLimits& limits) noexcept
        {
            return (std::max)({
                static_cast<std::size_t>(limits.maximum_scene_name_bytes),
                static_cast<std::size_t>(limits.maximum_object_name_bytes),
                static_cast<std::size_t>(limits.maximum_type_bytes),
                static_cast<std::size_t>(limits.maximum_category_bytes),
                static_cast<std::size_t>(
                    limits.maximum_snapshot_string_bytes)});
        }
        [[nodiscard]] constexpr bool snapshot_visibility_round_trips(
            bool visible,
            bool editorOnly) noexcept
        {
            const bool runtimeVisible = visible && !editorOnly;
            const bool editorVisible = visible;
            const bool storedEditorOnly = editorOnly;
            return runtimeVisible == (visible && !editorOnly) &&
                editorVisible == visible &&
                storedEditorOnly == editorOnly;
        }

        static_assert(snapshot_visibility_round_trips(false, false));
        static_assert(snapshot_visibility_round_trips(false, true));
        static_assert(snapshot_visibility_round_trips(true, false));
        static_assert(snapshot_visibility_round_trips(true, true));


        [[nodiscard]] inline bool finite_vector(Float3 value) noexcept
        {
            return std::isfinite(value.x) &&
                std::isfinite(value.y) &&
                std::isfinite(value.z);
        }

        [[nodiscard]] inline bool finite_color(LinearRgb value) noexcept
        {
            return std::isfinite(value.r) &&
                std::isfinite(value.g) &&
                std::isfinite(value.b);
        }

        [[nodiscard]] inline bool same_content_hash(
            const SceneContentHash& left,
            const SceneContentHash& right) noexcept
        {
            for (std::size_t index = 0; index < left.words.size(); ++index)
            {
                if (left.words[index] != right.words[index])
                    return false;
            }
            return true;
        }

        [[nodiscard]] inline float length_squared(Float3 value) noexcept
        {
            return value.x * value.x + value.y * value.y + value.z * value.z;
        }

        [[nodiscard]] inline bool valid_transform(
            const TransformComponent& transform) noexcept
        {
            return finite_vector(transform.position) &&
                finite_vector(transform.rotation_degrees) &&
                finite_vector(transform.scale) &&
                transform.scale.x > 0.0f &&
                transform.scale.y > 0.0f &&
                transform.scale.z > 0.0f;
        }

        [[nodiscard]] inline std::uint64_t saturating_add(
            std::uint64_t left,
            std::uint64_t right) noexcept
        {
            const std::uint64_t maximum =
                (std::numeric_limits<std::uint64_t>::max)();
            return right > maximum - left ? maximum : left + right;
        }

        [[nodiscard]] inline std::uint64_t saturating_multiply(
            std::uint64_t left,
            std::uint64_t right) noexcept
        {
            const std::uint64_t maximum =
                (std::numeric_limits<std::uint64_t>::max)();
            if (left == 0 || right == 0)
                return 0;
            return left > maximum / right ? maximum : left * right;
        }

        template<typename Value>
        void append_integral(
            epochengine::authoring::DeterministicHashBuilder& builder,
            Value value) noexcept
        {
            static_assert(
                std::is_integral_v<Value> || std::is_enum_v<Value>);
            if constexpr (std::is_enum_v<Value>)
                builder.append_enum(value);
            else if constexpr (
                std::is_same_v<std::remove_cv_t<Value>, bool>)
                builder.append_bool(value);
            else if constexpr (sizeof(Value) <= sizeof(std::uint8_t))
                builder.append_u8(static_cast<std::uint8_t>(value));
            else if constexpr (sizeof(Value) <= sizeof(std::uint16_t))
                builder.append_u16(static_cast<std::uint16_t>(value));
            else if constexpr (sizeof(Value) <= sizeof(std::uint32_t))
                builder.append_u32(static_cast<std::uint32_t>(value));
            else
                builder.append_u64(static_cast<std::uint64_t>(value));
        }

        inline void append_float3(
            epochengine::authoring::DeterministicHashBuilder& builder,
            Float3 value) noexcept
        {
            builder.append_float(value.x);
            builder.append_float(value.y);
            builder.append_float(value.z);
        }

        inline void append_color(
            epochengine::authoring::DeterministicHashBuilder& builder,
            LinearRgb value) noexcept
        {
            builder.append_float(value.r);
            builder.append_float(value.g);
            builder.append_float(value.b);
        }

        [[nodiscard]] inline epochengine::lighting::LightDesc light_desc(
            const TransformComponent& transform,
            const LightComponent& light) noexcept
        {
            return {
                .kind = light.kind,
                .color = light.color,
                .intensity = light.intensity,
                .position = transform.position,
                .direction = light.direction,
                .range = light.range,
                .innerConeCosine = light.inner_cone_cosine,
                .outerConeCosine = light.outer_cone_cosine,
                .enabled = light.enabled
            };
        }

        [[nodiscard]] inline std::uint64_t string_bytes(
            const std::string& value) noexcept
        {
            return static_cast<std::uint64_t>(value.size());
        }

        [[nodiscard]] inline std::uint64_t ground_bytes(
            const GroundComponent& ground) noexcept
        {
            return saturating_add(
                sizeof(GroundComponent),
                saturating_multiply(
                    ground.terrain.normalizedSamples.size(),
                    sizeof(float)));
        }

        [[nodiscard]] inline std::uint64_t object_bytes(
            const SceneObjectDescriptor& object) noexcept
        {
            std::uint64_t bytes = sizeof(SceneObjectDescriptor);
            bytes = saturating_add(bytes, string_bytes(object.metadata.name));
            bytes = saturating_add(
                bytes,
                string_bytes(object.metadata.generic_type));
            bytes = saturating_add(
                bytes,
                string_bytes(object.metadata.category));
            if (object.ground)
                bytes = saturating_add(bytes, ground_bytes(*object.ground));
            if (object.spawn)
                bytes = saturating_add(bytes, string_bytes(object.spawn->role));
            return bytes;
        }
    }
    inline std::optional<SceneObjectDescriptor>
        descriptor_from_snapshot_object(
            const epochengine::scene::SceneObjectSnapshot& source,
            std::uint32_t sort_order,
            const DocumentLimits& limits)
    {
        if (source.id == epochengine::scene::kInvalidSceneObjectId ||
            source.name.empty() || source.type.empty() ||
            source.name.size() > limits.maximum_object_name_bytes ||
            source.type.size() > limits.maximum_type_bytes ||
            source.category.size() > limits.maximum_category_bytes ||
            !epochengine::scene::finite_scene_vec3(source.position) ||
            !epochengine::scene::finite_scene_vec3(source.rotation) ||
            !epochengine::scene::finite_scene_vec3(source.scale) ||
            source.scale[0] <= 0.0f ||
            source.scale[1] <= 0.0f ||
            source.scale[2] <= 0.0f)
        {
            return std::nullopt;
        }

        SceneObjectDescriptor converted{};
        converted.id = {source.id};
        converted.metadata = {
            .name = source.name,
            .generic_type = source.type,
            .category = source.category,
            .sort_order = sort_order
        };
        converted.transform = {
            .position = {
                source.position[0],
                source.position[1],
                source.position[2]},
            .rotation_degrees = {
                source.rotation[0],
                source.rotation[1],
                source.rotation[2]},
            .scale = {
                source.scale[0],
                source.scale[1],
                source.scale[2]}
        };
        converted.runtime_visible = source.visible && !source.editor_only;
        converted.editor_visible = source.visible;
        converted.editor_only = source.editor_only;

        switch (detail::snapshot_object_kind(source.type))
        {
        case ObjectKind::camera:
        {
            constexpr float kDegreesToRadians =
                0.01745329251994329576923690768489f;
            const float pitch = source.rotation[0] * kDegreesToRadians;
            const float yaw = source.rotation[1] * kDegreesToRadians;
            const float pitchCosine = std::cos(pitch);
            const Float3 forward{
                std::sin(yaw) * pitchCosine,
                std::sin(pitch),
                std::cos(yaw) * pitchCosine
            };
            converted.camera = CameraComponent{
                .look_target = {
                    converted.transform.position.x + forward.x,
                    converted.transform.position.y + forward.y,
                    converted.transform.position.z + forward.z},
                .primary = false
            };
            break;
        }
        case ObjectKind::ground:
        {
            epochengine::terrain::HeightfieldDescriptor descriptor{};
            descriptor.origin = {
                converted.transform.position.x - 8.0f,
                converted.transform.position.y,
                converted.transform.position.z - 8.0f
            };
            descriptor.baseHeight = converted.transform.position.y;
            const auto built = epochengine::terrain::make_heightfield(
                descriptor,
                {},
                limits.terrain);
            if (!built)
                return std::nullopt;
            converted.ground = GroundComponent{
                .terrain = built.heightfield
            };
            break;
        }
        case ObjectKind::light:
            converted.light = LightComponent{
                .color = {1.0f, 0.97f, 0.88f},
                .intensity = 3.0f,
                .direction = {-0.45f, -1.0f, -0.35f}
            };
            break;
        case ObjectKind::spawn:
            converted.spawn = SpawnComponent{
                .enabled = true
            };
            break;
        case ObjectKind::generic:
            break;
        }
        return converted;
    }

    inline SceneDocument::ObjectSlot* SceneDocument::resolve_slot(
        State& state,
        ObjectHandle handle) noexcept
    {
        if (!handle.valid() || handle.index >= state.slots.size())
            return nullptr;
        ObjectSlot& slot = state.slots[handle.index];
        return slot.active && slot.generation == handle.generation
            ? &slot
            : nullptr;
    }

    inline const SceneDocument::ObjectSlot* SceneDocument::resolve_slot(
        const State& state,
        ObjectHandle handle) noexcept
    {
        if (!handle.valid() || handle.index >= state.slots.size())
            return nullptr;
        const ObjectSlot& slot = state.slots[handle.index];
        return slot.active && slot.generation == handle.generation
            ? &slot
            : nullptr;
    }

    inline ObjectHandle SceneDocument::handle_for(
        const State& state,
        ObjectId id) noexcept
    {
        if (!id.valid())
            return {};
        for (std::size_t index = 0; index < state.slots.size(); ++index)
        {
            const ObjectSlot& slot = state.slots[index];
            if (slot.active && slot.object.id == id)
            {
                return {
                    static_cast<std::uint32_t>(index),
                    slot.generation
                };
            }
        }
        return {};
    }

    inline bool SceneDocument::limits_valid(
        const DocumentLimits& limits,
        const HistoryPolicy& history) noexcept
    {
        const bool sceneLimitsValid =
            limits.maximum_active_objects > 0 &&
            limits.maximum_object_slots >= limits.maximum_active_objects &&
            limits.maximum_object_slots < ObjectHandle::invalid_index &&
            limits.maximum_scene_name_bytes > 0 &&
            limits.maximum_object_name_bytes > 0 &&
            limits.maximum_type_bytes > 0 &&
            limits.maximum_category_bytes > 0 &&
            limits.maximum_spawn_role_bytes > 0 &&
            limits.maximum_lights > 0 &&
            limits.maximum_grounds > 0 &&
            limits.maximum_terrain_sample_bytes >= sizeof(float) &&
            limits.maximum_primary_cameras > 0 &&
            limits.maximum_operations_per_transaction > 0 &&
            limits.maximum_transaction_label_bytes > 0 &&
            limits.maximum_history_transactions > 0 &&
            limits.maximum_snapshot_string_bytes > 0 &&
            std::isfinite(limits.maximum_fixed_step_seconds) &&
            limits.maximum_fixed_step_seconds > 0.0;
        const bool supportedHistoryMode =
            history.mode == HistoryMode::disabled ||
            history.mode == HistoryMode::semantic_operations;
        return sceneLimitsValid &&
            supportedHistoryMode &&
            static_cast<bool>(epochengine::authoring::validate(history));
    }

    inline DocumentValidation SceneDocument::validate_state(
        const State& state,
        const DocumentLimits& limits)
    {
        DocumentValidation result{};
        if (!state.identity.scene.valid() ||
            !state.identity.project.valid() ||
            state.identity.schema_version != kSceneDocumentSchemaVersion ||
            state.identity.canonical_name.empty())
        {
            result.code = ResultCode::invalid_identity;
            return result;
        }
        if (state.identity.canonical_name.size() >
            limits.maximum_scene_name_bytes)
        {
            result.code = ResultCode::string_limit_exceeded;
            return result;
        }

        const SnapshotSourceMetadata& snapshot = state.snapshot_metadata;
        const auto snapshotStringBounded = [&limits](
            std::string_view value) noexcept
        {
            return value.size() <= limits.maximum_snapshot_string_bytes;
        };
        if (snapshot.scene_id.empty())
        {
            result.code = ResultCode::invalid_identity;
            return result;
        }
        if (!snapshotStringBounded(snapshot.scene_id) ||
            !snapshotStringBounded(snapshot.project_id) ||
            !snapshotStringBounded(snapshot.world_name) ||
            !snapshotStringBounded(snapshot.document_kind) ||
            !snapshotStringBounded(snapshot.support_tier))
        {
            result.code = ResultCode::string_limit_exceeded;
            return result;
        }
        if (snapshot.packages.size() > limits.maximum_snapshot_packages ||
            snapshot.timeline_keys.size() >
                limits.maximum_snapshot_timeline_keys)
        {
            result.code = ResultCode::invalid_document;
            return result;
        }
        for (std::size_t index = 0; index < snapshot.packages.size(); ++index)
        {
            const std::string& package = snapshot.packages[index];
            if (package.empty() || !snapshotStringBounded(package))
            {
                result.code = package.empty()
                    ? ResultCode::invalid_document
                    : ResultCode::string_limit_exceeded;
                return result;
            }
            for (std::size_t prior = 0; prior < index; ++prior)
            {
                if (snapshot.packages[prior] == package)
                {
                    result.code = ResultCode::invalid_document;
                    return result;
                }
            }
        }
        for (const epochengine::scene::SceneTimelineKey& key :
             snapshot.timeline_keys)
        {
            if (!std::isfinite(key.simulated_seconds) ||
                key.simulated_seconds < 0.0 ||
                !snapshotStringBounded(key.label) ||
                !snapshotStringBounded(key.event_kind) ||
                !snapshotStringBounded(key.target_name) ||
                !snapshotStringBounded(key.payload))
            {
                result.code = ResultCode::invalid_document;
                return result;
            }
        }
        if (state.slots.size() > limits.maximum_object_slots)
        {
            result.code = ResultCode::object_slot_limit_exceeded;
            return result;
        }

        std::vector<const ObjectSlot*> active{};
        active.reserve(state.slots.size());
        std::uint32_t primaryCameras{};
        std::uint64_t terrainSampleBytes{};
        for (std::size_t index = 0; index < state.slots.size(); ++index)
        {
            const ObjectSlot& slot = state.slots[index];
            if (!slot.active)
                continue;
            const ObjectHandle handle{
                static_cast<std::uint32_t>(index),
                slot.generation
            };
            result.object = handle;
            ++result.active_objects;
            if (result.active_objects > limits.maximum_active_objects)
            {
                result.code = ResultCode::object_limit_exceeded;
                return result;
            }
            if (slot.generation == 0 || !slot.object.id.valid())
            {
                result.code = ResultCode::invalid_identity;
                return result;
            }
            const ObjectMetadata& metadata = slot.object.metadata;
            if (metadata.name.empty() ||
                metadata.name.size() > limits.maximum_object_name_bytes ||
                metadata.generic_type.empty() ||
                metadata.generic_type.size() > limits.maximum_type_bytes ||
                metadata.category.size() > limits.maximum_category_bytes)
            {
                result.code = ResultCode::string_limit_exceeded;
                return result;
            }
            if (!detail::valid_transform(slot.object.transform))
            {
                result.code = ResultCode::invalid_transform;
                return result;
            }
            if (slot.object.editor_only && slot.object.runtime_visible)
            {
                result.code = ResultCode::invalid_document;
                return result;
            }

            const std::uint32_t componentCount =
                static_cast<std::uint32_t>(slot.object.camera.has_value()) +
                static_cast<std::uint32_t>(slot.object.ground.has_value()) +
                static_cast<std::uint32_t>(slot.object.light.has_value()) +
                static_cast<std::uint32_t>(slot.object.spawn.has_value());
            if (componentCount > 1)
            {
                result.code = ResultCode::conflicting_components;
                return result;
            }

            if (slot.object.camera)
            {
                ++result.cameras;
                const CameraComponent& camera = *slot.object.camera;
                if (!detail::finite_vector(camera.look_target) ||
                    !detail::finite_vector(camera.up) ||
                    detail::length_squared(camera.up) <= 0.0001f ||
                    !std::isfinite(camera.vertical_field_of_view_degrees) ||
                    camera.vertical_field_of_view_degrees <= 1.0f ||
                    camera.vertical_field_of_view_degrees >= 179.0f ||
                    !std::isfinite(camera.near_plane) ||
                    !std::isfinite(camera.far_plane) ||
                    camera.near_plane <= 0.0f ||
                    camera.far_plane <= camera.near_plane)
                {
                    result.code = ResultCode::invalid_camera;
                    return result;
                }
                const Float3 cameraToTarget{
                    slot.object.transform.position.x - camera.look_target.x,
                    slot.object.transform.position.y - camera.look_target.y,
                    slot.object.transform.position.z - camera.look_target.z
                };
                if (detail::length_squared(cameraToTarget) <= 0.0001f)
                {
                    result.code = ResultCode::invalid_camera;
                    return result;
                }
                if (camera.primary)
                    ++primaryCameras;
            }
            if (slot.object.ground)
            {
                ++result.grounds;
                if (result.grounds > limits.maximum_grounds)
                {
                    result.code = ResultCode::invalid_ground;
                    return result;
                }
                const GroundComponent& ground = *slot.object.ground;
                terrainSampleBytes = detail::saturating_add(
                    terrainSampleBytes,
                    detail::saturating_multiply(
                        ground.terrain.normalizedSamples.size(),
                        sizeof(float)));
                const auto terrainValidation = epochengine::terrain::validate(
                    ground.terrain.descriptor,
                    limits.terrain);
                if (!ground.terrain.valid() || !terrainValidation ||
                    terrainSampleBytes >
                        limits.maximum_terrain_sample_bytes ||
                    !detail::finite_color(ground.base_color) ||
                    ground.base_color.r < 0.0f ||
                    ground.base_color.g < 0.0f ||
                    ground.base_color.b < 0.0f ||
                    !std::isfinite(ground.roughness) ||
                    ground.roughness < 0.0f ||
                    ground.roughness > 1.0f)
                {
                    result.code = ResultCode::invalid_ground;
                    return result;
                }
            }
            if (slot.object.light)
            {
                ++result.lights;
                if (result.lights > limits.maximum_lights)
                {
                    result.code = ResultCode::light_limit_exceeded;
                    return result;
                }
                const auto lightValidation = epochengine::lighting::validate(
                    detail::light_desc(
                        slot.object.transform,
                        *slot.object.light));
                if (!lightValidation.valid ||
                    ((slot.object.light->kind ==
                          epochengine::lighting::LightKind::Directional ||
                      slot.object.light->kind ==
                          epochengine::lighting::LightKind::Spot) &&
                     detail::length_squared(
                         slot.object.light->direction) <= 0.0001f) ||
                    !detail::finite_color(slot.object.light->color) ||
                    slot.object.light->color.r < 0.0f ||
                    slot.object.light->color.g < 0.0f ||
                    slot.object.light->color.b < 0.0f)
                {
                    result.code = ResultCode::invalid_light;
                    return result;
                }
            }
            if (slot.object.spawn)
            {
                ++result.spawns;
                const SpawnComponent& spawn = *slot.object.spawn;
                if (spawn.role.empty() ||
                    spawn.role.size() > limits.maximum_spawn_role_bytes ||
                    !std::isfinite(spawn.clearance_above_ground) ||
                    spawn.clearance_above_ground < 0.0f)
                {
                    result.code = spawn.role.size() >
                            limits.maximum_spawn_role_bytes
                        ? ResultCode::string_limit_exceeded
                        : ResultCode::invalid_spawn;
                    return result;
                }
            }
            active.push_back(&slot);
        }

        for (std::size_t left = 0; left < active.size(); ++left)
        {
            for (std::size_t right = left + 1;
                 right < active.size();
                 ++right)
            {
                if (active[left]->object.id == active[right]->object.id)
                {
                    result.code = ResultCode::duplicate_identity;
                    return result;
                }
                if (active[left]->object.metadata.name ==
                    active[right]->object.metadata.name)
                {
                    result.code = ResultCode::duplicate_name;
                    return result;
                }
            }
        }

        if (result.grounds != 0)
        {
            for (const ObjectSlot* candidate : active)
            {
                if (!candidate->object.spawn ||
                    !candidate->object.spawn->enabled)
                {
                    continue;
                }
                const SpawnComponent& spawn = *candidate->object.spawn;
                bool supportedByGround{};
                for (const ObjectSlot* groundObject : active)
                {
                    if (!groundObject->object.ground)
                        continue;
                    const auto surface = epochengine::terrain::sample_surface(
                        groundObject->object.ground->terrain,
                        candidate->object.transform.position.x,
                        candidate->object.transform.position.z);
                    if (surface &&
                        candidate->object.transform.position.y >=
                            surface->position.y +
                                spawn.clearance_above_ground)
                    {
                        supportedByGround = true;
                        break;
                    }
                }
                if (!supportedByGround)
                {
                    result.code = ResultCode::invalid_spawn;
                    return result;
                }
            }
        }

        const ProjectSettings& project = state.project;
        const bool validRunState =
            project.run_state ==
                epochengine::scene_tier0::ProjectRunState::blocked ||
            project.run_state ==
                epochengine::scene_tier0::ProjectRunState::ready_to_launch;
        if (!project.project.valid() ||
            !project.entry_scene.valid() ||
            project.project != state.identity.project ||
            project.entry_scene != state.identity.scene ||
            !std::isfinite(project.fixed_step_seconds) ||
            project.fixed_step_seconds <= 0.0 ||
            project.fixed_step_seconds > limits.maximum_fixed_step_seconds ||
            !validRunState)
        {
            result.code = ResultCode::invalid_project_settings;
            return result;
        }

        const ObjectHandle primaryCamera =
            handle_for(state, project.primary_camera);
        const ObjectHandle primarySpawn =
            handle_for(state, project.primary_spawn);
        const bool ready = project.run_state ==
            epochengine::scene_tier0::ProjectRunState::ready_to_launch;
        if ((project.primary_camera.valid() && !primaryCamera.valid()) ||
            (project.primary_spawn.valid() && !primarySpawn.valid()))
        {
            result.code = ResultCode::unresolved_reference;
            return result;
        }
        if (primaryCamera.valid())
        {
            const ObjectSlot* camera = resolve_slot(state, primaryCamera);
            if (camera == nullptr ||
                !camera->object.camera ||
                !camera->object.camera->primary)
            {
                result.code = ResultCode::unresolved_reference;
                return result;
            }
        }
        if (primarySpawn.valid())
        {
            const ObjectSlot* spawn = resolve_slot(state, primarySpawn);
            if (spawn == nullptr ||
                !spawn->object.spawn ||
                !spawn->object.spawn->enabled)
            {
                result.code = ResultCode::unresolved_reference;
                return result;
            }
        }
        if (primaryCameras > limits.maximum_primary_cameras ||
            (ready && primaryCameras != 1) ||
            (ready && (!primaryCamera.valid() || !primarySpawn.valid())))
        {
            result.code = ResultCode::invalid_project_settings;
            return result;
        }

        result.object = {};
        result.code = ResultCode::success;
        return result;
    }

    inline SceneContentHash SceneDocument::content_hash(
        const State& state)
    {
        epochengine::authoring::DeterministicHashBuilder hash{
            "epoch.scene.document.content.v1"};
        detail::append_integral(hash, kSceneDocumentSchemaVersion);
        detail::append_integral(hash, state.identity.scene.value);
        detail::append_integral(hash, state.identity.project.value);
        hash.append_string(state.identity.canonical_name);
        detail::append_integral(hash, state.identity.schema_version);
        detail::append_integral(hash, state.project.project.value);
        detail::append_integral(hash, state.project.entry_scene.value);
        detail::append_integral(hash, state.project.primary_camera.value);
        detail::append_integral(hash, state.project.primary_spawn.value);
        detail::append_integral(hash, state.project.run_state);
        hash.append_double(state.project.fixed_step_seconds);
        detail::append_integral(hash, state.project.start_paused);
        hash.append_string(state.snapshot_metadata.scene_id);
        hash.append_string(state.snapshot_metadata.project_id);
        hash.append_string(state.snapshot_metadata.world_name);
        hash.append_string(state.snapshot_metadata.document_kind);
        hash.append_string(state.snapshot_metadata.support_tier);
        detail::append_integral(
            hash,
            state.snapshot_metadata.packages.size());
        for (const std::string& package : state.snapshot_metadata.packages)
            hash.append_string(package);
        detail::append_integral(
            hash,
            state.snapshot_metadata.timeline_keys.size());
        for (const epochengine::scene::SceneTimelineKey& key :
             state.snapshot_metadata.timeline_keys)
        {
            hash.append_double(key.simulated_seconds);
            detail::append_integral(hash, key.frame_index);
            hash.append_string(key.label);
            hash.append_string(key.event_kind);
            hash.append_string(key.target_name);
            hash.append_string(key.payload);
        }

        std::vector<const SceneObjectDescriptor*> objects{};
        objects.reserve(state.slots.size());
        for (const ObjectSlot& slot : state.slots)
        {
            if (slot.active)
                objects.push_back(&slot.object);
        }
        std::sort(
            objects.begin(),
            objects.end(),
            [](const SceneObjectDescriptor* left,
               const SceneObjectDescriptor* right)
            {
                return left->id.value < right->id.value;
            });
        detail::append_integral(hash, objects.size());
        for (const SceneObjectDescriptor* object : objects)
        {
            detail::append_integral(hash, object->id.value);
            hash.append_string(object->metadata.name);
            hash.append_string(object->metadata.generic_type);
            hash.append_string(object->metadata.category);
            detail::append_integral(hash, object->metadata.sort_order);
            detail::append_float3(hash, object->transform.position);
            detail::append_float3(hash, object->transform.rotation_degrees);
            detail::append_float3(hash, object->transform.scale);
            detail::append_integral(hash, object->runtime_visible);
            detail::append_integral(hash, object->editor_visible);
            detail::append_integral(hash, object->editor_only);
            detail::append_integral(hash, object_kind(*object));

            if (object->camera)
            {
                const CameraComponent& camera = *object->camera;
                detail::append_float3(hash, camera.look_target);
                detail::append_float3(hash, camera.up);
                hash.append_float(camera.vertical_field_of_view_degrees);
                hash.append_float(camera.near_plane);
                hash.append_float(camera.far_plane);
                detail::append_integral(hash, camera.primary);
            }
            if (object->ground)
            {
                const GroundComponent& ground = *object->ground;
                const auto& descriptor = ground.terrain.descriptor;
                detail::append_integral(hash, descriptor.asset.value);
                detail::append_integral(hash, descriptor.kind);
                detail::append_float3(hash, descriptor.origin);
                detail::append_integral(hash, descriptor.widthSamples);
                detail::append_integral(hash, descriptor.depthSamples);
                hash.append_float(descriptor.sampleSpacing);
                hash.append_float(descriptor.baseHeight);
                hash.append_float(descriptor.heightScale);
                detail::append_integral(hash, descriptor.materialSlot);
                detail::append_integral(hash, descriptor.collisionQueries);
                detail::append_integral(hash, ground.terrain.normalizedSamples.size());
                for (const float sample : ground.terrain.normalizedSamples)
                    hash.append_float(sample);
                detail::append_color(hash, ground.base_color);
                hash.append_float(ground.roughness);
                detail::append_integral(hash, ground.receives_lighting);
                detail::append_integral(hash, ground.receives_shadows);
                detail::append_integral(hash, ground.casts_shadows);
            }
            if (object->light)
            {
                const LightComponent& light = *object->light;
                detail::append_integral(hash, light.kind);
                detail::append_color(hash, light.color);
                hash.append_float(light.intensity);
                detail::append_float3(hash, light.direction);
                hash.append_float(light.range);
                hash.append_float(light.inner_cone_cosine);
                hash.append_float(light.outer_cone_cosine);
                detail::append_integral(hash, light.enabled);
                detail::append_integral(hash, light.casts_shadows);
            }
            if (object->spawn)
            {
                const SpawnComponent& spawn = *object->spawn;
                hash.append_string(spawn.role);
                hash.append_float(spawn.clearance_above_ground);
                detail::append_integral(hash, spawn.enabled);
            }
        }
        return hash.finish();
    }

    inline ResultCode SceneDocument::apply_operation(
        State& state,
        SceneOperation& operation,
        SceneOperation& inverse,
        const DocumentLimits& limits,
        ApplyMode mode)
    {
        return std::visit(
            [&](auto& value) -> ResultCode
            {
                using Type = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_same_v<Type, ObjectCreatedOperation>)
                {
                    if (!value.object.id.valid())
                        return ResultCode::invalid_identity;
                    for (const ObjectSlot& slot : state.slots)
                    {
                        if (slot.object.id == value.object.id)
                        {
                            const bool restoringSameSlot =
                                mode == ApplyMode::replay &&
                                value.handle.valid() &&
                                value.handle.index < state.slots.size() &&
                                &slot == &state.slots[value.handle.index] &&
                                !slot.active;
                            if (!restoringSameSlot)
                                return ResultCode::duplicate_identity;
                        }
                    }

                    if (mode == ApplyMode::command)
                    {
                        if (value.handle.valid())
                            return ResultCode::invalid_operation;
                        if (state.slots.size() >= limits.maximum_object_slots)
                            return ResultCode::object_slot_limit_exceeded;
                        const std::size_t activeCount = std::count_if(
                            state.slots.begin(),
                            state.slots.end(),
                            [](const ObjectSlot& slot)
                            {
                                return slot.active;
                            });
                        if (activeCount >= limits.maximum_active_objects)
                            return ResultCode::object_limit_exceeded;
                        ObjectSlot slot{};
                        slot.generation = 1;
                        slot.active = true;
                        slot.object = value.object;
                        state.slots.push_back(std::move(slot));
                        value.handle = {
                            static_cast<std::uint32_t>(state.slots.size() - 1),
                            1
                        };
                    }
                    else
                    {
                        if (!value.handle.valid() ||
                            value.handle.index >= state.slots.size())
                        {
                            return ResultCode::invalid_handle;
                        }
                        ObjectSlot& slot = state.slots[value.handle.index];
                        if (slot.active ||
                            slot.generation != epochengine::authoring::next_generation(
                                value.handle.generation))
                        {
                            return ResultCode::stale_handle;
                        }
                        slot.generation = value.handle.generation;
                        slot.active = true;
                        slot.object = value.object;
                    }
                    inverse = ObjectDestroyedOperation{value.handle};
                    return ResultCode::success;
                }
                else if constexpr (
                    std::is_same_v<Type, ObjectDestroyedOperation>)
                {
                    ObjectSlot* slot = resolve_slot(state, value.object);
                    if (slot == nullptr)
                        return value.object.valid()
                            ? ResultCode::stale_handle
                            : ResultCode::invalid_handle;
                    inverse = ObjectCreatedOperation{
                        .handle = value.object,
                        .object = slot->object
                    };
                    slot->active = false;
                    slot->generation = epochengine::authoring::next_generation(
                        slot->generation);
                    return ResultCode::success;
                }
                else if constexpr (
                    std::is_same_v<Type, ObjectMetadataChangedOperation>)
                {
                    ObjectSlot* slot = resolve_slot(state, value.object);
                    if (slot == nullptr)
                        return value.object.valid()
                            ? ResultCode::stale_handle
                            : ResultCode::invalid_handle;
                    inverse = ObjectMetadataChangedOperation{
                        value.object,
                        slot->object.metadata
                    };
                    slot->object.metadata = value.value;
                    return ResultCode::success;
                }
                else if constexpr (
                    std::is_same_v<Type, ObjectTransformChangedOperation>)
                {
                    ObjectSlot* slot = resolve_slot(state, value.object);
                    if (slot == nullptr)
                        return value.object.valid()
                            ? ResultCode::stale_handle
                            : ResultCode::invalid_handle;
                    inverse = ObjectTransformChangedOperation{
                        value.object,
                        slot->object.transform
                    };
                    slot->object.transform = value.value;
                    return ResultCode::success;
                }
                else if constexpr (
                    std::is_same_v<Type, ObjectVisibilityChangedOperation>)
                {
                    ObjectSlot* slot = resolve_slot(state, value.object);
                    if (slot == nullptr)
                        return value.object.valid()
                            ? ResultCode::stale_handle
                            : ResultCode::invalid_handle;
                    inverse = ObjectVisibilityChangedOperation{
                        value.object,
                        slot->object.runtime_visible,
                        slot->object.editor_visible,
                        slot->object.editor_only
                    };
                    slot->object.runtime_visible = value.runtime_visible;
                    slot->object.editor_visible = value.editor_visible;
                    slot->object.editor_only = value.editor_only;
                    return ResultCode::success;
                }
                else if constexpr (
                    std::is_same_v<Type, CameraComponentChangedOperation>)
                {
                    ObjectSlot* slot = resolve_slot(state, value.object);
                    if (slot == nullptr)
                        return value.object.valid()
                            ? ResultCode::stale_handle
                            : ResultCode::invalid_handle;
                    inverse = CameraComponentChangedOperation{
                        value.object,
                        slot->object.camera
                    };
                    slot->object.camera = value.value;
                    return ResultCode::success;
                }
                else if constexpr (
                    std::is_same_v<Type, GroundComponentChangedOperation>)
                {
                    ObjectSlot* slot = resolve_slot(state, value.object);
                    if (slot == nullptr)
                        return value.object.valid()
                            ? ResultCode::stale_handle
                            : ResultCode::invalid_handle;
                    inverse = GroundComponentChangedOperation{
                        value.object,
                        slot->object.ground
                    };
                    slot->object.ground = value.value;
                    return ResultCode::success;
                }
                else if constexpr (
                    std::is_same_v<Type, LightComponentChangedOperation>)
                {
                    ObjectSlot* slot = resolve_slot(state, value.object);
                    if (slot == nullptr)
                        return value.object.valid()
                            ? ResultCode::stale_handle
                            : ResultCode::invalid_handle;
                    inverse = LightComponentChangedOperation{
                        value.object,
                        slot->object.light
                    };
                    slot->object.light = value.value;
                    return ResultCode::success;
                }
                else if constexpr (
                    std::is_same_v<Type, SpawnComponentChangedOperation>)
                {
                    ObjectSlot* slot = resolve_slot(state, value.object);
                    if (slot == nullptr)
                        return value.object.valid()
                            ? ResultCode::stale_handle
                            : ResultCode::invalid_handle;
                    inverse = SpawnComponentChangedOperation{
                        value.object,
                        slot->object.spawn
                    };
                    slot->object.spawn = value.value;
                    return ResultCode::success;
                }
                else
                {
                    inverse = ProjectSettingsChangedOperation{state.project};
                    state.project = value.value;
                    return ResultCode::success;
                }
            },
            operation);
    }

    inline std::uint64_t SceneDocument::estimate_operation_bytes(
        const SceneOperation& operation) noexcept
    {
        return std::visit(
            [](const auto& value) -> std::uint64_t
            {
                using Type = std::remove_cvref_t<decltype(value)>;
                std::uint64_t bytes = sizeof(Type);
                if constexpr (std::is_same_v<Type, ObjectCreatedOperation>)
                {
                    bytes = detail::saturating_add(
                        bytes,
                        detail::object_bytes(value.object));
                }
                else if constexpr (
                    std::is_same_v<Type, ObjectMetadataChangedOperation>)
                {
                    bytes = detail::saturating_add(
                        bytes,
                        detail::string_bytes(value.value.name));
                    bytes = detail::saturating_add(
                        bytes,
                        detail::string_bytes(value.value.generic_type));
                    bytes = detail::saturating_add(
                        bytes,
                        detail::string_bytes(value.value.category));
                }
                else if constexpr (
                    std::is_same_v<Type, GroundComponentChangedOperation>)
                {
                    if (value.value)
                    {
                        bytes = detail::saturating_add(
                            bytes,
                            detail::ground_bytes(*value.value));
                    }
                }
                else if constexpr (
                    std::is_same_v<Type, SpawnComponentChangedOperation>)
                {
                    if (value.value)
                    {
                        bytes = detail::saturating_add(
                            bytes,
                            detail::string_bytes(value.value->role));
                    }
                }
                return bytes;
            },
            operation);
    }

    inline std::uint64_t SceneDocument::estimate_record_bytes(
        const TransactionRecord& record) noexcept
    {
        std::uint64_t bytes = sizeof(TransactionRecord);
        bytes = detail::saturating_add(bytes, record.label.size());
        for (const SceneOperation& operation : record.forward)
        {
            bytes = detail::saturating_add(
                bytes,
                estimate_operation_bytes(operation));
        }
        for (const SceneOperation& operation : record.inverse)
        {
            bytes = detail::saturating_add(
                bytes,
                estimate_operation_bytes(operation));
        }
        return bytes;
    }

    inline SceneDocumentBuildResult SceneDocument::create(
        SceneSeed seed,
        DocumentLimits limits,
        HistoryPolicy history)
    {
        SceneDocument document{};
        document.limits_ = limits;
        document.history_policy_ = history;
        if (!limits_valid(limits, history) ||
            seed.initial_revision_sequence == 0)
        {
            return {
                ResultCode::invalid_limits,
                {.code = ResultCode::invalid_limits},
                std::move(document)
            };
        }
        if (seed.objects.size() > limits.maximum_active_objects ||
            seed.objects.size() > limits.maximum_object_slots)
        {
            return {
                ResultCode::object_limit_exceeded,
                {.code = ResultCode::object_limit_exceeded},
                std::move(document)
            };
        }

        document.state_.identity = std::move(seed.identity);
        document.state_.project = seed.project;
        if (seed.snapshot_metadata)
        {
            document.state_.snapshot_metadata =
                std::move(*seed.snapshot_metadata);
        }
        else
        {
            document.state_.snapshot_metadata.scene_id =
                std::to_string(document.state_.identity.scene.value);
            document.state_.snapshot_metadata.project_id =
                std::to_string(document.state_.identity.project.value);
            document.state_.snapshot_metadata.world_name =
                document.state_.identity.canonical_name;
        }
        document.state_.slots.reserve(seed.objects.size());
        for (SceneObjectDescriptor& object : seed.objects)
        {
            ObjectSlot slot{};
            slot.generation = 1;
            slot.active = true;
            slot.object = std::move(object);
            document.state_.slots.push_back(std::move(slot));
        }
        const DocumentValidation validation = validate_state(
            document.state_,
            document.limits_);
        if (!validation)
        {
            return {
                validation.code,
                validation,
                std::move(document)
            };
        }
        document.revision_ = {
            content_hash(document.state_),
            seed.initial_revision_sequence
        };
        document.initialized_ = true;
        return {
            ResultCode::success,
            validation,
            std::move(document)
        };
    }

    inline SceneDocumentBuildResult SceneDocument::from_tier0(
        const epochengine::scene_tier0::Tier0Scene& source,
        DocumentLimits limits,
        HistoryPolicy history)
    {
        const epochengine::scene_tier0::Tier0Limits tier0Limits{
            .maximumObjects = limits.maximum_active_objects,
            .maximumDirectionalLights = limits.maximum_lights,
            .terrain = limits.terrain
        };
        if (!epochengine::scene_tier0::validate(source, tier0Limits))
        {
            SceneDocument document{};
            document.limits_ = limits;
            document.history_policy_ = history;
            return {
                ResultCode::tier0_seed_invalid,
                {.code = ResultCode::tier0_seed_invalid},
                std::move(document)
            };
        }

        SceneSeed seed{};
        seed.identity = {
            .scene = source.id,
            .project = source.project.project,
            .canonical_name = std::string{source.canonicalName},
            .schema_version = kSceneDocumentSchemaVersion
        };
        seed.project = {
            .project = source.project.project,
            .entry_scene = source.project.entryScene,
            .primary_camera = source.project.primaryCamera,
            .primary_spawn = source.project.primarySpawn,
            .run_state = source.project.state,
            .fixed_step_seconds = source.project.fixedStepSeconds,
            .start_paused = source.project.startPaused
        };
        seed.initial_revision_sequence = source.revision;
        seed.objects.reserve(source.objects.size());
        for (std::size_t index = 0; index < source.objects.size(); ++index)
        {
            const auto& object = source.objects[index];
            SceneObjectDescriptor converted{};
            converted.id = object.id;
            converted.metadata.name = std::string{object.canonicalName};
            converted.metadata.sort_order =
                static_cast<std::uint32_t>(index);
            converted.transform = {
                .position = object.transform.position,
                .rotation_degrees = object.transform.rotationDegrees,
                .scale = object.transform.scale
            };
            converted.runtime_visible = object.runtimeVisible;
            converted.editor_visible = object.editorVisible;
            converted.editor_only =
                object.editorVisible && !object.runtimeVisible;
            switch (object.kind)
            {
            case epochengine::scene_tier0::ObjectKind::camera:
                converted.metadata.generic_type = "Camera";
                converted.metadata.category = "Editor";
                converted.camera = CameraComponent{
                    .look_target = source.camera.lookTarget,
                    .up = source.camera.up,
                    .vertical_field_of_view_degrees =
                        source.camera.verticalFieldOfViewDegrees,
                    .near_plane = source.camera.nearPlane,
                    .far_plane = source.camera.farPlane,
                    .primary = source.camera.primary
                };
                break;
            case epochengine::scene_tier0::ObjectKind::ground:
                converted.metadata.generic_type = "Ground";
                converted.metadata.category = "World";
                converted.ground = GroundComponent{
                    .terrain = source.groundTerrain,
                    .base_color = source.ground.baseColor,
                    .roughness = source.ground.roughness,
                    .receives_lighting = source.ground.receivesLighting,
                    .receives_shadows = source.ground.receivesShadows,
                    .casts_shadows = source.ground.castsShadows
                };
                break;
            case epochengine::scene_tier0::ObjectKind::directional_light:
                converted.metadata.generic_type = "Light";
                converted.metadata.category = "Lighting";
                converted.light = LightComponent{
                    .kind = source.directionalLight.light.kind,
                    .color = source.directionalLight.light.color,
                    .intensity = source.directionalLight.light.intensity,
                    .direction = source.directionalLight.light.direction,
                    .range = source.directionalLight.light.range,
                    .inner_cone_cosine =
                        source.directionalLight.light.innerConeCosine,
                    .outer_cone_cosine =
                        source.directionalLight.light.outerConeCosine,
                    .enabled = source.directionalLight.light.enabled,
                    .casts_shadows = source.directionalLight.castsShadows
                };
                break;
            case epochengine::scene_tier0::ObjectKind::spawn_point:
                converted.metadata.generic_type = "Spawn";
                converted.metadata.category = "Gameplay";
                converted.spawn = SpawnComponent{
                    .role = std::string{source.spawn.role},
                    .clearance_above_ground =
                        source.spawn.clearanceAboveGround,
                    .enabled = source.spawn.enabled
                };
                converted.editor_only = false;
                break;
            }
            seed.objects.push_back(std::move(converted));
        }
        return create(std::move(seed), limits, history);
    }
    inline SceneDocumentBuildResult SceneDocument::from_snapshot(
        const epochengine::scene::SceneSnapshot& source,
        DocumentLimits limits,
        HistoryPolicy history)
    {
        SceneDocument rejected{};
        rejected.limits_ = limits;
        rejected.history_policy_ = history;
        if (!limits_valid(limits, history))
        {
            return {
                ResultCode::invalid_limits,
                {.code = ResultCode::invalid_limits},
                std::move(rejected)
            };
        }
        if (source.objects.size() > limits.maximum_active_objects ||
            source.objects.size() > limits.maximum_object_slots)
        {
            return {
                ResultCode::object_limit_exceeded,
                {.code = ResultCode::object_limit_exceeded},
                std::move(rejected)
            };
        }
        if (source.packages.size() > limits.maximum_snapshot_packages ||
            source.timeline_keys.size() >
                limits.maximum_snapshot_timeline_keys ||
            !std::isfinite(source.captured_simulated_seconds) ||
            source.captured_simulated_seconds < 0.0)
        {
            return {
                ResultCode::invalid_document,
                {.code = ResultCode::invalid_document},
                std::move(rejected)
            };
        }

        epochengine::scene::SceneSnapshot normalized = source;
        epochengine::scene::normalize_scene_document(normalized);
        const epochengine::scene::SceneDocumentLimits snapshotLimits{
            .maximum_objects = limits.maximum_active_objects,
            .maximum_timeline_keys =
                limits.maximum_snapshot_timeline_keys,
            .maximum_packages = limits.maximum_snapshot_packages,
            .maximum_string_bytes = detail::snapshot_string_limit(limits)
        };
        if (!epochengine::scene::validate_scene_document(
                normalized,
                epochengine::scene::SceneDocumentRequirement::authoring,
                snapshotLimits))
        {
            return {
                ResultCode::invalid_document,
                {.code = ResultCode::invalid_document},
                std::move(rejected)
            };
        }

        const SceneId internalScene =
            detail::internal_id_from_public_text<SceneId>(
                normalized.scene_id,
                "epoch.scene.snapshot");
        const ProjectId internalProject =
            detail::internal_id_from_public_text<ProjectId>(
                normalized.project_id,
                normalized.scene_id);
        SceneSeed seed{};
        seed.identity = {
            .scene = internalScene,
            .project = internalProject,
            .canonical_name = normalized.world_name.empty()
                ? normalized.scene_id
                : normalized.world_name,
            .schema_version = kSceneDocumentSchemaVersion
        };
        const bool runnable = static_cast<bool>(
            epochengine::scene::validate_scene_document(
                normalized,
                epochengine::scene::SceneDocumentRequirement::runnable,
                snapshotLimits));
        seed.project = {
            .project = internalProject,
            .entry_scene = internalScene,
            .primary_camera = {normalized.primary_camera},
            .primary_spawn = {normalized.primary_spawn},
            .run_state = runnable
                ? epochengine::scene_tier0::ProjectRunState::ready_to_launch
                : epochengine::scene_tier0::ProjectRunState::blocked
        };
        seed.snapshot_metadata = SnapshotSourceMetadata{
            .scene_id = normalized.scene_id,
            .project_id = normalized.project_id,
            .world_name = normalized.world_name,
            .document_kind = normalized.document_kind,
            .support_tier = normalized.support_tier,
            .packages = normalized.packages,
            .timeline_keys = normalized.timeline_keys
        };
        seed.initial_revision_sequence = normalized.revision;
        seed.objects.reserve(normalized.objects.size());
        for (std::size_t index = 0; index < normalized.objects.size(); ++index)
        {
            std::optional<SceneObjectDescriptor> converted =
                descriptor_from_snapshot_object(
                    normalized.objects[index],
                    static_cast<std::uint32_t>(index),
                    limits);
            if (!converted)
            {
                return {
                    ResultCode::invalid_document,
                    {.code = ResultCode::invalid_document},
                    std::move(rejected)
                };
            }
            if (converted->camera)
            {
                converted->camera->primary =
                    converted->id.value == normalized.primary_camera;
            }
            seed.objects.push_back(std::move(*converted));
        }
        return create(std::move(seed), limits, history);
    }


    inline SceneDocumentBuildResult SceneDocument::make_default_tier0(
        DocumentLimits limits,
        HistoryPolicy history)
    {
        const epochengine::scene_tier0::Tier0BuildResult built =
            epochengine::scene_tier0::make_default_scene({
                .maximumObjects = limits.maximum_active_objects,
                .maximumDirectionalLights = limits.maximum_lights,
                .terrain = limits.terrain
            });
        if (!built)
        {
            SceneDocument document{};
            document.limits_ = limits;
            document.history_policy_ = history;
            return {
                ResultCode::tier0_seed_invalid,
                {.code = ResultCode::tier0_seed_invalid},
                std::move(document)
            };
        }
        return from_tier0(built.scene, limits, history);
    }

    inline DocumentValidation SceneDocument::validate() const
    {
        if (!initialized_ ||
            !epochengine::authoring::validate(revision_))
        {
            return {.code = ResultCode::invalid_document};
        }
        return validate_state(state_, limits_);
    }

    inline std::optional<SceneObject> SceneDocument::object(
        ObjectHandle handle) const
    {
        const ObjectSlot* slot = resolve_slot(state_, handle);
        if (slot == nullptr)
            return std::nullopt;
        return SceneObject{handle, slot->object};
    }

    inline std::optional<ObjectHandle> SceneDocument::find(
        ObjectId id) const noexcept
    {
        const ObjectHandle handle = handle_for(state_, id);
        return handle.valid()
            ? std::optional<ObjectHandle>{handle}
            : std::nullopt;
    }

    inline std::vector<SceneObject> SceneDocument::objects() const
    {
        std::vector<SceneObject> result{};
        result.reserve(state_.slots.size());
        for (std::size_t index = 0; index < state_.slots.size(); ++index)
        {
            const ObjectSlot& slot = state_.slots[index];
            if (!slot.active)
                continue;
            result.push_back({
                {static_cast<std::uint32_t>(index), slot.generation},
                slot.object
            });
        }
        std::sort(
            result.begin(),
            result.end(),
            [](const SceneObject& left, const SceneObject& right)
            {
                if (left.descriptor.metadata.sort_order !=
                    right.descriptor.metadata.sort_order)
                {
                    return left.descriptor.metadata.sort_order <
                        right.descriptor.metadata.sort_order;
                }
                return left.descriptor.id.value < right.descriptor.id.value;
            });
        return result;
    }

    inline TransactionResult SceneDocument::apply_transaction(
        const SceneTransaction& transaction)
    {
        return apply_transaction(
            std::span<const SceneOperation>{transaction.operations},
            transaction.label);
    }

    inline TransactionResult SceneDocument::apply_transaction(
        std::span<const SceneOperation> operations,
        std::string_view label)
    {
        if (!initialized_)
            return {.code = ResultCode::invalid_document};
        if (operations.empty())
            return {.code = ResultCode::unchanged, .revision = revision_};
        if (operations.size() > limits_.maximum_operations_per_transaction)
        {
            return {
                .code = ResultCode::transaction_operation_limit_exceeded,
                .revision = revision_
            };
        }
        if (label.size() > limits_.maximum_transaction_label_bytes)
        {
            return {
                .code = ResultCode::string_limit_exceeded,
                .revision = revision_
            };
        }
        if (revision_.sequence ==
            (std::numeric_limits<std::uint64_t>::max)())
        {
            return {
                .code = ResultCode::revision_overflow,
                .revision = revision_
            };
        }

        State candidate = state_;
        std::vector<SceneOperation> forward{operations.begin(), operations.end()};
        std::vector<SceneOperation> inverse{};
        inverse.reserve(forward.size());
        ObjectHandle primaryObject{};
        for (SceneOperation& operation : forward)
        {
            SceneOperation inverseOperation{
                ProjectSettingsChangedOperation{candidate.project}};
            const ResultCode applied = apply_operation(
                candidate,
                operation,
                inverseOperation,
                limits_,
                ApplyMode::command);
            if (applied != ResultCode::success)
                return {.code = applied, .revision = revision_};
            if (!primaryObject.valid())
            {
                primaryObject = std::visit(
                    [](const auto& value) -> ObjectHandle
                    {
                        using Type = std::remove_cvref_t<decltype(value)>;
                        if constexpr (
                            std::is_same_v<Type, ObjectCreatedOperation>)
                            return value.handle;
                        else if constexpr (
                            std::is_same_v<Type, ProjectSettingsChangedOperation>)
                            return {};
                        else
                            return value.object;
                    },
                    operation);
            }
            inverse.push_back(std::move(inverseOperation));
        }
        std::reverse(inverse.begin(), inverse.end());

        const DocumentValidation validation = validate_state(
            candidate,
            limits_);
        if (!validation)
            return {.code = validation.code, .revision = revision_};
        const SceneContentHash nextContent = content_hash(candidate);
        if (detail::same_content_hash(nextContent, revision_.content))
            return {.code = ResultCode::unchanged, .revision = revision_};

        TransactionRecord record{};
        record.id = {next_transaction_id_};
        record.label = std::string{label};
        record.before = revision_;
        record.after = {nextContent, revision_.sequence + 1};
        record.forward = std::move(forward);
        record.inverse = std::move(inverse);
        record.retained_bytes = estimate_record_bytes(record);

        std::vector<TransactionRecord> nextHistory{};
        std::uint64_t nextRetainedBytes{};
        std::uint64_t nextOperationCount{};
        if (history_policy_.mode != HistoryMode::disabled)
        {
            if (!history_policy_.allow_branching &&
                history_cursor_ != history_.size())
            {
                return {
                    .code = ResultCode::invalid_operation,
                    .revision = revision_
                };
            }
            for (const SceneOperation& operation : record.forward)
            {
                if (estimate_operation_bytes(operation) >
                    history_policy_.maximum_operation_bytes)
                {
                    return {
                        .code = ResultCode::history_memory_limit_exceeded,
                        .revision = revision_
                    };
                }
            }
            for (const SceneOperation& operation : record.inverse)
            {
                if (estimate_operation_bytes(operation) >
                    history_policy_.maximum_operation_bytes)
                {
                    return {
                        .code = ResultCode::history_memory_limit_exceeded,
                        .revision = revision_
                    };
                }
            }
            const std::size_t retainedTransactions = history_cursor_;
            if (retainedTransactions >= limits_.maximum_history_transactions)
            {
                return {
                    .code = ResultCode::history_transaction_limit_exceeded,
                    .revision = revision_
                };
            }
            for (std::size_t index = 0;
                 index < retainedTransactions;
                 ++index)
            {
                nextRetainedBytes = detail::saturating_add(
                    nextRetainedBytes,
                    history_[index].retained_bytes);
                nextOperationCount = detail::saturating_add(
                    nextOperationCount,
                    history_[index].forward.size());
            }
            nextRetainedBytes = detail::saturating_add(
                nextRetainedBytes,
                record.retained_bytes);
            nextOperationCount = detail::saturating_add(
                nextOperationCount,
                record.forward.size());
            if (nextOperationCount > history_policy_.maximum_operations)
            {
                return {
                    .code = ResultCode::history_operation_limit_exceeded,
                    .revision = revision_
                };
            }
            if (nextRetainedBytes > history_policy_.maximum_memory_bytes)
            {
                return {
                    .code = ResultCode::history_memory_limit_exceeded,
                    .revision = revision_
                };
            }
            nextHistory.reserve(retainedTransactions + 1);
            nextHistory.insert(
                nextHistory.end(),
                history_.begin(),
                history_.begin() +
                    static_cast<std::ptrdiff_t>(retainedTransactions));
            nextHistory.push_back(std::move(record));
        }

        state_ = std::move(candidate);
        revision_ = history_policy_.mode != HistoryMode::disabled
            ? nextHistory.back().after
            : SceneRevision{nextContent, revision_.sequence + 1};
        if (history_policy_.mode != HistoryMode::disabled)
        {
            history_ = std::move(nextHistory);
            history_cursor_ = history_.size();
            retained_history_bytes_ = nextRetainedBytes;
            history_operation_count_ = nextOperationCount;
        }
        ++next_transaction_id_;
        if (next_transaction_id_ == 0)
            next_transaction_id_ = 1;
        return {
            .code = ResultCode::success,
            .revision = revision_,
            .transaction = {next_transaction_id_ == 1
                ? (std::numeric_limits<std::uint64_t>::max)()
                : next_transaction_id_ - 1},
            .primary_object = primaryObject,
            .applied_operations = static_cast<std::uint32_t>(operations.size())
        };
    }

    inline bool SceneDocument::can_undo() const noexcept
    {
        return initialized_ && history_policy_.mode != HistoryMode::disabled &&
            history_cursor_ > 0;
    }

    inline bool SceneDocument::can_redo() const noexcept
    {
        return initialized_ && history_policy_.mode != HistoryMode::disabled &&
            history_cursor_ < history_.size();
    }

    inline TransactionResult SceneDocument::replay_history(bool forward)
    {
        if (!initialized_)
            return {.code = ResultCode::invalid_document};
        if (revision_.sequence ==
            (std::numeric_limits<std::uint64_t>::max)())
        {
            return {
                .code = ResultCode::revision_overflow,
                .revision = revision_
            };
        }
        if (forward ? !can_redo() : !can_undo())
        {
            return {
                .code = forward
                    ? ResultCode::nothing_to_redo
                    : ResultCode::nothing_to_undo,
                .revision = revision_
            };
        }

        const std::size_t recordIndex = forward
            ? history_cursor_
            : history_cursor_ - 1;
        const TransactionRecord& record = history_[recordIndex];
        const std::vector<SceneOperation>& selected = forward
            ? record.forward
            : record.inverse;
        State candidate = state_;
        ObjectHandle primaryObject{};
        for (const SceneOperation& source : selected)
        {
            SceneOperation operation = source;
            SceneOperation discardedInverse{
                ProjectSettingsChangedOperation{candidate.project}};
            const ResultCode applied = apply_operation(
                candidate,
                operation,
                discardedInverse,
                limits_,
                ApplyMode::replay);
            if (applied != ResultCode::success)
            {
                return {
                    .code = ResultCode::history_corrupt,
                    .revision = revision_
                };
            }
            if (!primaryObject.valid())
            {
                primaryObject = std::visit(
                    [](const auto& value) -> ObjectHandle
                    {
                        using Type = std::remove_cvref_t<decltype(value)>;
                        if constexpr (
                            std::is_same_v<Type, ObjectCreatedOperation>)
                            return value.handle;
                        else if constexpr (
                            std::is_same_v<Type, ProjectSettingsChangedOperation>)
                            return {};
                        else
                            return value.object;
                    },
                    operation);
            }
        }
        const DocumentValidation validation = validate_state(candidate, limits_);
        if (!validation)
        {
            return {
                .code = ResultCode::history_corrupt,
                .revision = revision_
            };
        }
        const SceneContentHash expected = forward
            ? record.after.content
            : record.before.content;
        const SceneContentHash actual = content_hash(candidate);
        if (!detail::same_content_hash(actual, expected))
        {
            return {
                .code = ResultCode::history_corrupt,
                .revision = revision_
            };
        }

        state_ = std::move(candidate);
        revision_ = {actual, revision_.sequence + 1};
        if (forward)
            ++history_cursor_;
        else
            --history_cursor_;
        return {
            .code = ResultCode::success,
            .revision = revision_,
            .transaction = record.id,
            .primary_object = primaryObject,
            .applied_operations =
                static_cast<std::uint32_t>(selected.size())
        };
    }

    inline TransactionResult SceneDocument::undo()
    {
        return replay_history(false);
    }

    inline TransactionResult SceneDocument::redo()
    {
        return replay_history(true);
    }

    inline std::vector<TransactionSummary> SceneDocument::history() const
    {
        std::vector<TransactionSummary> result{};
        result.reserve(history_.size());
        for (std::size_t index = 0; index < history_.size(); ++index)
        {
            const TransactionRecord& record = history_[index];
            result.push_back({
                .id = record.id,
                .label = record.label,
                .before = record.before,
                .after = record.after,
                .operation_count =
                    static_cast<std::uint32_t>(record.forward.size()),
                .retained_bytes = record.retained_bytes,
                .currently_applied = index < history_cursor_
            });
        }
        return result;
    }

    inline DocumentMetrics SceneDocument::metrics() const noexcept
    {
        DocumentMetrics result{};
        result.allocated_slots =
            static_cast<std::uint32_t>(state_.slots.size());
        result.history_transactions =
            static_cast<std::uint32_t>(history_.size());
        result.applied_transactions =
            static_cast<std::uint32_t>(history_cursor_);
        result.history_operations = history_operation_count_;
        result.retained_history_bytes = retained_history_bytes_;
        result.revision_sequence = revision_.sequence;
        for (const ObjectSlot& slot : state_.slots)
        {
            if (slot.active)
                ++result.active_objects;
        }
        return result;
    }

    inline SnapshotProjectionResult SceneDocument::project_snapshot(
        std::uint64_t frame_index,
        double simulated_seconds) const
    {
        if (!initialized_)
            return {.code = ResultCode::invalid_document};
        if (!std::isfinite(simulated_seconds) || simulated_seconds < 0.0)
            return {.code = ResultCode::invalid_operation};
        const DocumentValidation validation = validate();
        if (!validation)
            return {.code = validation.code};

        const SnapshotSourceMetadata& metadata = state_.snapshot_metadata;
        epochengine::scene::SceneSnapshot snapshot{};
        snapshot.scene_id = metadata.scene_id;
        snapshot.project_id = metadata.project_id;
        snapshot.world_name = metadata.world_name;
        snapshot.document_kind = metadata.document_kind;
        snapshot.support_tier = metadata.support_tier;
        snapshot.packages = metadata.packages;
        snapshot.timeline_keys = metadata.timeline_keys;
        snapshot.revision = revision_.sequence;
        snapshot.primary_camera = state_.project.primary_camera.value;
        snapshot.primary_spawn = state_.project.primary_spawn.value;
        snapshot.captured_frame_index = frame_index;
        snapshot.captured_simulated_seconds = simulated_seconds;
        const std::vector<SceneObject> ordered = objects();
        snapshot.objects.reserve(ordered.size());
        for (const SceneObject& object : ordered)
        {
            const SceneObjectDescriptor& source = object.descriptor;
            std::string type = source.metadata.generic_type;
            std::string category = source.metadata.category;
            switch (object_kind(source))
            {
            case ObjectKind::camera:
                type = "Camera";
                if (category.empty())
                    category = "Editor";
                break;
            case ObjectKind::ground:
                type = "Ground";
                if (category.empty())
                    category = "World";
                break;
            case ObjectKind::light:
                type = "Light";
                if (category.empty())
                    category = "Lighting";
                break;
            case ObjectKind::spawn:
                type = "Spawn";
                if (category.empty())
                    category = "Gameplay";
                break;
            case ObjectKind::generic:
                if (category.empty())
                    category = "Uncategorized";
                break;
            }
            snapshot.objects.push_back({
                .id = source.id.value,
                .name = source.metadata.name,
                .type = std::move(type),
                .category = std::move(category),
                .position = {
                    source.transform.position.x,
                    source.transform.position.y,
                    source.transform.position.z
                },
                .rotation = {
                    source.transform.rotation_degrees.x,
                    source.transform.rotation_degrees.y,
                    source.transform.rotation_degrees.z
                },
                .scale = {
                    source.transform.scale.x,
                    source.transform.scale.y,
                    source.transform.scale.z
                },
                .visible = source.editor_visible,
                .editor_only = source.editor_only
            });
        }

        epochengine::scene::normalize_scene_document(snapshot);
        const auto requirement = state_.project.run_state ==
                epochengine::scene_tier0::ProjectRunState::ready_to_launch
            ? epochengine::scene::SceneDocumentRequirement::runnable
            : epochengine::scene::SceneDocumentRequirement::authoring;
        const epochengine::scene::SceneDocumentLimits snapshotLimits{
            .maximum_objects = limits_.maximum_active_objects,
            .maximum_timeline_keys = limits_.maximum_snapshot_timeline_keys,
            .maximum_packages = limits_.maximum_snapshot_packages,
            .maximum_string_bytes = detail::snapshot_string_limit(limits_)
        };
        if (!epochengine::scene::validate_scene_document(
                snapshot,
                requirement,
                snapshotLimits))
        {
            return {.code = ResultCode::invalid_document};
        }
        return {
            .code = ResultCode::success,
            .snapshot = std::move(snapshot)
        };
    }

    struct SnapshotIngestionContractResult final
    {
        bool semantic_round_trip{};
        bool stable_object_ids{};
        bool revision_preserved{};
        bool components_mapped{};
        bool visibility_states_preserved{};
        bool source_metadata_preserved{};
        bool clean_history{};
        bool malformed_rejected{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return semantic_round_trip &&
                stable_object_ids &&
                revision_preserved &&
                components_mapped &&
                visibility_states_preserved &&
                source_metadata_preserved &&
                clean_history &&
                malformed_rejected;
        }
    };

    namespace detail
    {
        [[nodiscard]] inline bool same_timeline_key(
            const epochengine::scene::SceneTimelineKey& left,
            const epochengine::scene::SceneTimelineKey& right) noexcept
        {
            return left.simulated_seconds == right.simulated_seconds &&
                left.frame_index == right.frame_index &&
                left.label == right.label &&
                left.event_kind == right.event_kind &&
                left.target_name == right.target_name &&
                left.payload == right.payload;
        }

        [[nodiscard]] inline bool same_snapshot_object(
            const epochengine::scene::SceneObjectSnapshot& left,
            const epochengine::scene::SceneObjectSnapshot& right) noexcept
        {
            return left.id == right.id &&
                left.name == right.name &&
                left.type == right.type &&
                left.category == right.category &&
                left.position == right.position &&
                left.rotation == right.rotation &&
                left.scale == right.scale &&
                left.visible == right.visible &&
                left.editor_only == right.editor_only;
        }

        [[nodiscard]] inline bool same_snapshot_semantics(
            const epochengine::scene::SceneSnapshot& left,
            const epochengine::scene::SceneSnapshot& right) noexcept
        {
            if (left.scene_id != right.scene_id ||
                left.project_id != right.project_id ||
                left.world_name != right.world_name ||
                left.document_kind != right.document_kind ||
                left.support_tier != right.support_tier ||
                left.revision != right.revision ||
                left.primary_camera != right.primary_camera ||
                left.primary_spawn != right.primary_spawn ||
                left.captured_frame_index != right.captured_frame_index ||
                left.captured_simulated_seconds !=
                    right.captured_simulated_seconds ||
                left.packages != right.packages ||
                left.objects.size() != right.objects.size() ||
                left.timeline_keys.size() != right.timeline_keys.size())
            {
                return false;
            }
            for (std::size_t index = 0; index < left.objects.size(); ++index)
            {
                if (!same_snapshot_object(
                        left.objects[index],
                        right.objects[index]))
                {
                    return false;
                }
            }
            for (std::size_t index = 0;
                 index < left.timeline_keys.size();
                 ++index)
            {
                if (!same_timeline_key(
                        left.timeline_keys[index],
                        right.timeline_keys[index]))
                {
                    return false;
                }
            }
            return true;
        }
    }

    [[nodiscard]] inline SnapshotIngestionContractResult
        scene_document_snapshot_ingestion_contract()
    {
        using SnapshotObject = epochengine::scene::SceneObjectSnapshot;
        epochengine::scene::SceneSnapshot source{};
        source.scene_id = "sample.scene.public-id";
        source.project_id = "sample.project.public-id";
        source.world_name = "Canonical Round Trip";
        source.document_kind = "scene";
        source.support_tier = "tier0";
        source.revision = 73;
        source.primary_camera = 101;
        source.primary_spawn = 104;
        source.captured_frame_index = 19;
        source.captured_simulated_seconds = 0.25;
        source.packages = {"engine_arcade", "forest_factory"};
        source.timeline_keys = {{
            .simulated_seconds = 0.25,
            .frame_index = 19,
            .label = "Spawn Ready",
            .event_kind = "scene.spawn.ready",
            .target_name = "PlayerSpawn",
            .payload = "primary"
        }};
        source.objects = {
            SnapshotObject{
                .id = 101,
                .name = "PrimaryCamera",
                .type = "Camera",
                .category = "Editor",
                .position = {4.0f, 3.0f, 5.0f},
                .rotation = {-20.0f, -140.0f, 0.0f},
                .visible = true,
                .editor_only = true
            },
            SnapshotObject{
                .id = 102,
                .name = "Ground",
                .type = "Ground",
                .category = "World",
                .visible = true,
                .editor_only = false
            },
            SnapshotObject{
                .id = 103,
                .name = "Sun",
                .type = "Light",
                .category = "Lighting",
                .position = {0.0f, 6.0f, 0.0f},
                .rotation = {-50.0f, -35.0f, 0.0f},
                .visible = false,
                .editor_only = true
            },
            SnapshotObject{
                .id = 104,
                .name = "PlayerSpawn",
                .type = "Spawn",
                .category = "Gameplay",
                .position = {0.0f, 0.05f, 0.0f},
                .visible = true,
                .editor_only = false
            },
            SnapshotObject{
                .id = 201,
                .name = "VisibleRuntime",
                .type = "Entity",
                .category = "Tests",
                .position = {1.0f, 1.0f, 1.0f},
                .visible = true,
                .editor_only = false
            },
            SnapshotObject{
                .id = 202,
                .name = "VisibleEditorOnly",
                .type = "Entity",
                .category = "Tests",
                .position = {2.0f, 1.0f, 1.0f},
                .visible = true,
                .editor_only = true
            },
            SnapshotObject{
                .id = 203,
                .name = "HiddenRuntime",
                .type = "Entity",
                .category = "Tests",
                .position = {3.0f, 1.0f, 1.0f},
                .visible = false,
                .editor_only = false
            },
            SnapshotObject{
                .id = 204,
                .name = "HiddenEditorOnly",
                .type = "Entity",
                .category = "Tests",
                .position = {4.0f, 1.0f, 1.0f},
                .visible = false,
                .editor_only = true
            }
        };

        const SceneDocumentBuildResult built =
            SceneDocument::from_snapshot(source);
        SnapshotIngestionContractResult result{};
        if (built)
        {
            const SnapshotProjectionResult projected =
                built->project_snapshot(
                    source.captured_frame_index,
                    source.captured_simulated_seconds);
            if (projected)
            {
                result.semantic_round_trip =
                    detail::same_snapshot_semantics(
                        source,
                        projected.snapshot);
                result.revision_preserved =
                    built->revision().sequence == source.revision &&
                    projected.snapshot.revision == source.revision;
                result.source_metadata_preserved =
                    projected.snapshot.scene_id == source.scene_id &&
                    projected.snapshot.project_id == source.project_id &&
                    projected.snapshot.world_name == source.world_name &&
                    projected.snapshot.document_kind == source.document_kind &&
                    projected.snapshot.support_tier == source.support_tier &&
                    projected.snapshot.packages == source.packages &&
                    projected.snapshot.timeline_keys.size() ==
                        source.timeline_keys.size();
                result.visibility_states_preserved = true;
                for (std::size_t index = 0;
                     index < source.objects.size();
                     ++index)
                {
                    result.visibility_states_preserved =
                        result.visibility_states_preserved &&
                        projected.snapshot.objects[index].visible ==
                            source.objects[index].visible &&
                        projected.snapshot.objects[index].editor_only ==
                            source.objects[index].editor_only;
                }
            }

            result.stable_object_ids = true;
            result.components_mapped = true;
            for (const SnapshotObject& object : source.objects)
            {
                const std::optional<ObjectHandle> handle =
                    built->find({object.id});
                const std::optional<SceneObject> stored = handle
                    ? built->object(*handle)
                    : std::nullopt;
                result.stable_object_ids = result.stable_object_ids &&
                    stored && stored->descriptor.id.value == object.id;
                if (!stored)
                {
                    result.components_mapped = false;
                    continue;
                }
                const ObjectKind expected =
                    detail::snapshot_object_kind(object.type);
                result.components_mapped = result.components_mapped &&
                    object_kind(stored->descriptor) == expected &&
                    stored->descriptor.editor_visible == object.visible &&
                    stored->descriptor.editor_only == object.editor_only &&
                    stored->descriptor.runtime_visible ==
                        (object.visible && !object.editor_only);
            }
            const DocumentMetrics metrics = built->metrics();
            result.clean_history = built->history().empty() &&
                !built->can_undo() && !built->can_redo() &&
                metrics.history_transactions == 0 &&
                metrics.applied_transactions == 0 &&
                metrics.history_operations == 0 &&
                metrics.retained_history_bytes == 0;
        }

        epochengine::scene::SceneSnapshot duplicate = source;
        duplicate.objects.back().id = duplicate.objects.front().id;
        epochengine::scene::SceneSnapshot nonFinite = source;
        nonFinite.objects.front().scale[0] =
            (std::numeric_limits<float>::quiet_NaN)();
        DocumentLimits bounded{};
        bounded.maximum_active_objects =
            static_cast<std::uint32_t>(source.objects.size() - 1);
        result.malformed_rejected =
            !SceneDocument::from_snapshot(duplicate) &&
            !SceneDocument::from_snapshot(nonFinite) &&
            !SceneDocument::from_snapshot(source, bounded);
        return result;
    }
}
