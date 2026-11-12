/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module asset.gui_artifact;

export namespace epochengine::asset::gui
{
    inline constexpr std::uint32_t artifact_schema_version = 1u;
    inline constexpr std::uint32_t invalid_widget_index =
        (std::numeric_limits<std::uint32_t>::max)();

    struct ContentHash final
    {
        std::array<std::uint8_t, 32u> bytes{};

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return bytes == std::array<std::uint8_t, 32u>{};
        }

        friend constexpr bool operator==(
            const ContentHash& left,
            const ContentHash& right) noexcept
        {
            return left.bytes == right.bytes;
        }
        friend constexpr auto operator<=>(
            const ContentHash&,
            const ContentHash&) noexcept = default;
    };

    struct SourceRevision final
    {
        ContentHash content{};
        std::uint64_t sequence{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return !content.empty() && sequence != 0u;
        }

        friend constexpr bool operator==(
            const SourceRevision& left,
            const SourceRevision& right) noexcept
        {
            return left.content.bytes == right.content.bytes
                && left.sequence == right.sequence;
        }
        friend constexpr auto operator<=>(
            const SourceRevision&,
            const SourceRevision&) noexcept = default;
    };

    struct WidgetId final
    {
        std::uint64_t value{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return value != 0u;
        }

        friend constexpr bool operator==(
            WidgetId left, WidgetId right) noexcept
        {
            return left.value == right.value;
        }
        friend constexpr auto operator<=>(WidgetId, WidgetId) noexcept = default;
    };

    enum class WidgetKind : std::uint8_t
    {
        canvas,
        panel,
        button,
        text,
        image,
        image_button,
        text_input,
        slider,
        scroll_area,
        tab_set,
        tab_page
    };

    enum class LayoutFlow : std::uint8_t
    {
        absolute,
        horizontal,
        vertical,
        overlay
    };

    enum class SizeRule : std::uint8_t
    {
        fixed,
        content,
        fill
    };

    struct Insets final
    {
        float left{};
        float top{};
        float right{};
        float bottom{};

        friend constexpr bool operator==(
            Insets left, Insets right) noexcept
        {
            return left.left == right.left && left.top == right.top
                && left.right == right.right && left.bottom == right.bottom;
        }
    };

    struct Layout final
    {
        LayoutFlow flow{LayoutFlow::absolute};
        SizeRule width_rule{SizeRule::fixed};
        SizeRule height_rule{SizeRule::fixed};
        float x{};
        float y{};
        float width{100.0f};
        float height{30.0f};
        float minimum_width{};
        float minimum_height{};
        float maximum_width{16'384.0f};
        float maximum_height{16'384.0f};
        float flex_grow{};
        float spacing{};
        Insets padding{};
        bool clip_children{};

        friend constexpr bool operator==(
            const Layout& left, const Layout& right) noexcept
        {
            return left.flow == right.flow
                && left.width_rule == right.width_rule
                && left.height_rule == right.height_rule
                && left.x == right.x && left.y == right.y
                && left.width == right.width && left.height == right.height
                && left.minimum_width == right.minimum_width
                && left.minimum_height == right.minimum_height
                && left.maximum_width == right.maximum_width
                && left.maximum_height == right.maximum_height
                && left.flex_grow == right.flex_grow
                && left.spacing == right.spacing
                && left.padding == right.padding
                && left.clip_children == right.clip_children;
        }
    };

    struct Color final
    {
        float red{};
        float green{};
        float blue{};
        float alpha{1.0f};

        friend constexpr bool operator==(
            Color left, Color right) noexcept
        {
            return left.red == right.red && left.green == right.green
                && left.blue == right.blue && left.alpha == right.alpha;
        }
    };

    struct Style final
    {
        Color background{0.16f, 0.17f, 0.19f, 1.0f};
        Color foreground{0.94f, 0.94f, 0.94f, 1.0f};
        Color border{0.32f, 0.34f, 0.38f, 1.0f};
        Color accent{0.20f, 0.56f, 0.86f, 1.0f};
        float border_width{1.0f};
        float corner_radius{2.0f};
        float opacity{1.0f};
        std::string style_class{};

        friend bool operator==(
            const Style& left, const Style& right) noexcept
        {
            return left.background == right.background
                && left.foreground == right.foreground
                && left.border == right.border && left.accent == right.accent
                && left.border_width == right.border_width
                && left.corner_radius == right.corner_radius
                && left.opacity == right.opacity
                && left.style_class == right.style_class;
        }
    };

    struct CompiledWidget final
    {
        WidgetId id{};
        std::uint32_t parent{invalid_widget_index};
        std::vector<std::uint32_t> children{};
        WidgetKind kind{WidgetKind::panel};
        std::string name{};
        Layout layout{};
        Style style{};
        bool enabled{true};
        bool visible{true};
        bool focusable{};
        bool accepts_pointer{};
        std::int32_t tab_index{-1};
        std::string action{};
        std::string tooltip{};
        std::string primary_text{};
        std::string secondary_text{};
        std::string asset_path{};
        std::uint32_t maximum_length{4'096u};
        std::uint32_t selected_child{invalid_widget_index};
        double minimum{};
        double maximum{1.0};
        double value{};
        double step{0.01};
        float horizontal_offset{};
        float vertical_offset{};
        bool wrap{true};
        bool preserve_aspect{true};
        bool multiline{};
        bool read_only{};
        bool allow_horizontal{};
        bool allow_vertical{true};
        bool closeable{};

        friend bool operator==(
            const CompiledWidget& left,
            const CompiledWidget& right) noexcept
        {
            return left.id == right.id && left.parent == right.parent
                && left.children == right.children && left.kind == right.kind
                && left.name == right.name && left.layout == right.layout
                && left.style == right.style && left.enabled == right.enabled
                && left.visible == right.visible
                && left.focusable == right.focusable
                && left.accepts_pointer == right.accepts_pointer
                && left.tab_index == right.tab_index
                && left.action == right.action && left.tooltip == right.tooltip
                && left.primary_text == right.primary_text
                && left.secondary_text == right.secondary_text
                && left.asset_path == right.asset_path
                && left.maximum_length == right.maximum_length
                && left.selected_child == right.selected_child
                && left.minimum == right.minimum && left.maximum == right.maximum
                && left.value == right.value && left.step == right.step
                && left.horizontal_offset == right.horizontal_offset
                && left.vertical_offset == right.vertical_offset
                && left.wrap == right.wrap
                && left.preserve_aspect == right.preserve_aspect
                && left.multiline == right.multiline
                && left.read_only == right.read_only
                && left.allow_horizontal == right.allow_horizontal
                && left.allow_vertical == right.allow_vertical
                && left.closeable == right.closeable;
        }
    };

    struct ArtifactIdentity final
    {
        ContentHash key{};
        SourceRevision source_revision{};
        std::uint32_t schema_version{artifact_schema_version};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return !key.empty() && static_cast<bool>(source_revision)
                && schema_version == artifact_schema_version;
        }

        friend constexpr bool operator==(
            const ArtifactIdentity& left,
            const ArtifactIdentity& right) noexcept
        {
            return left.key.bytes == right.key.bytes
                && left.source_revision.content.bytes
                    == right.source_revision.content.bytes
                && left.source_revision.sequence
                    == right.source_revision.sequence
                && left.schema_version == right.schema_version;
        }
        friend constexpr auto operator<=>(
            const ArtifactIdentity&,
            const ArtifactIdentity&) noexcept = default;
    };

    struct CompiledGuiArtifact final
    {
        ArtifactIdentity identity{};
        std::string name{};
        float virtual_width{1'280.0f};
        float virtual_height{720.0f};
        bool pixel_snap{true};
        std::uint32_t root{invalid_widget_index};
        std::vector<CompiledWidget> widgets{};

        friend bool operator==(
            const CompiledGuiArtifact& left,
            const CompiledGuiArtifact& right) noexcept
        {
            return left.identity == right.identity && left.name == right.name
                && left.virtual_width == right.virtual_width
                && left.virtual_height == right.virtual_height
                && left.pixel_snap == right.pixel_snap
                && left.root == right.root && left.widgets == right.widgets;
        }
    };

    struct ArtifactLimits final
    {
        std::uint32_t maximum_widgets{4'096u};
        std::uint32_t maximum_children_per_widget{1'024u};
        std::uint32_t maximum_hierarchy_depth{256u};
        std::uint32_t maximum_name_bytes{256u};
        std::uint32_t maximum_text_bytes{65'536u};
        std::uint32_t maximum_path_bytes{1'024u};
        std::uint32_t maximum_style_class_bytes{128u};
        std::uint32_t maximum_action_bytes{256u};
        std::uint32_t maximum_tooltip_bytes{4'096u};
        std::uint64_t maximum_total_string_bytes{64ull * 1024ull * 1024ull};
        std::uint64_t maximum_serialized_bytes{96ull * 1024ull * 1024ull};
        float maximum_layout_extent{1'000'000.0f};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_widgets != 0u
                && maximum_children_per_widget != 0u
                && maximum_hierarchy_depth != 0u
                && maximum_name_bytes != 0u && maximum_text_bytes != 0u
                && maximum_path_bytes != 0u
                && maximum_style_class_bytes != 0u
                && maximum_action_bytes != 0u
                && maximum_tooltip_bytes != 0u
                && maximum_total_string_bytes != 0u
                && maximum_serialized_bytes != 0u
                && maximum_layout_extent > 0.0f;
        }
    };

    enum class ArtifactCode : std::uint8_t
    {
        ready,
        invalid_limits,
        invalid_identity,
        invalid_artifact,
        invalid_widget,
        invalid_layout,
        invalid_style,
        invalid_content,
        invalid_path,
        duplicate_identity,
        dangling_reference,
        malformed_hierarchy,
        content_hash_mismatch,
        capacity_exceeded,
        unsupported_schema,
        malformed_payload,
        integrity_failure,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view artifact_code_name(
        ArtifactCode code) noexcept
    {
        switch (code)
        {
        case ArtifactCode::ready: return "ready";
        case ArtifactCode::invalid_limits: return "invalid_limits";
        case ArtifactCode::invalid_identity: return "invalid_identity";
        case ArtifactCode::invalid_artifact: return "invalid_artifact";
        case ArtifactCode::invalid_widget: return "invalid_widget";
        case ArtifactCode::invalid_layout: return "invalid_layout";
        case ArtifactCode::invalid_style: return "invalid_style";
        case ArtifactCode::invalid_content: return "invalid_content";
        case ArtifactCode::invalid_path: return "invalid_path";
        case ArtifactCode::duplicate_identity: return "duplicate_identity";
        case ArtifactCode::dangling_reference: return "dangling_reference";
        case ArtifactCode::malformed_hierarchy: return "malformed_hierarchy";
        case ArtifactCode::content_hash_mismatch:
            return "content_hash_mismatch";
        case ArtifactCode::capacity_exceeded: return "capacity_exceeded";
        case ArtifactCode::unsupported_schema: return "unsupported_schema";
        case ArtifactCode::malformed_payload: return "malformed_payload";
        case ArtifactCode::integrity_failure: return "integrity_failure";
        case ArtifactCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct ValidationResult final
    {
        ArtifactCode code{ArtifactCode::invalid_artifact};
        std::uint32_t hierarchy_depth{};
        std::uint64_t total_string_bytes{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ArtifactCode::ready;
        }
    };

    struct SerializationResult final
    {
        ArtifactCode code{ArtifactCode::malformed_payload};
        std::vector<std::byte> bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ArtifactCode::ready && !bytes.empty();
        }
    };

    struct DeserializationResult final
    {
        ArtifactCode code{ArtifactCode::malformed_payload};
        CompiledGuiArtifact artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ArtifactCode::ready;
        }
    };

    [[nodiscard]] bool valid_logical_asset_path(std::string_view path) noexcept;
    [[nodiscard]] ContentHash payload_content(
        const CompiledGuiArtifact& artifact) noexcept;
    [[nodiscard]] ValidationResult validate(
        const CompiledGuiArtifact& artifact,
        const ArtifactLimits& limits = {}) noexcept;
    [[nodiscard]] SerializationResult serialize(
        const CompiledGuiArtifact& artifact,
        const ArtifactLimits& limits = {}) noexcept;
    [[nodiscard]] DeserializationResult deserialize(
        std::span<const std::byte> bytes,
        const ArtifactLimits& limits = {}) noexcept;
}
