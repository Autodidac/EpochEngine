// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

export module editor.candidate_artifact;
import platform.child_process;

// Host-owned execution evidence, not a model permission or OS sandbox.
// Callers inspect real files and reject redirects before submitting bindings.
export namespace epochengine::editor::candidate_artifact
{
    enum class Lane : unsigned char { DebugEditor, ReleaseEditor, HeadlessCi, FullValidation };

    struct Binding final
    {
        std::string workspace_root{};
        std::uint32_t generation{};
        Lane lane{Lane::DebugEditor};
        std::string executable_path{};
        platform::child_process::ExecutableIdentity identity{};
        std::uint64_t build_ticket{};
        friend bool operator==(const Binding&, const Binding&) = default;
    };

    [[nodiscard]] inline std::string_view lane_name(Lane lane) noexcept
    {
        switch (lane)
        {
        case Lane::DebugEditor: return "debug_editor";
        case Lane::ReleaseEditor: return "release_editor";
        case Lane::HeadlessCi: return "headless_ci";
        case Lane::FullValidation: return "full_validation";
        }
        return "invalid";
    }

    [[nodiscard]] inline std::string_view relative_executable(Lane lane) noexcept
    {
        switch (lane)
        {
        case Lane::DebugEditor: return "x64/Debug/EpochEditor.exe";
        case Lane::ReleaseEditor:
        case Lane::FullValidation: return "x64/Release/EpochEditor.exe";
        case Lane::HeadlessCi: return "x64/Debug/HeadlessCI.exe";
        }
        return {};
    }

    [[nodiscard]] inline bool valid_root(std::string_view root)
    {
        if (root.empty() || root.size() > 32768u || root.back() == '/'
            || root.find_first_of("\r\n\0", 0u, 3u) != std::string_view::npos)
            return false;
        const std::filesystem::path path{root};
        return path.is_absolute() && path != path.root_path()
            && path.lexically_normal().generic_string() == root;
    }

    [[nodiscard]] inline bool valid_binding(const Binding& binding)
    {
        return binding.generation != 0 && binding.build_ticket != 0
            && valid_root(binding.workspace_root) && binding.identity.valid()
            && !relative_executable(binding.lane).empty()
            && binding.executable_path == binding.workspace_root + "/"
                + std::string{relative_executable(binding.lane)};
    }

    // Local audit text only. This is never parsed to restore execution authority.
    [[nodiscard]] inline std::string canonical_evidence(const Binding& binding)
    {
        const auto quote = [](std::string_view value)
        {
            std::string out{"\""};
            for (const unsigned char byte : value)
            {
                if (byte == '"' || byte == '\\') out.push_back('\\');
                if (byte < 0x20u)
                {
                    constexpr char hex[] = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(hex[byte >> 4u]);
                    out.push_back(hex[byte & 15u]);
                }
                else out.push_back(static_cast<char>(byte));
            }
            out.push_back('"');
            return out;
        };
        return "{\"schema\":\"epoch.candidate-executable/v1\",\"workspace\":"
            + quote(binding.workspace_root) + ",\"generation\":"
            + std::to_string(binding.generation) + ",\"build_ticket\":"
            + std::to_string(binding.build_ticket) + ",\"lane\":"
            + quote(lane_name(binding.lane)) + ",\"executable\":"
            + quote(binding.executable_path) + ",\"size_bytes\":"
            + std::to_string(binding.identity.size_bytes) + ",\"sha256\":"
            + quote(binding.identity.sha256) + "}";
    }

    class Ledger final
    {
        struct Proof final
        {
            std::uint64_t ticket{};
            std::optional<Binding> binding{};
            bool contract_passed{};
        };
        std::string workspace_{};
        std::uint32_t generation_{};
        std::uint64_t next_ticket_{};
        std::array<Proof, 3> proofs_{};
        bool full_validation_passed_{};

        [[nodiscard]] bool matches(std::string_view root, std::uint32_t generation) const noexcept
        {
            return generation_ != 0 && generation == generation_ && root == workspace_;
        }
        [[nodiscard]] bool contracts_passed() const noexcept
        {
            for (const auto& proof : proofs_)
                if (!proof.binding || !proof.contract_passed) return false;
            return true;
        }

    public:
        void invalidate() noexcept
        {
            workspace_.clear();
            generation_ = 0;
            proofs_ = {};
            full_validation_passed_ = false;
            // Never recycle a dispatched build ticket, even for the same root.
        }

        [[nodiscard]] bool reset(std::string root, std::uint32_t generation)
        {
            invalidate();
            if (generation == 0 || !valid_root(root)) return false;
            workspace_ = std::move(root);
            generation_ = generation;
            return true;
        }

        [[nodiscard]] std::uint64_t begin_build(Lane lane) noexcept
        {
            const auto index = static_cast<unsigned>(lane);
            if (index >= proofs_.size() || generation_ == 0) return 0;
            for (auto i = index; i < proofs_.size(); ++i) proofs_[i] = {};
            full_validation_passed_ = false;
            if (next_ticket_ == (std::numeric_limits<std::uint64_t>::max)())
                return 0;
            proofs_[index].ticket = ++next_ticket_;
            return next_ticket_;
        }

        [[nodiscard]] bool accept_build(Binding binding)
        {
            const auto index = static_cast<unsigned>(binding.lane);
            if (index >= proofs_.size() || !valid_binding(binding)
                || !matches(binding.workspace_root, binding.generation)
                || proofs_[index].ticket != binding.build_ticket
                || proofs_[index].binding)
                return false;
            proofs_[index].binding = std::move(binding);
            proofs_[index].contract_passed = false;
            full_validation_passed_ = false;
            return true;
        }

        [[nodiscard]] std::optional<Binding> for_test(
            std::string_view root, std::uint32_t generation, Lane lane) const
        {
            if (!matches(root, generation)) return std::nullopt;
            const bool full = lane == Lane::FullValidation;
            const auto index = full ? 1u : static_cast<unsigned>(lane);
            if (index >= proofs_.size() || !proofs_[index].binding
                || (full && !contracts_passed())) return std::nullopt;
            auto binding = proofs_[index].binding;
            binding->lane = lane;
            return binding;
        }

        [[nodiscard]] bool accept_test(
            const Binding& binding,
            const platform::child_process::ExecutableIdentity& before,
            const platform::child_process::ExecutableIdentity& after,
            bool passed)
        {
            const auto expected = for_test(binding.workspace_root, binding.generation, binding.lane);
            if (!expected || *expected != binding) return false;
            const bool full = binding.lane == Lane::FullValidation;
            const auto index = full ? 1u : static_cast<unsigned>(binding.lane);
            if (!passed || before != binding.identity || after != binding.identity)
            {
                proofs_[index] = {};
                full_validation_passed_ = false;
                return false;
            }
            if (full) full_validation_passed_ = true;
            else proofs_[index].contract_passed = true;
            return true;
        }

        [[nodiscard]] std::optional<Binding> for_preview(
            std::string_view root, std::uint32_t generation) const
        {
            if (!matches(root, generation) || !contracts_passed()
                || !full_validation_passed_) return std::nullopt;
            return proofs_[1].binding;
        }
    };
}
