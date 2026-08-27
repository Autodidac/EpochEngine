module;

#include "engine.config.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <mutex>
#include <string_view>
#include <vector>

#if EPOCH_ENABLE_PHYSICAL_INPUT
#include <SDL3/SDL.h>
#endif

module input.controller;

#if EPOCH_ENABLE_PHYSICAL_INPUT
import sdl.state;
#endif

namespace epochengine::controller_input
{
#if EPOCH_ENABLE_PHYSICAL_INPUT
    namespace
    {
        struct ProviderState final
        {
            bool initialized{};
            bool initialized_subsystem{};
            std::array<SDL_Gamepad*, maximum_controller_slots> gamepads{};
            std::array<SDL_JoystickID, maximum_controller_slots> instance_ids{};
        };

        ProviderState g_provider{};

        [[nodiscard]] constexpr SDL_GamepadButton to_sdl_button(
            Button button) noexcept
        {
            switch (button)
            {
            case Button::south: return SDL_GAMEPAD_BUTTON_SOUTH;
            case Button::east: return SDL_GAMEPAD_BUTTON_EAST;
            case Button::west: return SDL_GAMEPAD_BUTTON_WEST;
            case Button::north: return SDL_GAMEPAD_BUTTON_NORTH;
            case Button::back: return SDL_GAMEPAD_BUTTON_BACK;
            case Button::guide: return SDL_GAMEPAD_BUTTON_GUIDE;
            case Button::start: return SDL_GAMEPAD_BUTTON_START;
            case Button::left_stick: return SDL_GAMEPAD_BUTTON_LEFT_STICK;
            case Button::right_stick: return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
            case Button::left_shoulder: return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
            case Button::right_shoulder: return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
            case Button::dpad_up: return SDL_GAMEPAD_BUTTON_DPAD_UP;
            case Button::dpad_down: return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
            case Button::dpad_left: return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
            case Button::dpad_right: return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
            case Button::count: break;
            }
            return SDL_GAMEPAD_BUTTON_INVALID;
        }

        [[nodiscard]] constexpr SDL_GamepadAxis to_sdl_axis(
            Axis axis) noexcept
        {
            switch (axis)
            {
            case Axis::left_x: return SDL_GAMEPAD_AXIS_LEFTX;
            case Axis::left_y: return SDL_GAMEPAD_AXIS_LEFTY;
            case Axis::right_x: return SDL_GAMEPAD_AXIS_RIGHTX;
            case Axis::right_y: return SDL_GAMEPAD_AXIS_RIGHTY;
            case Axis::left_trigger: return SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
            case Axis::right_trigger: return SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
            case Axis::count: break;
            }
            return SDL_GAMEPAD_AXIS_INVALID;
        }

        void close_slot(std::size_t slot) noexcept
        {
            if (slot >= maximum_controller_slots)
                return;
            if (g_provider.gamepads[slot])
                SDL_CloseGamepad(g_provider.gamepads[slot]);
            g_provider.gamepads[slot] = nullptr;
            g_provider.instance_ids[slot] = 0u;
        }

        [[nodiscard]] bool contains_id(
            const std::vector<SDL_JoystickID>& ids,
            SDL_JoystickID id) noexcept
        {
            return std::binary_search(ids.begin(), ids.end(), id);
        }

        [[nodiscard]] std::vector<SDL_JoystickID> connected_ids()
        {
            int count = 0;
            SDL_JoystickID* raw = SDL_GetGamepads(&count);
            std::vector<SDL_JoystickID> ids{};
            if (raw && count > 0)
                ids.assign(raw, raw + count);
            if (raw)
                SDL_free(raw);
            std::sort(ids.begin(), ids.end());
            ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
            return ids;
        }

        void reconcile_gamepads(const std::vector<SDL_JoystickID>& ids) noexcept
        {
            for (std::size_t slot = 0u;
                 slot < maximum_controller_slots; ++slot)
            {
                if (!g_provider.gamepads[slot])
                    continue;
                if (!SDL_GamepadConnected(g_provider.gamepads[slot])
                    || !contains_id(ids, g_provider.instance_ids[slot]))
                {
                    close_slot(slot);
                }
            }

            for (const SDL_JoystickID id : ids)
            {
                const bool alreadyOpen = std::find(
                    g_provider.instance_ids.begin(),
                    g_provider.instance_ids.end(),
                    id) != g_provider.instance_ids.end();
                if (alreadyOpen)
                    continue;
                const auto vacant = std::find(
                    g_provider.gamepads.begin(),
                    g_provider.gamepads.end(),
                    nullptr);
                if (vacant == g_provider.gamepads.end())
                    break;
                SDL_Gamepad* opened = SDL_OpenGamepad(id);
                if (!opened)
                    continue;
                const std::size_t slot = static_cast<std::size_t>(
                    std::distance(g_provider.gamepads.begin(), vacant));
                g_provider.gamepads[slot] = opened;
                g_provider.instance_ids[slot] = id;
            }
        }

        [[nodiscard]] ProviderFrame capture_frame() noexcept
        {
            ProviderFrame frame{};
            frame.provider_available = g_provider.initialized;
            for (std::size_t slot = 0u;
                 slot < maximum_controller_slots; ++slot)
            {
                SDL_Gamepad* gamepad = g_provider.gamepads[slot];
                if (!gamepad || !SDL_GamepadConnected(gamepad))
                    continue;
                auto& output = frame.devices[slot];
                output.connected = true;
                output.provider_id = static_cast<std::uint64_t>(
                    g_provider.instance_ids[slot]);
                for (std::size_t button = 0u;
                     button < output.buttons.size(); ++button)
                {
                    output.buttons[button] = SDL_GetGamepadButton(
                        gamepad,
                        to_sdl_button(static_cast<Button>(button)));
                }
                for (std::size_t axis = 0u;
                     axis < output.axes.size(); ++axis)
                {
                    const Sint16 value = SDL_GetGamepadAxis(
                        gamepad,
                        to_sdl_axis(static_cast<Axis>(axis)));
                    output.axes[axis] = value
                        == (std::numeric_limits<std::int16_t>::min)()
                        ? static_cast<std::int16_t>(-32'767)
                        : static_cast<std::int16_t>(value);
                }
            }
            return frame;
        }
    }
#endif

    ControllerCode initialize_physical_input() noexcept
    {
#if EPOCH_ENABLE_PHYSICAL_INPUT
        std::scoped_lock runtimeGuard{
            epochengine::sdlcontext::state::runtime_api_mutex()};
        if (g_provider.initialized)
            return ControllerCode::ready;
        if ((SDL_WasInit(SDL_INIT_GAMEPAD) & SDL_INIT_GAMEPAD) == 0u)
        {
            if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
                return ControllerCode::initialization_failed;
            g_provider.initialized_subsystem = true;
        }
        g_provider.initialized = true;
        const PublishResult published = publish_provider_frame(
            ProviderFrame{.provider_available = true});
        return published ? ControllerCode::ready : published.code;
#else
        return ControllerCode::unavailable;
#endif
    }

    ControllerCode poll_physical_input() noexcept
    {
#if EPOCH_ENABLE_PHYSICAL_INPUT
        if (!g_provider.initialized)
            return ControllerCode::unavailable;
        SDL_UpdateGamepads();
        const auto ids = connected_ids();
        reconcile_gamepads(ids);
        const PublishResult published = publish_provider_frame(capture_frame());
        return published.code;
#else
        return ControllerCode::unavailable;
#endif
    }

    void shutdown_physical_input() noexcept
    {
#if EPOCH_ENABLE_PHYSICAL_INPUT
        std::scoped_lock runtimeGuard{
            epochengine::sdlcontext::state::runtime_api_mutex()};
        for (std::size_t slot = 0u;
             slot < maximum_controller_slots; ++slot)
        {
            close_slot(slot);
        }
        g_provider.initialized = false;
        (void)publish_provider_frame({});
        if (g_provider.initialized_subsystem)
        {
            SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
            g_provider.initialized_subsystem = false;
        }
#endif
    }

    std::string_view physical_provider_name() noexcept
    {
#if EPOCH_ENABLE_PHYSICAL_INPUT
        return "SDL3 Gamepad";
#else
        return "Not compiled";
#endif
    }
}
