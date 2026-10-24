#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <glad/glad.h>

namespace epochengine::openglcontext::frame_admission
{
    // Bound submitted GPU work, not frames per second. A completed fence never
    // adds a deadline; only a GPU that has not finished the oldest frame waits.
    inline constexpr std::size_t frames_in_flight = 2u;
    inline constexpr GLuint64 wait_slice_ns = 1'000'000u;

    struct State final
    {
        std::array<GLsync, frames_in_flight> fences{};
        std::size_t next{};
    };

    enum class AdmissionStatus : std::uint8_t { ready, cancelled, failed };

    struct Result final
    {
        AdmissionStatus status = AdmissionStatus::ready;
        std::uint32_t blocking_waits{};
    };

    [[nodiscard]] inline bool available() noexcept
    {
        return glFenceSync && glClientWaitSync && glDeleteSync;
    }

    template <typename Continue>
    [[nodiscard]] Result admit(State& state, Continue&& keep_running,
        std::uint32_t maximum_blocking_waits = (std::numeric_limits<std::uint32_t>::max)()) noexcept
    {
        Result result{};
        if (!keep_running())
            return {AdmissionStatus::cancelled, 0u};
        if (!available())
            return {AdmissionStatus::failed, 0u};

        const GLsync oldest = state.fences[state.next];
        if (!oldest)
            return result;

        bool poll = true;
        for (;;)
        {
            if (!keep_running())
                return {AdmissionStatus::cancelled, result.blocking_waits};
            if (!poll && result.blocking_waits >= maximum_blocking_waits)
                return {AdmissionStatus::failed, result.blocking_waits};
            // Fence and wait belong to the same native context. Request the
            // guaranteed flush through that fence on the initial poll only;
            // later finite waits need not flush newer commands.
            const GLenum outcome = glClientWaitSync(
                oldest, poll ? GL_SYNC_FLUSH_COMMANDS_BIT : 0u,
                poll ? 0u : wait_slice_ns);
            if (!poll)
                ++result.blocking_waits;
            poll = false;
            if (outcome == GL_ALREADY_SIGNALED || outcome == GL_CONDITION_SATISFIED)
            {
                glDeleteSync(oldest);
                state.fences[state.next] = nullptr;
                return result;
            }
            if (outcome != GL_TIMEOUT_EXPIRED)
                return {AdmissionStatus::failed, result.blocking_waits};
        }
    }

    [[nodiscard]] inline bool submitted(State& state) noexcept
    {
        // Calling without successful admission must never overwrite a live
        // fence and silently admit more work.
        if (!available() || state.fences[state.next])
            return false;
        const GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0u);
        if (!fence)
            return false;
        state.fences[state.next] = fence;
        state.next = (state.next + 1u) % frames_in_flight;
        return true;
    }

    // Call only with the same native context current. Deleting an unsignaled
    // fence does not turn cleanup into an unbounded GPU completion wait.
    [[nodiscard]] inline bool release(State& state) noexcept
    {
        bool released = true;
        for (auto& fence : state.fences)
        {
            if (fence)
            {
                if (glDeleteSync)
                    glDeleteSync(fence);
                else
                    released = false;
                fence = nullptr;
            }
        }
        state.next = 0u;
        return released;
    }
}
