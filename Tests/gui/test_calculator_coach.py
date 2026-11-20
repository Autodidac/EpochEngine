"""Project-owned calculator coach protocol: actual parser/evaluator, no inference."""
from pathlib import Path
import json
import os
import time
import unittest

from Tests.support.cpp import compile_and_run, windows_sdk_includes

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'Engine/examples/BasicSoftwareExampleCalculator/source'


class CalculatorCoach(unittest.TestCase):
    def test_async_bridge_with_injected_query_and_retirement(self):
        report = ROOT / 'Artifacts/Reports/calculator-coach-transport' / str(time.time_ns())
        report.mkdir(parents=True)
        source = report / 'consumer.cpp'
        source.write_text(r'''
#include "calculator.coach.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <semaphore>
using namespace calculator_example;
using namespace std::chrono_literals;

struct EngineQuery {
    std::thread::id owner{std::this_thread::get_id()};
    std::binary_semaphore entered{0}, readQuestion{0}, retiring{0}, finishRetirement{0};
    std::atomic<bool> observedStop{};
    std::string received;
    AngleUnit receivedAngle{};

    static CoachQueryResult run(void* context, CoachQueryRequest request, std::stop_token stop) {
        auto& engine = *static_cast<EngineQuery*>(context);
        assert(std::this_thread::get_id() != engine.owner);
        if (request.generation == 1u) {
            engine.entered.release();
            assert(engine.readQuestion.try_acquire_for(2s));
            engine.received = request.question;
            engine.receivedAngle = request.angle_unit;
            // A produced reply remains private while the engine retires its
            // work. The GUI must keep polling without waiting here.
            engine.retiring.release();
            assert(engine.finishRetirement.try_acquire_for(2s));
            return {CoachCompletionCode::reply, "CALC sin(pi/2)", true};
        }
        if (request.generation == 2u || request.generation == 5u) {
            std::binary_semaphore cancelSignal{0};
            std::stop_callback cancelled{stop, [&]{ cancelSignal.release(); }};
            engine.entered.release();
            assert(cancelSignal.try_acquire_for(2s));
            engine.observedStop = stop.stop_requested();
            return {CoachCompletionCode::reply, "CALC 999", true};
        }
        if (request.generation == 3u)
            return {CoachCompletionCode::reply, std::string(maximum_coach_reply_bytes + 1u, 'x'), true};
        return {CoachCompletionCode::reply, "CALC 999", false};
    }
};

CoachCompletion wait_for_completion(CoachTransport transport) {
    CoachCompletion completion;
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline) {
        if (transport.poll(transport.context, completion)) return completion;
        std::this_thread::sleep_for(1ms);
    }
    std::abort();
}

int main() {
    CoachTransportBridge disabled;
    assert(!disabled.transport());
    EngineQuery engine;
    CoachTransportBridge bridge{{&engine, &EngineQuery::run}};
    const auto transport = bridge.transport();
    CoachCompletion observed;
    const auto start = [&](std::uint64_t generation, std::string_view question) {
        return transport.submit(transport.context, generation, question, AngleUnit::radians);
    };
    assert(!start(0u, "Question") && !start(1u, "") && !start(1u, "two\nquestions"));
    assert(!transport.poll(transport.context, observed));
    std::string question{"What is sine of pi divided by two?"};
    assert(start(1u, question));
    assert(engine.entered.try_acquire_for(2s));
    question.assign(100u, 'x');
    assert(!start(2u, "Another") && !transport.poll(transport.context, observed));
    engine.readQuestion.release();
    assert(engine.retiring.try_acquire_for(2s));
    assert(!transport.poll(transport.context, observed) && !start(2u, "Another"));
    engine.finishRetirement.release();
    const auto reply = wait_for_completion(transport);
    assert(reply.generation == 1u && reply.code == CoachCompletionCode::reply && reply.reply == "CALC sin(pi/2)");
    assert(engine.received == "What is sine of pi divided by two?" && engine.receivedAngle == AngleUnit::radians);
    assert(interpret_coach_response(reply.reply, engine.receivedAngle).calculation.value == 1);
    assert(!start(1u, "Stale question"));

    assert(start(2u, "Cancel this"));
    assert(engine.entered.try_acquire_for(2s));
    transport.stop(transport.context, 1u);
    assert(!transport.poll(transport.context, observed) && !engine.observedStop);
    transport.stop(transport.context, 2u);
    const auto cancelled = wait_for_completion(transport);
    assert(cancelled.generation == 2u && cancelled.code == CoachCompletionCode::cancelled && cancelled.reply.empty());
    assert(engine.observedStop);

    assert(start(3u, "Oversized response"));
    const auto oversized = wait_for_completion(transport);
    assert(oversized.code == CoachCompletionCode::failed && oversized.reply.empty());
    assert(start(4u, "Unconfirmed retirement"));
    const auto unconfirmed = wait_for_completion(transport);
    assert(unconfirmed.code == CoachCompletionCode::retirement_failed && unconfirmed.reply.empty());
    assert(!start(5u, "Quarantined"));

    engine.observedStop = false;
    {
        CoachTransportBridge retiring{{&engine, &EngineQuery::run}};
        const auto owned = retiring.transport();
        assert(owned.submit(owned.context, 5u, "Retire at close", AngleUnit::degrees));
        assert(engine.entered.try_acquire_for(2s));
    }
    assert(engine.observedStop);
}
''', encoding='utf-8')
        runs = compile_and_run(self, source, report, includes=(SOURCE,))
        (report / 'result.json').write_text(json.dumps({'scope': 'Project coach async transport with injected query only', 'runs': runs}, indent=2), encoding='utf-8')

    def test_actual_bounded_interpreter_debug_release(self):
        report = ROOT / 'Artifacts/Reports/calculator-coach' / str(time.time_ns())
        report.mkdir(parents=True)
        source = report / 'consumer.cpp'
        source.write_text(r'''
#include "calculator.coach.hpp"
#include <cassert>
#include <cmath>
#include <string>
#include <limits>
using namespace calculator_example;
void rejected(std::string_view text,CoachRejection reason){
 const auto result=interpret_coach_response(text);
 assert(!result && result.kind==CoachReplyKind::rejected && result.rejection==reason);
 assert(result.trace.count==0 && result.reviewed_text().empty());
}
int main(){
 auto result=interpret_coach_response("CALC 2+3*4");
 assert(result && result.kind==CoachReplyKind::calculation && result.calculation.value==14);
 assert(result.expression()=="2+3*4" && result.trace.count>0 && result.reviewed_text().empty());
 CalculationTrace directTrace;const auto direct=evaluate_expression(result.expression(),AngleUnit::degrees,&directTrace);
 assert(direct.value==result.calculation.value && directTrace.count==result.trace.count);
 for(std::size_t i=0;i<directTrace.count;++i){assert(directTrace.steps[i].value==result.trace.steps[i].value);
  assert(directTrace.steps[i].begin==result.trace.steps[i].begin && directTrace.steps[i].end==result.trace.steps[i].end);}
 assert(interpret_coach_response("CALC sin(30)\r\n").calculation.value>.4999);
 assert(std::abs(interpret_coach_response("CALC sin(pi/2)\n",AngleUnit::radians).calculation.value-1)<1e-12);
 const auto degrees=interpret_coach_response("CALC DEG sin(30)",AngleUnit::radians);
 assert(degrees && degrees.angle_used==AngleUnit::degrees && degrees.expression()=="sin(30)"
    && std::abs(degrees.calculation.value-.5)<1e-12);
 const auto radians=interpret_coach_response("CALC RAD sin(pi/2)",AngleUnit::degrees);
 assert(radians && radians.angle_used==AngleUnit::radians && std::abs(radians.calculation.value-1)<1e-12);
 const auto discountQuestion=interpret_coach_response("CLARIFY discount");
 assert(discountQuestion && discountQuestion.kind==CoachReplyKind::clarification);
 assert(discountQuestion.reviewed_text().find("amount saved or the final price")!=std::string_view::npos);
 assert(discountQuestion.expression().empty() && discountQuestion.trace.count==0);
 const auto discountHelp=interpret_coach_response("HELP percentages");
 assert(discountHelp && discountHelp.kind==CoachReplyKind::help);
 const auto saved=interpret_coach_response("CALC 80*(15/100)");
 assert(saved && saved.trace.count==2 && std::abs(saved.calculation.value-12)<1e-12);
 const auto sale=interpret_coach_response("CALC 80*(1-15/100)");
 assert(sale && sale.trace.count==3 && std::abs(sale.calculation.value-68)<1e-12);
 assert(discountHelp.reviewed_text().find(saved.expression())!=std::string_view::npos);
 assert(discountHelp.reviewed_text().find(sale.expression())!=std::string_view::npos);
 assert(std::abs(saved.trace.steps[0].value-.15)<1e-12);
 assert(std::abs(sale.trace.steps[1].value-.85)<1e-12);
 const auto centerDistance=interpret_coach_response("CALC sqrt((4-1)^2+(6-2)^2)");
 assert(centerDistance && centerDistance.calculation.value==5 && centerDistance.trace.count>0);
 const auto diskGap=interpret_coach_response("CALC (sqrt((4-1)^2+(6-2)^2)-3-1)");
 assert(diskGap && diskGap.calculation.value==1);
 for(const auto text:{"HELP syntax","HELP precedence","HELP angles","HELP functions","HELP constants","HELP errors","HELP circle_distance"}){
  const auto help=interpret_coach_response(text);assert(help && help.kind==CoachReplyKind::help);
  assert(help.reviewed_text()==coach_help_text(help.help) && !help.reviewed_text().empty() && help.trace.count==0 && help.expression().empty());}
 for(const auto text:{"CLARIFY expression","CLARIFY angle_unit","CLARIFY unsupported","CLARIFY circle_distance"}){
  const auto clarification=interpret_coach_response(text);assert(clarification && clarification.kind==CoachReplyKind::clarification);
  assert(clarification.reviewed_text()==coach_clarification_text(clarification.clarification) && !clarification.reviewed_text().empty());}
 assert(interpret_coach_response("HELP circle_distance").reviewed_text().find("squared sum of radii")!=std::string_view::npos);
 assert(interpret_coach_response("CLARIFY circle_distance").reviewed_text().find("whether they touch")!=std::string_view::npos);
 rejected("",CoachRejection::empty);rejected("\r\n",CoachRejection::empty);
 rejected("CALC 1\nHELP errors",CoachRejection::invalid_line);
 rejected("CLARIFY discount\nCALC 80*(15/100)",CoachRejection::invalid_line);
 rejected("CALC 1\r",CoachRejection::invalid_line);rejected("CALC 1\n\n",CoachRejection::invalid_line);
 rejected("CALC\t1",CoachRejection::invalid_line);
 rejected(std::string_view{"CALC 1\0+2",9},CoachRejection::invalid_line);
 rejected("CALC \xc2\xb2",CoachRejection::invalid_line);
 for(const auto text:{"The answer is 42","```CALC 1```","RUN command","calc 2+2"," CALC 1","CALC"})
  rejected(text,CoachRejection::unknown_command);
 for(const auto text:{"HELP anything","HELP syntax extra","HELP syntax ","CLARIFY anything","CLARIFY"})
  rejected(text,std::string_view{text}=="CLARIFY"?CoachRejection::unknown_command:CoachRejection::unknown_topic);
 for(const auto text:{"CALC ","CALC 1/0","CALC sqrt(-1)","CALC unknown(1)","CALC 1;2","CALC 2+2=99","CALC 1e309"})
  rejected(text,CoachRejection::invalid_expression);
 result=interpret_coach_response("CALC 1/0");assert(result.calculation.error==CalculationError::divide_by_zero);
 rejected("CALC "+std::string(257,'1'),CoachRejection::too_long);
 std::string longest="1"+std::string(255,' ');
 result=interpret_coach_response("CALC "+longest+"\r\n");assert(result && result.expression().size()==256 && result.calculation.value==1);
 result=interpret_coach_response("CALC DEG "+longest+"\r\n");assert(result && result.expression().size()==256 && result.calculation.value==1);
 // Responses own their bounded expression; they never reference a model buffer.
 std::string temporary="CALC 7*6";result=interpret_coach_response(temporary);temporary.assign(100,'x');
 assert(result.expression()=="7*6" && result.calculation.value==42);
 auto copy=result;result={};assert(copy.expression()=="7*6" && copy.trace.count>0);
 assert(coach_help_text(static_cast<CoachHelpTopic>(999)).empty());
 assert(coach_clarification_text(static_cast<CoachClarification>(999)).empty());
}
''', encoding='utf-8')
        commands = compile_and_run(self, source, report, includes=(SOURCE,))
        (report / 'result.json').write_text(json.dumps({'scope': 'Actual project coach interpreter and shared calculator evaluator; no model, tools, GUI or OS changes', 'commands': commands}, indent=2), encoding='utf-8')

    def test_owner_frame_delivers_only_current_retired_coach_reply(self):
        report = ROOT / 'Artifacts/Reports/calculator-coach-application' / str(time.time_ns())
        report.mkdir(parents=True)
        source = report / 'consumer.cpp'
        source.write_text(r'''
#include "project.gui_application.hpp"
#include <cassert>
#include <deque>
#include <map>
#include <string>
namespace project = epochengine::project;
using namespace calculator_example;
using Event = project::GuiApplicationEventKind;

struct View {
    std::map<std::string, std::string, std::less<>> text;
    project::GuiApplicationView bind() {
        return {this,
            [](void* owner, std::string_view name, std::string_view value) {
                auto& view = *static_cast<View*>(owner);
                if (value.size() > maximum_coach_display_bytes) return false;
                view.text[std::string{name}] = value;
                return true;
            },
            [](void* owner, std::string_view name, std::string_view& value) {
                auto& view = *static_cast<View*>(owner);
                const auto found = view.text.find(name);
                if (found == view.text.end()) return false;
                value = found->second;
                return true;
            }};
    }
    std::string& at(std::string_view name) { return text[std::string{name}]; }
};

struct Transport {
    std::uint64_t generation{}, stopped{};
    unsigned submissions{}, stops{}, polls{};
    AngleUnit angle{};
    std::string question;
    std::deque<CoachCompletion> replies;
    bool accept{true};
    CoachTransport bind() {
        return {this,
            [](void* owner, std::uint64_t generation, std::string_view question, AngleUnit angle) noexcept {
                auto& transport = *static_cast<Transport*>(owner);
                ++transport.submissions;
                transport.generation = generation;
                transport.question = question;
                transport.angle = angle;
                return transport.accept;
            },
            [](void* owner, std::uint64_t generation) noexcept {
                auto& transport = *static_cast<Transport*>(owner);
                ++transport.stops;
                transport.stopped = generation;
            },
            [](void* owner, CoachCompletion& result) noexcept {
                auto& transport = *static_cast<Transport*>(owner);
                ++transport.polls;
                if (transport.replies.empty()) return false;
                result = transport.replies.front();
                transport.replies.pop_front();
                return true;
            }};
    }
};

struct ModelPicker {
    static inline unsigned opens{};
    static inline CoachModelImport state{CoachModelImport::pending};
    static bool open() noexcept { ++opens; state = CoachModelImport::pending; return true; }
    static CoachModelImport poll() noexcept { return state; }
};

int main() {
    View gui;
    gui.at(worked_steps_widget).clear();
    const auto view = gui.bind();
    {
        auto manual = make_project_gui_application();
        manual.on_frame(view);
        assert(gui.at(coach_status_widget) == "No Coach connected. See Reference for offline help.");
        assert(manual.handle({Event::action, "calculator.coach.ask"}, view));
        assert(manual.handle({Event::action, "calculator.8"}, view));
        assert(gui.at(display_widget) == "8");
    }

    Transport transport;
    {
        auto app = make_project_gui_application(transport.bind());
        const auto action = [&](std::string_view id) { assert(app.handle({Event::action, id}, view)); };
        app.on_frame(view);
        assert(gui.at(coach_status_widget) == "Coach ready." && transport.polls == 0);
        gui.at(coach_question_widget) = "   ";
        action("calculator.coach.ask");
        assert(transport.submissions == 0 && gui.at(coach_status_widget) == "Enter a question first.");
        gui.at(coach_question_widget).assign(maximum_coach_question_bytes + 1, 'x');
        action("calculator.coach.ask");
        assert(transport.submissions == 0 && gui.at(coach_status_widget).starts_with("Use a single question"));

        gui.at(coach_question_widget) = "  What is sin(30)? ";
        gui.at(scientific_expression_widget) = "3+4";
        action("calculator.scientific.evaluate");
        action("calculator.coach.ask");
        const auto first = transport.generation;
        assert(first && transport.question == "What is sin(30)?" && transport.angle == AngleUnit::degrees);
        assert(gui.at(coach_status_widget) == "Waiting for reply.");
        action("calculator.coach.ask");
        assert(transport.submissions == 1);
        app.on_frame(view);
        assert(transport.polls == 1 && gui.at(coach_reply_widget).empty());
        action("calculator.7");
        action("calculator.scientific.angle");
        action("calculator.scientific.evaluate");
        transport.replies.push_back({first, CoachCompletionCode::reply, "CALC sin(30)"});
        app.on_frame(view);
        assert(gui.at(coach_status_widget) == "Calculation complete.");
        assert(gui.at(coach_reply_widget) == "Degrees\nsin(30) = 0.5");
        assert(gui.at(coach_steps_widget) == "sin(30) = 0.5");
        assert(gui.at(display_widget) == "7" && gui.at(scientific_expression_widget) == "3+4");
        assert(gui.at(scientific_result_widget) == "7" && gui.at(worked_steps_widget) == "3+4 = 7");

        action("calculator.coach.ask");
        const auto second = transport.generation;
        assert(second > first);
        action("calculator.coach.stop");
        action("calculator.coach.stop");
        action("calculator.coach.ask");
        assert(transport.stops == 1 && transport.stopped == second && transport.submissions == 2);
        assert(gui.at(coach_status_widget) == "Stopping coach request.");
        transport.replies.push_back({second, CoachCompletionCode::reply, "CALC 999"});
        app.on_frame(view);
        assert(gui.at(coach_status_widget) == "Stopped." && gui.at(coach_reply_widget).empty());

        action("calculator.coach.ask");
        const auto third = transport.generation;
        assert(third > second && transport.angle == AngleUnit::radians);
        transport.replies.push_back({second, CoachCompletionCode::reply, "CALC 999"});
        transport.replies.push_back({third, CoachCompletionCode::reply, "CLARIFY discount"});
        app.on_frame(view);
        assert(gui.at(coach_status_widget) == "Waiting for reply." && gui.at(coach_reply_widget).empty());
        app.on_frame(view);
        assert(gui.at(coach_status_widget) == "Coach needs clarification.");
        assert(gui.at(coach_reply_widget) == coach_clarification_text(CoachClarification::discount));

        action("calculator.coach.ask");
        transport.replies.push_back({transport.generation, CoachCompletionCode::reply, "CALC 1\nRUN something"});
        app.on_frame(view);
        assert(gui.at(coach_status_widget) == "Coach could not give a valid calculator reply.");
        assert(gui.at(coach_reply_widget).empty() && gui.at(scientific_result_widget) == "7");

        action("calculator.coach.ask");
        transport.replies.push_back({transport.generation, CoachCompletionCode::failed, "CALC 999"});
        app.on_frame(view);
        assert(gui.at(coach_status_widget) == "Coach request failed. Try again.");
        assert(gui.at(coach_reply_widget).empty());

        action("calculator.coach.ask");
        action("calculator.coach.stop");
        transport.replies.push_back({transport.generation, CoachCompletionCode::retirement_failed, "CALC 999"});
        app.on_frame(view);
        assert(gui.at(coach_status_widget) == "Coach worker retirement could not be verified.");
        assert(gui.at(coach_reply_widget).empty());

        transport.accept = false;
        action("calculator.coach.ask");
        assert(gui.at(coach_status_widget) == "Coach could not start the request. Try again.");
        transport.accept = true;
        action("calculator.coach.ask");
    }
    assert(transport.stops == 3 && transport.stopped == transport.generation);

    Transport android;
    auto app = make_project_gui_application(android.bind(), {false, ModelPicker::open, ModelPicker::poll});
    const auto ask = [&] { assert(app.handle({Event::action, "calculator.coach.ask"}, view)); };
    const auto report = [&](CoachModelImport state) { ModelPicker::state = state; app.on_frame(view); };
    app.on_frame(view);
    assert(gui.at(coach_status_widget) == coach_add_model_status);
    assert(ModelPicker::opens == 0 && android.submissions == 0);
    ask();
    ask();
    assert(ModelPicker::opens == 1 && android.submissions == 0);
    report(CoachModelImport::cancelled);
    assert(gui.at(coach_status_widget).starts_with("Model selection cancelled."));
    assert(app.handle({Event::action, "calculator.8"}, view) && gui.at(display_widget) == "8");
    ask();
    report(CoachModelImport::wrong_file);
    assert(gui.at(coach_status_widget).starts_with("Choose Qwen3"));
    ask();
    report(CoachModelImport::low_storage);
    assert(gui.at(coach_status_widget).starts_with("Free at least"));
    ask();
    report(CoachModelImport::installed);
    assert(ModelPicker::opens == 4 && android.submissions == 0);
    assert(gui.at(coach_status_widget).starts_with("Coach ready."));

    gui.at(coach_question_widget) = "four squared";
    ask();
    assert(android.submissions == 1);
    android.replies.push_back({android.generation, CoachCompletionCode::model_rejected, {}});
    app.on_frame(view);
    assert(gui.at(coach_status_widget) == "Model invalid. Tap Ask.");
    assert(ModelPicker::opens == 4 && android.submissions == 1);
    ask();
    assert(ModelPicker::opens == 5 && android.submissions == 1);
}
''', encoding='utf-8')
        includes = [SOURCE, ROOT / 'Engine/include']
        if os.name == 'nt':
            kits = windows_sdk_includes(Path('C:/Program Files (x86)/Windows Kits/10'))
            if kits:
                includes.extend(kits[-1] / directory for directory in ('um', 'shared'))
        runs = compile_and_run(self, source, report, includes=includes)
        (report / 'result.json').write_text(json.dumps({'scope': 'Project GUI application with injected transport and widget ports', 'runs': runs}, indent=2), encoding='utf-8')


if __name__ == '__main__':
    unittest.main()
