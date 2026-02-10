module; // REQUIRED global module fragment

// ============================================================
// Named module
// ============================================================

export module aengine.context.commandqueue;

// ------------------------------------------------------------
// Standard library
// ------------------------------------------------------------
import <atomic>;
import <cstddef>;
import <cstdint>;
import <functional>;
import <mutex>;
import <queue>;
import <utility>;

// ============================================================
// Command queue (thread-safe, no raw mutex access)
// ============================================================

export namespace almondnamespace::core
{
    enum class RenderPath : std::uint8_t
    {
        // Bitmask flags that describe how queued commands touch GPU state.
        // Unknown means "no explicit render path" (e.g., Vulkan or non-draw work).
        Unknown = 0,
        SFML = 1,
        // OpenGL indicates explicit OpenGL calls that should avoid SFML state resets.
        OpenGL = 2,
        Vulkan = 4
    };

    struct CommandQueue
    {
        using RenderCommand = std::function<void()>;

        // Push a command (thread-safe)
        void enqueue(RenderCommand cmd)
        {
            enqueue(std::move(cmd), RenderPath::Unknown);
        }

        void enqueue(RenderCommand cmd, RenderPath path)
        {
            if (!cmd) return;
            std::lock_guard<std::mutex> lock(mutex_);
            commands_.push(std::move(cmd));
            depth_.fetch_add(1, std::memory_order_relaxed);
            if (path != RenderPath::Unknown)
            {
                render_flags_ |= static_cast<std::uint8_t>(path);
            }
        }

        // Remove all queued commands (thread-safe)
        void clear() noexcept
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::queue<RenderCommand> empty;
            commands_.swap(empty);
            depth_.store(0, std::memory_order_relaxed);
            render_flags_ = 0;
        }

        // Execute all queued commands. Returns true if anything ran.
        bool drain()
        {
            std::queue<RenderCommand> local;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (commands_.empty())
                    return false;
                local.swap(commands_);
                depth_.store(0, std::memory_order_relaxed);
                render_flags_ = 0;
            }

            while (!local.empty())
            {
                auto cmd = std::move(local.front());
                local.pop();
                cmd(); // cmd is guaranteed non-empty from enqueue(), but safe anyway.
            }
            return true;
        }

        // Depth snapshot for telemetry (thread-safe)
        [[nodiscard]] std::size_t depth() const noexcept
        {
            return depth_.load(std::memory_order_relaxed);
        }

        [[nodiscard]] std::uint8_t render_flags_snapshot() const noexcept
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return render_flags_;
        }

        [[nodiscard]] bool has_sfml_draws_snapshot() const noexcept
        {
            return (render_flags_snapshot() & static_cast<std::uint8_t>(RenderPath::SFML)) != 0u;
        }

        [[nodiscard]] bool has_opengl_draws_snapshot() const noexcept
        {
            return (render_flags_snapshot() & static_cast<std::uint8_t>(RenderPath::OpenGL)) != 0u;
        }

        [[nodiscard]] bool has_vulkan_draws_snapshot() const noexcept
        {
            return (render_flags_snapshot() & static_cast<std::uint8_t>(RenderPath::Vulkan)) != 0u;
        }

        // Optional: run at most one command (useful for budgeted pumping)
        bool try_run_one()
        {
            RenderCommand cmd;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (commands_.empty())
                    return false;
                cmd = std::move(commands_.front());
                commands_.pop();
                depth_.fetch_sub(1, std::memory_order_relaxed);
            }
            if (cmd) cmd();
            return true;
        }

    private:
        mutable std::mutex mutex_;
        std::queue<RenderCommand> commands_;
        std::atomic_size_t depth_{ 0 };
        std::uint8_t render_flags_{ 0 };
    };
}
