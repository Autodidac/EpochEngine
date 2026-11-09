// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace epochengine::ai::model_behavior
{
    // Publisher/name labels describe a model variant, never tool permissions
    // or proof that an unlabelled model is safe, aligned or suitable for a task.
    struct Classification final
    {
        std::uint8_t labels{};
        [[nodiscard]] constexpr bool flagged() const noexcept { return labels != 0u; }
        friend constexpr bool operator==(Classification, Classification) = default;
    };
    inline constexpr std::uint8_t uncensored = 1u, abliterated = 2u, derestricted = 4u;
    inline constexpr std::string_view warning =
        "This model is labelled uncensored, abliterated or derestricted. "
        "Its safeguards may have been changed or removed. Choose it explicitly. "
        "The label does not grant tool access; your approval settings still apply.";

    namespace detail
    {
        [[nodiscard]] constexpr char lower(char value) noexcept
        { return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value; }
        [[nodiscard]] constexpr bool alphanumeric(char value) noexcept
        { return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9'); }
        [[nodiscard]] constexpr bool contains(std::string_view text, std::string_view label) noexcept
        {
            for (std::size_t start{}; start < text.size(); ++start)
            {
                if (lower(text[start]) != label.front()) continue;
                std::size_t matched{};
                for (std::size_t index = start; index < text.size(); ++index)
                {
                    const auto value = lower(text[index]);
                    if (!alphanumeric(value)) continue;
                    if (value != label[matched]) break;
                    if (++matched == label.size()) return true;
                }
            }
            return false;
        }
    }

    [[nodiscard]] constexpr Classification classify_text(std::string_view text,
        Classification result = {}) noexcept
    {
        if (detail::contains(text, "uncensored")) result.labels |= uncensored;
        if (detail::contains(text, "abliterated")) result.labels |= abliterated;
        if (detail::contains(text, "derestricted")) result.labels |= derestricted;
        return result;
    }

    [[nodiscard]] inline Classification classify(std::span<const std::string_view> identities,
        std::span<const std::string> behavior_labels = {}, std::span<const std::string> tags = {}) noexcept
    {
        Classification result;
        for (const auto value : identities) result = classify_text(value, result);
        for (const auto& value : behavior_labels) result = classify_text(value, result);
        for (const auto& value : tags) result = classify_text(value, result);
        return result;
    }

    [[nodiscard]] inline std::string summary(Classification value)
    {
        std::string result;
        for (const auto& [flag, label] : {
                std::pair{uncensored, std::string_view{"Uncensored"}},
                std::pair{abliterated, std::string_view{"Abliterated"}},
                std::pair{derestricted, std::string_view{"Derestricted"}}})
        {
            if ((value.labels & flag) == 0u) continue;
            if (!result.empty()) result += " / ";
            result += label;
        }
        return result;
    }

    [[nodiscard]] inline std::string choice_label(std::string_view name, Classification value)
    {
        std::string result{name};
        if (value.flagged()) result += " [" + summary(value) + "]";
        return result;
    }
}
