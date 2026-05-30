/************************************************
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•—  â–ˆâ–ˆâ•—   *
 *  â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ•”â•â•â•  â–ˆâ–ˆâ•”â•â•â•â• â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘     â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â•šâ•â•â•â•â•â•â•â•šâ•â•      â•šâ•â•â•â•â•â•  â•šâ•â•â•â•â•â•â•šâ•â•  â•šâ•â•   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
module;

#include <atomic>
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>

#if defined(_WIN32)
#include <wtypes.h>
#elif defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XInput2.h>
#endif

// ainput.ixx
export module engine.input;

// ------------------------------------------------------------
// Engine / framework modules (no includes)
// ------------------------------------------------------------
//import engine.platform;

import context.window;
//import aengine.context.state;

// Platform framework lives behind aplatform / aframework
#if defined(_WIN32)
import framework;
#elif defined(__APPLE__)
#elif defined(__linux__)
#endif

// ------------------------------------------------------------
// Standard library imports
// ------------------------------------------------------------

// ============================================================
// Input core
// ============================================================
namespace epochnamespace::input
{
    // --------------------------------------------------------
    // Key / Mouse enums
    // --------------------------------------------------------
    export enum Key : std::uint16_t
    {
        Unknown = 0,
        A, B, C, D, E, F, G, H, I, J,
        K, L, M, N, O, P, Q, R, S, T,
        U, V, W, X, Y, Z,
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        Space, Apostrophe, Comma, Minus, Period, Slash,
        Semicolon, Equal, LeftBracket, Backslash, RightBracket, GraveAccent,
        Escape, Enter, Tab, Backspace, Insert, Delete,
        Right, Left, Down, Up,
        PageUp, PageDown, Home, End,
        CapsLock, ScrollLock, NumLock,
        PrintScreen, Pause,
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10,
        F11, F12, F13, F14, F15, F16, F17, F18, F19, F20,
        F21, F22, F23, F24,
        LeftShift, RightShift,
        LeftControl, RightControl,
        LeftAlt, RightAlt,
        LeftSuper, RightSuper,
        Menu,
        KP0, KP1, KP2, KP3, KP4, KP5, KP6, KP7, KP8, KP9,
        KPDecimal, KPDivide, KPMultiply,
        KPSubtract, KPAdd, KPEnter, KPEqual,
        Count
    };

    export enum MouseButton : std::uint8_t
    {
        MouseLeft = 0,
        MouseRight,
        MouseMiddle,
        MouseButton4,
        MouseButton5,
        MouseButton6,
        MouseButton7,
        MouseButton8,
        MouseCount
    };

    export enum class Action : std::uint8_t
    {
        MoveForward,
        MoveBackward,
        MoveLeft,
        MoveRight,
        MoveUp,
        MoveDown,
        LookLeft,
        LookRight,
        LookUp,
        LookDown,
        ResetCamera,
        Cancel,
        Confirm,
        FrameSelection,
        ClipboardCopy,
        ClipboardPaste,
        ContextMenu,
        PlayInEditor,
        TimelinePlayPause,
        TimelineStepFrame,
        PackageInstallSelected,
        Count
    };

    export struct ActionBinding
    {
        Action action{ Action::Count };
        Key primary{ Key::Unknown };
        Key secondary{ Key::Unknown };
        bool control{ false };
        bool shift{ false };
        bool alt{ false };
        MouseButton mouse{ MouseButton::MouseCount };
    };

    export struct InputProfile
    {
        std::array<ActionBinding, static_cast<std::size_t>(Action::Count)> bindings{};
        float mouse_look_sensitivity{ 0.20f };
        float wheel_zoom_step{ 1.30f };
    };

    export enum class ProfilePreset : std::uint8_t
    {
        EditorDefault,
        RuntimeWASD,
        ArrowPilot,
        LeftHanded,
        Count
    };

    export inline constexpr std::size_t action_index(Action action) noexcept
    {
        return static_cast<std::size_t>(action);
    }

    export inline constexpr ActionBinding key_binding(
        Action action,
        Key primary,
        Key secondary = Key::Unknown,
        bool control = false,
        bool shift = false,
        bool alt = false) noexcept
    {
        return { action, primary, secondary, control, shift, alt, MouseButton::MouseCount };
    }

    export inline constexpr ActionBinding mouse_binding(
        Action action,
        MouseButton mouse,
        bool control = false,
        bool shift = false,
        bool alt = false) noexcept
    {
        return { action, Key::Unknown, Key::Unknown, control, shift, alt, mouse };
    }

    export inline constexpr InputProfile make_default_profile() noexcept
    {
        InputProfile profile{};
        profile.bindings[action_index(Action::MoveForward)] = key_binding(Action::MoveForward, Key::W);
        profile.bindings[action_index(Action::MoveBackward)] = key_binding(Action::MoveBackward, Key::S);
        profile.bindings[action_index(Action::MoveLeft)] = key_binding(Action::MoveLeft, Key::A);
        profile.bindings[action_index(Action::MoveRight)] = key_binding(Action::MoveRight, Key::D);
        profile.bindings[action_index(Action::MoveUp)] = key_binding(Action::MoveUp, Key::E, Key::PageUp);
        profile.bindings[action_index(Action::MoveDown)] = key_binding(Action::MoveDown, Key::Q, Key::PageDown);
        profile.bindings[action_index(Action::LookLeft)] = key_binding(Action::LookLeft, Key::Left);
        profile.bindings[action_index(Action::LookRight)] = key_binding(Action::LookRight, Key::Right);
        profile.bindings[action_index(Action::LookUp)] = key_binding(Action::LookUp, Key::Up);
        profile.bindings[action_index(Action::LookDown)] = key_binding(Action::LookDown, Key::Down);
        profile.bindings[action_index(Action::ResetCamera)] = key_binding(Action::ResetCamera, Key::Home);
        profile.bindings[action_index(Action::Cancel)] = key_binding(Action::Cancel, Key::Escape);
        profile.bindings[action_index(Action::Confirm)] = key_binding(Action::Confirm, Key::Enter, Key::KPEnter);
        profile.bindings[action_index(Action::FrameSelection)] = key_binding(Action::FrameSelection, Key::F);
        profile.bindings[action_index(Action::ClipboardCopy)] = key_binding(Action::ClipboardCopy, Key::C, Key::Unknown, true);
        profile.bindings[action_index(Action::ClipboardPaste)] = key_binding(Action::ClipboardPaste, Key::V, Key::Unknown, true);
        profile.bindings[action_index(Action::ContextMenu)] = mouse_binding(Action::ContextMenu, MouseButton::MouseRight);
        profile.bindings[action_index(Action::PlayInEditor)] = key_binding(Action::PlayInEditor, Key::Enter, Key::KPEnter, true);
        profile.bindings[action_index(Action::TimelinePlayPause)] = key_binding(Action::TimelinePlayPause, Key::Space);
        profile.bindings[action_index(Action::TimelineStepFrame)] = key_binding(Action::TimelineStepFrame, Key::Period);
        profile.bindings[action_index(Action::PackageInstallSelected)] = key_binding(Action::PackageInstallSelected, Key::I, Key::Unknown, true);
        return profile;
    }

    export inline constexpr InputProfile make_profile(ProfilePreset preset) noexcept
    {
        InputProfile profile = make_default_profile();

        switch (preset)
        {
        case ProfilePreset::RuntimeWASD:
            profile.mouse_look_sensitivity = 0.16f;
            profile.wheel_zoom_step = 1.00f;
            break;
        case ProfilePreset::ArrowPilot:
            profile.bindings[action_index(Action::MoveForward)] = key_binding(Action::MoveForward, Key::Up, Key::W);
            profile.bindings[action_index(Action::MoveBackward)] = key_binding(Action::MoveBackward, Key::Down, Key::S);
            profile.bindings[action_index(Action::MoveLeft)] = key_binding(Action::MoveLeft, Key::Left, Key::A);
            profile.bindings[action_index(Action::MoveRight)] = key_binding(Action::MoveRight, Key::Right, Key::D);
            profile.bindings[action_index(Action::LookLeft)] = key_binding(Action::LookLeft, Key::Unknown);
            profile.bindings[action_index(Action::LookRight)] = key_binding(Action::LookRight, Key::Unknown);
            profile.bindings[action_index(Action::LookUp)] = key_binding(Action::LookUp, Key::Unknown);
            profile.bindings[action_index(Action::LookDown)] = key_binding(Action::LookDown, Key::Unknown);
            profile.mouse_look_sensitivity = 0.12f;
            profile.wheel_zoom_step = 0.90f;
            break;
        case ProfilePreset::LeftHanded:
            profile.bindings[action_index(Action::MoveForward)] = key_binding(Action::MoveForward, Key::I);
            profile.bindings[action_index(Action::MoveBackward)] = key_binding(Action::MoveBackward, Key::K);
            profile.bindings[action_index(Action::MoveLeft)] = key_binding(Action::MoveLeft, Key::J);
            profile.bindings[action_index(Action::MoveRight)] = key_binding(Action::MoveRight, Key::L);
            profile.bindings[action_index(Action::MoveUp)] = key_binding(Action::MoveUp, Key::U, Key::PageUp);
            profile.bindings[action_index(Action::MoveDown)] = key_binding(Action::MoveDown, Key::O, Key::PageDown);
            profile.mouse_look_sensitivity = 0.16f;
            profile.wheel_zoom_step = 1.00f;
            break;
        case ProfilePreset::EditorDefault:
        case ProfilePreset::Count:
        default:
            break;
        }

        return profile;
    }

    export inline constexpr std::string_view profile_preset_id(ProfilePreset preset) noexcept
    {
        switch (preset)
        {
        case ProfilePreset::RuntimeWASD:
            return "runtime-wasd";
        case ProfilePreset::ArrowPilot:
            return "arrow-pilot";
        case ProfilePreset::LeftHanded:
            return "left-handed";
        case ProfilePreset::EditorDefault:
        case ProfilePreset::Count:
        default:
            return "editor-default";
        }
    }

    export inline constexpr std::string_view profile_preset_label(ProfilePreset preset) noexcept
    {
        switch (preset)
        {
        case ProfilePreset::RuntimeWASD:
            return "Runtime WASD";
        case ProfilePreset::ArrowPilot:
            return "Arrow Pilot";
        case ProfilePreset::LeftHanded:
            return "Left-Handed IJKL";
        case ProfilePreset::EditorDefault:
        case ProfilePreset::Count:
        default:
            return "Editor Default";
        }
    }

    export inline constexpr ProfilePreset profile_preset_from_id(std::string_view id) noexcept
    {
        if (id == "runtime-wasd" || id == "runtime" || id == "wasd")
            return ProfilePreset::RuntimeWASD;
        if (id == "arrow-pilot" || id == "arrows" || id == "arrow")
            return ProfilePreset::ArrowPilot;
        if (id == "left-handed" || id == "left" || id == "ijkl")
            return ProfilePreset::LeftHanded;
        return ProfilePreset::EditorDefault;
    }

    export inline constexpr std::string_view action_label(Action action) noexcept
    {
        switch (action)
        {
        case Action::MoveForward: return "Move Forward";
        case Action::MoveBackward: return "Move Backward";
        case Action::MoveLeft: return "Move Left";
        case Action::MoveRight: return "Move Right";
        case Action::MoveUp: return "Move Up";
        case Action::MoveDown: return "Move Down";
        case Action::LookLeft: return "Look Left";
        case Action::LookRight: return "Look Right";
        case Action::LookUp: return "Look Up";
        case Action::LookDown: return "Look Down";
        case Action::ResetCamera: return "Reset Camera To Center";
        case Action::Cancel: return "Cancel";
        case Action::Confirm: return "Confirm";
        case Action::FrameSelection: return "Frame Selection";
        case Action::ClipboardCopy: return "Copy";
        case Action::ClipboardPaste: return "Paste";
        case Action::ContextMenu: return "Context Menu";
        case Action::PlayInEditor: return "Play In Editor";
        case Action::TimelinePlayPause: return "Timeline Play/Pause";
        case Action::TimelineStepFrame: return "Timeline Step Frame";
        case Action::PackageInstallSelected: return "Install Selected Package";
        case Action::Count:
        default:
            return "Unknown";
        }
    }

    export inline std::string key_name(Key key)
    {
        const auto value = static_cast<int>(key);
        if (value >= static_cast<int>(Key::A) && value <= static_cast<int>(Key::Z))
            return std::string(1, static_cast<char>('A' + value - static_cast<int>(Key::A)));
        if (value >= static_cast<int>(Key::Num0) && value <= static_cast<int>(Key::Num9))
            return std::string(1, static_cast<char>('0' + value - static_cast<int>(Key::Num0)));

        switch (key)
        {
        case Key::Space: return "Space";
        case Key::Escape: return "Escape";
        case Key::Enter: return "Enter";
        case Key::Tab: return "Tab";
        case Key::Backspace: return "Backspace";
        case Key::Insert: return "Insert";
        case Key::Delete: return "Delete";
        case Key::Right: return "Right";
        case Key::Left: return "Left";
        case Key::Down: return "Down";
        case Key::Up: return "Up";
        case Key::PageUp: return "PageUp";
        case Key::PageDown: return "PageDown";
        case Key::Home: return "Home";
        case Key::End: return "End";
        case Key::Period: return ".";
        case Key::KPEnter: return "Keypad Enter";
        case Key::Unknown:
        default:
            return "Unbound";
        }
    }

    export inline std::string mouse_button_name(MouseButton mouse)
    {
        switch (mouse)
        {
        case MouseButton::MouseLeft: return "Mouse Left";
        case MouseButton::MouseRight: return "Mouse Right";
        case MouseButton::MouseMiddle: return "Mouse Middle";
        case MouseButton::MouseButton4: return "Mouse 4";
        case MouseButton::MouseButton5: return "Mouse 5";
        case MouseButton::MouseButton6: return "Mouse 6";
        case MouseButton::MouseButton7: return "Mouse 7";
        case MouseButton::MouseButton8: return "Mouse 8";
        case MouseButton::MouseCount:
        default:
            return "No Mouse";
        }
    }

    export inline std::string binding_text(const ActionBinding& binding)
    {
        std::string text{};
        if (binding.control)
            text += "Ctrl+";
        if (binding.shift)
            text += "Shift+";
        if (binding.alt)
            text += "Alt+";

        if (binding.mouse != MouseButton::MouseCount)
        {
            text += mouse_button_name(binding.mouse);
        }
        else
        {
            text += key_name(binding.primary);
            if (binding.secondary != Key::Unknown)
            {
                text += " / ";
                text += key_name(binding.secondary);
            }
        }

        return text;
    }

    export inline bool validate_profile(const InputProfile& profile) noexcept
    {
        if (profile.mouse_look_sensitivity < 0.01f || profile.wheel_zoom_step < 0.05f)
            return false;

        for (std::size_t index = 0; index < profile.bindings.size(); ++index)
        {
            const auto expectedAction = static_cast<Action>(index);
            const auto& binding = profile.bindings[index];
            if (binding.action != expectedAction)
                return false;
        }

        return true;
    }

    export inline std::size_t bound_action_count(const InputProfile& profile) noexcept
    {
        std::size_t count = 0;
        for (const auto& binding : profile.bindings)
        {
            if (binding.primary != Key::Unknown
                || binding.secondary != Key::Unknown
                || binding.mouse != MouseButton::MouseCount)
            {
                ++count;
            }
        }

        return count;
    }

    export inline std::string profile_summary(ProfilePreset preset)
    {
        const InputProfile profile = make_profile(preset);
        std::string summary{ "input profile " };
        summary += std::string(profile_preset_label(preset));
        summary += " actions ";
        summary += std::to_string(static_cast<std::size_t>(Action::Count));
        summary += " bound ";
        summary += std::to_string(bound_action_count(profile));
        summary += " reset ";
        summary += binding_text(profile.bindings[action_index(Action::ResetCamera)]);
        summary += " context ";
        summary += binding_text(profile.bindings[action_index(Action::ContextMenu)]);
        return summary;
    }

    // --------------------------------------------------------
    // State storage (header-only â†’ module globals)
    // --------------------------------------------------------
    inline std::thread::id        g_pollingThread{};
    inline std::atomic<bool>     g_pollingThreadLocked{ false };
    inline std::mutex            g_profileMutex{};
    inline InputProfile          g_activeProfile{ make_default_profile() };
    export inline std::shared_mutex g_inputMutex{};

    export inline std::bitset<Key::Count>               keyDown{};
    export inline std::bitset<Key::Count>               keyPressed{};
    export inline std::bitset<MouseButton::MouseCount>  mouseDown{};
    export inline std::bitset<MouseButton::MouseCount>  mousePressed{};

    export inline std::atomic<int>   mouseX{ 0 };
    export inline std::atomic<int>   mouseY{ 0 };
    export inline std::atomic<int>   mouseWheel{ 0 };
    export inline std::atomic<bool>  mouseCoordsAreGlobal{ true };

    // --------------------------------------------------------
    // Thread ownership helpers
    // --------------------------------------------------------
    export inline void set_mouse_coords_are_global(bool v)
    {
        mouseCoordsAreGlobal.store(v, std::memory_order_release);
    }

    export inline bool are_mouse_coords_global()
    {
        return mouseCoordsAreGlobal.load(std::memory_order_acquire);
    }

    export inline void designate_polling_thread(std::thread::id id)
    {
        g_pollingThread = id;
        g_pollingThreadLocked.store(true, std::memory_order_release);
    }

    export inline void designate_polling_thread_to_current()
    {
        designate_polling_thread(std::this_thread::get_id());
    }

    export inline void clear_polling_thread_designation()
    {
        g_pollingThread = {};
        g_pollingThreadLocked.store(false, std::memory_order_release);
    }

    export inline bool can_poll_on_this_thread()
    {
        if (!g_pollingThreadLocked.load(std::memory_order_acquire))
            return true;
        return std::this_thread::get_id() == g_pollingThread;
    }

    export inline void set_active_profile(const InputProfile& profile)
    {
        std::scoped_lock lock(g_profileMutex);
        g_activeProfile = profile;
    }

    export inline void set_active_profile(ProfilePreset preset)
    {
        set_active_profile(make_profile(preset));
    }

    export inline InputProfile active_profile_snapshot()
    {
        std::scoped_lock lock(g_profileMutex);
        return g_activeProfile;
    }

    export inline ActionBinding binding_for(Action action)
    {
        const auto profile = active_profile_snapshot();
        const auto index = action_index(action);
        if (index >= profile.bindings.size())
            return {};
        return profile.bindings[index];
    }

    export inline void set_action_binding(Action action, Key primary, Key secondary)
    {
        auto profile = active_profile_snapshot();
        const auto index = action_index(action);
        if (index >= profile.bindings.size())
            return;

        profile.bindings[index] = key_binding(action, primary, secondary);
        set_active_profile(profile);
    }

    export inline void set_action_binding(
        Action action,
        Key primary,
        Key secondary,
        bool control,
        bool shift,
        bool alt)
    {
        auto profile = active_profile_snapshot();
        const auto index = action_index(action);
        if (index >= profile.bindings.size())
            return;

        profile.bindings[index] = key_binding(action, primary, secondary, control, shift, alt);
        set_active_profile(profile);
    }

    export inline void set_action_mouse_binding(Action action, MouseButton mouse)
    {
        auto profile = active_profile_snapshot();
        const auto index = action_index(action);
        if (index >= profile.bindings.size())
            return;

        profile.bindings[index] = mouse_binding(action, mouse);
        set_active_profile(profile);
    }

    export inline void set_camera_tuning(float mouse_look_sensitivity, float wheel_zoom_step)
    {
        auto profile = active_profile_snapshot();
        profile.mouse_look_sensitivity = mouse_look_sensitivity < 0.01f ? 0.01f : mouse_look_sensitivity;
        profile.wheel_zoom_step = wheel_zoom_step < 0.05f ? 0.05f : wheel_zoom_step;
        set_active_profile(profile);
    }

    export inline float mouse_look_sensitivity()
    {
        return active_profile_snapshot().mouse_look_sensitivity;
    }

    export inline float wheel_zoom_step()
    {
        return active_profile_snapshot().wheel_zoom_step;
    }

    // ========================================================
    // Win32 implementation
    // ========================================================
#if defined(_WIN32)

    export inline constexpr int map_key_to_vk(Key k)
    {
        switch (k)
        {
        case Key::A: return 'A'; case Key::B: return 'B'; case Key::C: return 'C'; case Key::D: return 'D';
        case Key::E: return 'E'; case Key::F: return 'F'; case Key::G: return 'G'; case Key::H: return 'H';
        case Key::I: return 'I'; case Key::J: return 'J'; case Key::K: return 'K'; case Key::L: return 'L';
        case Key::M: return 'M'; case Key::N: return 'N'; case Key::O: return 'O'; case Key::P: return 'P';
        case Key::Q: return 'Q'; case Key::R: return 'R'; case Key::S: return 'S'; case Key::T: return 'T';
        case Key::U: return 'U'; case Key::V: return 'V'; case Key::W: return 'W'; case Key::X: return 'X';
        case Key::Y: return 'Y'; case Key::Z: return 'Z';

        case Key::Num0: return '0'; case Key::Num1: return '1'; case Key::Num2: return '2';
        case Key::Num3: return '3'; case Key::Num4: return '4'; case Key::Num5: return '5';
        case Key::Num6: return '6'; case Key::Num7: return '7'; case Key::Num8: return '8'; case Key::Num9: return '9';

        case Key::Escape: return VK_ESCAPE;
        case Key::Enter: return VK_RETURN;
        case Key::Tab: return VK_TAB;
        case Key::Backspace: return VK_BACK;
        case Key::Space: return VK_SPACE;
        case Key::Apostrophe: return VK_OEM_7;
        case Key::Comma: return VK_OEM_COMMA;
        case Key::Minus: return VK_OEM_MINUS;
        case Key::Period: return VK_OEM_PERIOD;
        case Key::Slash: return VK_OEM_2;
        case Key::Semicolon: return VK_OEM_1;
        case Key::Equal: return VK_OEM_PLUS;
        case Key::LeftBracket: return VK_OEM_4;
        case Key::Backslash: return VK_OEM_5;
        case Key::RightBracket: return VK_OEM_6;
        case Key::GraveAccent: return VK_OEM_3;
        case Key::Insert: return VK_INSERT;
        case Key::Delete: return VK_DELETE;
        case Key::Left: return VK_LEFT;
        case Key::Right: return VK_RIGHT;
        case Key::Up: return VK_UP;
        case Key::Down: return VK_DOWN;
        case Key::PageUp: return VK_PRIOR;
        case Key::PageDown: return VK_NEXT;
        case Key::Home: return VK_HOME;
        case Key::End: return VK_END;
        case Key::CapsLock: return VK_CAPITAL;
        case Key::ScrollLock: return VK_SCROLL;
        case Key::NumLock: return VK_NUMLOCK;
        case Key::PrintScreen: return VK_SNAPSHOT;
        case Key::Pause: return VK_PAUSE;

        case Key::LeftShift: return VK_LSHIFT;
        case Key::RightShift: return VK_RSHIFT;
        case Key::LeftControl: return VK_LCONTROL;
        case Key::RightControl: return VK_RCONTROL;
        case Key::LeftAlt: return VK_LMENU;
        case Key::RightAlt: return VK_RMENU;
        case Key::LeftSuper: return VK_LWIN;
        case Key::RightSuper: return VK_RWIN;
        case Key::Menu: return VK_APPS;

        case Key::F1: return VK_F1; case Key::F2: return VK_F2; case Key::F3: return VK_F3; case Key::F4: return VK_F4;
        case Key::F5: return VK_F5; case Key::F6: return VK_F6; case Key::F7: return VK_F7; case Key::F8: return VK_F8;
        case Key::F9: return VK_F9; case Key::F10: return VK_F10; case Key::F11: return VK_F11; case Key::F12: return VK_F12;
        case Key::F13: return VK_F13; case Key::F14: return VK_F14; case Key::F15: return VK_F15; case Key::F16: return VK_F16;
        case Key::F17: return VK_F17; case Key::F18: return VK_F18; case Key::F19: return VK_F19; case Key::F20: return VK_F20;
        case Key::F21: return VK_F21; case Key::F22: return VK_F22; case Key::F23: return VK_F23; case Key::F24: return VK_F24;

        case Key::KP0: return VK_NUMPAD0; case Key::KP1: return VK_NUMPAD1; case Key::KP2: return VK_NUMPAD2;
        case Key::KP3: return VK_NUMPAD3; case Key::KP4: return VK_NUMPAD4; case Key::KP5: return VK_NUMPAD5;
        case Key::KP6: return VK_NUMPAD6; case Key::KP7: return VK_NUMPAD7; case Key::KP8: return VK_NUMPAD8; case Key::KP9: return VK_NUMPAD9;
        case Key::KPDecimal: return VK_DECIMAL;
        case Key::KPDivide: return VK_DIVIDE;
        case Key::KPMultiply: return VK_MULTIPLY;
        case Key::KPSubtract: return VK_SUBTRACT;
        case Key::KPAdd: return VK_ADD;
        case Key::KPEnter: return VK_RETURN;
        case Key::KPEqual: return VK_OEM_PLUS;

        default: return 0;
        }
    }

    export inline constexpr int map_mouse_to_vk(MouseButton b)
    {
        switch (b)
        {
        case MouseButton::MouseLeft:   return VK_LBUTTON;
        case MouseButton::MouseRight:  return VK_RBUTTON;
        case MouseButton::MouseMiddle: return VK_MBUTTON;
        case MouseButton::MouseButton4:return XBUTTON1;
        case MouseButton::MouseButton5:return XBUTTON2;
        default: return 0;
        }
    }

    export inline void inject_virtual_key_event(int vk, bool down)
    {
        if (!vk)
            return;

        std::unique_lock lock(g_inputMutex);
        for (std::uint16_t k = 0; k < Key::Count; ++k)
        {
            if (map_key_to_vk(static_cast<Key>(k)) != vk)
                continue;

            const bool wasDown = keyDown.test(k);
            if (down)
            {
                if (!wasDown)
                    keyPressed.set(k);
                keyDown.set(k);
            }
            else
            {
                keyDown.reset(k);
            }
            return;
        }
    }

    // --------------------------------------------------------
    // Per-frame polling
    // --------------------------------------------------------
    export inline void poll_input()
    {
        if (!can_poll_on_this_thread())
            return;

        std::unique_lock lock(g_inputMutex);

        keyPressed.reset();
        mousePressed.reset();
        mouseWheel.store(0, std::memory_order_relaxed);

        for (std::uint16_t k = 0; k < Key::Count; ++k)
        {
            int vk = map_key_to_vk(static_cast<Key>(k));
            if (!vk) continue;

            bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
            if (down && !keyDown.test(k))
                keyPressed.set(k);

            keyDown.set(k, down);
        }

        for (std::uint8_t b = 0; b < MouseButton::MouseCount; ++b)
        {
            int vk = map_mouse_to_vk(static_cast<MouseButton>(b));
            if (!vk) continue;

            bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
            if (down && !mouseDown.test(b))
                mousePressed.set(b);

            mouseDown.set(b, down);
        }

        POINT p{};
        if (GetCursorPos(&p))
        {
            mouseX.store(p.x, std::memory_order_relaxed);
            mouseY.store(p.y, std::memory_order_relaxed);
            set_mouse_coords_are_global(true);
        }
    }

#endif // _WIN32

    // ========================================================
    // Query helpers (platform independent)
    // ========================================================
    export inline bool is_key_held(Key k)
    {
        std::shared_lock lock(g_inputMutex);
        return keyDown.test(static_cast<size_t>(k));
    }

    export inline bool is_key_down(Key k)
    {
        std::shared_lock lock(g_inputMutex);
        return keyPressed.test(static_cast<size_t>(k));
    }

    export inline bool is_mouse_button_held(MouseButton b)
    {
        std::shared_lock lock(g_inputMutex);
        return mouseDown.test(static_cast<size_t>(b));
    }

    export inline bool is_mouse_button_down(MouseButton b)
    {
        std::shared_lock lock(g_inputMutex);
        return mousePressed.test(static_cast<size_t>(b));
    }

    export inline bool binding_modifiers_satisfied(const ActionBinding& binding)
    {
        const bool controlHeld = is_key_held(Key::LeftControl) || is_key_held(Key::RightControl);
        const bool shiftHeld = is_key_held(Key::LeftShift) || is_key_held(Key::RightShift);
        const bool altHeld = is_key_held(Key::LeftAlt) || is_key_held(Key::RightAlt);
        return (!binding.control || controlHeld)
            && (!binding.shift || shiftHeld)
            && (!binding.alt || altHeld);
    }

    export inline bool binding_key_held(const ActionBinding& binding)
    {
        return binding_modifiers_satisfied(binding)
            && ((binding.primary != Key::Unknown && is_key_held(binding.primary))
                || (binding.secondary != Key::Unknown && is_key_held(binding.secondary)));
    }

    export inline bool binding_key_pressed(const ActionBinding& binding)
    {
        return binding_modifiers_satisfied(binding)
            && ((binding.primary != Key::Unknown && is_key_down(binding.primary))
                || (binding.secondary != Key::Unknown && is_key_down(binding.secondary)));
    }

    export inline bool binding_mouse_held(const ActionBinding& binding)
    {
        return binding.mouse != MouseButton::MouseCount
            && binding_modifiers_satisfied(binding)
            && is_mouse_button_held(binding.mouse);
    }

    export inline bool binding_mouse_pressed(const ActionBinding& binding)
    {
        return binding.mouse != MouseButton::MouseCount
            && binding_modifiers_satisfied(binding)
            && is_mouse_button_down(binding.mouse);
    }

    export inline bool action_held(Action action)
    {
        const auto binding = binding_for(action);
        return binding_key_held(binding) || binding_mouse_held(binding);
    }

    export inline bool action_pressed(Action action)
    {
        const auto binding = binding_for(action);
        return binding_key_pressed(binding) || binding_mouse_pressed(binding);
    }
} // namespace epochnamespace::input

// ============================================================
// Win32 WndProc hook (module-visible, header-only â†’ module)
// ============================================================
#if defined(_WIN32) && !defined(EPOCH_MAIN_HEADLESS)

export inline LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
    using namespace epochnamespace::input;

    switch (msg)
    {
    case WM_LBUTTONDOWN: mouseDown.set(MouseLeft); break;
    case WM_LBUTTONUP:   mouseDown.reset(MouseLeft); break;
    case WM_RBUTTONDOWN: mouseDown.set(MouseRight); break;
    case WM_RBUTTONUP:   mouseDown.reset(MouseRight); break;
    case WM_MOUSEWHEEL:
        mouseWheel.fetch_add(GET_WHEEL_DELTA_WPARAM(wParam), std::memory_order_relaxed);
        break;
    default:
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

#endif
