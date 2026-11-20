// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

#include <epoch.engine.hpp>
#include <array>
#include <cstdint>
#include <limits>
#include <utility>

#include "calculator.arithmetic.hpp"
#include "calculator.coach.hpp"
#include "calculator.scientific.hpp"

namespace calculator_example
{
    inline constexpr std::string_view scientific_expression_widget = "CalculatorExpression";
    inline constexpr std::string_view scientific_result_widget = "CalculatorScientificResult";
    inline constexpr std::string_view worked_steps_widget = "CalculatorWorkedSteps";
    inline constexpr std::string_view help_tip_widget = "CalculatorScientificTip";
    inline constexpr std::string_view coach_question_widget = "CalculatorCoachQuestion";
    inline constexpr std::string_view coach_status_widget = "CalculatorCoachStatus";
    inline constexpr std::string_view coach_reply_widget = "CalculatorCoachReply";
    inline constexpr std::string_view coach_steps_widget = "CalculatorCoachWorkedSteps";
    inline constexpr std::string_view coach_unavailable_status = "No Coach connected. See Reference for offline help.";
    inline constexpr std::string_view coach_add_model_status = "Tap Ask to add a model.";
    inline constexpr std::size_t maximum_coach_display_bytes = 256u;

    struct ScientificTip final
    {
        std::string_view text;
        std::string_view expression;
        std::string_view radians_expression{};

        [[nodiscard]] constexpr std::string_view expression_for(AngleUnit angle) const noexcept
        {
            return angle == AngleUnit::radians && !radians_expression.empty()
                ? radians_expression : expression;
        }
    };

    inline constexpr std::array<ScientificTip, 5> scientific_tips{{
        {"2^8 raises 2\nto the eighth power.", "2^8"},
        {"Try sqrt(16).\nabs(-7) returns 7.", "sqrt(16)"},
        {"Degrees: sin(30)\nRadians: sin(pi/6)", "sin(30)", "sin(pi/6)"},
        {"Try log(100).\nUse ln for natural log.", "log(100)"},
        {"Try 2 + 3 * 4.\nMultiply first.", "2 + 3 * 4"}
    }};

    class Application final : public epochengine::project::GuiApplication
    {
    public:
        explicit Application(CoachTransport coach = {}, CoachModelPicker model_picker = {}) noexcept
            : coach_{coach}, coach_model_picker_{model_picker} {}
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
        ~Application() override
        {
            if (coach_generation_ != 0u && !coach_stopping_)
                coach_.stop(coach_.context, coach_generation_);
        }

        [[nodiscard]] bool handle(epochengine::project::GuiApplicationEvent event,
            epochengine::project::GuiApplicationView view) override
        {
            using Kind = epochengine::project::GuiApplicationEventKind;
            if (event.kind == Kind::action)
            {
                if (event.value == "calculator.coach.ask") return ask_coach(view);
                if (event.value == "calculator.coach.stop") return stop_coach(view);
                if (event.value == "calculator.scientific.next_tip")
                {
                    const auto next = (scientific_tip_ + 1u) % scientific_tips.size();
                    if (!view.set_text || !view.set_text(view.context, help_tip_widget, scientific_tips[next].text))
                        return false;
                    scientific_tip_ = next;
                    return true;
                }
                if (event.value == "calculator.scientific.run_example")
                {
                    const auto expression = scientific_tips[scientific_tip_].expression_for(angle_unit_);
                    return view.set_text && view.set_text(view.context, scientific_expression_widget, expression)
                        && show_calculation(expression, view);
                }
                if (event.value == "calculator.scientific.evaluate")
                {
                    std::string_view expression;
                    return view.get_text && view.get_text(view.context, scientific_expression_widget, expression)
                        && show_calculation(expression, view);
                }
                if (event.value == "calculator.scientific.angle")
                {
                    const auto next = angle_unit_ == AngleUnit::degrees ? AngleUnit::radians : AngleUnit::degrees;
                    if (!view.set_text || !view.set_text(view.context, "CalculatorAngleStatus",
                            next == AngleUnit::degrees ? "Degrees" : "Radians"))
                        return false;
                    angle_unit_ = next;
                    // Results describe the angle unit used during evaluation.
                    // Clear the previous calculation when that unit changes.
                    clear_output(view, scientific_result_widget);
                    clear_output(view, worked_steps_widget);
                    return true;
                }
                const auto* key = find_key(event.value);
                return key && apply(*key, view);
            }

            char input{};
            switch (event.kind)
            {
            case Kind::action: return false;
            case Kind::backspace: input = 'b'; break;
            case Kind::erase: input = 'c'; break;
            case Kind::enter: input = '='; break;
            case Kind::text:
                if (event.value.size() != 1u) return false;
                input = event.value.front();
                // Enter can produce both a key command and a text notification.
                if (input == '\r' || input == '\n') return true;
                break;
            }
            for (const auto& key : keys)
                if (key.input == input) return apply(key, view);
            return false;
        }

        void on_frame(epochengine::project::GuiApplicationView view) override
        {
            if (!coach_status_initialized_)
            {
                const auto status = coach_ && coach_model_picker_ && !coach_model_picker_.installed
                    ? coach_add_model_status : coach_ ? "Coach ready." : coach_unavailable_status;
                set_coach_status(view, status);
                // Old project layouts can have no coach controls at all.
                coach_status_initialized_ = true;
            }
            if (coach_import_pending_) poll_model_import(view);
            if (coach_generation_ == 0u) return;

            CoachCompletion completion;
            if (!coach_.poll(coach_.context, completion) || completion.generation != coach_generation_)
                return;

            coach_generation_ = 0u;
            const bool stopped = std::exchange(coach_stopping_, false);
            if (completion.code == CoachCompletionCode::retirement_failed)
            {
                set_coach_status(view, "Coach worker retirement could not be verified.");
                return;
            }
            if (stopped || completion.code == CoachCompletionCode::cancelled)
            {
                set_coach_status(view, "Stopped.");
                return;
            }
            if (completion.code == CoachCompletionCode::model_rejected && coach_model_picker_)
            {
                coach_model_picker_.installed = false;
                set_coach_status(view, "Model invalid. Tap Ask.");
                return;
            }
            if (completion.code != CoachCompletionCode::reply)
            {
                set_coach_status(view, "Coach request failed. Try again.");
                return;
            }

            const auto response = interpret_coach_response(completion.reply, coach_angle_unit_);
            if (!response)
            {
                set_coach_status(view, "Coach could not give a valid calculator reply.");
                return;
            }
            publish_coach_response(response, view);
        }

    private:
        void set_coach_status(epochengine::project::GuiApplicationView view, std::string_view text)
        {
            if (view.set_text && view.set_text(view.context, coach_status_widget, text))
                coach_status_initialized_ = true;
        }

        void poll_model_import(epochengine::project::GuiApplicationView view)
        {
            const auto result = coach_model_picker_.poll();
            if (result == CoachModelImport::pending) return;
            coach_import_pending_ = false;
            switch (result)
            {
            case CoachModelImport::pending: break;
            case CoachModelImport::installed:
                coach_model_picker_.installed = true;
                set_coach_status(view, "Coach ready.");
                break;
            case CoachModelImport::cancelled:
                set_coach_status(view, "Model selection cancelled.");
                break;
            case CoachModelImport::wrong_file:
                set_coach_status(view, "Choose Qwen3-0.6B Q4_0.");
                break;
            case CoachModelImport::low_storage:
                set_coach_status(view, "Free at least 409 MiB.");
                break;
            case CoachModelImport::failed:
                set_coach_status(view, "Model import failed.");
                break;
            case CoachModelImport::timed_out:
                set_coach_status(view, "Model import timed out.");
                break;
            }
        }

        [[nodiscard]] bool ask_coach(epochengine::project::GuiApplicationView view)
        {
            if (!coach_)
            {
                set_coach_status(view, coach_unavailable_status);
                return true;
            }
            if (coach_generation_ != 0u) return true;
            if (coach_model_picker_ && !coach_model_picker_.installed)
            {
                if (coach_import_pending_) return true;
                coach_import_pending_ = coach_model_picker_.open();
                set_coach_status(view, coach_import_pending_
                    ? "Choose the 409 MiB local model." : "Could not open the model picker. Try again.");
                return true;
            }

            std::string_view question;
            if (!view.get_text || !view.get_text(view.context, coach_question_widget, question))
                return false;
            constexpr std::string_view whitespace = " \t\r\n";
            const auto begin = question.find_first_not_of(whitespace);
            if (begin == std::string_view::npos)
            {
                set_coach_status(view, "Enter a question first.");
                return true;
            }
            question = question.substr(begin, question.find_last_not_of(whitespace) - begin + 1u);
            if (!valid_coach_question(question))
            {
                set_coach_status(view, "Use a single question of up to 256 bytes.");
                return true;
            }
            if (next_coach_generation_ == (std::numeric_limits<std::uint64_t>::max)())
            {
                set_coach_status(view, "Restart the calculator to start another coach request.");
                return true;
            }

            // The view invalidates borrowed text on mutation. Own the bounded
            // question before setting status or handing it to the transport.
            const std::string owned_question{question};
            if (!view.set_text || !view.set_text(view.context, coach_status_widget, "Waiting for reply."))
                return false;
            coach_status_initialized_ = true;
            const auto generation = ++next_coach_generation_;
            if (!coach_.submit(coach_.context, generation, owned_question, angle_unit_))
            {
                set_coach_status(view, "Coach could not start the request. Try again.");
                return true;
            }
            coach_generation_ = generation;
            coach_angle_unit_ = angle_unit_;
            clear_output(view, coach_reply_widget);
            clear_output(view, coach_steps_widget);
            return true;
        }

        [[nodiscard]] bool stop_coach(epochengine::project::GuiApplicationView view)
        {
            if (!coach_)
            {
                set_coach_status(view, coach_unavailable_status);
                return true;
            }
            if (coach_generation_ == 0u)
            {
                set_coach_status(view, "No coach request is running.");
                return true;
            }
            if (!std::exchange(coach_stopping_, true))
            {
                coach_.stop(coach_.context, coach_generation_);
                set_coach_status(view, "Stopping coach request.");
            }
            return true;
        }

        void publish_coach_response(const CoachResponse& response, epochengine::project::GuiApplicationView view)
        {
            std::string calculation;
            std::string steps;
            auto text = response.reviewed_text();
            std::string_view status = response.kind == CoachReplyKind::clarification
                ? "Coach needs clarification." : "Coach replied.";
            if (response.kind == CoachReplyKind::calculation)
            {
                std::string value;
                if (!format_calculation_value(response.calculation.value, value))
                {
                    set_coach_status(view, "Coach could not display the result.");
                    return;
                }
                calculation = response.angle_used == AngleUnit::degrees ? "Degrees\n" : "Radians\n";
                const auto expression = response.expression();
                const auto space = maximum_coach_display_bytes - calculation.size() - value.size() - 3u;
                if (expression.size() > space)
                    calculation += std::string{expression.substr(0u, space - 3u)} + "...";
                else
                    calculation += expression;
                calculation += " = " + value;
                steps = worked_steps(expression, response.trace);
                text = calculation;
                status = "Calculation complete.";
            }
            if (!view.set_text || !view.set_text(view.context, coach_reply_widget, text)
                || !view.set_text(view.context, coach_steps_widget, steps))
                status = "Coach display is unavailable.";
            set_coach_status(view, status);
        }

        static void clear_output(epochengine::project::GuiApplicationView view,
            std::string_view widget)
        {
            std::string_view value;
            if (view.get_text && view.get_text(view.context, widget, value) && !value.empty())
                (void)view.set_text(view.context, widget, {});
        }

        [[nodiscard]] static std::string worked_steps(std::string_view expression,
            const CalculationTrace& trace)
        {
            if (trace.count == 0) return "No intermediate operations.";
            std::string lines;
            const auto append_step = [&](std::size_t index) {
                const auto& step = trace.steps[index];
                std::string value;
                if (!format_calculation_value(step.value, value)) return;
                constexpr std::size_t line_width = 38u;
                static_assert(line_width >= display_limit + 6u);
                const auto expression_width = line_width - 3u - value.size();
                const auto part = expression.substr(step.begin, step.end - step.begin);
                if (!lines.empty()) lines += '\n';
                const bool shortened = part.size() > expression_width;
                const auto shown = shortened ? part.substr(0u, expression_width - 3u) : part;
                for (const char ch : shown)
                    lines += ch == '\t' || ch == '\r' || ch == '\n' ? ' ' : ch;
                if (shortened) lines += "...";
                lines += " = ";
                lines += value;
            };
            if (trace.count <= 3u)
                for (std::size_t index = 0u; index < trace.count; ++index) append_step(index);
            else
            {
                append_step(0u);
                lines += "\n" + std::to_string(trace.count - 2u) + " steps omitted";
                append_step(trace.count - 1u);
            }
            return lines;
        }

        [[nodiscard]] bool show_calculation(std::string_view expression,
            epochengine::project::GuiApplicationView view) const
        {
            if (!view.set_text) return false;
            std::string_view previous_steps;
            const bool show_steps = view.get_text
                && view.get_text(view.context, worked_steps_widget, previous_steps);
            CalculationTrace trace;
            const auto result = evaluate_expression(expression, angle_unit_, show_steps ? &trace : nullptr);
            std::string text;
            if (!result) text = calculation_error_message(result.error);
            else if (!format_calculation_value(result.value, text)) return false;
            const auto steps = show_steps && result ? worked_steps(expression, trace) : std::string{};
            if (!view.set_text(view.context, scientific_result_widget, text)) return false;
            return !show_steps || view.set_text(view.context, worked_steps_widget, steps);
        }

        [[nodiscard]] bool apply(const Key& key, epochengine::project::GuiApplicationView view)
        {
            // Keep arithmetic and the visible display in sync if an edit removed
            // or replaced the named display widget.
            if (!view.set_text) return false;
            auto next = calculator_;
            next.press(key);
            if (!view.set_text(view.context, display_widget, next.display())) return false;
            calculator_ = std::move(next);
            return true;
        }

        Calculator calculator_{};
        AngleUnit angle_unit_{AngleUnit::degrees};
        std::size_t scientific_tip_{};
        CoachTransport coach_{};
        CoachModelPicker coach_model_picker_{};
        std::uint64_t next_coach_generation_{};
        std::uint64_t coach_generation_{};
        AngleUnit coach_angle_unit_{AngleUnit::degrees};
        bool coach_stopping_{};
        bool coach_status_initialized_{};
        bool coach_import_pending_{};
    };
}

[[nodiscard]] inline calculator_example::Application make_project_gui_application(
    calculator_example::CoachTransport coach = {}, calculator_example::CoachModelPicker model_picker = {})
{
    return calculator_example::Application{coach, model_picker};
}
