/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

export module ai.voice_session;

export namespace epochengine::ai::voice
{
    enum class Mode : std::uint8_t { dictation, conversation };
    enum class Permission : std::uint8_t { not_requested, granted, denied };
    enum class State : std::uint8_t
    {
        unavailable, idle, awaiting_microphone_consent, capturing,
        transcribing, transcript_review, awaiting_response, speaking, faulted
    };
    enum class Result : std::uint8_t
    {
        success, unavailable, consent_required, consent_denied, invalid_state,
        invalid_policy, empty_transcript, transcript_too_large, speech_too_large,
        interruption_unavailable
    };

    struct ProviderCapabilities final
    {
        bool microphone_capture{};
        bool speech_to_text{};
        bool text_to_speech{};
        bool interruption{};

        [[nodiscard]] constexpr bool can_transcribe() const noexcept
        {
            return microphone_capture && speech_to_text;
        }
    };

    struct Policy final
    {
        std::size_t maximum_transcript_bytes{32u * 1024u};
        std::size_t maximum_speech_bytes{64u * 1024u};
        bool require_transcript_review{true};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_transcript_bytes > 0u
                && maximum_transcript_bytes <= 1024u * 1024u
                && maximum_speech_bytes > 0u
                && maximum_speech_bytes <= 1024u * 1024u
                && require_transcript_review;
        }
    };

    struct Snapshot final
    {
        ProviderCapabilities capabilities{};
        Policy policy{};
        Permission microphone_permission{Permission::not_requested};
        Mode mode{Mode::dictation};
        State state{State::unavailable};
        std::string pending_transcript{};
        std::string status{};
        std::uint64_t generation{};
        bool session_active{};
    };

    [[nodiscard]] constexpr std::string_view state_name(State state) noexcept
    {
        switch (state)
        {
        case State::unavailable: return "unavailable";
        case State::idle: return "idle";
        case State::awaiting_microphone_consent: return "awaiting microphone consent";
        case State::capturing: return "listening";
        case State::transcribing: return "transcribing";
        case State::transcript_review: return "review transcript";
        case State::awaiting_response: return "waiting for response";
        case State::speaking: return "speaking";
        case State::faulted: return "faulted";
        }
        return "unknown";
    }

    class Session final
    {
    public:
        [[nodiscard]] Result configure(ProviderCapabilities capabilities, Policy policy = {})
        {
            if (!policy.valid())
                return fail(Result::invalid_policy, State::faulted, "Voice policy is invalid.");
            capabilities_ = capabilities;
            policy_ = policy;
            pending_transcript_.clear();
            session_active_ = false;
            const bool ready = capabilities_.can_transcribe();
            state_ = ready ? State::idle : State::unavailable;
            status_ = ready
                ? "Voice providers are ready; microphone use still requires approval."
                : "A microphone capture and speech-to-text provider are required.";
            ++generation_;
            return ready ? Result::success : Result::unavailable;
        }

        [[nodiscard]] Result request_start(Mode mode)
        {
            if (!capabilities_.can_transcribe())
                return fail(Result::unavailable, state_,
                    "Voice input requires capture and speech-to-text providers.");
            if (state_ != State::idle)
                return Result::invalid_state;
            mode_ = mode;
            if (microphone_permission_ == Permission::denied)
                return fail(Result::consent_denied, State::idle,
                    "Microphone use was denied for this Epoch session.");
            if (microphone_permission_ != Permission::granted)
                return fail(Result::consent_required, State::awaiting_microphone_consent,
                    "Approve microphone use for this visible voice session.");
            session_active_ = true;
            return fail(Result::success, State::capturing,
                "Listening. Stop or cancel remains available.");
        }

        [[nodiscard]] Result resolve_microphone_consent(bool approved)
        {
            if (state_ != State::awaiting_microphone_consent)
                return Result::invalid_state;
            microphone_permission_ = approved ? Permission::granted : Permission::denied;
            session_active_ = approved;
            return fail(approved ? Result::success : Result::consent_denied,
                approved ? State::capturing : State::idle,
                approved
                    ? "Microphone approved for this visible Epoch session."
                    : "Microphone use was denied; no input device was opened.");
        }

        [[nodiscard]] Result finish_capture()
        {
            if (state_ != State::capturing)
                return Result::invalid_state;
            return fail(Result::success, State::transcribing,
                "Capture stopped. Waiting for the speech-to-text provider.");
        }

        [[nodiscard]] Result submit_transcript(std::string transcript)
        {
            if (state_ != State::transcribing)
                return Result::invalid_state;
            if (transcript.empty())
                return fail(Result::empty_transcript, State::transcribing,
                    "The speech-to-text provider returned an empty transcript.");
            if (transcript.size() > policy_.maximum_transcript_bytes)
                return fail(Result::transcript_too_large, State::transcribing,
                    "The transcript exceeded the configured limit.");
            pending_transcript_ = std::move(transcript);
            return fail(Result::success, State::transcript_review,
                "Review the transcript before Epoch uses it.");
        }

        [[nodiscard]] std::optional<std::string> approve_transcript()
        {
            if (state_ != State::transcript_review || pending_transcript_.empty())
                return std::nullopt;
            std::string approved = std::move(pending_transcript_);
            pending_transcript_.clear();
            state_ = mode_ == Mode::conversation ? State::awaiting_response : State::idle;
            if (mode_ == Mode::dictation)
                session_active_ = false;
            status_ = mode_ == Mode::conversation
                ? "Transcript approved. Waiting for the selected model response."
                : "Transcript approved and returned to the focused text field.";
            ++generation_;
            return approved;
        }

        [[nodiscard]] Result reject_transcript()
        {
            if (state_ != State::transcript_review)
                return Result::invalid_state;
            pending_transcript_.clear();
            session_active_ = false;
            return fail(Result::success, State::idle,
                "Transcript discarded. No model request was sent.");
        }

        [[nodiscard]] Result begin_speech(std::size_t text_bytes)
        {
            if (!capabilities_.text_to_speech)
                return fail(Result::unavailable, state_,
                    "No text-to-speech provider is configured.");
            if (state_ != State::awaiting_response && state_ != State::idle)
                return Result::invalid_state;
            if (text_bytes == 0u || text_bytes > policy_.maximum_speech_bytes)
                return fail(Result::speech_too_large, state_,
                    "The speech request is empty or exceeds the configured limit.");
            return fail(Result::success, State::speaking,
                "Speaking through the approved local voice provider.");
        }

        [[nodiscard]] Result complete_speech()
        {
            if (state_ != State::speaking)
                return Result::invalid_state;
            const State next = session_active_ && mode_ == Mode::conversation
                ? State::capturing : State::idle;
            return fail(Result::success, next,
                next == State::capturing
                    ? "Conversation session is listening for the next turn."
                    : "Voice output complete.");
        }

        [[nodiscard]] Result interrupt_speech()
        {
            if (state_ != State::speaking)
                return Result::invalid_state;
            if (!capabilities_.interruption)
                return Result::interruption_unavailable;
            const State next = session_active_ && mode_ == Mode::conversation
                ? State::capturing : State::idle;
            return fail(Result::success, next, "Voice output interrupted by the operator.");
        }

        void cancel() noexcept
        {
            pending_transcript_.clear();
            session_active_ = false;
            state_ = capabilities_.can_transcribe() ? State::idle : State::unavailable;
            status_ = "Voice session stopped; microphone capture is inactive.";
            ++generation_;
        }

        void reset_session_permission() noexcept
        {
            cancel();
            microphone_permission_ = Permission::not_requested;
            status_ = "Microphone approval will be requested before the next voice session.";
            ++generation_;
        }

        [[nodiscard]] Snapshot snapshot() const
        {
            return {.capabilities = capabilities_, .policy = policy_,
                .microphone_permission = microphone_permission_, .mode = mode_,
                .state = state_, .pending_transcript = pending_transcript_,
                .status = status_, .generation = generation_,
                .session_active = session_active_};
        }

    private:
        [[nodiscard]] Result fail(Result result, State state, std::string status)
        {
            state_ = state;
            status_ = std::move(status);
            ++generation_;
            return result;
        }

        ProviderCapabilities capabilities_{};
        Policy policy_{};
        Permission microphone_permission_{Permission::not_requested};
        Mode mode_{Mode::dictation};
        State state_{State::unavailable};
        std::string pending_transcript_{};
        std::string status_{"A microphone capture and speech-to-text provider are required."};
        std::uint64_t generation_{};
        bool session_active_{};
    };

    [[nodiscard]] inline bool run_voice_session_contract()
    {
        Session session{};
        if (session.request_start(Mode::dictation) != Result::unavailable)
            return false;
        if (session.configure({.microphone_capture = true, .speech_to_text = true})
                != Result::success
            || session.request_start(Mode::dictation) != Result::consent_required
            || session.resolve_microphone_consent(true) != Result::success
            || session.finish_capture() != Result::success
            || session.submit_transcript("create a light") != Result::success)
            return false;
        const auto approved = session.approve_transcript();
        if (!approved || *approved != "create a light" || session.snapshot().session_active)
            return false;

        session.reset_session_permission();
        if (session.request_start(Mode::conversation) != Result::consent_required
            || session.resolve_microphone_consent(false) != Result::consent_denied
            || session.request_start(Mode::conversation) != Result::consent_denied)
            return false;

        if (session.configure({.microphone_capture = true, .speech_to_text = true,
                .text_to_speech = true, .interruption = true}) != Result::success)
            return false;
        session.reset_session_permission();
        if (session.request_start(Mode::conversation) != Result::consent_required
            || session.resolve_microphone_consent(true) != Result::success
            || session.finish_capture() != Result::success
            || session.submit_transcript("hello") != Result::success
            || !session.approve_transcript()
            || session.begin_speech(12u) != Result::success
            || session.interrupt_speech() != Result::success
            || session.snapshot().state != State::capturing)
            return false;
        session.cancel();
        const auto final = session.snapshot();
        return final.state == State::idle
            && !final.session_active
            && final.pending_transcript.empty();
    }
}