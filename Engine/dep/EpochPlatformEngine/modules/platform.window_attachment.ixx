// SPDX-License-Identifier: MPL-2.0
module;
#include <cstdint>
#include <memory>
export module platform.window_attachment;
import core.error;
export import platform.window;
export namespace epochengine::platform::window_adapter
{
    // Owns a reversible native attachment lease, never the foreign process,
    // renderer, or window. Identity evidence is not OS confinement or trust.
    class INativeWindowAttachment
    {
    public:
        virtual ~INativeWindowAttachment() noexcept = default;
        [[nodiscard]] virtual WindowHandle window() const noexcept = 0;
        [[nodiscard]] virtual WindowHandle host() const noexcept = 0;
        [[nodiscard]] virtual std::uint64_t process_id() const noexcept = 0;
        [[nodiscard]] virtual bool same_window() const noexcept = 0;
        [[nodiscard]] virtual bool attached() const noexcept = 0;
        [[nodiscard]] virtual bool place(int x, int y, int width, int height) noexcept = 0;
        [[nodiscard]] virtual bool placement_current() const noexcept = 0;
        virtual void hide_during_retirement(bool hide) noexcept = 0;
        virtual void restore() noexcept = 0;
    };

    // Caller admits the foreign process/window. The host must belong to this
    // process and the calling thread. Destruction restores only the same lease.
    [[nodiscard]] core::error::result<std::shared_ptr<INativeWindowAttachment>>
        attach_native_window(WindowHandle candidate, WindowHandle host,
            std::uint64_t process_id) noexcept;
}
