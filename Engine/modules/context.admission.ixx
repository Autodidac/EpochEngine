/***********************************************
 * This file is part of the Epoch Project.
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ***********************************************/

module;

#include <cstdint>

export module context.admission;

import context.type;

// Admission only: this module creates no window, renderer, process, or queue.
// Host adapters supply observations; model/child assertions are not authority.
export namespace epochengine::core::contextadmission
{
    enum class Support : std::uint8_t { unknown, unsupported, supported };
    enum class Purpose : std::uint8_t
    {
        native_window,
        external_process_window,
        headless,
        platform_window
    };
    enum class Ownership : std::uint8_t { none, owned, borrowed, external_process };
    enum class Dispatch : std::uint8_t { owner_thread, owner_queue };
    enum class InstancePolicy : std::uint8_t
    {
        unknown,
        single_active_per_process,
        independent
    };
    enum class Activation : std::uint8_t
    {
        unknown,
        none,
        wgl_or_glx,
        raylib_owner,
        sdl_owner,
        sfml_owner,
        vulkan_queue_owner,
        d3d11_owner,
        software_owner,
        external_process
    };

    struct BackendTraits final
    {
        Support native_surface{Support::unknown};
        InstancePolicy instances{InstancePolicy::unknown};
        Activation activation{Activation::unknown};
    };

    // These describe the existing adapters, not library-wide guarantees or
    // compile-time availability. Unknown multiplicity is not permission to
    // instantiate a second adapter in the same process.
    [[nodiscard]] constexpr BackendTraits backend_traits(ContextType backend) noexcept
    {
        switch (backend)
        {
        case ContextType::OpenGL:
            return {Support::supported, InstancePolicy::unknown, Activation::wgl_or_glx};
        case ContextType::RayLib:
            return {Support::supported, InstancePolicy::single_active_per_process, Activation::raylib_owner};
        case ContextType::SDL:
            return {Support::supported, InstancePolicy::single_active_per_process, Activation::sdl_owner};
        case ContextType::SFML:
            return {Support::supported, InstancePolicy::single_active_per_process, Activation::sfml_owner};
        case ContextType::Vulkan:
            return {Support::supported, InstancePolicy::unknown, Activation::vulkan_queue_owner};
        case ContextType::DirectX:
            return {Support::supported, InstancePolicy::unknown, Activation::d3d11_owner};
        case ContextType::Software:
            return {Support::supported, InstancePolicy::single_active_per_process, Activation::software_owner};
        case ContextType::Noop:
            return {Support::unsupported, InstancePolicy::independent, Activation::none};
        case ContextType::None:
            return {Support::unsupported, InstancePolicy::unknown, Activation::unknown};
        case ContextType::Custom:
            return {};
        }
        return {};
    }

    struct ContextIdentity final
    {
        std::uint64_t context_id{};
        std::uint64_t generation{};
        std::uint64_t process_id{};
        // Host-observed process creation/lease token, not a child-selected ID.
        std::uint64_t process_start_token{};
        std::uintptr_t native_window{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return context_id != 0 && generation != 0
                && process_id != 0 && process_start_token != 0;
        }
        [[nodiscard]] constexpr bool operator==(const ContextIdentity&) const noexcept = default;
    };

    struct ObservedCapabilities final
    {
        Support availability{Support::unknown};
        Support embedding{Support::unknown};
        Support input_route{Support::unknown};
        Support native_presentation{Support::unknown};
        Support final_capture{Support::unknown};
    };

    struct ContextProfile
    {
        ContextType backend{ContextType::None};
        Purpose purpose{Purpose::native_window};
        Ownership window_ownership{Ownership::none};
        Ownership resource_ownership{Ownership::none};
        std::uint32_t active_same_backend_instances{};
        ObservedCapabilities capabilities{};
        bool requires_embedding{};
        bool requires_input{};
        bool requires_final_capture{};
    };

    struct AdmissionRequest final : ContextProfile
    {
        ContextIdentity identity{};
        std::uint64_t host_process_id{};
        std::uint64_t owner_thread_id{};
        std::uint64_t event_thread_id{};
        std::uint64_t calling_thread_id{};
        Dispatch dispatch{Dispatch::owner_thread};
    };

    enum class Refusal : std::uint8_t
    {
        none,
        invalid_identity,
        invalid_purpose,
        unavailable,
        capability_unknown,
        capability_unsupported,
        invalid_ownership,
        invalid_thread_binding,
        wrong_thread,
        instance_limit,
        instance_policy_unknown,
        stale_identity,
        stale_surface,
        not_ready,
        invalid_evidence,
        insufficient_evidence
    };

    struct Admission final
    {
        AdmissionRequest request{};
        BackendTraits traits{};
        Refusal refusal{Refusal::invalid_identity};

        [[nodiscard]] constexpr bool allowed() const noexcept { return refusal == Refusal::none; }
    };

    [[nodiscard]] constexpr Refusal require_support(Support support) noexcept
    {
        switch (support)
        {
        case Support::supported: return Refusal::none;
        case Support::unsupported: return Refusal::capability_unsupported;
        case Support::unknown: return Refusal::capability_unknown;
        }
        return Refusal::capability_unknown;
    }

    // Pure profile admission is usable before any process/thread/window exists.
    // It never asserts that construction, ownership, or presentation occurred.
    [[nodiscard]] constexpr Refusal preflight(const ContextProfile& request) noexcept
    {
        const auto traits = backend_traits(request.backend);
        if (request.capabilities.availability != Support::supported)
            return request.capabilities.availability == Support::unsupported
                ? Refusal::unavailable : Refusal::capability_unknown;

        const bool native_ownership =
            (request.window_ownership == Ownership::owned || request.window_ownership == Ownership::borrowed)
            && (request.resource_ownership == Ownership::owned || request.resource_ownership == Ownership::borrowed);
        switch (request.purpose)
        {
        case Purpose::native_window:
            if (traits.native_surface != Support::supported)
                return Refusal::invalid_purpose;
            if (!native_ownership)
                return Refusal::invalid_ownership;
            break;
        case Purpose::external_process_window:
            if (request.backend != ContextType::Custom)
                return Refusal::invalid_purpose;
            if (request.window_ownership != Ownership::external_process
                || request.resource_ownership != Ownership::external_process)
                return Refusal::invalid_ownership;
            break;
        case Purpose::headless:
            if (request.backend != ContextType::Noop)
                return Refusal::invalid_purpose;
            if (request.window_ownership != Ownership::none
                || request.resource_ownership != Ownership::none)
                return Refusal::invalid_ownership;
            if (request.requires_embedding || request.requires_input || request.requires_final_capture)
                return Refusal::capability_unsupported;
            break;
        case Purpose::platform_window:
            // A window/event host has no renderer. Custom is not inferred to
            // support drawing, and must never be relabeled Software or Noop.
            if (request.backend != ContextType::Custom)
                return Refusal::invalid_purpose;
            if ((request.window_ownership != Ownership::owned && request.window_ownership != Ownership::borrowed)
                || request.resource_ownership != Ownership::none)
                return Refusal::invalid_ownership;
            if (request.requires_final_capture)
                return Refusal::capability_unsupported;
            break;
        default:
            return Refusal::invalid_purpose;
        }
        if (request.active_same_backend_instances != 0
            && request.purpose == Purpose::native_window)
        {
            if (traits.instances == InstancePolicy::single_active_per_process)
                return Refusal::instance_limit;
            if (traits.instances != InstancePolicy::independent)
                return Refusal::instance_policy_unknown;
        }
        if (request.requires_embedding)
            if (const auto refusal = require_support(request.capabilities.embedding); refusal != Refusal::none)
                return refusal;
        if (request.requires_input)
            if (const auto refusal = require_support(request.capabilities.input_route); refusal != Refusal::none)
                return refusal;
        if (request.requires_final_capture)
            if (const auto refusal = require_support(request.capabilities.final_capture); refusal != Refusal::none)
                return refusal;
        return Refusal::none;
    }

    [[nodiscard]] constexpr Admission admit(const AdmissionRequest& request) noexcept
    {
        Admission result{request, backend_traits(request.backend), preflight(request)};
        const auto reject = [&result](Refusal refusal) constexpr
        {
            result.refusal = refusal;
            return result;
        };
        if (!request.identity.valid() || request.host_process_id == 0)
            return reject(Refusal::invalid_identity);
        if (!result.allowed())
            return result;
        if (request.purpose == Purpose::external_process_window)
        {
            if (request.identity.process_id == request.host_process_id || request.identity.native_window == 0)
                return reject(Refusal::invalid_ownership);
            result.traits = {Support::supported, InstancePolicy::independent, Activation::external_process};
        }
        else
        {
            if (request.identity.process_id != request.host_process_id
                || (request.purpose == Purpose::headless && request.identity.native_window != 0))
                return reject(Refusal::invalid_ownership);
            if (request.purpose == Purpose::platform_window)
                result.traits = {Support::supported, InstancePolicy::unknown, Activation::none};
        }
        if (request.owner_thread_id == 0 || request.calling_thread_id == 0
            || (request.purpose != Purpose::headless && request.event_thread_id == 0))
            return reject(Refusal::invalid_thread_binding);
        if (request.dispatch != Dispatch::owner_thread && request.dispatch != Dispatch::owner_queue)
            return reject(Refusal::invalid_thread_binding);
        if (request.dispatch == Dispatch::owner_thread && request.owner_thread_id != request.calling_thread_id)
            return reject(Refusal::wrong_thread);
        return result;
    }

    enum class EvidenceKind : std::uint8_t
    {
        initialized,
        attached,
        native_presented,
        child_reported_present,
        host_captured_frame
    };

    struct SurfaceEvidence final
    {
        ContextIdentity identity{};
        std::uint64_t surface_generation{};
        std::uint64_t frame_generation{};
        std::uint32_t width{};
        std::uint32_t height{};
        bool backend_ready{};
        EvidenceKind kind{EvidenceKind::initialized};
    };

    // The evidence kind is intentionally not an ordered quality score. Capture
    // need not prove a native Present; attachment never proves a rendered frame.
    [[nodiscard]] constexpr Refusal evaluate_readiness(
        const Admission& admission,
        const ContextIdentity& current_identity,
        std::uint64_t current_surface_generation,
        const SurfaceEvidence& evidence,
        EvidenceKind required) noexcept
    {
        if (!admission.allowed())
            return admission.refusal;
        if (const auto checked = admit(admission.request); !checked.allowed())
            return checked.refusal;
        // Re-admit after a native handle is acquired/replaced. An old lease
        // cannot become current merely because a numeric PID/HWND is reused.
        if (current_identity != admission.request.identity || evidence.identity != current_identity)
            return Refusal::stale_identity;
        if (evidence.surface_generation != current_surface_generation)
            return Refusal::stale_surface;
        if (!evidence.backend_ready)
            return Refusal::not_ready;
        const bool is_frame = evidence.kind == EvidenceKind::native_presented
            || evidence.kind == EvidenceKind::child_reported_present
            || evidence.kind == EvidenceKind::host_captured_frame;
        if (evidence.kind != EvidenceKind::initialized && evidence.kind != EvidenceKind::attached && !is_frame)
            return Refusal::invalid_evidence;
        if (evidence.kind != EvidenceKind::initialized
            && (current_identity.native_window == 0 || current_surface_generation == 0))
            return Refusal::invalid_evidence;
        if (is_frame && (evidence.frame_generation == 0 || evidence.width == 0 || evidence.height == 0))
            return Refusal::invalid_evidence;
        if (!is_frame && evidence.frame_generation != 0)
            return Refusal::invalid_evidence;
        if (admission.request.purpose == Purpose::headless && evidence.kind != EvidenceKind::initialized)
            return Refusal::capability_unsupported;
        if (admission.request.purpose == Purpose::platform_window && is_frame)
            return Refusal::capability_unsupported;
        if (evidence.kind == EvidenceKind::child_reported_present
            && admission.request.purpose != Purpose::external_process_window)
            return Refusal::invalid_evidence;
        if (evidence.kind == EvidenceKind::native_presented)
        {
            if (admission.request.purpose == Purpose::external_process_window)
                return Refusal::invalid_evidence;
            if (const auto refusal = require_support(admission.request.capabilities.native_presentation);
                refusal != Refusal::none)
                return refusal;
        }
        if (evidence.kind == EvidenceKind::host_captured_frame)
            if (const auto refusal = require_support(admission.request.capabilities.final_capture);
                refusal != Refusal::none)
                return refusal;
        return evidence.kind == required ? Refusal::none : Refusal::insufficient_evidence;
    }
}
