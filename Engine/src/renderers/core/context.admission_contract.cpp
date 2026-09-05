// SPDX-License-Identifier: LicenseRef-MIT-NoSell
import context.admission;
import context.type;

namespace
{
    using epochengine::core::ContextType;
    using namespace epochengine::core::contextadmission;

    [[nodiscard]] constexpr AdmissionRequest native_request(ContextType backend = ContextType::Software)
    {
        AdmissionRequest request{};
        request.backend = backend;
        request.identity = {1, 1, 10, 100, 1000};
        request.host_process_id = 10;
        request.window_ownership = Ownership::borrowed;
        request.resource_ownership = Ownership::owned;
        request.owner_thread_id = 3;
        request.event_thread_id = 2;
        request.calling_thread_id = 3;
        request.capabilities.availability = Support::supported;
        return request;
    }

    [[nodiscard]] constexpr bool admission_contract()
    {
        auto request = native_request();
        if (!admit(request).allowed() || admit(AdmissionRequest{}).allowed())
            return false;
        request.capabilities.availability = Support::unknown;
        if (admit(request).refusal != Refusal::capability_unknown)
            return false;
        request.capabilities.availability = Support::unsupported;
        if (admit(request).refusal != Refusal::unavailable)
            return false;
        request = native_request();
        request.requires_embedding = true;
        if (admit(request).refusal != Refusal::capability_unknown)
            return false;
        request.capabilities.embedding = Support::unsupported;
        if (admit(request).refusal != Refusal::capability_unsupported)
            return false;
        request.capabilities.embedding = Support::supported;
        if (!admit(request).allowed())
            return false;
        request = native_request();
        request.requires_input = true;
        if (admit(request).refusal != Refusal::capability_unknown)
            return false;
        request.capabilities.input_route = Support::supported;
        request.requires_final_capture = true;
        if (admit(request).refusal != Refusal::capability_unknown)
            return false;
        request.capabilities.final_capture = Support::supported;
        if (!admit(request).allowed())
            return false;

        constexpr ContextType singletons[]{ContextType::Software, ContextType::RayLib, ContextType::SDL, ContextType::SFML};
        for (const auto backend : singletons)
        {
            request = native_request(backend);
            request.active_same_backend_instances = 1;
            if (admit(request).refusal != Refusal::instance_limit)
                return false;
        }
        constexpr ContextType unproven[]{ContextType::OpenGL, ContextType::Vulkan, ContextType::DirectX};
        for (const auto backend : unproven)
        {
            request = native_request(backend);
            if (!admit(request).allowed())
                return false;
            request.active_same_backend_instances = 1;
            if (admit(request).refusal != Refusal::instance_policy_unknown)
                return false;
        }
        if (backend_traits(ContextType::DirectX).activation != Activation::d3d11_owner)
            return false;

        request = native_request();
        request.calling_thread_id = 99;
        if (admit(request).refusal != Refusal::wrong_thread)
            return false;
        request.dispatch = Dispatch::owner_queue;
        if (!admit(request).allowed())
            return false;
        request.event_thread_id = 0;
        if (admit(request).refusal != Refusal::invalid_thread_binding)
            return false;
        request = native_request();
        request.resource_ownership = Ownership::external_process;
        if (admit(request).refusal != Refusal::invalid_ownership)
            return false;
        request = native_request();
        request.identity.process_id = 11;
        if (admit(request).refusal != Refusal::invalid_ownership)
            return false;
        request = native_request(ContextType::Custom);
        if (admit(request).refusal != Refusal::invalid_purpose)
            return false;

        request = native_request(ContextType::Noop);
        request.purpose = Purpose::headless;
        request.identity.native_window = 0;
        request.window_ownership = Ownership::none;
        request.resource_ownership = Ownership::none;
        request.event_thread_id = 0;
        request.active_same_backend_instances = 5;
        if (!admit(request).allowed())
            return false;
        request.requires_final_capture = true;
        request.capabilities.final_capture = Support::supported;
        if (admit(request).refusal != Refusal::capability_unsupported)
            return false;
        request.requires_final_capture = false;
        request.backend = ContextType::Software;
        return admit(request).refusal == Refusal::invalid_purpose;
    }

    [[nodiscard]] constexpr bool evidence_contract()
    {
        auto request = native_request();
        auto admission = admit(request);
        SurfaceEvidence evidence{request.identity, 7, 0, 0, 0, true, EvidenceKind::attached};
        const auto evaluate = [&admission, &request, &evidence](EvidenceKind required) constexpr
        {
            return evaluate_readiness(admission, request.identity, 7, evidence, required);
        };
        if (evaluate(EvidenceKind::attached) != Refusal::none
            || evaluate(EvidenceKind::native_presented) != Refusal::insufficient_evidence
            || evaluate(EvidenceKind::host_captured_frame) != Refusal::insufficient_evidence)
            return false;
        evidence.backend_ready = false;
        if (evaluate(EvidenceKind::attached) != Refusal::not_ready)
            return false;
        evidence.backend_ready = true;
        evidence.surface_generation = 6;
        if (evaluate(EvidenceKind::attached) != Refusal::stale_surface)
            return false;
        evidence.surface_generation = 7;
        evidence.identity.generation = 2;
        if (evaluate(EvidenceKind::attached) != Refusal::stale_identity)
            return false;
        evidence.identity = request.identity;
        evidence.identity.process_start_token += 1;
        if (evaluate(EvidenceKind::attached) != Refusal::stale_identity)
            return false;
        evidence.identity = request.identity;
        evidence.identity.native_window += 1;
        if (evaluate(EvidenceKind::attached) != Refusal::stale_identity)
            return false;
        evidence.identity = request.identity;
        evidence.frame_generation = 1;
        if (evaluate(EvidenceKind::attached) != Refusal::invalid_evidence)
            return false;
        evidence.kind = EvidenceKind::native_presented;
        if (evaluate(EvidenceKind::native_presented) != Refusal::invalid_evidence)
            return false;
        evidence.width = 640;
        evidence.height = 480;
        if (evaluate(EvidenceKind::native_presented) != Refusal::capability_unknown)
            return false;
        request.capabilities.native_presentation = Support::supported;
        request.capabilities.final_capture = Support::supported;
        admission = admit(request);
        if (evaluate(EvidenceKind::native_presented) != Refusal::none)
            return false;
        evidence.kind = EvidenceKind::host_captured_frame;
        if (evaluate(EvidenceKind::host_captured_frame) != Refusal::none
            || evaluate(EvidenceKind::native_presented) != Refusal::insufficient_evidence)
            return false;
        evidence.kind = EvidenceKind::child_reported_present;
        if (evaluate(EvidenceKind::child_reported_present) != Refusal::invalid_evidence)
            return false;
        auto stale_current = request.identity;
        stale_current.context_id += 1;
        return evaluate_readiness(admission, stale_current, 7, evidence, EvidenceKind::attached)
            == Refusal::stale_identity;
    }

    [[nodiscard]] constexpr bool external_contract()
    {
        auto request = native_request(ContextType::Custom);
        request.purpose = Purpose::external_process_window;
        request.identity.process_id = 20;
        request.window_ownership = Ownership::external_process;
        request.resource_ownership = Ownership::external_process;
        request.capabilities.final_capture = Support::supported;
        auto admission = admit(request);
        if (!admission.allowed() || admission.traits.activation != Activation::external_process)
            return false;
        SurfaceEvidence evidence{request.identity, 1, 1, 640, 480, true, EvidenceKind::child_reported_present};
        if (evaluate_readiness(admission, request.identity, 1, evidence, EvidenceKind::child_reported_present) != Refusal::none
            || evaluate_readiness(admission, request.identity, 1, evidence, EvidenceKind::host_captured_frame) != Refusal::insufficient_evidence)
            return false;
        evidence.kind = EvidenceKind::native_presented;
        if (evaluate_readiness(admission, request.identity, 1, evidence, EvidenceKind::native_presented) != Refusal::invalid_evidence)
            return false;
        evidence.kind = EvidenceKind::host_captured_frame;
        if (evaluate_readiness(admission, request.identity, 1, evidence, EvidenceKind::host_captured_frame) != Refusal::none)
            return false;
        request.window_ownership = Ownership::owned;
        if (admit(request).refusal != Refusal::invalid_ownership)
            return false;
        request.window_ownership = Ownership::external_process;
        request.identity.process_id = request.host_process_id;
        if (admit(request).refusal != Refusal::invalid_ownership)
            return false;
        request.identity.process_id = 20;
        request.identity.native_window = 0;
        return admit(request).refusal == Refusal::invalid_ownership;
    }

    [[nodiscard]] constexpr bool profile_contract()
    {
        ContextProfile profile{};
        profile.backend = ContextType::Noop;
        profile.purpose = Purpose::headless;
        profile.capabilities.availability = Support::supported;
        if (preflight(profile) != Refusal::none)
            return false;
        AdmissionRequest unobserved{};
        static_cast<ContextProfile&>(unobserved) = profile;
        if (admit(unobserved).refusal != Refusal::invalid_identity)
            return false;
        profile.backend = ContextType::Custom;
        profile.purpose = Purpose::platform_window;
        profile.window_ownership = Ownership::owned;
        profile.requires_input = true;
        if (preflight(profile) != Refusal::capability_unknown)
            return false;
        profile.capabilities.input_route = Support::supported;
        if (preflight(profile) != Refusal::none)
            return false;
        profile.requires_final_capture = true;
        profile.capabilities.final_capture = Support::supported;
        if (preflight(profile) != Refusal::capability_unsupported)
            return false;
        profile.requires_final_capture = false;
        auto request = native_request();
        static_cast<ContextProfile&>(request) = profile;
        const auto admission = admit(request);
        if (!admission.allowed())
            return false;
        SurfaceEvidence evidence{request.identity, 1, 0, 0, 0, true, EvidenceKind::attached};
        if (evaluate_readiness(admission, request.identity, 1, evidence, EvidenceKind::attached) != Refusal::none)
            return false;
        evidence.kind = EvidenceKind::host_captured_frame;
        evidence.frame_generation = 1;
        evidence.width = 640;
        evidence.height = 480;
        if (evaluate_readiness(admission, request.identity, 1, evidence, EvidenceKind::host_captured_frame)
            != Refusal::capability_unsupported)
            return false;
        request = native_request(ContextType::Noop);
        request.purpose = Purpose::headless;
        request.identity.native_window = 0;
        request.window_ownership = Ownership::none;
        request.resource_ownership = Ownership::none;
        evidence = {request.identity, 0, 0, 0, 0, true, EvidenceKind::initialized};
        if (evaluate_readiness(admit(request), request.identity, 0, evidence, EvidenceKind::initialized)
            != Refusal::none)
            return false;
        profile.purpose = static_cast<Purpose>(255);
        if (preflight(profile) != Refusal::invalid_purpose)
            return false;
        request = native_request(static_cast<ContextType>(255));
        return admit(request).refusal == Refusal::invalid_purpose;
    }

    static_assert(admission_contract());
    static_assert(evidence_contract());
    static_assert(external_contract());
    static_assert(profile_contract());
}

int main()
{
    return admission_contract() && evidence_contract() && external_contract() && profile_contract() ? 0 : 1;
}
