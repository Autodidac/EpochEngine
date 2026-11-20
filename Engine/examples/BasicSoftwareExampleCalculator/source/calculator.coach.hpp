// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <semaphore>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "calculator.scientific.hpp"

namespace calculator_example
{
    enum class CoachReplyKind { rejected, calculation, help, clarification };
    enum class CoachRejection { none, empty, too_long, invalid_line, unknown_command, unknown_topic, invalid_expression };
    enum class CoachHelpTopic { syntax, precedence, angles, functions, constants, errors, circle_distance, percentages };
    enum class CoachClarification { expression, angle_unit, unsupported, circle_distance, discount };

    // Command and optional angle unit, the expression limit, and one optional CRLF.
    inline constexpr std::size_t maximum_coach_reply_bytes = 9u + maximum_expression_length + 2u;
    inline constexpr std::size_t maximum_coach_question_bytes = 256u;

    [[nodiscard]] constexpr bool valid_coach_question(std::string_view question) noexcept
    {
        return !question.empty() && question.size() <= maximum_coach_question_bytes
            && question.find('\0') == std::string_view::npos
            && question.find_first_of("\r\n") == std::string_view::npos;
    }

    enum class CoachCompletionCode { reply, failed, cancelled, retirement_failed, model_rejected };

    enum class CoachModelImport { pending, installed, cancelled, wrong_file, low_storage, failed, timed_out };

    struct CoachModelPicker final
    {
        bool installed{};
        bool (*open)() noexcept{};
        CoachModelImport (*poll)() noexcept{};

        [[nodiscard]] explicit operator bool() const noexcept { return open && poll; }
    };

    struct CoachCompletion final
    {
        std::uint64_t generation{};
        CoachCompletionCode code{CoachCompletionCode::failed};
        std::string_view reply{};
    };

    struct CoachTransport final
    {
        // GUI owner only. These calls are nonblocking. Submit copies accepted
        // input, Stop requests cancellation, and Poll reports terminal status.
        // A successful reply requires confirmed retirement. Its text is
        // borrowed until the next transport call.
        void* context{};
        bool (*submit)(void*, std::uint64_t generation, std::string_view question, AngleUnit) noexcept{};
        void (*stop)(void*, std::uint64_t generation) noexcept{};
        bool (*poll)(void*, CoachCompletion&) noexcept{};

        [[nodiscard]] explicit operator bool() const noexcept { return submit && stop && poll; }
    };

    struct CoachQueryRequest final
    {
        std::uint64_t generation{};
        std::string_view question{};
        AngleUnit angle_unit{AngleUnit::degrees};
    };

    struct CoachQueryResult final
    {
        CoachCompletionCode code{CoachCompletionCode::failed};
        std::string reply{};
        bool retirement_confirmed{};
    };

    struct CoachQuery final
    {
        // Trust and resource admission belong to the injected engine owner.
        // The owner must outlive the bridge and provide bounded cancellation.
        // Any stop callback it registers must only signal, never wait.
        void* context{};
        CoachQueryResult (*run)(void*, CoachQueryRequest, std::stop_token){};
    };

    class CoachTransportBridge final
    {
    public:
        explicit CoachTransportBridge(CoachQuery query = {}) noexcept : query_{query} {}
        CoachTransportBridge(const CoachTransportBridge&) = delete;
        CoachTransportBridge& operator=(const CoachTransportBridge&) = delete;
        ~CoachTransportBridge()
        {
            cancellation_.request_stop();
            if (worker_.joinable())
            {
                worker_.request_stop();
                work_available_.release();
            }
        }

        [[nodiscard]] CoachTransport transport() noexcept
        {
            if (!query_.run) return {};
            return {this,
                [](void* context, std::uint64_t generation, std::string_view question, AngleUnit angle) noexcept {
                    return static_cast<CoachTransportBridge*>(context)->submit(generation, question, angle);
                },
                [](void* context, std::uint64_t generation) noexcept {
                    auto& bridge = *static_cast<CoachTransportBridge*>(context);
                    if (bridge.active_ && bridge.generation_ == generation)
                        bridge.cancellation_.request_stop();
                },
                [](void* context, CoachCompletion& completion) noexcept {
                    return static_cast<CoachTransportBridge*>(context)->poll(completion);
                }};
        }

    private:
        struct PendingQuestion final
        {
            std::uint64_t generation{};
            std::string question{};
            AngleUnit angle{AngleUnit::degrees};
            std::stop_token cancellation{};
        };

        [[nodiscard]] bool submit(std::uint64_t generation, std::string_view question, AngleUnit angle) noexcept
        {
            if (!query_.run || quarantined_ || active_ || generation <= generation_
                || !valid_coach_question(question))
                return false;

            try
            {
                std::stop_source cancellation;
                PendingQuestion pending{generation, std::string{question}, angle, cancellation.get_token()};
                if (!worker_.joinable())
                    worker_ = std::jthread{[this](std::stop_token stop) { run(stop); }};
                pending_ = std::move(pending);
                cancellation_ = std::move(cancellation);
                generation_ = generation;
                active_ = true;
                work_available_.release();
                return true;
            }
            catch (...) { return false; }
        }

        void run(std::stop_token stop)
        {
            for (;;)
            {
                work_available_.acquire();
                if (stop.stop_requested()) return;
                const auto pending = std::move(pending_);

                CoachQueryResult result;
                if (pending.cancellation.stop_requested())
                    result = {CoachCompletionCode::cancelled, {}, true};
                else
                {
                    try
                    {
                        result = query_.run(query_.context,
                            {pending.generation, pending.question, pending.angle}, pending.cancellation);
                    }
                    catch (...) { result = {}; }
                }
                if (result.code == CoachCompletionCode::reply && result.reply.size() > maximum_coach_reply_bytes)
                    result.code = CoachCompletionCode::failed;
                if (!result.retirement_confirmed || result.code != CoachCompletionCode::reply
                    || pending.cancellation.stop_requested())
                    std::string{}.swap(result.reply);

                completed_ = std::move(result);
                result_ready_.store(true, std::memory_order_release);
            }
        }

        [[nodiscard]] bool poll(CoachCompletion& completion) noexcept
        {
            if (!active_ || !result_ready_.load(std::memory_order_acquire)) return false;
            auto result = std::move(completed_);
            result_ready_.store(false, std::memory_order_relaxed);

            if (!result.retirement_confirmed || result.code == CoachCompletionCode::retirement_failed)
            {
                quarantined_ = true;
                result.code = CoachCompletionCode::retirement_failed;
            }
            else if (cancellation_.stop_requested())
                result.code = CoachCompletionCode::cancelled;

            reply_ = result.code == CoachCompletionCode::reply ? std::move(result.reply) : std::string{};
            completion = {generation_, result.code, reply_};
            active_ = false;
            return true;
        }

        CoachQuery query_{};
        std::uint64_t generation_{};
        bool active_{};
        bool quarantined_{};
        std::string reply_{};
        std::stop_source cancellation_{std::nostopstate};
        // At most one pending question plus a possible shutdown wake. The
        // semaphore hands pending_ to the worker; result_ready_ hands the
        // matching completed_ back before the GUI can submit the next turn.
        std::counting_semaphore<2> work_available_{0};
        PendingQuestion pending_{};
        CoachQueryResult completed_{};
        std::atomic<bool> result_ready_{};
        // The worker parks between requests. Its destructor joins before
        // either handoff slot is destroyed.
        std::jthread worker_{};
    };

    [[nodiscard]] constexpr std::string_view coach_help_text(CoachHelpTopic topic) noexcept
    {
        switch (topic)
        {
        case CoachHelpTopic::syntax:
            return "Use numbers, parentheses, +, -, *, / and ^. Write multiplication explicitly: 2*(3+4).";
        case CoachHelpTopic::precedence:
            return "Parentheses group operations. Powers associate right: 2^3^2 is 512. -2^2 is -4; (-2)^2 is 4.";
        case CoachHelpTopic::angles:
            return "The current Degrees or Radians setting controls trigonometry. sin(30) is 0.5 in Degrees; sin(pi/2) is 1 in Radians.";
        case CoachHelpTopic::functions:
            return "Supported one-argument functions: sqrt, abs, sin, cos, tan, asin, acos, atan, ln, log, log10 and exp. Use parentheses.";
        case CoachHelpTopic::constants:
            return "Use pi and e as constants. Scientific notation is supported, for example 1.5e3 for 1500.";
        case CoachHelpTopic::errors:
            return "Check parentheses and function names. Division by zero, invalid function domains and non-finite results are rejected. Expressions are limited to 256 bytes and 32 levels.";
        case CoachHelpTopic::circle_distance:
            return "Subtract the centers to get dx and dy. Center distance is sqrt(dx*dx+dy*dy). "
                   "For overlap, compare dx*dx+dy*dy with the squared sum of radii. "
                   "For the gap between solid disks, subtract the radii from center distance and clamp at zero.";
        case CoachHelpTopic::percentages:
            return "Divide the percentage by 100. For 15% off 80, use 80*(15/100) to find the discount amount, "
                   "or 80*(1-15/100) to find the final price.";
        }
        return {};
    }

    [[nodiscard]] constexpr std::string_view coach_clarification_text(CoachClarification reason) noexcept
    {
        switch (reason)
        {
        case CoachClarification::expression: return "What numeric expression would you like to calculate?";
        case CoachClarification::angle_unit: return "Should this trigonometric calculation use Degrees or Radians?";
        case CoachClarification::unsupported: return "I can calculate supported numeric expressions or explain the calculator's controls and syntax. Which would you like?";
        case CoachClarification::circle_distance: return "Do you need the distance between centers, the gap between the disks, or just whether they touch?";
        case CoachClarification::discount: return "Do you want the amount saved or the final price after the discount?";
        }
        return {};
    }

    struct CoachResponse final
    {
        CoachReplyKind kind{CoachReplyKind::rejected};
        CoachRejection rejection{CoachRejection::empty};
        CoachHelpTopic help{CoachHelpTopic::syntax};
        CoachClarification clarification{CoachClarification::expression};
        AngleUnit angle_used{AngleUnit::degrees};
        CalculationResult calculation{.error = CalculationError::expected_value};
        CalculationTrace trace{};
        std::array<char, maximum_expression_length> expression_bytes{};
        std::size_t expression_size{};

        [[nodiscard]] explicit operator bool() const noexcept
        { return kind != CoachReplyKind::rejected && rejection == CoachRejection::none; }

        [[nodiscard]] std::string_view expression() const noexcept
        { return {expression_bytes.data(), expression_size}; }

        [[nodiscard]] std::string_view reviewed_text() const noexcept
        {
            if (kind == CoachReplyKind::help) return coach_help_text(help);
            if (kind == CoachReplyKind::clarification) return coach_clarification_text(clarification);
            return {};
        }
    };

    // This is a project-owned response interpreter, not a tool executor. It
    // accepts one typed line only. All prose comes from reviewed local text;
    // the model cannot supply a result, code, UI action, file path or command.
    [[nodiscard]] inline CoachResponse interpret_coach_response(std::string_view reply,
        AngleUnit angle = AngleUnit::degrees) noexcept
    {
        CoachResponse result;
        if (reply.size() > maximum_coach_reply_bytes)
        { result.rejection = CoachRejection::too_long; return result; }
        if (reply.ends_with("\r\n")) reply.remove_suffix(2u);
        else if (reply.ends_with('\n')) reply.remove_suffix(1u);
        if (reply.empty()) return result;
        for (const unsigned char ch : reply)
            if (ch < 32u || ch > 126u)
            { result.rejection = CoachRejection::invalid_line; return result; }

        if (reply.starts_with("CALC "))
        {
            auto expression = reply.substr(5u);
            if (expression.starts_with("DEG ") || expression.starts_with("RAD "))
            {
                angle = expression.starts_with("DEG ") ? AngleUnit::degrees : AngleUnit::radians;
                expression.remove_prefix(4u);
            }
            if (expression.size() > maximum_expression_length)
            { result.rejection = CoachRejection::too_long; return result; }
            result.angle_used = angle;
            result.expression_size = expression.size();
            for (std::size_t i = 0; i < expression.size(); ++i) result.expression_bytes[i] = expression[i];
            result.calculation = evaluate_expression(result.expression(), angle, &result.trace);
            if (!result.calculation)
            { result.rejection = CoachRejection::invalid_expression; return result; }
            result.kind = CoachReplyKind::calculation;
        }
        else if (reply.starts_with("HELP "))
        {
            const auto topic = reply.substr(5u);
            if (topic == "syntax") result.help = CoachHelpTopic::syntax;
            else if (topic == "precedence") result.help = CoachHelpTopic::precedence;
            else if (topic == "angles") result.help = CoachHelpTopic::angles;
            else if (topic == "functions") result.help = CoachHelpTopic::functions;
            else if (topic == "constants") result.help = CoachHelpTopic::constants;
            else if (topic == "errors") result.help = CoachHelpTopic::errors;
            else if (topic == "circle_distance") result.help = CoachHelpTopic::circle_distance;
            else if (topic == "percentages") result.help = CoachHelpTopic::percentages;
            else { result.rejection = CoachRejection::unknown_topic; return result; }
            result.kind = CoachReplyKind::help;
        }
        else if (reply.starts_with("CLARIFY "))
        {
            const auto reason = reply.substr(8u);
            if (reason == "expression") result.clarification = CoachClarification::expression;
            else if (reason == "angle_unit") result.clarification = CoachClarification::angle_unit;
            else if (reason == "unsupported") result.clarification = CoachClarification::unsupported;
            else if (reason == "circle_distance") result.clarification = CoachClarification::circle_distance;
            else if (reason == "discount") result.clarification = CoachClarification::discount;
            else { result.rejection = CoachRejection::unknown_topic; return result; }
            result.kind = CoachReplyKind::clarification;
        }
        else { result.rejection = CoachRejection::unknown_command; return result; }
        result.rejection = CoachRejection::none;
        return result;
    }
}
