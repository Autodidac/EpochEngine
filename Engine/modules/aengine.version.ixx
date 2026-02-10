module;

export module aengine.version;

import <array>;
import <cstdio>;
import <string>;
import <string_view>;

export namespace almondnamespace
{
    // ─────────────────────────────────────────────
    // Version constants (compile-time safe)
    // ─────────────────────────────────────────────

    export constexpr int major = 0;
    export constexpr int minor = 81;
    export constexpr int revision = 23;

    export constexpr std::string_view kEngineName = "Almond Shell";

    // ─────────────────────────────────────────────
    // Accessors
    // ─────────────────────────────────────────────

    export constexpr int GetMajor() noexcept
    {
        return major;
    }

    export constexpr int GetMinor() noexcept
    {
        return minor;
    }

    export constexpr int GetRevision() noexcept
    {
        return revision;
    }

    export constexpr std::string_view GetEngineNameView() noexcept
    {
        return kEngineName;
    }

    export const char* GetEngineName() noexcept
    {
        return kEngineName.data();
    }

    // ─────────────────────────────────────────────
    // Version formatting
    // ─────────────────────────────────────────────

    export const char* GetEngineVersion() noexcept
    {
        thread_local std::array<char, 32> buffer{};
        std::snprintf(
            buffer.data(),
            buffer.size(),
            "%d.%d.%d",
            major,
            minor,
            revision
        );
        return buffer.data();
    }

    export std::string GetEngineVersionString()
    {
        return std::string{ GetEngineVersion() };
    }
}

// Legacy alias (kept intentionally)
export namespace almondshell = almondnamespace;
