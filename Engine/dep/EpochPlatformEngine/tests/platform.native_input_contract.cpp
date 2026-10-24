// SPDX-License-Identifier: MPL-2.0
#include <array>
#include <cstdint>
import platform.window;
import platform.window_attachment;
import core.error;

namespace
{
    using namespace epochengine::platform::window_adapter;
    struct Trace
    {
        std::array<NativeInputAction, 4> actions{};
        unsigned count{};
        bool close{};
        bool drag{};
        bool payload{};
    };
    NativeInputResult capture(void* owner, NativeInputAction action,
        const NativeInputMessage& message) noexcept
    {
        auto& trace = *static_cast<Trace*>(owner);
        if (trace.count < trace.actions.size()) trace.actions[trace.count++] = action;
        trace.payload = message.window.value == 0x1234 && message.message == 0x4567
            && message.key == 0x87654321 && message.flags == -12345;
        if (action == NativeInputAction::close_shortcut) return {trace.close, 19};
        if (action == NativeInputAction::proxy_drag) return {trace.drag, 37};
        return {true, 99}; // Observational callback results must not consume native focus/mouse messages.
    }
}
int main()
{
    using namespace epochengine::platform::window_adapter;
    Trace trace;
    NativeInputBinding binding{&trace, capture};
    NativeInputMessage message{{0x1234}, NativeInputKind::other, 0x4567, 0x87654321, -12345};
    for (const auto kind : {NativeInputKind::text, NativeInputKind::key})
    {
        trace = {}; message.kind = kind;
        const auto result = dispatch_native_input(binding, message);
        if (!result.handled || result.value || trace.count != 1
            || trace.actions[0] != NativeInputAction::input || !trace.payload) return 1;
    }
    for (const auto kind : {NativeInputKind::left_down, NativeInputKind::move, NativeInputKind::left_up})
    {
        for (const bool drag : {false, true})
        {
            trace = {}; trace.drag = drag; message.kind = kind;
            const auto result = dispatch_native_input(binding, message);
            if (result.handled != drag || result.value != 37 || trace.count != 2
                || trace.actions[0] != NativeInputAction::input
                || trace.actions[1] != NativeInputAction::proxy_drag || !trace.payload) return 2;
        }
    }
    for (const auto kind : {NativeInputKind::right_down, NativeInputKind::right_up, NativeInputKind::wheel})
    {
        trace = {}; message.kind = kind;
        if (dispatch_native_input(binding, message).handled || trace.count != 1
            || trace.actions[0] != NativeInputAction::input) return 3;
    }
    const std::array kinds{NativeInputKind::focus_gained, NativeInputKind::focus_lost, NativeInputKind::retiring};
    const std::array actions{NativeInputAction::focus_gained, NativeInputAction::focus_lost, NativeInputAction::retiring};
    for (unsigned index = 0; index < kinds.size(); ++index)
    {
        trace = {}; message.kind = kinds[index];
        if (dispatch_native_input(binding, message).handled || trace.count != 1
            || trace.actions[0] != actions[index]) return 4;
    }
    message.kind = NativeInputKind::key; message.close_shortcut = true;
    trace = {}; trace.close = true;
    const auto close = dispatch_native_input(binding, message);
    if (!close.handled || close.value != 19 || trace.count != 1
        || trace.actions[0] != NativeInputAction::close_shortcut) return 5;
    trace = {}; message.repeated = true;
    if (!dispatch_native_input(binding, message).handled || trace.count) return 6;
    trace = {}; message.repeated = false;
    if (!dispatch_native_input(binding, message).handled || trace.count != 2
        || trace.actions[0] != NativeInputAction::close_shortcut
        || trace.actions[1] != NativeInputAction::input) return 7;
    trace = {}; message.close_shortcut = false; message.kind = NativeInputKind::other;
    if (dispatch_native_input(binding, message).handled || trace.count) return 8;
    if (dispatch_native_input({}, message).handled) return 9;
    if (attach_native_input({}, binding)) return 10;
    detach_native_input({}, binding);
    const auto attachment = attach_native_window({}, {}, 0);
    if (attachment) return 11;
#if defined(_WIN32)
    if (attachment.error().c.value != epochengine::core::error::core_domain::invalid_argument) return 12;
#else
    if (attachment.error().c.value != epochengine::core::error::core_domain::unsupported) return 12;
#endif
    return 0;
}
