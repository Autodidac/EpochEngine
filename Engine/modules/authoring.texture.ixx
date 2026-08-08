/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

export module authoring.texture;
export import asset.texture_artifact;

export namespace epochengine::authoring::texture
{
    using asset::texture::ArtifactCompilationStatus;
    using asset::texture::ArtifactFormat;
    using asset::texture::ColorSpace;
    using asset::texture::CompiledTextureArtifact;
    using asset::texture::CompiledTextureArtifactIdentity;
    using asset::texture::CompiledTextureMip;
    using asset::texture::ContentHash;
    using asset::texture::DocumentRevision;
    using asset::texture::MipmapPolicy;
    using asset::texture::TextureCompileProfile;
    using asset::texture::build_compiled_texture_artifact_identity;
    using asset::texture::compiled_texture_payload_content;
    struct DocumentHandle final
    {
        static constexpr std::uint32_t invalid_index =
            (std::numeric_limits<std::uint32_t>::max)();

        std::uint32_t index{ invalid_index };
        std::uint32_t generation{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return index != invalid_index && generation != 0;
        }

        [[nodiscard]] friend constexpr bool operator==(
            const DocumentHandle&,
            const DocumentHandle&) noexcept = default;
    };

    struct LayerHandle final
    {
        static constexpr std::uint32_t invalid_index =
            (std::numeric_limits<std::uint32_t>::max)();

        std::uint32_t index{ invalid_index };
        std::uint32_t generation{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return index != invalid_index && generation != 0;
        }

        [[nodiscard]] friend constexpr bool operator==(
            const LayerHandle&,
            const LayerHandle&) noexcept = default;
    };

    struct CheckpointHandle final
    {
        std::uint64_t value{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return value != 0;
        }

        [[nodiscard]] friend constexpr bool operator==(
            const CheckpointHandle&,
            const CheckpointHandle&) noexcept = default;
    };

    struct OperationId final
    {
        std::uint64_t value{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return value != 0;
        }

        [[nodiscard]] friend constexpr bool operator==(
            const OperationId&,
            const OperationId&) noexcept = default;
    };

    struct BranchIdentity final
    {
        std::uint64_t timeline{ 1 };
        std::uint64_t branch{ 1 };

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return timeline != 0 && branch != 0;
        }

        [[nodiscard]] friend constexpr bool operator==(
            const BranchIdentity&,
            const BranchIdentity&) noexcept = default;
    };

    struct TemporalPoint final
    {
        BranchIdentity stream{};
        std::int64_t tick{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return static_cast<bool>(stream);
        }

        [[nodiscard]] friend constexpr bool operator==(
            const TemporalPoint&,
            const TemporalPoint&) noexcept = default;
    };

    enum class ResultCode : std::uint8_t
    {
        success,
        unchanged,
        invalid_document,
        invalid_descriptor,
        invalid_handle,
        stale_handle,
        invalid_temporal_point,
        temporal_stream_mismatch,
        temporal_regression,
        invalid_operation,
        unsupported_brush,
        layer_limit_exceeded,
        tile_limit_exceeded,
        canonical_memory_budget_exceeded,
        history_operation_limit_exceeded,
        history_memory_budget_exceeded,
        checkpoint_limit_exceeded,
        checkpoint_memory_budget_exceeded,
        stroke_sample_limit_exceeded,
        stroke_tile_limit_exceeded,
        nothing_to_undo,
        nothing_to_redo,
        checkpoint_not_found,
        invalid_compile_profile,
        residency_unavailable,
        arithmetic_overflow
    };

    enum class PixelFormat : std::uint8_t
    {
        rgba8_unorm,
        rgba8_srgb
    };

    struct CanvasDescriptor final
    {
        std::uint32_t width{ 1024 };
        std::uint32_t height{ 1024 };
        std::uint16_t tile_extent{ 128 };
        std::uint8_t mip_count{ 1 };
        PixelFormat format{ PixelFormat::rgba8_srgb };
        ColorSpace color_space{ ColorSpace::srgb };
        std::uint32_t schema_version{ 1 };

        [[nodiscard]] friend constexpr bool operator==(
            const CanvasDescriptor&,
            const CanvasDescriptor&) noexcept = default;
    };

    enum class HistoryMode : std::uint8_t
    {
        disabled,
        semantic_operations,
        checkpointed_operations
    };

    struct HistoryPolicy final
    {
        HistoryMode mode{ HistoryMode::checkpointed_operations };
        std::uint32_t checkpoint_interval_operations{ 32 };
        std::uint32_t maximum_operations{ 4096 };
        std::uint32_t maximum_checkpoints{ 16 };
        std::uint64_t maximum_operation_bytes{ 64ull * 1024ull * 1024ull };
        std::uint64_t maximum_checkpoint_bytes{ 256ull * 1024ull * 1024ull };
        bool allow_branching{ true };

        [[nodiscard]] friend constexpr bool operator==(
            const HistoryPolicy&,
            const HistoryPolicy&) noexcept = default;
    };

    struct DocumentLimits final
    {
        std::uint32_t maximum_dimension{ 65'536 };
        std::uint16_t maximum_tile_extent{ 512 };
        std::uint8_t maximum_mip_count{ 17 };
        std::uint32_t maximum_layers{ 256 };
        std::uint64_t maximum_sparse_tiles{ 1'000'000 };
        std::uint64_t maximum_canonical_tile_bytes{
            2ull * 1024ull * 1024ull * 1024ull
        };
        std::uint32_t maximum_stroke_samples{ 16'384 };
        std::uint32_t maximum_tiles_per_stroke{ 65'536 };
        std::uint32_t maximum_layer_name_bytes{ 256 };
        std::uint32_t maximum_brush_radius_subpixels{ 16'777'216 };

        [[nodiscard]] friend constexpr bool operator==(
            const DocumentLimits&,
            const DocumentLimits&) noexcept = default;
    };

    struct PixelRgba8 final
    {
        std::uint8_t r{};
        std::uint8_t g{};
        std::uint8_t b{};
        std::uint8_t a{};

        [[nodiscard]] friend constexpr bool operator==(
            const PixelRgba8&,
            const PixelRgba8&) noexcept = default;
    };

    struct TileCoordinate final
    {
        std::uint8_t mip{};
        std::uint32_t x{};
        std::uint32_t y{};

        [[nodiscard]] friend constexpr bool operator==(
            const TileCoordinate&,
            const TileCoordinate&) noexcept = default;
    };

    struct TileSnapshot final
    {
        TileCoordinate coordinate{};
        std::uint16_t width{};
        std::uint16_t height{};
        std::vector<std::byte> texels{};
        ContentHash content{};

        [[nodiscard]] bool implicit_clear() const noexcept
        {
            return texels.empty();
        }
    };

    enum class BlendMode : std::uint8_t
    {
        normal,
        multiply,
        screen,
        add,
        subtract
    };

    struct LayerDescriptor final
    {
        std::string name{ "Layer" };
        BlendMode blend{ BlendMode::normal };
        std::uint16_t opacity{ 65'535 };
        bool visible{ true };
        bool locked{};

        [[nodiscard]] friend bool operator==(
            const LayerDescriptor&,
            const LayerDescriptor&) noexcept = default;
    };

    struct LayerInfo final
    {
        LayerHandle handle{};
        LayerDescriptor descriptor{};
        std::uint32_t order{};
        std::uint64_t stored_tile_count{};
        std::uint64_t stored_bytes{};
        ContentHash content{};
    };

    enum class BrushProgram : std::uint8_t
    {
        round_stamp_v1
    };

    struct StrokeSample final
    {
        std::int64_t x_subpixels{};
        std::int64_t y_subpixels{};
        std::uint16_t pressure{ 65'535 };
        std::int16_t tilt_x{};
        std::int16_t tilt_y{};

        [[nodiscard]] friend constexpr bool operator==(
            const StrokeSample&,
            const StrokeSample&) noexcept = default;
    };

    struct StrokeDescriptor final
    {
        LayerHandle target{};
        std::uint8_t mip{};
        BrushProgram program{ BrushProgram::round_stamp_v1 };
        std::uint32_t algorithm_version{ 1 };
        PixelRgba8 color{ 255, 255, 255, 255 };
        std::uint32_t radius_subpixels{ 256 };
        std::uint16_t opacity{ 65'535 };
        std::uint16_t hardness{ 65'535 };
        std::uint8_t channel_mask{ 0x0f };
        std::uint64_t seed{};
        std::vector<StrokeSample> samples{};

        [[nodiscard]] friend bool operator==(
            const StrokeDescriptor&,
            const StrokeDescriptor&) noexcept = default;
    };

    struct LayerCreatedOperation final
    {
        LayerHandle layer{};
        LayerDescriptor descriptor{};
        std::uint32_t insertion_index{};
    };

    struct LayerRemovedOperation final
    {
        LayerHandle layer{};
        std::uint32_t previous_index{};
        ContentHash removed_content{};
    };

    struct LayerMovedOperation final
    {
        LayerHandle layer{};
        std::uint32_t previous_index{};
        std::uint32_t next_index{};
    };

    struct LayerPropertiesChangedOperation final
    {
        LayerHandle layer{};
        LayerDescriptor before{};
        LayerDescriptor after{};
    };

    struct TextureStrokeAppliedOperation final
    {
        StrokeDescriptor stroke{};
        std::vector<TileCoordinate> affected_tiles{};
    };

    using TextureOperationPayload = std::variant<
        LayerCreatedOperation,
        LayerRemovedOperation,
        LayerMovedOperation,
        LayerPropertiesChangedOperation,
        TextureStrokeAppliedOperation>;

    struct OperationRecord final
    {
        OperationId id{};
        TemporalPoint at{};
        DocumentRevision before{};
        DocumentRevision after{};
        TextureOperationPayload payload{};
        std::uint64_t retained_bytes{};
    };

    struct OperationSummary final
    {
        OperationId id{};
        TemporalPoint at{};
        std::string_view kind{};
        DocumentRevision before{};
        DocumentRevision after{};
        std::uint64_t retained_bytes{};
        bool currently_applied{};
    };

    struct MutationResult final
    {
        ResultCode code{ ResultCode::invalid_operation };
        DocumentRevision revision{};
        OperationId operation{};
        LayerHandle layer{};
        std::uint64_t affected_tiles{};
        bool automatic_checkpoint_captured{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResultCode::success
                || code == ResultCode::unchanged;
        }
    };

    struct CheckpointInfo final
    {
        CheckpointHandle handle{};
        DocumentRevision revision{};
        std::uint64_t operation_cursor{};
        std::uint64_t retained_bytes{};
        TemporalPoint at{};
    };

    struct CheckpointResult final
    {
        ResultCode code{ ResultCode::checkpoint_not_found };
        CheckpointInfo checkpoint{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    struct DocumentMetrics final
    {
        std::uint64_t active_layers{};
        std::uint64_t allocated_layer_slots{};
        std::uint64_t sparse_tile_count{};
        std::uint64_t canonical_tile_bytes{};
        std::uint64_t operation_count{};
        std::uint64_t applied_operation_count{};
        std::uint64_t retained_operation_bytes{};
        std::uint64_t checkpoint_count{};
        std::uint64_t retained_checkpoint_bytes{};
        std::uint64_t automatic_checkpoints_skipped{};
        std::uint64_t revision_sequence{};
    };

    struct ArtifactCompilationLimits final
    {
        std::uint64_t maximum_output_bytes{
            512ull * 1024ull * 1024ull
        };
        std::uint8_t maximum_mip_count{ 17 };
    };

    enum class PhysicalResidencyKind : std::uint8_t
    {
        standalone_image,
        atlas_region,
        bindless_image,
        sparse_pages,
        hybrid_sparse_tail
    };

    struct PhysicalResidencyCapabilities final
    {
        bool standalone_images{ true };
        bool atlas_regions{};
        bool bindless_images{};
        bool sparse_images{};
        std::uint32_t maximum_texture_dimension{ 16'384 };
        std::uint32_t maximum_atlas_dimension{ 8'192 };
        std::uint32_t sparse_page_extent{ 128 };
        std::uint32_t available_bindless_descriptors{};
        std::uint64_t maximum_resident_bytes{
            512ull * 1024ull * 1024ull
        };
    };

    struct PhysicalResidencyPolicy final
    {
        PhysicalResidencyKind preferred{
            PhysicalResidencyKind::standalone_image
        };
        bool allow_fallback{ true };
        bool allow_atlas_for_small_textures{};
        std::uint32_t atlas_small_texture_limit{ 2048 };
        std::uint8_t minimum_streamed_mip_count{ 2 };
        std::uint64_t target_resident_bytes{
            128ull * 1024ull * 1024ull
        };
    };

    struct ResidencyTier final
    {
        PhysicalResidencyKind kind{
            PhysicalResidencyKind::standalone_image
        };
        std::uint8_t first_mip{};
        std::uint8_t mip_count{};
        std::uint64_t logical_tile_count{};
        std::uint64_t estimated_resident_bytes{};
        std::uint32_t descriptor_count{};
    };

    enum class ResidencyPlanStatus : std::uint8_t
    {
        ready_for_allocation,
        invalid_artifact,
        invalid_capabilities,
        no_compatible_strategy,
        budget_exceeded,
        arithmetic_overflow
    };

    struct PhysicalResidencyPlan final
    {
        ResidencyPlanStatus status{
            ResidencyPlanStatus::no_compatible_strategy
        };
        CompiledTextureArtifactIdentity artifact{};
        ContentHash cache_key{};
        std::vector<ResidencyTier> tiers{};
        std::uint64_t estimated_resident_bytes{};
        std::uint32_t descriptor_count{};
        bool disposable{ true };
        bool requires_runtime_allocation{ true };

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return status == ResidencyPlanStatus::ready_for_allocation;
        }
    };

    class TextureDocument final
    {
    public:
        TextureDocument(
            DocumentHandle handle,
            BranchIdentity branch,
            CanvasDescriptor descriptor = {},
            HistoryPolicy history = {},
            DocumentLimits limits = {}) noexcept;
        ~TextureDocument();

        TextureDocument(TextureDocument&&) noexcept;
        TextureDocument& operator=(TextureDocument&&) noexcept;

        TextureDocument(const TextureDocument&) = delete;
        TextureDocument& operator=(const TextureDocument&) = delete;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] DocumentHandle handle() const noexcept;
        [[nodiscard]] BranchIdentity branch() const noexcept;
        [[nodiscard]] const CanvasDescriptor& descriptor() const noexcept;
        [[nodiscard]] const HistoryPolicy& history_policy() const noexcept;
        [[nodiscard]] const DocumentLimits& limits() const noexcept;
        [[nodiscard]] DocumentRevision revision() const noexcept;
        [[nodiscard]] TemporalPoint current_time() const noexcept;
        [[nodiscard]] DocumentMetrics metrics() const noexcept;

        [[nodiscard]] std::vector<LayerInfo> layers() const;
        [[nodiscard]] std::optional<LayerInfo> layer(
            LayerHandle handle) const;
        [[nodiscard]] std::optional<TileSnapshot> tile(
            LayerHandle layer,
            TileCoordinate coordinate) const;
        [[nodiscard]] PixelRgba8 pixel(
            LayerHandle layer,
            std::uint8_t mip,
            std::uint32_t x,
            std::uint32_t y) const noexcept;

        [[nodiscard]] MutationResult create_layer(
            LayerDescriptor descriptor,
            std::uint32_t insertion_index,
            TemporalPoint at);
        [[nodiscard]] MutationResult remove_layer(
            LayerHandle layer,
            TemporalPoint at);
        [[nodiscard]] MutationResult move_layer(
            LayerHandle layer,
            std::uint32_t insertion_index,
            TemporalPoint at);
        [[nodiscard]] MutationResult set_layer_properties(
            LayerHandle layer,
            LayerDescriptor descriptor,
            TemporalPoint at);
        [[nodiscard]] MutationResult apply_stroke(
            StrokeDescriptor stroke,
            TemporalPoint at);

        [[nodiscard]] bool can_undo() const noexcept;
        [[nodiscard]] bool can_redo() const noexcept;
        [[nodiscard]] MutationResult undo(TemporalPoint at);
        [[nodiscard]] MutationResult redo(TemporalPoint at);

        [[nodiscard]] CheckpointResult capture_checkpoint(
            TemporalPoint at);
        [[nodiscard]] MutationResult restore_checkpoint(
            CheckpointHandle checkpoint,
            TemporalPoint at);
        [[nodiscard]] std::vector<CheckpointInfo> checkpoints() const;

        [[nodiscard]] std::vector<OperationSummary>
            operation_summaries() const;
        [[nodiscard]] std::vector<OperationRecord>
            operation_records() const;

        [[nodiscard]] CompiledTextureArtifactIdentity
            compiled_artifact_identity(
                TextureCompileProfile profile) const noexcept;
        [[nodiscard]] CompiledTextureArtifact compile_artifact(
            TextureCompileProfile profile,
            ArtifactCompilationLimits limits = {}) const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_{};
    };

    [[nodiscard]] PhysicalResidencyPlan plan_physical_residency(
        const CompiledTextureArtifactIdentity& artifact,
        const PhysicalResidencyCapabilities& capabilities,
        const PhysicalResidencyPolicy& policy) noexcept;

    [[nodiscard]] const char* result_code_name(ResultCode code) noexcept;
    [[nodiscard]] const char* residency_kind_name(
        PhysicalResidencyKind kind) noexcept;
    [[nodiscard]] const char* residency_plan_status_name(
        ResidencyPlanStatus status) noexcept;
}
