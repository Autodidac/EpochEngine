/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

module asset.gui_artifact;

import core.sha256;

namespace epochengine::asset::gui
{
    namespace
    {
        constexpr std::array<std::uint8_t, 8u> magic{
            'E', 'P', 'G', 'U', 'I', 'A', '0', '1'};
        constexpr std::size_t digest_bytes = 32u;

        class Writer final
        {
        public:
            explicit Writer(std::uint64_t maximumBytes) noexcept
                : maximum_bytes_(maximumBytes)
            {
            }

            [[nodiscard]] bool valid() const noexcept { return valid_; }
            [[nodiscard]] std::span<const std::byte> bytes() const noexcept
            {
                return bytes_;
            }
            [[nodiscard]] std::vector<std::byte> take() noexcept
            {
                return std::move(bytes_);
            }

            void raw(std::span<const std::uint8_t> values)
            {
                if (!reserve(values.size()))
                    return;
                for (const auto value : values)
                    bytes_.push_back(static_cast<std::byte>(value));
            }

            void u8(std::uint8_t value)
            {
                if (reserve(1u))
                    bytes_.push_back(static_cast<std::byte>(value));
            }
            void boolean(bool value) { u8(value ? 1u : 0u); }
            void u32(std::uint32_t value) { integer(value); }
            void i32(std::int32_t value)
            {
                integer(std::bit_cast<std::uint32_t>(value));
            }
            void u64(std::uint64_t value) { integer(value); }
            void f32(float value) { u32(std::bit_cast<std::uint32_t>(value)); }
            void f64(double value) { u64(std::bit_cast<std::uint64_t>(value)); }

            void string(std::string_view value)
            {
                if (value.size()
                    > (std::numeric_limits<std::uint32_t>::max)())
                {
                    valid_ = false;
                    return;
                }
                u32(static_cast<std::uint32_t>(value.size()));
                if (!reserve(value.size()))
                    return;
                for (const unsigned char character : value)
                    bytes_.push_back(static_cast<std::byte>(character));
            }

        private:
            template<std::unsigned_integral Integer>
            void integer(Integer value)
            {
                if (!reserve(sizeof(Integer)))
                    return;
                for (std::size_t index = 0u; index < sizeof(Integer); ++index)
                {
                    bytes_.push_back(static_cast<std::byte>(
                        (value >> (index * 8u)) & Integer{0xffu}));
                }
            }

            [[nodiscard]] bool reserve(std::size_t count)
            {
                if (!valid_ || count > maximum_bytes_
                    || bytes_.size() > maximum_bytes_ - count)
                {
                    valid_ = false;
                    return false;
                }
                bytes_.reserve(bytes_.size() + count);
                return true;
            }

            std::vector<std::byte> bytes_{};
            std::uint64_t maximum_bytes_{};
            bool valid_{true};
        };

        class Reader final
        {
        public:
            Reader(
                std::span<const std::byte> bytes,
                const ArtifactLimits& limits) noexcept
                : bytes_(bytes), limits_(limits)
            {
            }

            [[nodiscard]] bool valid() const noexcept { return valid_; }
            [[nodiscard]] bool finished() const noexcept
            {
                return valid_ && cursor_ == bytes_.size();
            }

            [[nodiscard]] std::uint8_t u8()
            {
                if (!require(1u))
                    return 0u;
                return std::to_integer<std::uint8_t>(bytes_[cursor_++]);
            }
            [[nodiscard]] bool boolean()
            {
                const auto value = u8();
                if (value > 1u)
                    valid_ = false;
                return value != 0u;
            }
            [[nodiscard]] std::uint32_t u32()
            {
                return integer<std::uint32_t>();
            }
            [[nodiscard]] std::int32_t i32()
            {
                return std::bit_cast<std::int32_t>(u32());
            }
            [[nodiscard]] std::uint64_t u64()
            {
                return integer<std::uint64_t>();
            }
            [[nodiscard]] float f32()
            {
                return std::bit_cast<float>(u32());
            }
            [[nodiscard]] double f64()
            {
                return std::bit_cast<double>(u64());
            }

            [[nodiscard]] ContentHash hash()
            {
                ContentHash result{};
                for (auto& value : result.bytes)
                    value = u8();
                return result;
            }

            [[nodiscard]] std::string string(std::uint32_t maximumBytes)
            {
                const auto count = u32();
                if (!valid_ || count > maximumBytes
                    || string_bytes_ > limits_.maximum_total_string_bytes
                    || count > limits_.maximum_total_string_bytes - string_bytes_
                    || !require(count))
                {
                    valid_ = false;
                    return {};
                }
                std::string result(count, '\0');
                for (std::uint32_t index = 0u; index < count; ++index)
                {
                    result[index] = static_cast<char>(
                        std::to_integer<std::uint8_t>(bytes_[cursor_ + index]));
                }
                cursor_ += count;
                string_bytes_ += count;
                return result;
            }

        private:
            template<std::unsigned_integral Integer>
            [[nodiscard]] Integer integer()
            {
                if (!require(sizeof(Integer)))
                    return {};
                Integer result{};
                for (std::size_t index = 0u; index < sizeof(Integer); ++index)
                {
                    result |= static_cast<Integer>(
                        std::to_integer<std::uint8_t>(bytes_[cursor_++]))
                        << (index * 8u);
                }
                return result;
            }

            [[nodiscard]] bool require(std::size_t count)
            {
                if (!valid_ || count > bytes_.size()
                    || cursor_ > bytes_.size() - count)
                {
                    valid_ = false;
                    return false;
                }
                return true;
            }

            std::span<const std::byte> bytes_{};
            const ArtifactLimits& limits_;
            std::size_t cursor_{};
            std::uint64_t string_bytes_{};
            bool valid_{true};
        };

        [[nodiscard]] bool valid_utf8(std::string_view text) noexcept
        {
            std::size_t index{};
            while (index < text.size())
            {
                const auto first = static_cast<std::uint8_t>(text[index]);
                if (first == 0u)
                    return false;
                if (first < 0x80u)
                {
                    ++index;
                    continue;
                }
                std::size_t count{};
                std::uint32_t codepoint{};
                if ((first & 0xe0u) == 0xc0u)
                {
                    count = 2u;
                    codepoint = first & 0x1fu;
                }
                else if ((first & 0xf0u) == 0xe0u)
                {
                    count = 3u;
                    codepoint = first & 0x0fu;
                }
                else if ((first & 0xf8u) == 0xf0u)
                {
                    count = 4u;
                    codepoint = first & 0x07u;
                }
                else
                {
                    return false;
                }
                if (index + count > text.size())
                    return false;
                for (std::size_t offset = 1u; offset < count; ++offset)
                {
                    const auto next = static_cast<std::uint8_t>(text[index + offset]);
                    if ((next & 0xc0u) != 0x80u)
                        return false;
                    codepoint = (codepoint << 6u) | (next & 0x3fu);
                }
                if ((count == 2u && codepoint < 0x80u)
                    || (count == 3u && codepoint < 0x800u)
                    || (count == 4u && codepoint < 0x1'0000u)
                    || codepoint > 0x10'ffffu
                    || (codepoint >= 0xd800u && codepoint <= 0xdfffu))
                {
                    return false;
                }
                index += count;
            }
            return true;
        }

        [[nodiscard]] constexpr bool valid_kind(WidgetKind value) noexcept
        {
            return value >= WidgetKind::canvas && value <= WidgetKind::tab_page;
        }
        [[nodiscard]] constexpr bool valid_flow(LayoutFlow value) noexcept
        {
            return value >= LayoutFlow::absolute && value <= LayoutFlow::overlay;
        }
        [[nodiscard]] constexpr bool valid_size_rule(SizeRule value) noexcept
        {
            return value >= SizeRule::fixed && value <= SizeRule::fill;
        }
        [[nodiscard]] bool finite(float value) noexcept
        {
            return std::isfinite(value);
        }
        [[nodiscard]] bool finite(double value) noexcept
        {
            return std::isfinite(value);
        }
        [[nodiscard]] bool unit(float value) noexcept
        {
            return finite(value) && value >= 0.0f && value <= 1.0f;
        }
        [[nodiscard]] bool valid_color(const Color& color) noexcept
        {
            return unit(color.red) && unit(color.green)
                && unit(color.blue) && unit(color.alpha);
        }

        [[nodiscard]] bool valid_layout(
            const Layout& layout,
            const ArtifactLimits& limits) noexcept
        {
            const float extent = limits.maximum_layout_extent;
            return valid_flow(layout.flow)
                && valid_size_rule(layout.width_rule)
                && valid_size_rule(layout.height_rule)
                && finite(layout.x) && finite(layout.y)
                && std::abs(layout.x) <= extent && std::abs(layout.y) <= extent
                && finite(layout.width) && layout.width >= 0.0f
                && layout.width <= extent
                && finite(layout.height) && layout.height >= 0.0f
                && layout.height <= extent
                && finite(layout.minimum_width) && layout.minimum_width >= 0.0f
                && finite(layout.minimum_height) && layout.minimum_height >= 0.0f
                && finite(layout.maximum_width)
                && layout.maximum_width >= layout.minimum_width
                && layout.maximum_width <= extent
                && finite(layout.maximum_height)
                && layout.maximum_height >= layout.minimum_height
                && layout.maximum_height <= extent
                && finite(layout.flex_grow) && layout.flex_grow >= 0.0f
                && finite(layout.spacing) && layout.spacing >= 0.0f
                && layout.spacing <= extent
                && finite(layout.padding.left) && layout.padding.left >= 0.0f
                && finite(layout.padding.top) && layout.padding.top >= 0.0f
                && finite(layout.padding.right) && layout.padding.right >= 0.0f
                && finite(layout.padding.bottom) && layout.padding.bottom >= 0.0f;
        }

        [[nodiscard]] bool valid_style(
            const Style& style,
            const ArtifactLimits& limits) noexcept
        {
            return valid_color(style.background)
                && valid_color(style.foreground)
                && valid_color(style.border)
                && valid_color(style.accent)
                && finite(style.border_width) && style.border_width >= 0.0f
                && style.border_width <= limits.maximum_layout_extent
                && finite(style.corner_radius) && style.corner_radius >= 0.0f
                && style.corner_radius <= limits.maximum_layout_extent
                && unit(style.opacity)
                && style.style_class.size() <= limits.maximum_style_class_bytes
                && valid_utf8(style.style_class);
        }

        [[nodiscard]] constexpr bool may_have_children(WidgetKind kind) noexcept
        {
            return kind == WidgetKind::canvas || kind == WidgetKind::panel
                || kind == WidgetKind::scroll_area
                || kind == WidgetKind::tab_set || kind == WidgetKind::tab_page;
        }

        void write_layout(Writer& writer, const Layout& value)
        {
            writer.u8(static_cast<std::uint8_t>(value.flow));
            writer.u8(static_cast<std::uint8_t>(value.width_rule));
            writer.u8(static_cast<std::uint8_t>(value.height_rule));
            writer.f32(value.x); writer.f32(value.y);
            writer.f32(value.width); writer.f32(value.height);
            writer.f32(value.minimum_width); writer.f32(value.minimum_height);
            writer.f32(value.maximum_width); writer.f32(value.maximum_height);
            writer.f32(value.flex_grow); writer.f32(value.spacing);
            writer.f32(value.padding.left); writer.f32(value.padding.top);
            writer.f32(value.padding.right); writer.f32(value.padding.bottom);
            writer.boolean(value.clip_children);
        }

        [[nodiscard]] Layout read_layout(Reader& reader)
        {
            Layout value{};
            value.flow = static_cast<LayoutFlow>(reader.u8());
            value.width_rule = static_cast<SizeRule>(reader.u8());
            value.height_rule = static_cast<SizeRule>(reader.u8());
            value.x = reader.f32(); value.y = reader.f32();
            value.width = reader.f32(); value.height = reader.f32();
            value.minimum_width = reader.f32();
            value.minimum_height = reader.f32();
            value.maximum_width = reader.f32();
            value.maximum_height = reader.f32();
            value.flex_grow = reader.f32(); value.spacing = reader.f32();
            value.padding.left = reader.f32(); value.padding.top = reader.f32();
            value.padding.right = reader.f32(); value.padding.bottom = reader.f32();
            value.clip_children = reader.boolean();
            return value;
        }

        void write_color(Writer& writer, const Color& value)
        {
            writer.f32(value.red); writer.f32(value.green);
            writer.f32(value.blue); writer.f32(value.alpha);
        }

        [[nodiscard]] Color read_color(Reader& reader)
        {
            return {reader.f32(), reader.f32(), reader.f32(), reader.f32()};
        }

        void write_style(Writer& writer, const Style& value)
        {
            write_color(writer, value.background);
            write_color(writer, value.foreground);
            write_color(writer, value.border);
            write_color(writer, value.accent);
            writer.f32(value.border_width);
            writer.f32(value.corner_radius);
            writer.f32(value.opacity);
            writer.string(value.style_class);
        }

        [[nodiscard]] Style read_style(Reader& reader, const ArtifactLimits& limits)
        {
            Style value{};
            value.background = read_color(reader);
            value.foreground = read_color(reader);
            value.border = read_color(reader);
            value.accent = read_color(reader);
            value.border_width = reader.f32();
            value.corner_radius = reader.f32();
            value.opacity = reader.f32();
            value.style_class = reader.string(limits.maximum_style_class_bytes);
            return value;
        }

        void write_widget(Writer& writer, const CompiledWidget& value)
        {
            writer.u64(value.id.value);
            writer.u32(value.parent);
            writer.u32(static_cast<std::uint32_t>(value.children.size()));
            for (const auto child : value.children)
                writer.u32(child);
            writer.u8(static_cast<std::uint8_t>(value.kind));
            writer.string(value.name);
            write_layout(writer, value.layout);
            write_style(writer, value.style);
            writer.boolean(value.enabled); writer.boolean(value.visible);
            writer.boolean(value.focusable); writer.boolean(value.accepts_pointer);
            writer.i32(value.tab_index);
            writer.string(value.action); writer.string(value.tooltip);
            writer.string(value.primary_text); writer.string(value.secondary_text);
            writer.string(value.asset_path);
            writer.u32(value.maximum_length); writer.u32(value.selected_child);
            writer.f64(value.minimum); writer.f64(value.maximum);
            writer.f64(value.value); writer.f64(value.step);
            writer.f32(value.horizontal_offset); writer.f32(value.vertical_offset);
            writer.boolean(value.wrap); writer.boolean(value.preserve_aspect);
            writer.boolean(value.multiline); writer.boolean(value.read_only);
            writer.boolean(value.allow_horizontal);
            writer.boolean(value.allow_vertical); writer.boolean(value.closeable);
        }

        [[nodiscard]] CompiledWidget read_widget(
            Reader& reader,
            const ArtifactLimits& limits)
        {
            CompiledWidget value{};
            value.id.value = reader.u64();
            value.parent = reader.u32();
            const auto childCount = reader.u32();
            if (childCount > limits.maximum_children_per_widget)
                return {};
            value.children.reserve(childCount);
            for (std::uint32_t index = 0u; index < childCount; ++index)
                value.children.push_back(reader.u32());
            value.kind = static_cast<WidgetKind>(reader.u8());
            value.name = reader.string(limits.maximum_name_bytes);
            value.layout = read_layout(reader);
            value.style = read_style(reader, limits);
            value.enabled = reader.boolean(); value.visible = reader.boolean();
            value.focusable = reader.boolean();
            value.accepts_pointer = reader.boolean();
            value.tab_index = reader.i32();
            value.action = reader.string(limits.maximum_action_bytes);
            value.tooltip = reader.string(limits.maximum_tooltip_bytes);
            value.primary_text = reader.string(limits.maximum_text_bytes);
            value.secondary_text = reader.string(limits.maximum_text_bytes);
            value.asset_path = reader.string(limits.maximum_path_bytes);
            value.maximum_length = reader.u32();
            value.selected_child = reader.u32();
            value.minimum = reader.f64(); value.maximum = reader.f64();
            value.value = reader.f64(); value.step = reader.f64();
            value.horizontal_offset = reader.f32();
            value.vertical_offset = reader.f32();
            value.wrap = reader.boolean();
            value.preserve_aspect = reader.boolean();
            value.multiline = reader.boolean();
            value.read_only = reader.boolean();
            value.allow_horizontal = reader.boolean();
            value.allow_vertical = reader.boolean();
            value.closeable = reader.boolean();
            return value;
        }

        void write_payload(Writer& writer, const CompiledGuiArtifact& artifact)
        {
            writer.raw(artifact.identity.source_revision.content.bytes);
            writer.u64(artifact.identity.source_revision.sequence);
            writer.string(artifact.name);
            writer.f32(artifact.virtual_width);
            writer.f32(artifact.virtual_height);
            writer.boolean(artifact.pixel_snap);
            writer.u32(artifact.root);
            writer.u32(static_cast<std::uint32_t>(artifact.widgets.size()));
            for (const auto& widget : artifact.widgets)
                write_widget(writer, widget);
        }

        [[nodiscard]] ContentHash hash_span(std::span<const std::byte> bytes) noexcept
        {
            const auto digest = core::sha256::hash(
                std::span<const std::uint8_t>{
                    reinterpret_cast<const std::uint8_t*>(bytes.data()),
                    bytes.size()});
            return {digest.bytes};
        }
    }

    bool valid_logical_asset_path(std::string_view path) noexcept
    {
        if (path.empty() || path.front() == '/' || path.front() == '\\'
            || path.back() == '/'
            || path.find('\\') != std::string_view::npos
            || path.find(':') != std::string_view::npos
            || !valid_utf8(path))
        {
            return false;
        }
        std::size_t begin{};
        while (begin < path.size())
        {
            const auto end = path.find('/', begin);
            const auto segment = path.substr(
                begin, end == std::string_view::npos ? path.size() - begin
                                                     : end - begin);
            if (segment.empty() || segment == "." || segment == "..")
                return false;
            begin = end == std::string_view::npos ? path.size() : end + 1u;
        }
        return true;
    }

    ContentHash payload_content(const CompiledGuiArtifact& artifact) noexcept
    {
        try
        {
            Writer writer{96ull * 1024ull * 1024ull};
            write_payload(writer, artifact);
            return writer.valid() ? hash_span(writer.bytes()) : ContentHash{};
        }
        catch (...)
        {
            return {};
        }
    }

    ValidationResult validate(
        const CompiledGuiArtifact& artifact,
        const ArtifactLimits& limits) noexcept
    {
        ValidationResult result{};
        if (!limits.valid())
        {
            result.code = ArtifactCode::invalid_limits;
            return result;
        }
        if (!artifact.identity || artifact.identity.schema_version
            != artifact_schema_version)
        {
            result.code = ArtifactCode::invalid_identity;
            return result;
        }
        if (artifact.widgets.empty() || artifact.widgets.size() > limits.maximum_widgets
            || artifact.root >= artifact.widgets.size()
            || !finite(artifact.virtual_width) || artifact.virtual_width <= 0.0f
            || artifact.virtual_width > limits.maximum_layout_extent
            || !finite(artifact.virtual_height) || artifact.virtual_height <= 0.0f
            || artifact.virtual_height > limits.maximum_layout_extent)
        {
            result.code = ArtifactCode::invalid_artifact;
            return result;
        }
        if (artifact.name.size() > limits.maximum_name_bytes
            || !valid_utf8(artifact.name))
        {
            result.code = ArtifactCode::invalid_content;
            return result;
        }

        std::unordered_set<std::uint64_t> identities{};
        std::uint64_t stringBytes = artifact.name.size();
        try
        {
            identities.reserve(artifact.widgets.size());
            for (std::size_t index = 0u; index < artifact.widgets.size(); ++index)
            {
                const auto& widget = artifact.widgets[index];
                if (!widget.id || !valid_kind(widget.kind))
                {
                    result.code = ArtifactCode::invalid_widget;
                    return result;
                }
                if (!identities.insert(widget.id.value).second)
                {
                    result.code = ArtifactCode::duplicate_identity;
                    return result;
                }
                if (widget.parent != invalid_widget_index
                    && widget.parent >= artifact.widgets.size())
                {
                    result.code = ArtifactCode::dangling_reference;
                    return result;
                }
                if (widget.children.size() > limits.maximum_children_per_widget
                    || (!may_have_children(widget.kind) && !widget.children.empty()))
                {
                    result.code = ArtifactCode::invalid_widget;
                    return result;
                }
                if (!valid_layout(widget.layout, limits))
                {
                    result.code = ArtifactCode::invalid_layout;
                    return result;
                }
                if (!valid_style(widget.style, limits))
                {
                    result.code = ArtifactCode::invalid_style;
                    return result;
                }
                if (widget.name.empty() || widget.name.size() > limits.maximum_name_bytes
                    || widget.primary_text.size() > limits.maximum_text_bytes
                    || widget.secondary_text.size() > limits.maximum_text_bytes
                    || widget.asset_path.size() > limits.maximum_path_bytes
                    || widget.action.size() > limits.maximum_action_bytes
                    || widget.tooltip.size() > limits.maximum_tooltip_bytes
                    || !valid_utf8(widget.name) || !valid_utf8(widget.primary_text)
                    || !valid_utf8(widget.secondary_text)
                    || !valid_utf8(widget.action) || !valid_utf8(widget.tooltip))
                {
                    result.code = ArtifactCode::invalid_content;
                    return result;
                }
                if (!widget.asset_path.empty()
                    && (!valid_utf8(widget.asset_path)
                        || !valid_logical_asset_path(widget.asset_path)))
                {
                    result.code = ArtifactCode::invalid_path;
                    return result;
                }
                const auto addString = [&stringBytes, &limits](std::size_t bytes)
                {
                    if (bytes > limits.maximum_total_string_bytes - stringBytes)
                        return false;
                    stringBytes += bytes;
                    return true;
                };
                if (!addString(widget.name.size())
                    || !addString(widget.style.style_class.size())
                    || !addString(widget.action.size())
                    || !addString(widget.tooltip.size())
                    || !addString(widget.primary_text.size())
                    || !addString(widget.secondary_text.size())
                    || !addString(widget.asset_path.size()))
                {
                    result.code = ArtifactCode::capacity_exceeded;
                    return result;
                }
                if (widget.kind == WidgetKind::text_input
                    && (widget.maximum_length == 0u
                        || widget.maximum_length > limits.maximum_text_bytes
                        || widget.primary_text.size() > widget.maximum_length))
                {
                    result.code = ArtifactCode::invalid_content;
                    return result;
                }
                if (widget.kind == WidgetKind::slider
                    && (!finite(widget.minimum) || !finite(widget.maximum)
                        || !finite(widget.value) || !finite(widget.step)
                        || widget.maximum < widget.minimum || widget.step <= 0.0
                        || widget.value < widget.minimum
                        || widget.value > widget.maximum))
                {
                    result.code = ArtifactCode::invalid_content;
                    return result;
                }
                if (widget.kind == WidgetKind::tab_set
                    && widget.selected_child != invalid_widget_index
                    && (widget.selected_child >= artifact.widgets.size()
                        || std::find(
                            widget.children.begin(), widget.children.end(),
                            widget.selected_child) == widget.children.end()))
                {
                    result.code = ArtifactCode::dangling_reference;
                    return result;
                }
            }
        }
        catch (...)
        {
            result.code = ArtifactCode::allocation_failure;
            return result;
        }

        if (artifact.widgets[artifact.root].kind != WidgetKind::canvas
            || artifact.widgets[artifact.root].parent != invalid_widget_index)
        {
            result.code = ArtifactCode::malformed_hierarchy;
            return result;
        }

        std::vector<std::uint8_t> visited(artifact.widgets.size(), 0u);
        std::vector<std::pair<std::uint32_t, std::uint32_t>> stack{};
        try
        {
            stack.push_back({artifact.root, 1u});
            while (!stack.empty())
            {
                const auto [index, depth] = stack.back();
                stack.pop_back();
                if (depth > limits.maximum_hierarchy_depth || visited[index] != 0u)
                {
                    result.code = ArtifactCode::malformed_hierarchy;
                    return result;
                }
                visited[index] = 1u;
                result.hierarchy_depth = (std::max)(result.hierarchy_depth, depth);
                const auto& widget = artifact.widgets[index];
                std::unordered_set<std::uint32_t> local{};
                for (const auto child : widget.children)
                {
                    if (child >= artifact.widgets.size()
                        || !local.insert(child).second
                        || artifact.widgets[child].parent != index
                        || (widget.kind == WidgetKind::tab_set
                            && artifact.widgets[child].kind != WidgetKind::tab_page)
                        || (artifact.widgets[child].kind == WidgetKind::tab_page
                            && widget.kind != WidgetKind::tab_set))
                    {
                        result.code = ArtifactCode::malformed_hierarchy;
                        return result;
                    }
                    stack.push_back({child, depth + 1u});
                }
                if (widget.kind == WidgetKind::tab_set
                    && !widget.children.empty()
                    && widget.selected_child == invalid_widget_index)
                {
                    result.code = ArtifactCode::invalid_content;
                    return result;
                }
            }
        }
        catch (...)
        {
            result.code = ArtifactCode::allocation_failure;
            return result;
        }
        if (std::find(visited.begin(), visited.end(), 0u) != visited.end())
        {
            result.code = ArtifactCode::malformed_hierarchy;
            return result;
        }

        if (payload_content(artifact) != artifact.identity.key)
        {
            result.code = ArtifactCode::content_hash_mismatch;
            return result;
        }
        result.code = ArtifactCode::ready;
        result.total_string_bytes = stringBytes;
        return result;
    }

    SerializationResult serialize(
        const CompiledGuiArtifact& artifact,
        const ArtifactLimits& limits) noexcept
    {
        const auto validation = validate(artifact, limits);
        if (!validation)
            return {validation.code};
        try
        {
            Writer writer{limits.maximum_serialized_bytes};
            writer.raw(magic);
            writer.u32(artifact_schema_version);
            writer.raw(artifact.identity.key.bytes);
            write_payload(writer, artifact);
            if (!writer.valid())
                return {ArtifactCode::capacity_exceeded};
            const auto digest = core::sha256::hash(
                std::span<const std::uint8_t>{
                    reinterpret_cast<const std::uint8_t*>(writer.bytes().data()),
                    writer.bytes().size()});
            writer.raw(digest.bytes);
            if (!writer.valid())
                return {ArtifactCode::capacity_exceeded};
            return {ArtifactCode::ready, writer.take()};
        }
        catch (...)
        {
            return {ArtifactCode::allocation_failure};
        }
    }

    DeserializationResult deserialize(
        std::span<const std::byte> bytes,
        const ArtifactLimits& limits) noexcept
    {
        if (!limits.valid())
            return {ArtifactCode::invalid_limits};
        if (bytes.size() < magic.size() + sizeof(std::uint32_t)
                + digest_bytes * 3u
            || bytes.size() > limits.maximum_serialized_bytes)
        {
            return {ArtifactCode::malformed_payload};
        }
        try
        {
            const auto body = bytes.first(bytes.size() - digest_bytes);
            const auto expected = core::sha256::hash(
                std::span<const std::uint8_t>{
                    reinterpret_cast<const std::uint8_t*>(body.data()),
                    body.size()});
            for (std::size_t index = 0u; index < digest_bytes; ++index)
            {
                if (expected.bytes[index]
                    != std::to_integer<std::uint8_t>(
                        bytes[body.size() + index]))
                {
                    return {ArtifactCode::integrity_failure};
                }
            }

            Reader reader{body, limits};
            for (const auto expectedByte : magic)
            {
                if (reader.u8() != expectedByte)
                    return {ArtifactCode::malformed_payload};
            }
            const auto schema = reader.u32();
            if (schema != artifact_schema_version)
                return {ArtifactCode::unsupported_schema};

            CompiledGuiArtifact artifact{};
            artifact.identity.schema_version = schema;
            artifact.identity.key = reader.hash();
            artifact.identity.source_revision.content = reader.hash();
            artifact.identity.source_revision.sequence = reader.u64();
            artifact.name = reader.string(limits.maximum_name_bytes);
            artifact.virtual_width = reader.f32();
            artifact.virtual_height = reader.f32();
            artifact.pixel_snap = reader.boolean();
            artifact.root = reader.u32();
            const auto count = reader.u32();
            if (!reader.valid() || count == 0u || count > limits.maximum_widgets)
                return {ArtifactCode::capacity_exceeded};
            artifact.widgets.reserve(count);
            for (std::uint32_t index = 0u; index < count; ++index)
                artifact.widgets.push_back(read_widget(reader, limits));
            if (!reader.finished())
                return {ArtifactCode::malformed_payload};

            const auto validation = validate(artifact, limits);
            if (!validation)
                return {validation.code};
            return {ArtifactCode::ready, std::move(artifact)};
        }
        catch (...)
        {
            return {ArtifactCode::allocation_failure};
        }
    }
}
