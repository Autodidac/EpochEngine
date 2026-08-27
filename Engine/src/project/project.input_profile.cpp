/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

module project.input_profile;

import platform.filesystem;

namespace epochengine::project_input
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr std::array<std::byte, 8> kSourceMagic{
            std::byte{'E'}, std::byte{'P'}, std::byte{'I'}, std::byte{'N'},
            std::byte{'S'}, std::byte{'R'}, std::byte{'C'}, std::byte{'1'}};
        constexpr std::array<std::byte, 8> kArtifactMagic{
            std::byte{'E'}, std::byte{'P'}, std::byte{'I'}, std::byte{'N'},
            std::byte{'A'}, std::byte{'R'}, std::byte{'T'}, std::byte{'1'}};
        constexpr std::uint16_t kCodecVersion{1u};
        constexpr ModifierMask kAllModifiers{
            modifier_mask(Modifier::shift)
            | modifier_mask(Modifier::control)
            | modifier_mask(Modifier::alt)
            | modifier_mask(Modifier::super)};

        class HashBuilder final
        {
        public:
            HashBuilder() noexcept
                : words_{
                    14695981039346656037ull,
                    1099511628211ull ^ 0x9e3779b97f4a7c15ull,
                    14695981039346656037ull ^ 0xd6e8feb86659fd93ull,
                    1099511628211ull ^ 0xa0761d6478bd642full}
            {
            }

            void byte(std::uint8_t value) noexcept
            {
                constexpr std::array<std::uint64_t, 4> primes{
                    1099511628211ull,
                    14029467366897019727ull,
                    1609587929392839161ull,
                    9650029242287828579ull};
                for (std::size_t index = 0; index < words_.size(); ++index)
                {
                    words_[index] ^= static_cast<std::uint64_t>(value)
                        + static_cast<std::uint64_t>(index * 0x51u);
                    words_[index] *= primes[index];
                    words_[index] ^= words_[index] >> (13u + index * 3u);
                }
            }

            void u16(std::uint16_t value) noexcept
            {
                for (std::uint32_t shift = 0u; shift < 16u; shift += 8u)
                    byte(static_cast<std::uint8_t>(value >> shift));
            }

            void u32(std::uint32_t value) noexcept
            {
                for (std::uint32_t shift = 0u; shift < 32u; shift += 8u)
                    byte(static_cast<std::uint8_t>(value >> shift));
            }

            void i32(std::int32_t value) noexcept
            {
                u32(static_cast<std::uint32_t>(value));
            }

            void u64(std::uint64_t value) noexcept
            {
                for (std::uint32_t shift = 0u; shift < 64u; shift += 8u)
                    byte(static_cast<std::uint8_t>(value >> shift));
            }

            void text(std::string_view value) noexcept
            {
                u64(static_cast<std::uint64_t>(value.size()));
                for (char character : value)
                    byte(static_cast<std::uint8_t>(character));
            }

            void hash(const ContentHash& value) noexcept
            {
                for (std::uint64_t word : value.words)
                    u64(word);
            }

            [[nodiscard]] ContentHash finish() const noexcept
            {
                ContentHash result{words_};
                if (result.empty())
                    result.words[0] = 1u;
                return result;
            }

        private:
            std::array<std::uint64_t, 4> words_{};
        };

        class ByteWriter final
        {
        public:
            explicit ByteWriter(std::uint64_t maximumBytes) noexcept
                : maximum_bytes_(maximumBytes)
            {
            }

            [[nodiscard]] bool valid() const noexcept
            {
                return valid_;
            }

            [[nodiscard]] std::vector<std::byte> release() noexcept
            {
                return std::move(bytes_);
            }

            void raw(std::span<const std::byte> value)
            {
                if (!valid_ || value.size() > maximum_bytes_
                    || bytes_.size() > maximum_bytes_ - value.size())
                {
                    valid_ = false;
                    return;
                }
                bytes_.insert(bytes_.end(), value.begin(), value.end());
            }

            void u8(std::uint8_t value)
            {
                const std::byte byte{value};
                raw(std::span<const std::byte>{&byte, 1u});
            }

            void u16(std::uint16_t value)
            {
                for (std::uint32_t shift = 0u; shift < 16u; shift += 8u)
                    u8(static_cast<std::uint8_t>(value >> shift));
            }

            void u32(std::uint32_t value)
            {
                for (std::uint32_t shift = 0u; shift < 32u; shift += 8u)
                    u8(static_cast<std::uint8_t>(value >> shift));
            }

            void i32(std::int32_t value)
            {
                u32(static_cast<std::uint32_t>(value));
            }

            void u64(std::uint64_t value)
            {
                for (std::uint32_t shift = 0u; shift < 64u; shift += 8u)
                    u8(static_cast<std::uint8_t>(value >> shift));
            }

            void hash(const ContentHash& value)
            {
                for (std::uint64_t word : value.words)
                    u64(word);
            }

            void text(std::string_view value)
            {
                if (value.size() > (std::numeric_limits<std::uint32_t>::max)())
                {
                    valid_ = false;
                    return;
                }
                u32(static_cast<std::uint32_t>(value.size()));
                raw(std::as_bytes(std::span{value.data(), value.size()}));
            }

        private:
            std::uint64_t maximum_bytes_{};
            std::vector<std::byte> bytes_{};
            bool valid_{true};
        };

        class ByteReader final
        {
        public:
            explicit ByteReader(std::span<const std::byte> bytes) noexcept
                : bytes_(bytes)
            {
            }

            [[nodiscard]] bool valid() const noexcept
            {
                return valid_;
            }

            [[nodiscard]] bool at_end() const noexcept
            {
                return valid_ && cursor_ == bytes_.size();
            }

            [[nodiscard]] bool matches(std::span<const std::byte> expected)
                noexcept
            {
                if (!available(expected.size()))
                    return false;
                const bool equal = std::equal(
                    expected.begin(), expected.end(), bytes_.begin() + cursor_);
                cursor_ += expected.size();
                if (!equal)
                    valid_ = false;
                return equal;
            }

            [[nodiscard]] std::uint8_t u8() noexcept
            {
                if (!available(1u))
                    return 0u;
                return std::to_integer<std::uint8_t>(bytes_[cursor_++]);
            }

            [[nodiscard]] std::uint16_t u16() noexcept
            {
                std::uint16_t value{};
                for (std::uint32_t shift = 0u; shift < 16u; shift += 8u)
                    value |= static_cast<std::uint16_t>(u8()) << shift;
                return value;
            }

            [[nodiscard]] std::uint32_t u32() noexcept
            {
                std::uint32_t value{};
                for (std::uint32_t shift = 0u; shift < 32u; shift += 8u)
                    value |= static_cast<std::uint32_t>(u8()) << shift;
                return value;
            }

            [[nodiscard]] std::int32_t i32() noexcept
            {
                return static_cast<std::int32_t>(u32());
            }

            [[nodiscard]] std::uint64_t u64() noexcept
            {
                std::uint64_t value{};
                for (std::uint32_t shift = 0u; shift < 64u; shift += 8u)
                    value |= static_cast<std::uint64_t>(u8()) << shift;
                return value;
            }

            [[nodiscard]] ContentHash hash() noexcept
            {
                ContentHash value{};
                for (std::uint64_t& word : value.words)
                    word = u64();
                return value;
            }

            [[nodiscard]] std::optional<std::string> text(
                std::uint32_t maximumBytes)
            {
                const std::uint32_t size = u32();
                if (!valid_ || size > maximumBytes || !available(size))
                    return std::nullopt;
                std::string result{
                    reinterpret_cast<const char*>(bytes_.data() + cursor_),
                    size};
                cursor_ += size;
                return result;
            }

        private:
            [[nodiscard]] bool available(std::size_t count) noexcept
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
            std::size_t cursor_{};
            bool valid_{true};
        };

        [[nodiscard]] constexpr bool valid_semantic(
            ActionSemantic semantic) noexcept
        {
            return semantic > ActionSemantic::invalid
                && semantic < ActionSemantic::count;
        }

        [[nodiscard]] constexpr std::size_t semantic_index(
            ActionSemantic semantic) noexcept
        {
            return static_cast<std::size_t>(semantic) - 1u;
        }

        [[nodiscard]] bool valid_display_name(
            std::string_view name,
            const ProfileLimits& limits) noexcept
        {
            if (name.empty() || name.size() > limits.maximum_display_name_bytes
                || name.front() == ' ' || name.back() == ' ')
            {
                return false;
            }
            for (const unsigned char byte : name)
            {
                if (byte < 0x20u || byte > 0x7eu)
                    return false;
            }
            return true;
        }

        [[nodiscard]] constexpr bool valid_key(std::uint16_t code) noexcept
        {
            return valid_key_code(static_cast<KeyCode>(code));
        }

        [[nodiscard]] constexpr bool valid_controller_button(
            std::uint16_t code) noexcept
        {
            return code > static_cast<std::uint16_t>(
                    ControllerButton::invalid)
                && code <= static_cast<std::uint16_t>(
                    ControllerButton::dpad_right);
        }

        [[nodiscard]] constexpr bool valid_controller_axis(
            std::uint16_t code) noexcept
        {
            return code > static_cast<std::uint16_t>(ControllerAxis::invalid)
                && code <= static_cast<std::uint16_t>(
                    ControllerAxis::right_trigger);
        }

        [[nodiscard]] constexpr bool valid_response(
            AxisResponse response) noexcept
        {
            return response >= AxisResponse::linear
                && response <= AxisResponse::cubic;
        }

        [[nodiscard]] constexpr bool valid_polarity(
            AxisPolarity polarity) noexcept
        {
            return polarity >= AxisPolarity::bipolar
                && polarity <= AxisPolarity::negative;
        }

        void append_action_hash(
            HashBuilder& hash,
            const ActionDefinition& action) noexcept
        {
            hash.u64(action.id.value);
            hash.byte(static_cast<std::uint8_t>(action.semantic));
            hash.byte(static_cast<std::uint8_t>(action.value_kind));
        }

        void append_binding_hash(
            HashBuilder& hash,
            const BindingDefinition& binding) noexcept
        {
            hash.u64(binding.id.value);
            hash.u64(binding.action.value);
            hash.byte(static_cast<std::uint8_t>(binding.device));
            hash.u16(binding.code);
            hash.byte(binding.controller_slot);
            hash.byte(static_cast<std::uint8_t>(binding.polarity));
            hash.byte(static_cast<std::uint8_t>(binding.response));
            hash.byte(binding.required_modifiers);
            hash.byte(binding.forbidden_modifiers);
            hash.i32(binding.scale_q15);
            hash.u16(binding.dead_zone_q15);
            hash.u16(binding.saturation_q15);
        }

        [[nodiscard]] bool same_binding_source(
            const BindingDefinition& left,
            const BindingDefinition& right) noexcept
        {
            return left.device == right.device
                && left.code == right.code
                && left.controller_slot == right.controller_slot
                && left.polarity == right.polarity
                && left.required_modifiers == right.required_modifiers
                && left.forbidden_modifiers == right.forbidden_modifiers;
        }

        [[nodiscard]] ValidationCode validate_source_shape(
            const ProfileSource& source,
            const ProfileLimits& limits,
            bool verifyHash) noexcept
        {
            if (!limits.valid())
                return ValidationCode::invalid_limits;
            if (!source.id)
                return ValidationCode::invalid_profile;
            if (!valid_display_name(source.display_name, limits))
                return ValidationCode::invalid_name;
            if (source.revision.sequence == 0u)
                return ValidationCode::invalid_revision;
            if (source.actions.size() > limits.maximum_actions)
                return ValidationCode::action_limit_exceeded;
            if (source.bindings.size() > limits.maximum_bindings)
                return ValidationCode::binding_limit_exceeded;

            constexpr std::size_t requiredActionCount =
                static_cast<std::size_t>(ActionSemantic::count) - 1u;
            if (source.actions.size() != requiredActionCount)
                return ValidationCode::missing_action;

            std::array<bool, requiredActionCount> foundActions{};
            for (std::size_t index = 0; index < source.actions.size(); ++index)
            {
                const ActionDefinition& action = source.actions[index];
                if (!action.id || !valid_semantic(action.semantic)
                    || action.id != stable_action_id(action.semantic)
                    || action.value_kind
                        != expected_value_kind(action.semantic))
                {
                    return ValidationCode::invalid_action;
                }
                if (index != 0u
                    && static_cast<std::uint8_t>(
                        source.actions[index - 1u].semantic)
                        >= static_cast<std::uint8_t>(action.semantic))
                {
                    return ValidationCode::duplicate_action;
                }
                const std::size_t semantic = semantic_index(action.semantic);
                if (foundActions[semantic])
                    return ValidationCode::duplicate_action;
                foundActions[semantic] = true;
            }
            if (std::find(foundActions.begin(), foundActions.end(), false)
                != foundActions.end())
            {
                return ValidationCode::missing_action;
            }

            std::array<std::uint32_t, requiredActionCount> bindingCounts{};
            for (std::size_t index = 0; index < source.bindings.size(); ++index)
            {
                const BindingDefinition& binding = source.bindings[index];
                if (!binding.id || !binding.action
                    || !valid_polarity(binding.polarity)
                    || !valid_response(binding.response)
                    || (binding.required_modifiers & ~kAllModifiers) != 0u
                    || (binding.forbidden_modifiers & ~kAllModifiers) != 0u
                    || (binding.required_modifiers
                        & binding.forbidden_modifiers) != 0u
                    || binding.scale_q15 == 0
                    || binding.scale_q15 < -2 * normalized_unit
                    || binding.scale_q15 > 2 * normalized_unit
                    || binding.dead_zone_q15 >= binding.saturation_q15
                    || binding.saturation_q15 > normalized_unit)
                {
                    return ValidationCode::invalid_binding;
                }
                if (index != 0u
                    && source.bindings[index - 1u].id.value
                        >= binding.id.value)
                {
                    return ValidationCode::duplicate_binding;
                }

                const auto actionIterator = std::find_if(
                    source.actions.begin(), source.actions.end(),
                    [&](const ActionDefinition& action)
                    {
                        return action.id == binding.action;
                    });
                if (actionIterator == source.actions.end())
                    return ValidationCode::binding_action_missing;

                const bool axisAction = actionIterator->value_kind
                    == ActionValueKind::axis;
                switch (binding.device)
                {
                case BindingDevice::keyboard:
                    if (!valid_key(binding.code)
                        || binding.controller_slot != 0u)
                    {
                        return ValidationCode::invalid_binding;
                    }
                    break;
                case BindingDevice::controller_button:
                    if (!valid_controller_button(binding.code)
                        || binding.controller_slot
                            >= limits.maximum_controller_slots
                        || binding.required_modifiers != 0u
                        || binding.forbidden_modifiers != 0u)
                    {
                        return ValidationCode::invalid_binding;
                    }
                    break;
                case BindingDevice::controller_axis:
                    if (!axisAction || !valid_controller_axis(binding.code)
                        || binding.controller_slot
                            >= limits.maximum_controller_slots
                        || binding.required_modifiers != 0u
                        || binding.forbidden_modifiers != 0u)
                    {
                        return ValidationCode::invalid_binding;
                    }
                    break;
                default:
                    return ValidationCode::invalid_binding;
                }

                if (binding.device != BindingDevice::controller_axis
                    && (binding.polarity != AxisPolarity::positive
                        || binding.response != AxisResponse::linear
                        || binding.dead_zone_q15 != 0u
                        || binding.saturation_q15 != normalized_unit))
                {
                    return ValidationCode::invalid_binding;
                }
                if (!axisAction
                    && (binding.device == BindingDevice::controller_axis
                        || binding.scale_q15 != normalized_unit))
                {
                    return ValidationCode::invalid_binding;
                }

                for (std::size_t prior = 0u; prior < index; ++prior)
                {
                    if (same_binding_source(source.bindings[prior], binding))
                        return ValidationCode::duplicate_binding_source;
                }

                const std::size_t actionIndex = semantic_index(
                    actionIterator->semantic);
                if (++bindingCounts[actionIndex]
                    > limits.maximum_bindings_per_action)
                {
                    return ValidationCode::binding_action_limit_exceeded;
                }
            }
            if (std::find(bindingCounts.begin(), bindingCounts.end(), 0u)
                != bindingCounts.end())
            {
                return ValidationCode::missing_action;
            }
            if (verifyHash)
            {
                if (!source.revision
                    || source.revision.content != profile_content_hash(source))
                {
                    return ValidationCode::content_hash_mismatch;
                }
            }
            return ValidationCode::ready;
        }

        void write_action(ByteWriter& writer, const ActionDefinition& action)
        {
            writer.u64(action.id.value);
            writer.u8(static_cast<std::uint8_t>(action.semantic));
            writer.u8(static_cast<std::uint8_t>(action.value_kind));
        }

        [[nodiscard]] ActionDefinition read_action(ByteReader& reader) noexcept
        {
            ActionDefinition action{};
            action.id.value = reader.u64();
            action.semantic = static_cast<ActionSemantic>(reader.u8());
            action.value_kind = static_cast<ActionValueKind>(reader.u8());
            return action;
        }

        void write_binding(
            ByteWriter& writer,
            const BindingDefinition& binding)
        {
            writer.u64(binding.id.value);
            writer.u64(binding.action.value);
            writer.u8(static_cast<std::uint8_t>(binding.device));
            writer.u16(binding.code);
            writer.u8(binding.controller_slot);
            writer.u8(static_cast<std::uint8_t>(binding.polarity));
            writer.u8(static_cast<std::uint8_t>(binding.response));
            writer.u8(binding.required_modifiers);
            writer.u8(binding.forbidden_modifiers);
            writer.i32(binding.scale_q15);
            writer.u16(binding.dead_zone_q15);
            writer.u16(binding.saturation_q15);
        }

        [[nodiscard]] BindingDefinition read_binding(ByteReader& reader)
            noexcept
        {
            BindingDefinition binding{};
            binding.id.value = reader.u64();
            binding.action.value = reader.u64();
            binding.device = static_cast<BindingDevice>(reader.u8());
            binding.code = reader.u16();
            binding.controller_slot = reader.u8();
            binding.polarity = static_cast<AxisPolarity>(reader.u8());
            binding.response = static_cast<AxisResponse>(reader.u8());
            binding.required_modifiers = reader.u8();
            binding.forbidden_modifiers = reader.u8();
            binding.scale_q15 = reader.i32();
            binding.dead_zone_q15 = reader.u16();
            binding.saturation_q15 = reader.u16();
            return binding;
        }

        void write_definition_body(
            ByteWriter& writer,
            std::string_view displayName,
            std::span<const ActionDefinition> actions,
            std::span<const BindingDefinition> bindings)
        {
            writer.text(displayName);
            writer.u32(static_cast<std::uint32_t>(actions.size()));
            for (const ActionDefinition& action : actions)
                write_action(writer, action);
            writer.u32(static_cast<std::uint32_t>(bindings.size()));
            for (const BindingDefinition& binding : bindings)
                write_binding(writer, binding);
        }

        [[nodiscard]] bool read_definition_body(
            ByteReader& reader,
            std::string& displayName,
            std::vector<ActionDefinition>& actions,
            std::vector<BindingDefinition>& bindings,
            const ProfileLimits& limits)
        {
            auto name = reader.text(limits.maximum_display_name_bytes);
            if (!name)
                return false;
            const std::uint32_t actionCount = reader.u32();
            if (!reader.valid() || actionCount > limits.maximum_actions)
                return false;
            actions.reserve(actionCount);
            for (std::uint32_t index = 0u; index < actionCount; ++index)
                actions.push_back(read_action(reader));

            const std::uint32_t bindingCount = reader.u32();
            if (!reader.valid() || bindingCount > limits.maximum_bindings)
                return false;
            bindings.reserve(bindingCount);
            for (std::uint32_t index = 0u; index < bindingCount; ++index)
                bindings.push_back(read_binding(reader));
            if (!reader.valid())
                return false;
            displayName = std::move(*name);
            return true;
        }

        [[nodiscard]] BindingDefinition digital_binding(
            std::string_view stableName,
            ActionSemantic semantic,
            BindingDevice device,
            std::uint16_t code,
            std::int32_t scale = normalized_unit,
            ModifierMask required = 0u,
            ModifierMask forbidden = 0u,
            std::uint8_t slot = 0u) noexcept
        {
            return {
                stable_binding_id(stableName),
                stable_action_id(semantic),
                device,
                code,
                slot,
                AxisPolarity::positive,
                AxisResponse::linear,
                required,
                forbidden,
                scale,
                0u,
                static_cast<std::uint16_t>(normalized_unit)};
        }

        [[nodiscard]] BindingDefinition controller_axis_binding(
            std::string_view stableName,
            ActionSemantic semantic,
            ControllerAxis axis,
            std::int32_t scale) noexcept
        {
            return {
                stable_binding_id(stableName),
                stable_action_id(semantic),
                BindingDevice::controller_axis,
                static_cast<std::uint16_t>(axis),
                0u,
                AxisPolarity::bipolar,
                AxisResponse::squared,
                0u,
                0u,
                scale,
                5'898u,
                31'129u};
        }

        [[nodiscard]] std::int32_t clamp_normalized(
            std::int64_t value) noexcept
        {
            return static_cast<std::int32_t>(std::clamp<std::int64_t>(
                value, -normalized_unit, normalized_unit));
        }

        [[nodiscard]] std::int32_t rounded_product(
            std::int32_t left,
            std::int32_t right) noexcept
        {
            const std::int64_t product = static_cast<std::int64_t>(left)
                * static_cast<std::int64_t>(right);
            if (product >= 0)
            {
                return static_cast<std::int32_t>(
                    (product + normalized_unit / 2) / normalized_unit);
            }
            return static_cast<std::int32_t>(
                -((-product + normalized_unit / 2) / normalized_unit));
        }

        [[nodiscard]] bool modifiers_match(
            ModifierMask active,
            const BindingDefinition& binding) noexcept
        {
            return (active & binding.required_modifiers)
                    == binding.required_modifiers
                && (active & binding.forbidden_modifiers) == 0u;
        }

        struct ReadFileResult final
        {
            StoreCode code{StoreCode::read_failure};
            std::vector<std::byte> bytes{};
        };

        [[nodiscard]] ReadFileResult read_file(
            const fs::path& path,
            std::uint64_t maximumBytes) noexcept
        {
            try
            {
                std::error_code error{};
                if (!fs::exists(path, error) || error)
                    return {error ? StoreCode::read_failure
                                  : StoreCode::not_found};
                if (!fs::is_regular_file(path, error) || error)
                    return {StoreCode::read_failure};
                const std::uintmax_t size = fs::file_size(path, error);
                if (error || size == 0u || size > maximumBytes
                    || size > static_cast<std::uintmax_t>(
                        (std::numeric_limits<std::streamsize>::max)()))
                {
                    return {StoreCode::integrity_failure};
                }
                std::vector<std::byte> bytes(static_cast<std::size_t>(size));
                std::ifstream input{path, std::ios::binary};
                if (!input)
                    return {StoreCode::read_failure};
                input.read(
                    reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
                if (!input || input.gcount()
                    != static_cast<std::streamsize>(bytes.size()))
                {
                    return {StoreCode::read_failure};
                }
                return {StoreCode::ready, std::move(bytes)};
            }
            catch (...)
            {
                return {StoreCode::read_failure};
            }
        }

        [[nodiscard]] StoreCode write_file(
            const fs::path& path,
            std::span<const std::byte> bytes) noexcept
        {
            try
            {
                std::ofstream output{
                    path, std::ios::binary | std::ios::trunc};
                if (!output)
                    return StoreCode::write_failure;
                output.write(
                    reinterpret_cast<const char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
                output.flush();
                if (!output)
                    return StoreCode::write_failure;
                output.close();
                return output ? StoreCode::ready : StoreCode::write_failure;
            }
            catch (...)
            {
                return StoreCode::write_failure;
            }
        }

        [[nodiscard]] std::uint64_t next_temporary_id() noexcept
        {
            static std::atomic<std::uint64_t> next{1u};
            std::uint64_t value = next.fetch_add(
                1u, std::memory_order_relaxed);
            if (value == 0u)
                value = next.fetch_add(1u, std::memory_order_relaxed);
            return value;
        }

        [[nodiscard]] fs::path temporary_path(const fs::path& destination)
        {
            fs::path temporary = destination;
            temporary += ".pending." + std::to_string(next_temporary_id());
            return temporary;
        }
    }

    ProfileId stable_profile_id(std::string_view stableName) noexcept
    {
        if (stableName.empty())
            return {};
        HashBuilder hash{};
        hash.text("epoch.project.input-profile.profile-id.v1");
        hash.text(stableName);
        return {hash.finish().words[0]};
    }

    ActionId stable_action_id(ActionSemantic semantic) noexcept
    {
        if (!valid_semantic(semantic))
            return {};
        HashBuilder hash{};
        hash.text("epoch.project.input-profile.action-id.v1");
        hash.text(action_semantic_name(semantic));
        return {hash.finish().words[0]};
    }

    BindingId stable_binding_id(std::string_view stableName) noexcept
    {
        if (stableName.empty())
            return {};
        HashBuilder hash{};
        hash.text("epoch.project.input-profile.binding-id.v1");
        hash.text(stableName);
        return {hash.finish().words[0]};
    }

    ActionValueKind expected_value_kind(ActionSemantic semantic) noexcept
    {
        return semantic == ActionSemantic::move_x
                || semantic == ActionSemantic::move_y
            ? ActionValueKind::axis
            : ActionValueKind::button;
    }

    ContentHash profile_content_hash(const ProfileSource& source) noexcept
    {
        HashBuilder hash{};
        hash.text("epoch.project.input-profile.source.v1");
        hash.u64(source.id.value);
        hash.text(source.display_name);
        hash.u64(static_cast<std::uint64_t>(source.actions.size()));
        for (const ActionDefinition& action : source.actions)
            append_action_hash(hash, action);
        hash.u64(static_cast<std::uint64_t>(source.bindings.size()));
        for (const BindingDefinition& binding : source.bindings)
            append_binding_hash(hash, binding);
        return hash.finish();
    }

    ValidationCode validate_profile_source(
        const ProfileSource& source,
        const ProfileLimits& limits) noexcept
    {
        try
        {
            return validate_source_shape(source, limits, true);
        }
        catch (...)
        {
            return ValidationCode::allocation_failure;
        }
    }

    ValidationCode seal_profile_source(
        ProfileSource& source,
        const ProfileLimits& limits) noexcept
    {
        try
        {
            std::sort(
                source.actions.begin(), source.actions.end(),
                [](const ActionDefinition& left, const ActionDefinition& right)
                {
                    return static_cast<std::uint8_t>(left.semantic)
                        < static_cast<std::uint8_t>(right.semantic);
                });
            std::sort(
                source.bindings.begin(), source.bindings.end(),
                [](const BindingDefinition& left,
                   const BindingDefinition& right)
                {
                    return left.id.value < right.id.value;
                });
            source.revision.content = {};
            const ValidationCode shape = validate_source_shape(
                source, limits, false);
            if (shape != ValidationCode::ready)
                return shape;
            source.revision.content = profile_content_hash(source);
            return validate_source_shape(source, limits, true);
        }
        catch (...)
        {
            return ValidationCode::allocation_failure;
        }
    }

    [[nodiscard]] static ProfileEditResult commit_profile_edit(
        ProfileSource source,
        const ProfileLimits& limits) noexcept
    {
        if (source.revision.sequence
            == (std::numeric_limits<std::uint64_t>::max)())
        {
            return {
                ProfileEditCode::revision_exhausted,
                ValidationCode::invalid_revision,
                {}};
        }
        ++source.revision.sequence;
        source.revision.content = {};
        const ValidationCode sealed = seal_profile_source(source, limits);
        if (sealed != ValidationCode::ready)
        {
            return {
                ProfileEditCode::validation_failed,
                sealed,
                {}};
        }
        return {
            ProfileEditCode::ready,
            ValidationCode::ready,
            std::move(source)};
    }

    ProfileEditResult rebind_keyboard(
        ProfileSource source,
        BindingId binding,
        KeyCode key,
        const ProfileLimits& limits) noexcept
    {
        try
        {
            const ValidationCode sourceCode =
                validate_profile_source(source, limits);
            if (sourceCode != ValidationCode::ready)
            {
                return {
                    ProfileEditCode::invalid_source,
                    sourceCode,
                    {}};
            }
            if (!binding)
            {
                return {
                    ProfileEditCode::invalid_binding,
                    ValidationCode::invalid_binding,
                    {}};
            }
            if (!valid_key_code(key))
            {
                return {
                    ProfileEditCode::invalid_key,
                    ValidationCode::invalid_binding,
                    {}};
            }
            const auto found = std::find_if(
                source.bindings.begin(),
                source.bindings.end(),
                [&](const BindingDefinition& candidate)
                {
                    return candidate.id == binding;
                });
            if (found == source.bindings.end())
            {
                return {
                    ProfileEditCode::invalid_binding,
                    ValidationCode::invalid_binding,
                    {}};
            }
            if (found->device != BindingDevice::keyboard)
            {
                return {
                    ProfileEditCode::unsupported_device,
                    ValidationCode::invalid_binding,
                    {}};
            }
            const auto code = static_cast<std::uint16_t>(key);
            if (found->code == code)
            {
                return {
                    ProfileEditCode::unchanged,
                    ValidationCode::ready,
                    std::move(source)};
            }
            found->code = code;
            return commit_profile_edit(std::move(source), limits);
        }
        catch (...)
        {
            return {
                ProfileEditCode::allocation_failure,
                ValidationCode::allocation_failure,
                {}};
        }
    }

    ProfileEditResult rebind_controller_button(
        ProfileSource source,
        BindingId binding,
        ControllerButton button,
        std::uint8_t controllerSlot,
        const ProfileLimits& limits) noexcept
    {
        try
        {
            const ValidationCode sourceCode =
                validate_profile_source(source, limits);
            if (sourceCode != ValidationCode::ready)
                return {ProfileEditCode::invalid_source, sourceCode, {}};
            if (!binding)
            {
                return {
                    ProfileEditCode::invalid_binding,
                    ValidationCode::invalid_binding,
                    {}};
            }
            if (!valid_controller_button(
                    static_cast<std::uint16_t>(button)))
            {
                return {
                    ProfileEditCode::invalid_controller_button,
                    ValidationCode::invalid_binding,
                    {}};
            }
            if (controllerSlot >= limits.maximum_controller_slots)
            {
                return {
                    ProfileEditCode::invalid_controller_slot,
                    ValidationCode::invalid_binding,
                    {}};
            }
            const auto found = std::find_if(
                source.bindings.begin(), source.bindings.end(),
                [&](const BindingDefinition& candidate)
                {
                    return candidate.id == binding;
                });
            if (found == source.bindings.end())
            {
                return {
                    ProfileEditCode::invalid_binding,
                    ValidationCode::invalid_binding,
                    {}};
            }
            if (found->device != BindingDevice::controller_button)
            {
                return {
                    ProfileEditCode::unsupported_device,
                    ValidationCode::invalid_binding,
                    {}};
            }
            const auto code = static_cast<std::uint16_t>(button);
            if (found->code == code
                && found->controller_slot == controllerSlot)
            {
                return {
                    ProfileEditCode::unchanged,
                    ValidationCode::ready,
                    std::move(source)};
            }
            found->code = code;
            found->controller_slot = controllerSlot;
            return commit_profile_edit(std::move(source), limits);
        }
        catch (...)
        {
            return {
                ProfileEditCode::allocation_failure,
                ValidationCode::allocation_failure,
                {}};
        }
    }

    ProfileEditResult rebind_controller_axis(
        ProfileSource source,
        BindingId binding,
        ControllerAxis axis,
        std::uint8_t controllerSlot,
        const ProfileLimits& limits) noexcept
    {
        try
        {
            const ValidationCode sourceCode =
                validate_profile_source(source, limits);
            if (sourceCode != ValidationCode::ready)
                return {ProfileEditCode::invalid_source, sourceCode, {}};
            if (!binding)
            {
                return {
                    ProfileEditCode::invalid_binding,
                    ValidationCode::invalid_binding,
                    {}};
            }
            if (!valid_controller_axis(static_cast<std::uint16_t>(axis)))
            {
                return {
                    ProfileEditCode::invalid_controller_axis,
                    ValidationCode::invalid_binding,
                    {}};
            }
            if (controllerSlot >= limits.maximum_controller_slots)
            {
                return {
                    ProfileEditCode::invalid_controller_slot,
                    ValidationCode::invalid_binding,
                    {}};
            }
            const auto found = std::find_if(
                source.bindings.begin(), source.bindings.end(),
                [&](const BindingDefinition& candidate)
                {
                    return candidate.id == binding;
                });
            if (found == source.bindings.end())
            {
                return {
                    ProfileEditCode::invalid_binding,
                    ValidationCode::invalid_binding,
                    {}};
            }
            if (found->device != BindingDevice::controller_axis)
            {
                return {
                    ProfileEditCode::unsupported_device,
                    ValidationCode::invalid_binding,
                    {}};
            }
            const auto code = static_cast<std::uint16_t>(axis);
            if (found->code == code
                && found->controller_slot == controllerSlot)
            {
                return {
                    ProfileEditCode::unchanged,
                    ValidationCode::ready,
                    std::move(source)};
            }
            found->code = code;
            found->controller_slot = controllerSlot;
            return commit_profile_edit(std::move(source), limits);
        }
        catch (...)
        {
            return {
                ProfileEditCode::allocation_failure,
                ValidationCode::allocation_failure,
                {}};
        }
    }

    ProfileEditResult set_controller_dead_zone(
        ProfileSource source,
        std::uint16_t deadZoneQ15,
        const ProfileLimits& limits) noexcept
    {
        try
        {
            const ValidationCode sourceCode =
                validate_profile_source(source, limits);
            if (sourceCode != ValidationCode::ready)
                return {ProfileEditCode::invalid_source, sourceCode, {}};
            if (deadZoneQ15 >= normalized_unit)
            {
                return {
                    ProfileEditCode::invalid_dead_zone,
                    ValidationCode::invalid_binding,
                    {}};
            }

            bool foundAxis{};
            bool changed{};
            for (auto& candidate : source.bindings)
            {
                if (candidate.device != BindingDevice::controller_axis)
                    continue;
                foundAxis = true;
                if (deadZoneQ15 >= candidate.saturation_q15)
                {
                    return {
                        ProfileEditCode::invalid_dead_zone,
                        ValidationCode::invalid_binding,
                        {}};
                }
                changed = changed
                    || candidate.dead_zone_q15 != deadZoneQ15;
                candidate.dead_zone_q15 = deadZoneQ15;
            }
            if (!foundAxis)
            {
                return {
                    ProfileEditCode::unsupported_device,
                    ValidationCode::invalid_binding,
                    {}};
            }
            if (!changed)
            {
                return {
                    ProfileEditCode::unchanged,
                    ValidationCode::ready,
                    std::move(source)};
            }
            return commit_profile_edit(std::move(source), limits);
        }
        catch (...)
        {
            return {
                ProfileEditCode::allocation_failure,
                ValidationCode::allocation_failure,
                {}};
        }
    }

    ProfileSource make_legacy_default_profile(
        std::uint64_t revisionSequence) noexcept
    {
        try
        {
            ProfileSource source{};
            source.id = stable_profile_id("legacy-default-2d");
            source.display_name = "Legacy Default 2D";
            source.revision.sequence = revisionSequence;
            for (std::uint8_t value =
                    static_cast<std::uint8_t>(ActionSemantic::move_x);
                value < static_cast<std::uint8_t>(ActionSemantic::count);
                ++value)
            {
                const ActionSemantic semantic =
                    static_cast<ActionSemantic>(value);
                source.actions.push_back({
                    stable_action_id(semantic),
                    semantic,
                    expected_value_kind(semantic)});
            }

            source.bindings = {
                digital_binding(
                    "legacy.move_x.a", ActionSemantic::move_x,
                    BindingDevice::keyboard,
                    static_cast<std::uint16_t>(KeyCode::a), -normalized_unit),
                digital_binding(
                    "legacy.move_x.d", ActionSemantic::move_x,
                    BindingDevice::keyboard,
                    static_cast<std::uint16_t>(KeyCode::d)),
                digital_binding(
                    "legacy.move_x.left", ActionSemantic::move_x,
                    BindingDevice::keyboard,
                    static_cast<std::uint16_t>(KeyCode::left), -normalized_unit),
                digital_binding(
                    "legacy.move_x.right", ActionSemantic::move_x,
                    BindingDevice::keyboard,
                    static_cast<std::uint16_t>(KeyCode::right)),
                controller_axis_binding(
                    "legacy.move_x.controller", ActionSemantic::move_x,
                    ControllerAxis::left_x, normalized_unit),
                digital_binding(
                    "legacy.move_y.w", ActionSemantic::move_y,
                    BindingDevice::keyboard,
                    static_cast<std::uint16_t>(KeyCode::w)),
                digital_binding(
                    "legacy.move_y.s", ActionSemantic::move_y,
                    BindingDevice::keyboard,
                    static_cast<std::uint16_t>(KeyCode::s), -normalized_unit),
                digital_binding(
                    "legacy.move_y.up", ActionSemantic::move_y,
                    BindingDevice::keyboard,
                    static_cast<std::uint16_t>(KeyCode::up)),
                digital_binding(
                    "legacy.move_y.down", ActionSemantic::move_y,
                    BindingDevice::keyboard,
                    static_cast<std::uint16_t>(KeyCode::down), -normalized_unit),
                controller_axis_binding(
                    "legacy.move_y.controller", ActionSemantic::move_y,
                    ControllerAxis::left_y, -normalized_unit),
                digital_binding(
                    "legacy.jump.space", ActionSemantic::jump,
                    BindingDevice::keyboard,
                    static_cast<std::uint16_t>(KeyCode::space)),
                digital_binding(
                    "legacy.jump.controller", ActionSemantic::jump,
                    BindingDevice::controller_button,
                    static_cast<std::uint16_t>(ControllerButton::south)),
                digital_binding(
                    "legacy.interact.e", ActionSemantic::interact,
                    BindingDevice::keyboard,
                    static_cast<std::uint16_t>(KeyCode::e)),
                digital_binding(
                    "legacy.interact.controller", ActionSemantic::interact,
                    BindingDevice::controller_button,
                    static_cast<std::uint16_t>(ControllerButton::west)),
                digital_binding(
                    "legacy.pause.p", ActionSemantic::pause,
                    BindingDevice::keyboard,
                    static_cast<std::uint16_t>(KeyCode::p)),
                digital_binding(
                    "legacy.pause.controller", ActionSemantic::pause,
                    BindingDevice::controller_button,
                    static_cast<std::uint16_t>(ControllerButton::start)),
                digital_binding(
                    "legacy.reset.r", ActionSemantic::reset,
                    BindingDevice::keyboard,
                    static_cast<std::uint16_t>(KeyCode::r)),
                digital_binding(
                    "legacy.reset.controller", ActionSemantic::reset,
                    BindingDevice::controller_button,
                    static_cast<std::uint16_t>(ControllerButton::back))};

            if (seal_profile_source(source) != ValidationCode::ready)
                return {};
            return source;
        }
        catch (...)
        {
            return {};
        }
    }

    SerializedProfile serialize_profile_source(
        const ProfileSource& source,
        const ProfileLimits& limits) noexcept
    {
        if (validate_profile_source(source, limits) != ValidationCode::ready)
            return {CodecCode::invalid_value};
        try
        {
            ByteWriter writer{limits.maximum_serialized_bytes};
            writer.raw(kSourceMagic);
            writer.u16(kCodecVersion);
            writer.u16(0u);
            writer.u64(source.id.value);
            writer.u64(source.revision.sequence);
            writer.hash(source.revision.content);
            write_definition_body(
                writer, source.display_name, source.actions, source.bindings);
            if (!writer.valid())
                return {CodecCode::serialized_budget_exceeded};
            return {CodecCode::ready, writer.release()};
        }
        catch (...)
        {
            return {CodecCode::allocation_failure};
        }
    }

    DeserializedProfile deserialize_profile_source(
        std::span<const std::byte> bytes,
        const ProfileLimits& limits) noexcept
    {
        if (!limits.valid() || bytes.empty()
            || bytes.size() > limits.maximum_serialized_bytes)
        {
            return {CodecCode::malformed_data};
        }
        try
        {
            ByteReader reader{bytes};
            if (!reader.matches(kSourceMagic))
                return {CodecCode::malformed_data};
            if (reader.u16() != kCodecVersion)
                return {CodecCode::unsupported_version};
            if (reader.u16() != 0u)
                return {CodecCode::malformed_data};

            ProfileSource source{};
            source.id.value = reader.u64();
            source.revision.sequence = reader.u64();
            source.revision.content = reader.hash();
            if (!read_definition_body(
                    reader,
                    source.display_name,
                    source.actions,
                    source.bindings,
                    limits))
            {
                return {CodecCode::malformed_data};
            }
            if (!reader.at_end())
                return {reader.valid() ? CodecCode::trailing_data
                                       : CodecCode::malformed_data};
            const ValidationCode valid = validate_profile_source(source, limits);
            if (valid == ValidationCode::content_hash_mismatch)
                return {CodecCode::integrity_failure};
            if (valid != ValidationCode::ready)
                return {CodecCode::malformed_data};
            return {CodecCode::ready, std::move(source)};
        }
        catch (...)
        {
            return {CodecCode::allocation_failure};
        }
    }

    ContentHash compiled_profile_hash(
        const CompiledInputProfile& artifact) noexcept
    {
        HashBuilder hash{};
        hash.text("epoch.project.input-profile.artifact.v1");
        hash.u64(artifact.project_key);
        hash.u64(artifact.profile_id.value);
        hash.text(artifact.display_name);
        hash.u64(artifact.source_revision.sequence);
        hash.hash(artifact.source_revision.content);
        hash.u64(static_cast<std::uint64_t>(artifact.actions.size()));
        for (const ActionDefinition& action : artifact.actions)
            append_action_hash(hash, action);
        hash.u64(static_cast<std::uint64_t>(artifact.bindings.size()));
        for (const BindingDefinition& binding : artifact.bindings)
            append_binding_hash(hash, binding);
        return hash.finish();
    }

    ValidationCode validate_compiled_profile(
        const CompiledInputProfile& artifact,
        const ProfileLimits& limits) noexcept
    {
        if (artifact.project_key == 0u || !artifact.profile_id
            || artifact.artifact_hash.empty())
        {
            return ValidationCode::invalid_profile;
        }
        try
        {
            ProfileSource source{
                artifact.profile_id,
                artifact.display_name,
                artifact.source_revision,
                artifact.actions,
                artifact.bindings};
            const ValidationCode sourceCode = validate_profile_source(
                source, limits);
            if (sourceCode != ValidationCode::ready)
                return sourceCode;
            return artifact.artifact_hash == compiled_profile_hash(artifact)
                ? ValidationCode::ready
                : ValidationCode::content_hash_mismatch;
        }
        catch (...)
        {
            return ValidationCode::allocation_failure;
        }
    }

    CompiledProfileResult compile_profile(
        std::string_view projectId,
        const ProfileSource& source,
        const ProfileLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        const ValidationCode valid = validate_profile_source(source, limits);
        if (valid != ValidationCode::ready)
            return {valid};
        const std::uint64_t projectKey = project_assets::stable_project_identity(
            projectId, registryLimits);
        if (projectKey == 0u)
            return {ValidationCode::invalid_profile};
        try
        {
            CompiledInputProfile artifact{
                projectKey,
                source.id,
                source.display_name,
                source.revision,
                {},
                source.actions,
                source.bindings};
            artifact.artifact_hash = compiled_profile_hash(artifact);
            const ValidationCode artifactCode = validate_compiled_profile(
                artifact, limits);
            return {artifactCode, std::move(artifact)};
        }
        catch (...)
        {
            return {ValidationCode::allocation_failure};
        }
    }

    SerializedProfile serialize_compiled_profile(
        const CompiledInputProfile& artifact,
        const ProfileLimits& limits) noexcept
    {
        if (validate_compiled_profile(artifact, limits)
            != ValidationCode::ready)
        {
            return {CodecCode::invalid_value};
        }
        try
        {
            ByteWriter writer{limits.maximum_serialized_bytes};
            writer.raw(kArtifactMagic);
            writer.u16(kCodecVersion);
            writer.u16(0u);
            writer.u64(artifact.project_key);
            writer.u64(artifact.profile_id.value);
            writer.u64(artifact.source_revision.sequence);
            writer.hash(artifact.source_revision.content);
            writer.hash(artifact.artifact_hash);
            write_definition_body(
                writer,
                artifact.display_name,
                artifact.actions,
                artifact.bindings);
            if (!writer.valid())
                return {CodecCode::serialized_budget_exceeded};
            return {CodecCode::ready, writer.release()};
        }
        catch (...)
        {
            return {CodecCode::allocation_failure};
        }
    }

    DeserializedCompiledProfile deserialize_compiled_profile(
        std::span<const std::byte> bytes,
        const ProfileLimits& limits) noexcept
    {
        if (!limits.valid() || bytes.empty()
            || bytes.size() > limits.maximum_serialized_bytes)
        {
            return {CodecCode::malformed_data};
        }
        try
        {
            ByteReader reader{bytes};
            if (!reader.matches(kArtifactMagic))
                return {CodecCode::malformed_data};
            if (reader.u16() != kCodecVersion)
                return {CodecCode::unsupported_version};
            if (reader.u16() != 0u)
                return {CodecCode::malformed_data};
            CompiledInputProfile artifact{};
            artifact.project_key = reader.u64();
            artifact.profile_id.value = reader.u64();
            artifact.source_revision.sequence = reader.u64();
            artifact.source_revision.content = reader.hash();
            artifact.artifact_hash = reader.hash();
            if (!read_definition_body(
                    reader,
                    artifact.display_name,
                    artifact.actions,
                    artifact.bindings,
                    limits))
            {
                return {CodecCode::malformed_data};
            }
            if (!reader.at_end())
                return {reader.valid() ? CodecCode::trailing_data
                                       : CodecCode::malformed_data};
            const ValidationCode valid = validate_compiled_profile(
                artifact, limits);
            if (valid == ValidationCode::content_hash_mismatch)
                return {CodecCode::integrity_failure};
            if (valid != ValidationCode::ready)
                return {CodecCode::malformed_data};
            return {CodecCode::ready, std::move(artifact)};
        }
        catch (...)
        {
            return {CodecCode::allocation_failure};
        }
    }

    std::int32_t apply_axis_response(
        std::int32_t rawValueQ15,
        const BindingDefinition& binding) noexcept
    {
        if (!valid_polarity(binding.polarity)
            || !valid_response(binding.response)
            || binding.scale_q15 == 0
            || binding.scale_q15 < -2 * normalized_unit
            || binding.scale_q15 > 2 * normalized_unit
            || binding.dead_zone_q15 >= binding.saturation_q15
            || binding.saturation_q15 > normalized_unit)
        {
            return 0;
        }
        rawValueQ15 = std::clamp(
            rawValueQ15, -normalized_unit, normalized_unit);
        std::int32_t polarized{};
        switch (binding.polarity)
        {
        case AxisPolarity::bipolar:
            polarized = rawValueQ15;
            break;
        case AxisPolarity::positive:
            polarized = (std::max)(rawValueQ15, 0);
            break;
        case AxisPolarity::negative:
            polarized = (std::max)(-rawValueQ15, 0);
            break;
        }
        const std::int32_t sign = polarized < 0 ? -1 : 1;
        const std::int32_t magnitude = polarized < 0
            ? -polarized : polarized;
        if (magnitude <= binding.dead_zone_q15)
            return 0;

        const std::int32_t range = static_cast<std::int32_t>(
            binding.saturation_q15 - binding.dead_zone_q15);
        std::int32_t shaped = magnitude >= binding.saturation_q15
            ? normalized_unit
            : static_cast<std::int32_t>(
                (static_cast<std::int64_t>(
                    magnitude - binding.dead_zone_q15) * normalized_unit
                    + range / 2) / range);
        if (binding.response == AxisResponse::squared)
        {
            shaped = rounded_product(shaped, shaped);
        }
        else if (binding.response == AxisResponse::cubic)
        {
            const std::int32_t squared = rounded_product(shaped, shaped);
            shaped = rounded_product(squared, shaped);
        }
        shaped *= sign;
        return clamp_normalized(rounded_product(shaped, binding.scale_q15));
    }

    ActionFrame evaluate_action_frame(
        const CompiledInputProfile& artifact,
        const InputSnapshot& input,
        const ProfileLimits& limits) noexcept
    {
        if (validate_compiled_profile(artifact, limits)
            != ValidationCode::ready)
        {
            return {EvaluationCode::invalid_artifact, input.frame_index};
        }
        const std::size_t sampleCount = input.keyboard.size()
            + input.controller_buttons.size() + input.controller_axes.size();
        if (sampleCount > limits.maximum_input_samples)
            return {EvaluationCode::input_limit_exceeded, input.frame_index};
        if ((input.modifiers & ~kAllModifiers) != 0u)
            return {EvaluationCode::invalid_input, input.frame_index};

        for (std::size_t index = 0u; index < input.keyboard.size(); ++index)
        {
            const KeyboardSample& sample = input.keyboard[index];
            if (!valid_key(static_cast<std::uint16_t>(sample.key))
                || (sample.pressed && !sample.held))
            {
                return {EvaluationCode::invalid_input, input.frame_index};
            }
            for (std::size_t prior = 0u; prior < index; ++prior)
            {
                if (input.keyboard[prior].key == sample.key)
                    return {EvaluationCode::duplicate_input, input.frame_index};
            }
        }
        for (std::size_t index = 0u;
            index < input.controller_buttons.size(); ++index)
        {
            const ControllerButtonSample& sample =
                input.controller_buttons[index];
            if (sample.controller_slot >= limits.maximum_controller_slots
                || !valid_controller_button(
                    static_cast<std::uint16_t>(sample.button))
                || (sample.pressed && !sample.held))
            {
                return {EvaluationCode::invalid_input, input.frame_index};
            }
            for (std::size_t prior = 0u; prior < index; ++prior)
            {
                const ControllerButtonSample& other =
                    input.controller_buttons[prior];
                if (other.controller_slot == sample.controller_slot
                    && other.button == sample.button)
                {
                    return {EvaluationCode::duplicate_input, input.frame_index};
                }
            }
        }
        for (std::size_t index = 0u;
            index < input.controller_axes.size(); ++index)
        {
            const ControllerAxisSample& sample = input.controller_axes[index];
            if (sample.controller_slot >= limits.maximum_controller_slots
                || !valid_controller_axis(
                    static_cast<std::uint16_t>(sample.axis))
                || sample.value_q15 == (std::numeric_limits<std::int16_t>::min)())
            {
                return {EvaluationCode::invalid_input, input.frame_index};
            }
            for (std::size_t prior = 0u; prior < index; ++prior)
            {
                const ControllerAxisSample& other =
                    input.controller_axes[prior];
                if (other.controller_slot == sample.controller_slot
                    && other.axis == sample.axis)
                {
                    return {EvaluationCode::duplicate_input, input.frame_index};
                }
            }
        }

        try
        {
            ActionFrame frame{
                EvaluationCode::ready,
                input.frame_index,
                {}};
            frame.actions.reserve(artifact.actions.size());
            for (const ActionDefinition& action : artifact.actions)
                frame.actions.push_back({action.id, 0, false});

            for (const BindingDefinition& binding : artifact.bindings)
            {
                const auto action = std::find_if(
                    artifact.actions.begin(), artifact.actions.end(),
                    [&](const ActionDefinition& candidate)
                    {
                        return candidate.id == binding.action;
                    });
                const std::size_t actionIndex = static_cast<std::size_t>(
                    action - artifact.actions.begin());
                std::int32_t raw{};
                bool pressed{};

                switch (binding.device)
                {
                case BindingDevice::keyboard:
                    if (!modifiers_match(input.modifiers, binding))
                        break;
                    for (const KeyboardSample& sample : input.keyboard)
                    {
                        if (static_cast<std::uint16_t>(sample.key)
                            == binding.code)
                        {
                            raw = sample.held ? normalized_unit : 0;
                            pressed = sample.pressed;
                            break;
                        }
                    }
                    break;
                case BindingDevice::controller_button:
                    for (const ControllerButtonSample& sample
                        : input.controller_buttons)
                    {
                        if (sample.controller_slot == binding.controller_slot
                            && static_cast<std::uint16_t>(sample.button)
                                == binding.code)
                        {
                            raw = sample.held ? normalized_unit : 0;
                            pressed = sample.pressed;
                            break;
                        }
                    }
                    break;
                case BindingDevice::controller_axis:
                    for (const ControllerAxisSample& sample
                        : input.controller_axes)
                    {
                        if (sample.controller_slot == binding.controller_slot
                            && static_cast<std::uint16_t>(sample.axis)
                                == binding.code)
                        {
                            raw = sample.value_q15;
                            break;
                        }
                    }
                    break;
                }

                const std::int32_t contribution = apply_axis_response(
                    raw, binding);
                ActionValue& output = frame.actions[actionIndex];
                output.value_q15 = clamp_normalized(
                    static_cast<std::int64_t>(output.value_q15)
                    + contribution);
                output.pressed = output.pressed || pressed;
            }
            return frame;
        }
        catch (...)
        {
            return {EvaluationCode::allocation_failure, input.frame_index};
        }
    }

    InjectionCode inject_action_impulses(
        const CompiledInputProfile& artifact,
        ActionFrame& frame,
        std::span<const ActionImpulse> impulses,
        const ProfileLimits& limits) noexcept
    {
        if (validate_compiled_profile(artifact, limits)
            != ValidationCode::ready)
        {
            return InjectionCode::invalid_artifact;
        }
        if (!frame || frame.actions.size() != artifact.actions.size())
            return InjectionCode::invalid_frame;
        if (impulses.size() > artifact.actions.size()
            || impulses.size() > limits.maximum_actions)
        {
            return InjectionCode::action_limit_exceeded;
        }

        for (std::size_t index = 0u; index < impulses.size(); ++index)
        {
            const ActionImpulse& impulse = impulses[index];
            if (!valid_semantic(impulse.semantic)
                || impulse.value_q15 < -normalized_unit
                || impulse.value_q15 > normalized_unit)
            {
                return InjectionCode::invalid_action;
            }
            for (std::size_t prior = 0u; prior < index; ++prior)
            {
                if (impulses[prior].semantic == impulse.semantic)
                    return InjectionCode::duplicate_action;
            }

            const ActionId id = stable_action_id(impulse.semantic);
            const auto artifactAction = std::find_if(
                artifact.actions.begin(), artifact.actions.end(),
                [&](const ActionDefinition& action)
                {
                    return action.id == id
                        && action.semantic == impulse.semantic;
                });
            if (artifactAction == artifact.actions.end())
                return InjectionCode::action_missing;
            const auto frameAction = std::find_if(
                frame.actions.begin(), frame.actions.end(),
                [&](const ActionValue& action)
                {
                    return action.action == id;
                });
            if (frameAction == frame.actions.end())
                return InjectionCode::invalid_frame;
        }

        for (const ActionImpulse& impulse : impulses)
        {
            const ActionId id = stable_action_id(impulse.semantic);
            const auto frameAction = std::find_if(
                frame.actions.begin(), frame.actions.end(),
                [&](const ActionValue& action)
                {
                    return action.action == id;
                });
            frameAction->value_q15 = clamp_normalized(
                static_cast<std::int64_t>(frameAction->value_q15)
                    + impulse.value_q15);
            frameAction->pressed = frameAction->pressed || impulse.pressed;
        }
        return InjectionCode::ready;
    }

    const ActionValue* find_action(
        const ActionFrame& frame,
        ActionSemantic semantic,
        const CompiledInputProfile& artifact) noexcept
    {
        if (!frame || !valid_semantic(semantic))
            return nullptr;
        const ActionId id = stable_action_id(semantic);
        const auto artifactAction = std::find_if(
            artifact.actions.begin(), artifact.actions.end(),
            [&](const ActionDefinition& action)
            {
                return action.id == id && action.semantic == semantic;
            });
        if (artifactAction == artifact.actions.end())
            return nullptr;
        const auto frameAction = std::find_if(
            frame.actions.begin(), frame.actions.end(),
            [&](const ActionValue& action)
            {
                return action.action == id;
            });
        return frameAction == frame.actions.end() ? nullptr : &*frameAction;
    }

    ProjectInputProfileStore::ProjectInputProfileStore(
        std::string projectId,
        std::filesystem::path projectRoot,
        StoreLimits limits) noexcept
        : project_id_(std::move(projectId)),
          project_root_(std::move(projectRoot)),
          limits_(limits)
    {
        if (!limits_.valid() || project_root_.empty())
            return;
        project_key_ = project_assets::stable_project_identity(
            project_id_, limits_.registry);
        if (project_key_ == 0u)
            return;
        try
        {
            std::error_code error{};
            if (project_root_.is_relative())
                project_root_ = fs::absolute(project_root_, error);
            if (error || project_root_.empty())
            {
                project_key_ = 0u;
                project_root_.clear();
                return;
            }
            project_root_ = project_root_.lexically_normal();
        }
        catch (...)
        {
            project_key_ = 0u;
            project_root_.clear();
        }
    }

    bool ProjectInputProfileStore::valid() const noexcept
    {
        return project_key_ != 0u && !project_root_.empty()
            && limits_.valid();
    }

    std::string_view ProjectInputProfileStore::project_id() const noexcept
    {
        return project_id_;
    }

    std::uint64_t ProjectInputProfileStore::project_key() const noexcept
    {
        return project_key_;
    }

    const std::filesystem::path&
        ProjectInputProfileStore::project_root() const noexcept
    {
        return project_root_;
    }

    std::filesystem::path ProjectInputProfileStore::source_path() const
    {
        return project_root_ / fs::path{canonical_source_path};
    }

    std::filesystem::path ProjectInputProfileStore::artifact_path() const
    {
        return project_root_ / fs::path{canonical_artifact_path};
    }

    const StoreLimits& ProjectInputProfileStore::limits() const noexcept
    {
        return limits_;
    }

    StoreMetrics ProjectInputProfileStore::metrics() const noexcept
    {
        return metrics_;
    }

    StoredProfile ProjectInputProfileStore::save_source(
        const ProfileSource& source) noexcept
    {
        ++metrics_.source_save_requests;
        if (!valid())
            return reject_source(StoreCode::invalid_store);
        const SerializedProfile serialized = serialize_profile_source(
            source, limits_.profile);
        if (!serialized)
            return reject_source(StoreCode::invalid_value);
        try
        {
            const fs::path destination = source_path();
            std::error_code error{};
            fs::create_directories(destination.parent_path(), error);
            if (error || !fs::is_directory(destination.parent_path(), error)
                || error)
            {
                return reject_source(StoreCode::directory_failure);
            }

            const ReadFileResult existing = read_file(
                destination, limits_.profile.maximum_serialized_bytes);
            if (existing.code == StoreCode::ready)
            {
                metrics_.bytes_read += existing.bytes.size();
                const DeserializedProfile decoded = deserialize_profile_source(
                    existing.bytes, limits_.profile);
                if (!decoded)
                    return reject_source(StoreCode::integrity_failure);
                if (existing.bytes == serialized.bytes
                    && decoded.source == source)
                {
                    ++metrics_.unchanged_writes;
                    return {
                        StoreCode::unchanged,
                        destination,
                        source.revision,
                        serialized.bytes.size()};
                }
                if (decoded.source.revision.sequence
                    > source.revision.sequence)
                {
                    return reject_source(StoreCode::stale_revision);
                }
                if (decoded.source.revision.sequence
                    == source.revision.sequence)
                {
                    return reject_source(StoreCode::revision_conflict);
                }
            }
            else if (existing.code != StoreCode::not_found)
            {
                return reject_source(existing.code);
            }

            const fs::path temporary = temporary_path(destination);
            const StoreCode written = write_file(temporary, serialized.bytes);
            if (written != StoreCode::ready)
            {
                fs::remove(temporary, error);
                return reject_source(written);
            }
            const ReadFileResult verified = read_file(
                temporary, limits_.profile.maximum_serialized_bytes);
            metrics_.bytes_read += verified.bytes.size();
            const DeserializedProfile decoded = verified.code == StoreCode::ready
                ? deserialize_profile_source(verified.bytes, limits_.profile)
                : DeserializedProfile{};
            if (verified.code != StoreCode::ready
                || verified.bytes != serialized.bytes
                || !decoded || decoded.source != source)
            {
                fs::remove(temporary, error);
                return reject_source(StoreCode::integrity_failure);
            }
            if (!platform::filesystem::atomic_replace_same_filesystem(
                    temporary, destination, error))
            {
                fs::remove(temporary, error);
                return reject_source(StoreCode::atomic_replace_failure);
            }
            ++metrics_.source_saves;
            metrics_.bytes_written += serialized.bytes.size();
            return {
                StoreCode::ready,
                destination,
                source.revision,
                serialized.bytes.size()};
        }
        catch (...)
        {
            return reject_source(StoreCode::allocation_failure);
        }
    }

    LoadedProfile ProjectInputProfileStore::load_source() noexcept
    {
        ++metrics_.source_load_requests;
        if (!valid())
            return reject_source_load(StoreCode::invalid_store);
        try
        {
            const fs::path path = source_path();
            ReadFileResult read = read_file(
                path, limits_.profile.maximum_serialized_bytes);
            if (read.code != StoreCode::ready)
                return reject_source_load(read.code);
            metrics_.bytes_read += read.bytes.size();
            DeserializedProfile decoded = deserialize_profile_source(
                read.bytes, limits_.profile);
            if (!decoded)
                return reject_source_load(StoreCode::integrity_failure);
            ++metrics_.source_loads;
            return {
                StoreCode::ready,
                path,
                std::move(decoded.source)};
        }
        catch (...)
        {
            return reject_source_load(StoreCode::allocation_failure);
        }
    }

    StoredArtifact ProjectInputProfileStore::publish_artifact(
        const CompiledInputProfile& artifact) noexcept
    {
        ++metrics_.artifact_save_requests;
        if (!valid())
            return reject_artifact(StoreCode::invalid_store);
        if (artifact.project_key != project_key_)
            return reject_artifact(StoreCode::invalid_value);
        const SerializedProfile serialized = serialize_compiled_profile(
            artifact, limits_.profile);
        if (!serialized)
            return reject_artifact(StoreCode::invalid_value);
        try
        {
            const fs::path destination = artifact_path();
            std::error_code error{};
            fs::create_directories(destination.parent_path(), error);
            if (error || !fs::is_directory(destination.parent_path(), error)
                || error)
            {
                return reject_artifact(StoreCode::directory_failure);
            }
            const ReadFileResult existing = read_file(
                destination, limits_.profile.maximum_serialized_bytes);
            if (existing.code == StoreCode::ready)
            {
                metrics_.bytes_read += existing.bytes.size();
                const DeserializedCompiledProfile decoded =
                    deserialize_compiled_profile(existing.bytes, limits_.profile);
                if (!decoded)
                    return reject_artifact(StoreCode::integrity_failure);
                if (existing.bytes == serialized.bytes
                    && decoded.artifact == artifact)
                {
                    ++metrics_.unchanged_writes;
                    return {
                        StoreCode::unchanged,
                        destination,
                        artifact.artifact_hash,
                        artifact.source_revision,
                        serialized.bytes.size()};
                }
                if (decoded.artifact.source_revision.sequence
                    > artifact.source_revision.sequence)
                {
                    return reject_artifact(StoreCode::stale_revision);
                }
                if (decoded.artifact.source_revision.sequence
                    == artifact.source_revision.sequence)
                {
                    return reject_artifact(StoreCode::revision_conflict);
                }
            }
            else if (existing.code != StoreCode::not_found)
            {
                return reject_artifact(existing.code);
            }

            const fs::path temporary = temporary_path(destination);
            const StoreCode written = write_file(temporary, serialized.bytes);
            if (written != StoreCode::ready)
            {
                fs::remove(temporary, error);
                return reject_artifact(written);
            }
            const ReadFileResult verified = read_file(
                temporary, limits_.profile.maximum_serialized_bytes);
            metrics_.bytes_read += verified.bytes.size();
            const DeserializedCompiledProfile decoded =
                verified.code == StoreCode::ready
                ? deserialize_compiled_profile(verified.bytes, limits_.profile)
                : DeserializedCompiledProfile{};
            if (verified.code != StoreCode::ready
                || verified.bytes != serialized.bytes
                || !decoded || decoded.artifact != artifact)
            {
                fs::remove(temporary, error);
                return reject_artifact(StoreCode::integrity_failure);
            }
            if (!platform::filesystem::atomic_replace_same_filesystem(
                    temporary, destination, error))
            {
                fs::remove(temporary, error);
                return reject_artifact(StoreCode::atomic_replace_failure);
            }
            ++metrics_.artifact_saves;
            metrics_.bytes_written += serialized.bytes.size();
            return {
                StoreCode::ready,
                destination,
                artifact.artifact_hash,
                artifact.source_revision,
                serialized.bytes.size()};
        }
        catch (...)
        {
            return reject_artifact(StoreCode::allocation_failure);
        }
    }

    LoadedArtifact ProjectInputProfileStore::load_artifact() noexcept
    {
        ++metrics_.artifact_load_requests;
        if (!valid())
            return reject_artifact_load(StoreCode::invalid_store);
        try
        {
            const fs::path path = artifact_path();
            ReadFileResult read = read_file(
                path, limits_.profile.maximum_serialized_bytes);
            if (read.code != StoreCode::ready)
                return reject_artifact_load(read.code);
            metrics_.bytes_read += read.bytes.size();
            DeserializedCompiledProfile decoded = deserialize_compiled_profile(
                read.bytes, limits_.profile);
            if (!decoded || decoded.artifact.project_key != project_key_)
                return reject_artifact_load(StoreCode::integrity_failure);
            ++metrics_.artifact_loads;
            return {
                StoreCode::ready,
                path,
                std::move(decoded.artifact)};
        }
        catch (...)
        {
            return reject_artifact_load(StoreCode::allocation_failure);
        }
    }

    StoredProfile ProjectInputProfileStore::reject_source(
        StoreCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {code};
    }

    LoadedProfile ProjectInputProfileStore::reject_source_load(
        StoreCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {code};
    }

    StoredArtifact ProjectInputProfileStore::reject_artifact(
        StoreCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {code};
    }

    LoadedArtifact ProjectInputProfileStore::reject_artifact_load(
        StoreCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {code};
    }
}
