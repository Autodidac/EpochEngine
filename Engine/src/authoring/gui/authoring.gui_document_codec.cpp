/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

module authoring.gui_document;

import core.sha256;

namespace epochengine::authoring::gui
{
    namespace
    {
        constexpr std::array<std::uint8_t, 8> kMagic{
            'E', 'P', 'G', 'U', 'I', '0', '0', '1'};
        constexpr std::uint64_t kMaximumCodecBytes =
            64ull * 1024ull * 1024ull;
        constexpr std::size_t kDigestBytes = 32u;

        class Writer final
        {
        public:
            explicit Writer(std::uint64_t maximumBytes) noexcept
                : maximum_bytes_((std::min)(maximumBytes, kMaximumCodecBytes))
            {
            }

            [[nodiscard]] bool valid() const noexcept
            {
                return valid_;
            }

            [[nodiscard]] std::vector<std::byte> take() noexcept
            {
                return std::move(bytes_);
            }

            void raw(std::span<const std::uint8_t> bytes)
            {
                if (!reserve(bytes.size()))
                    return;
                for (const std::uint8_t value : bytes)
                    bytes_.push_back(static_cast<std::byte>(value));
            }

            void u8(std::uint8_t value)
            {
                if (reserve(1u))
                    bytes_.push_back(static_cast<std::byte>(value));
            }

            void boolean(bool value)
            {
                u8(value ? 1u : 0u);
            }

            void u32(std::uint32_t value)
            {
                integer(value);
            }

            void i32(std::int32_t value)
            {
                integer(std::bit_cast<std::uint32_t>(value));
            }

            void u64(std::uint64_t value)
            {
                integer(value);
            }

            void f32(float value)
            {
                u32(std::bit_cast<std::uint32_t>(value));
            }

            void f64(double value)
            {
                u64(std::bit_cast<std::uint64_t>(value));
            }

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
                {
                    bytes_.push_back(static_cast<std::byte>(character));
                }
            }

            template<typename Tag, std::unsigned_integral Index>
            void handle(Handle<Tag, Index> value)
            {
                static_assert(sizeof(Index) <= sizeof(std::uint64_t));
                u64(static_cast<std::uint64_t>(value.index));
                u32(value.generation);
            }

        private:
            template<std::unsigned_integral Integer>
            void integer(Integer value)
            {
                if (!reserve(sizeof(Integer)))
                    return;
                for (std::size_t byte = 0u; byte < sizeof(Integer); ++byte)
                {
                    bytes_.push_back(static_cast<std::byte>(
                        (value >> (byte * 8u)) & Integer{0xffu}));
                }
            }

            [[nodiscard]] bool reserve(std::size_t count)
            {
                if (!valid_
                    || count > maximum_bytes_
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
                const DocumentLimits& limits) noexcept
                : bytes_(bytes)
                , limits_(limits)
            {
            }

            [[nodiscard]] bool valid() const noexcept
            {
                return valid_;
            }

            [[nodiscard]] bool finished() const noexcept
            {
                return valid_ && cursor_ == bytes_.size();
            }

            void invalidate() noexcept
            {
                valid_ = false;
            }

            [[nodiscard]] std::array<std::uint8_t, 8> magic()
            {
                std::array<std::uint8_t, 8> result{};
                for (std::uint8_t& value : result)
                    value = u8();
                return result;
            }

            [[nodiscard]] std::uint8_t u8()
            {
                if (!require(1u))
                    return 0u;
                return std::to_integer<std::uint8_t>(bytes_[cursor_++]);
            }

            [[nodiscard]] bool boolean()
            {
                const std::uint8_t value = u8();
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

            [[nodiscard]] std::string string(std::uint32_t maximumBytes)
            {
                const std::uint32_t count = u32();
                if (!valid_
                    || count > maximumBytes
                    || string_bytes_ > limits_.maximum_total_string_bytes
                    || count > limits_.maximum_total_string_bytes
                        - string_bytes_
                    || !require(count))
                {
                    valid_ = false;
                    return {};
                }
                std::string result{};
                result.resize(count);
                for (std::uint32_t index = 0u; index < count; ++index)
                {
                    result[index] = static_cast<char>(
                        std::to_integer<std::uint8_t>(
                            bytes_[cursor_ + index]));
                }
                cursor_ += count;
                string_bytes_ += count;
                return result;
            }

            template<typename Tag, std::unsigned_integral Index>
            [[nodiscard]] Handle<Tag, Index> handle()
            {
                const std::uint64_t rawIndex = u64();
                const std::uint32_t generation = u32();
                if (rawIndex
                    > static_cast<std::uint64_t>(
                        (std::numeric_limits<Index>::max)()))
                {
                    valid_ = false;
                    return {};
                }
                return {
                    static_cast<Index>(rawIndex),
                    generation};
            }

        private:
            template<std::unsigned_integral Integer>
            [[nodiscard]] Integer integer()
            {
                if (!require(sizeof(Integer)))
                    return 0u;
                Integer result{};
                for (std::size_t byte = 0u; byte < sizeof(Integer); ++byte)
                {
                    result |= static_cast<Integer>(
                        std::to_integer<std::uint8_t>(
                            bytes_[cursor_ + byte])) << (byte * 8u);
                }
                cursor_ += sizeof(Integer);
                return result;
            }

            [[nodiscard]] bool require(std::size_t count)
            {
                if (!valid_
                    || cursor_ > bytes_.size()
                    || count > bytes_.size() - cursor_)
                {
                    valid_ = false;
                    return false;
                }
                return true;
            }

            std::span<const std::byte> bytes_{};
            const DocumentLimits& limits_;
            std::size_t cursor_{};
            std::uint64_t string_bytes_{};
            bool valid_{true};
        };

        void write_color(Writer& writer, Color color)
        {
            writer.f32(color.red);
            writer.f32(color.green);
            writer.f32(color.blue);
            writer.f32(color.alpha);
        }

        [[nodiscard]] Color read_color(Reader& reader)
        {
            return {
                reader.f32(),
                reader.f32(),
                reader.f32(),
                reader.f32()};
        }

        void write_layout(Writer& writer, const LayoutDescriptor& value)
        {
            writer.u8(static_cast<std::uint8_t>(value.flow));
            writer.u8(static_cast<std::uint8_t>(value.width_rule));
            writer.u8(static_cast<std::uint8_t>(value.height_rule));
            writer.f32(value.x);
            writer.f32(value.y);
            writer.f32(value.width);
            writer.f32(value.height);
            writer.f32(value.minimum_width);
            writer.f32(value.minimum_height);
            writer.f32(value.maximum_width);
            writer.f32(value.maximum_height);
            writer.f32(value.flex_grow);
            writer.f32(value.spacing);
            writer.f32(value.padding.left);
            writer.f32(value.padding.top);
            writer.f32(value.padding.right);
            writer.f32(value.padding.bottom);
            writer.boolean(value.clip_children);
        }

        [[nodiscard]] LayoutDescriptor read_layout(Reader& reader)
        {
            LayoutDescriptor value{};
            value.flow = static_cast<LayoutFlow>(reader.u8());
            value.width_rule = static_cast<SizeRule>(reader.u8());
            value.height_rule = static_cast<SizeRule>(reader.u8());
            value.x = reader.f32();
            value.y = reader.f32();
            value.width = reader.f32();
            value.height = reader.f32();
            value.minimum_width = reader.f32();
            value.minimum_height = reader.f32();
            value.maximum_width = reader.f32();
            value.maximum_height = reader.f32();
            value.flex_grow = reader.f32();
            value.spacing = reader.f32();
            value.padding.left = reader.f32();
            value.padding.top = reader.f32();
            value.padding.right = reader.f32();
            value.padding.bottom = reader.f32();
            value.clip_children = reader.boolean();
            return value;
        }

        void write_style(Writer& writer, const StyleDescriptor& value)
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

        [[nodiscard]] StyleDescriptor read_style(
            Reader& reader,
            const DocumentLimits& limits)
        {
            StyleDescriptor value{};
            value.background = read_color(reader);
            value.foreground = read_color(reader);
            value.border = read_color(reader);
            value.accent = read_color(reader);
            value.border_width = reader.f32();
            value.corner_radius = reader.f32();
            value.opacity = reader.f32();
            value.style_class = reader.string(
                limits.maximum_style_class_bytes);
            return value;
        }

        void write_content(Writer& writer, const WidgetContent& content)
        {
            writer.u8(static_cast<std::uint8_t>(content.index()));
            std::visit([&](const auto& value)
            {
                using Type = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::same_as<Type, CanvasContent>)
                {
                    writer.f32(value.virtual_width);
                    writer.f32(value.virtual_height);
                    writer.boolean(value.pixel_snap);
                }
                else if constexpr (std::same_as<Type, PanelContent>)
                {
                    writer.string(value.title);
                }
                else if constexpr (std::same_as<Type, ButtonContent>)
                {
                    writer.string(value.label);
                }
                else if constexpr (std::same_as<Type, TextContent>)
                {
                    writer.string(value.text);
                    writer.boolean(value.wrap);
                }
                else if constexpr (std::same_as<Type, ImageContent>)
                {
                    writer.string(value.asset_path);
                    writer.string(value.alternative_text);
                    writer.boolean(value.preserve_aspect);
                }
                else if constexpr (std::same_as<Type, ImageButtonContent>)
                {
                    writer.string(value.asset_path);
                    writer.string(value.label);
                    writer.boolean(value.preserve_aspect);
                }
                else if constexpr (std::same_as<Type, TextInputContent>)
                {
                    writer.string(value.text);
                    writer.string(value.placeholder);
                    writer.u32(value.maximum_length);
                    writer.boolean(value.multiline);
                    writer.boolean(value.read_only);
                }
                else if constexpr (std::same_as<Type, SliderContent>)
                {
                    writer.string(value.label);
                    writer.f64(value.minimum);
                    writer.f64(value.maximum);
                    writer.f64(value.value);
                    writer.f64(value.step);
                }
                else if constexpr (std::same_as<Type, ScrollAreaContent>)
                {
                    writer.f32(value.horizontal_offset);
                    writer.f32(value.vertical_offset);
                    writer.boolean(value.allow_horizontal);
                    writer.boolean(value.allow_vertical);
                }
                else if constexpr (std::same_as<Type, TabSetContent>)
                {
                    writer.handle(value.selected_page);
                }
                else
                {
                    writer.string(value.label);
                    writer.boolean(value.closeable);
                }
            }, content);
        }

        [[nodiscard]] WidgetContent read_content(
            Reader& reader,
            const DocumentLimits& limits)
        {
            const std::uint8_t kind = reader.u8();
            switch (kind)
            {
            case 0u:
                return CanvasContent{
                    reader.f32(), reader.f32(), reader.boolean()};
            case 1u:
                return PanelContent{
                    reader.string(limits.maximum_text_bytes)};
            case 2u:
                return ButtonContent{
                    reader.string(limits.maximum_text_bytes)};
            case 3u:
                return TextContent{
                    reader.string(limits.maximum_text_bytes),
                    reader.boolean()};
            case 4u:
                return ImageContent{
                    reader.string(limits.maximum_asset_path_bytes),
                    reader.string(limits.maximum_text_bytes),
                    reader.boolean()};
            case 5u:
                return ImageButtonContent{
                    reader.string(limits.maximum_asset_path_bytes),
                    reader.string(limits.maximum_text_bytes),
                    reader.boolean()};
            case 6u:
                return TextInputContent{
                    reader.string(limits.maximum_text_bytes),
                    reader.string(limits.maximum_text_bytes),
                    reader.u32(),
                    reader.boolean(),
                    reader.boolean()};
            case 7u:
                return SliderContent{
                    reader.string(limits.maximum_text_bytes),
                    reader.f64(),
                    reader.f64(),
                    reader.f64(),
                    reader.f64()};
            case 8u:
                return ScrollAreaContent{
                    reader.f32(),
                    reader.f32(),
                    reader.boolean(),
                    reader.boolean()};
            case 9u:
                return TabSetContent{
                    reader.handle<WidgetTag, std::uint32_t>()};
            case 10u:
                return TabPageContent{
                    reader.string(limits.maximum_text_bytes),
                    reader.boolean()};
            default:
                reader.invalidate();
                return CanvasContent{};
            }
        }

        void write_descriptor(
            Writer& writer,
            const WidgetDescriptor& value)
        {
            writer.u8(static_cast<std::uint8_t>(value.kind));
            writer.string(value.name);
            write_layout(writer, value.layout);
            write_style(writer, value.style);
            write_content(writer, value.content);
            writer.boolean(value.interaction.enabled);
            writer.boolean(value.interaction.visible);
            writer.boolean(value.interaction.focusable);
            writer.boolean(value.interaction.accepts_pointer);
            writer.i32(value.interaction.tab_index);
            writer.string(value.interaction.action);
            writer.string(value.interaction.tooltip);
        }

        [[nodiscard]] WidgetDescriptor read_descriptor(
            Reader& reader,
            const DocumentLimits& limits)
        {
            WidgetDescriptor value{};
            value.kind = static_cast<WidgetKind>(reader.u8());
            value.name = reader.string(limits.maximum_name_bytes);
            value.layout = read_layout(reader);
            value.style = read_style(reader, limits);
            value.content = read_content(reader, limits);
            value.interaction.enabled = reader.boolean();
            value.interaction.visible = reader.boolean();
            value.interaction.focusable = reader.boolean();
            value.interaction.accepts_pointer = reader.boolean();
            value.interaction.tab_index = reader.i32();
            value.interaction.action = reader.string(
                limits.maximum_action_bytes);
            value.interaction.tooltip = reader.string(
                limits.maximum_tooltip_bytes);
            return value;
        }

        [[nodiscard]] std::span<const std::uint8_t> as_u8(
            std::span<const std::byte> bytes) noexcept
        {
            return {
                reinterpret_cast<const std::uint8_t*>(bytes.data()),
                bytes.size()};
        }
    }

    GuiDocument::GuiDocument(
        GuiDocumentSnapshot snapshot,
        DocumentLimits limits)
        : document_(snapshot.document)
        , branch_(snapshot.branch)
        , limits_(limits)
    {
        if (snapshot.schema_version != kSchemaVersion
            || !document_.valid()
            || !branch_.valid()
            || !snapshot.revision.valid()
            || !valid_limits(limits_)
            || snapshot.slots.size() > limits_.maximum_widget_slots
            || snapshot.roots.size() > limits_.maximum_active_widgets)
        {
            return;
        }

        try
        {
            State restored{};
            restored.slots.reserve(snapshot.slots.size());
            issued_generations_.reserve(snapshot.slots.size());
            for (WidgetSlotSnapshot& source : snapshot.slots)
            {
                if (source.generation == 0u)
                    return;
                WidgetSlot slot{};
                slot.generation = source.generation;
                if (source.widget)
                {
                    slot.widget = WidgetState{
                        source.widget->parent,
                        std::move(source.widget->descriptor),
                        std::move(source.widget->children)};
                }
                restored.slots.push_back(std::move(slot));
                issued_generations_.push_back(source.generation);
            }
            restored.roots = std::move(snapshot.roots);
            if (active_widget_count(restored)
                    > limits_.maximum_active_widgets
                || validate_state(restored))
            {
                return;
            }
            if (content_hash(restored) != snapshot.revision.content)
                return;

            state_ = std::move(restored);
            revision_sequence_ = snapshot.revision.sequence;
            revision_ = snapshot.revision;
            valid_ = true;
        }
        catch (...)
        {
            valid_ = false;
            state_ = {};
            issued_generations_.clear();
        }
    }

    SerializedGuiDocument serialize_gui_document(
        const GuiDocumentSnapshot& snapshot,
        std::uint64_t maximumBytes) noexcept
    {
        if (maximumBytes <= kDigestBytes
            || maximumBytes > kMaximumCodecBytes
            || snapshot.schema_version != kSchemaVersion
            || !snapshot.document.valid()
            || !snapshot.branch.valid()
            || !snapshot.revision.valid())
        {
            return {};
        }

        try
        {
            Writer writer{maximumBytes - kDigestBytes};
            writer.raw(kMagic);
            writer.u32(snapshot.schema_version);
            writer.handle(snapshot.document);
            writer.u64(snapshot.branch.timeline);
            writer.u64(snapshot.branch.branch);
            for (const std::uint64_t word : snapshot.revision.content.words)
                writer.u64(word);
            writer.u64(snapshot.revision.sequence);
            if (snapshot.slots.size()
                    > (std::numeric_limits<std::uint32_t>::max)()
                || snapshot.roots.size()
                    > (std::numeric_limits<std::uint32_t>::max)())
            {
                return {};
            }
            writer.u32(static_cast<std::uint32_t>(snapshot.slots.size()));
            for (const WidgetSlotSnapshot& slot : snapshot.slots)
            {
                writer.u32(slot.generation);
                writer.boolean(slot.widget.has_value());
                if (!slot.widget)
                    continue;
                writer.handle(slot.widget->parent);
                write_descriptor(writer, slot.widget->descriptor);
                if (slot.widget->children.size()
                    > (std::numeric_limits<std::uint32_t>::max)())
                {
                    return {};
                }
                writer.u32(static_cast<std::uint32_t>(
                    slot.widget->children.size()));
                for (const WidgetHandle child : slot.widget->children)
                    writer.handle(child);
            }
            writer.u32(static_cast<std::uint32_t>(snapshot.roots.size()));
            for (const WidgetHandle root : snapshot.roots)
                writer.handle(root);
            if (!writer.valid())
            {
                return {
                    SnapshotCodecCode::serialized_budget_exceeded,
                    {}};
            }

            std::vector<std::byte> bytes = writer.take();
            const core::sha256::Digest digest = core::sha256::hash(
                as_u8(bytes));
            if (bytes.size() > maximumBytes - digest.bytes.size())
            {
                return {
                    SnapshotCodecCode::serialized_budget_exceeded,
                    {}};
            }
            bytes.reserve(bytes.size() + digest.bytes.size());
            for (const std::uint8_t byte : digest.bytes)
                bytes.push_back(static_cast<std::byte>(byte));
            return {SnapshotCodecCode::ready, std::move(bytes)};
        }
        catch (...)
        {
            return {SnapshotCodecCode::allocation_failure, {}};
        }
    }

    DeserializedGuiDocument deserialize_gui_document(
        std::span<const std::byte> bytes,
        const DocumentLimits& limits) noexcept
    {
        if (bytes.size() <= kDigestBytes
            || bytes.size() > kMaximumCodecBytes)
        {
            return {SnapshotCodecCode::bounded_state_exceeded, std::nullopt};
        }

        try
        {
            const std::span payload = bytes.first(
                bytes.size() - kDigestBytes);
            const std::span digestBytes = bytes.last(kDigestBytes);
            const core::sha256::Digest actual = core::sha256::hash(
                as_u8(payload));
            for (std::size_t index = 0u;
                 index < actual.bytes.size();
                 ++index)
            {
                if (actual.bytes[index]
                    != std::to_integer<std::uint8_t>(digestBytes[index]))
                {
                    return {
                        SnapshotCodecCode::integrity_failure,
                        std::nullopt};
                }
            }

            Reader reader{payload, limits};
            if (reader.magic() != kMagic)
            {
                return {
                    SnapshotCodecCode::malformed_input,
                    std::nullopt};
            }
            GuiDocumentSnapshot snapshot{};
            snapshot.schema_version = reader.u32();
            if (snapshot.schema_version != kSchemaVersion)
            {
                return {
                    SnapshotCodecCode::unsupported_schema,
                    std::nullopt};
            }
            snapshot.document =
                reader.handle<DocumentTag, std::uint32_t>();
            snapshot.branch.timeline = reader.u64();
            snapshot.branch.branch = reader.u64();
            for (std::uint64_t& word : snapshot.revision.content.words)
                word = reader.u64();
            snapshot.revision.sequence = reader.u64();

            const std::uint32_t slotCount = reader.u32();
            if (!reader.valid()
                || slotCount > limits.maximum_widget_slots)
            {
                return {
                    SnapshotCodecCode::bounded_state_exceeded,
                    std::nullopt};
            }
            snapshot.slots.reserve(slotCount);
            for (std::uint32_t index = 0u; index < slotCount; ++index)
            {
                WidgetSlotSnapshot slot{};
                slot.generation = reader.u32();
                if (reader.boolean())
                {
                    WidgetSnapshotState widget{};
                    widget.parent =
                        reader.handle<WidgetTag, std::uint32_t>();
                    widget.descriptor = read_descriptor(reader, limits);
                    const std::uint32_t childCount = reader.u32();
                    if (!reader.valid()
                        || childCount > limits.maximum_children_per_widget)
                    {
                        return {
                            SnapshotCodecCode::bounded_state_exceeded,
                            std::nullopt};
                    }
                    widget.children.reserve(childCount);
                    for (std::uint32_t child = 0u;
                         child < childCount;
                         ++child)
                    {
                        widget.children.push_back(
                            reader.handle<WidgetTag, std::uint32_t>());
                    }
                    slot.widget = std::move(widget);
                }
                snapshot.slots.push_back(std::move(slot));
            }

            const std::uint32_t rootCount = reader.u32();
            if (!reader.valid()
                || rootCount > limits.maximum_active_widgets)
            {
                return {
                    SnapshotCodecCode::bounded_state_exceeded,
                    std::nullopt};
            }
            snapshot.roots.reserve(rootCount);
            for (std::uint32_t index = 0u; index < rootCount; ++index)
            {
                snapshot.roots.push_back(
                    reader.handle<WidgetTag, std::uint32_t>());
            }
            if (!reader.finished())
            {
                return {
                    SnapshotCodecCode::malformed_input,
                    std::nullopt};
            }

            GuiDocument validated{snapshot, limits};
            if (!validated.valid())
            {
                return {
                    SnapshotCodecCode::integrity_failure,
                    std::nullopt};
            }
            return {
                SnapshotCodecCode::ready,
                std::move(snapshot)};
        }
        catch (...)
        {
            return {
                SnapshotCodecCode::allocation_failure,
                std::nullopt};
        }
    }
}
