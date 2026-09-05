// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Opt-in production HTTP transport proof; never starts an editor or listener.
#include "../../include/core.stl_types.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

import ai.engine;
import core.log;

namespace
{
    namespace ai = epochengine::ai;
    namespace log = epochengine::core::log;
    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    constexpr std::string_view endpoint = "http://localhost:1234";
    constexpr std::string_view modelName = "qwen/qwen3.8-27b";
    constexpr std::string_view canary = "EPOCH_TRANSPORT_CANARY_B_OK";
    constexpr std::string_view cancelledReply =
        "Local-model request cancelled before completion.";

    void evidence(std::string_view message)
    {
        log::info("ai.transport_contract",
            epochengine::string_view{message.data(), message.size()});
    }

    [[nodiscard]] int fail(std::string_view message, int code = 1)
    {
        log::error("ai.transport_contract",
            epochengine::string_view{message.data(), message.size()});
        return code;
    }

    [[nodiscard]] bool explicit_run(int argc, char** argv)
    {
        bool run{};
        bool selectedEndpoint{};
        bool selectedModel{};
        for (int index = 1; index < argc; ++index)
        {
            const std::string_view argument{argv[index]};
            if (argument == "--run" && !run)
                run = true;
            else if (argument == "--endpoint" && !selectedEndpoint
                && index + 1 < argc)
                selectedEndpoint = std::string_view{argv[++index]} == endpoint;
            else if (argument == "--model" && !selectedModel
                && index + 1 < argc)
                selectedModel = std::string_view{argv[++index]} == modelName;
            else
                return false;
            if ((argument == "--endpoint" && !selectedEndpoint)
                || (argument == "--model" && !selectedModel))
                return false;
        }
        return run && selectedEndpoint && selectedModel;
    }

    // Only these two request tokens belong to this process. An overdue request
    // first receives cooperative cancellation. If the transport still cannot
    // retire, terminate this probe with failure rather than claim a clean join.
    // The caller should additionally supervise the probe's process deadline.
    class ProbeWatchdog final
    {
    public:
        ProbeWatchdog(std::stop_source first, std::stop_source second)
            : first_(std::move(first)), second_(std::move(second)),
              worker_([this] { watch(); }) {}

        ~ProbeWatchdog() { (void)finish(); }
        ProbeWatchdog(const ProbeWatchdog&) = delete;
        ProbeWatchdog& operator=(const ProbeWatchdog&) = delete;

        void arm(std::chrono::seconds duration)
        {
            {
                std::scoped_lock lock{mutex_};
                deadline_ = Clock::now() + duration;
            }
            changed_.notify_all();
        }

        [[nodiscard]] bool finish()
        {
            bool timely{};
            {
                std::scoped_lock lock{mutex_};
                finished_ = true;
                timely = !expired_;
            }
            changed_.notify_all();
            if (worker_.joinable())
                worker_.join();
            return timely;
        }

    private:
        void watch() noexcept
        {
            std::unique_lock lock{mutex_};
            while (!finished_)
            {
                const auto expected = deadline_;
                if (changed_.wait_until(lock, expected,
                    [this, expected] { return finished_ || deadline_ != expected; }))
                    continue;
                expired_ = true;
                lock.unlock();
                (void)first_.request_stop();
                (void)second_.request_stop();
                (void)fail("FAIL: probe deadline exceeded; stopping only its owned HTTP requests.", 70);
                lock.lock();
                if (changed_.wait_for(lock, 10s, [this] { return finished_; }))
                    return;
                lock.unlock();
                (void)fail("FAIL: owned HTTP requests did not retire after cancellation; exiting this probe without a clean-retirement claim.", 70);
                std::_Exit(70);
            }
        }

        std::stop_source first_;
        std::stop_source second_;
        std::mutex mutex_{};
        std::condition_variable changed_{};
        Clock::time_point deadline_{Clock::now() + 45s};
        bool finished_{};
        bool expired_{};
        std::jthread worker_;
    };

    struct RequestEvidence final
    {
        unsigned sent{};
        unsigned awaiting{};
        bool completed{};
        bool cancelled{};
        bool failed{};
        bool retirementFailed{};

        void observe(ai::ModelRequestStage stage)
        {
            evidence("Observed transport stage=" + std::to_string(static_cast<unsigned>(stage)));
            switch (stage)
            {
            case ai::ModelRequestStage::request_sent: ++sent; break;
            case ai::ModelRequestStage::awaiting_response: ++awaiting; break;
            case ai::ModelRequestStage::completed: completed = true; break;
            case ai::ModelRequestStage::cancelled: cancelled = true; break;
            case ai::ModelRequestStage::failed: failed = true; break;
            case ai::ModelRequestStage::retirement_failed: retirementFailed = true; break;
            default: break;
            }
        }
    };

    [[nodiscard]] int run_probe()
    {
        evidence("Explicit transport-only run: existing localhost:1234 endpoint; qwen/qwen3.8-27b; synthetic prompts only.");
        evidence("No editor, renderer, candidate build, source selection, listener, model installation or preference initialization is performed.");
        ai::EngineAiModel client{ai::EngineAiModel::Config{
            .endpoint = std::string{endpoint},
            .model = std::string{modelName}
        }};
        std::stop_source firstStop{};
        std::stop_source secondStop{};
        const std::stop_token retainedFirstToken = firstStop.get_token();
        ProbeWatchdog watchdog{firstStop, secondStop};

        RequestEvidence first{};
        Clock::time_point cancelledAt{};
        bool stopAccepted{};
        const auto firstReply = client.submit(
            "This is a disposable HTTP cancellation probe. Write a long numbered list of ordinary colors and shapes, at least 500 entries. Do not access files or tools.",
            ai::InferenceWorkload::chat,
            firstStop.get_token(),
            [&](ai::ModelRequestStage stage)
            {
                first.observe(stage);
                if (stage == ai::ModelRequestStage::awaiting_response
                    && first.awaiting == 1u)
                {
                    // The production worker emits this only after the request
                    // was sent and asynchronous receive was accepted. This is
                    // actual transport evidence, not a sleep or host-queue flag.
                    cancelledAt = Clock::now();
                    watchdog.arm(10s);
                    stopAccepted = firstStop.request_stop();
                    evidence("A: request_sent and awaiting_response observed; its own token was stopped.");
                }
            });

        if (first.sent == 0u || first.awaiting == 0u || !stopAccepted)
            return fail("NOT_EXERCISED: A never reached the pending HTTP receive cancellation point.", 3);
        const auto cancelDuration = Clock::now() - cancelledAt;
        evidence("A: cancellation return interval_ms="
            + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(cancelDuration).count()));
        if (!first.cancelled || first.completed || first.failed || first.retirementFailed
            || firstReply.transport_retirement_failed
            || first.sent != 1u || first.awaiting != 1u
            || !retainedFirstToken.stop_requested()
            || cancelDuration > 10s || firstReply.text != cancelledReply)
            return fail("FAIL: A did not settle as one bounded, non-retried cancellation.");

        // Allow the production chat deadline and its one transport retry. A
        // shorter probe timer would cancel a healthy-but-slow request before
        // the actual client policy could report its result.
        constexpr auto secondBudget = std::chrono::seconds{
            2u * ai::inference_budget(ai::InferenceWorkload::chat).timeout_seconds + 15u};
        watchdog.arm(secondBudget);
        RequestEvidence second{};
        bool oldStopRepeated{};
        bool oldStopChanged{};
        const auto secondReply = client.submit(
            "This is a disposable HTTP health check. Reply with exactly EPOCH_TRANSPORT_CANARY_B_OK and no other text. Do not access files or tools.",
            ai::InferenceWorkload::chat,
            secondStop.get_token(),
            [&](ai::ModelRequestStage stage)
            {
                second.observe(stage);
                if (stage == ai::ModelRequestStage::awaiting_response
                    && second.awaiting == 1u)
                {
                    oldStopRepeated = true;
                    oldStopChanged = firstStop.request_stop();
                    evidence("B: independent HTTP receive active; repeated A's already-stopped token only.");
                }
            });
        if (!oldStopRepeated || second.sent == 0u || second.awaiting == 0u)
            return fail("NOT_EXERCISED: B never reached the pending HTTP ownership-isolation point.", 3);
        if (oldStopChanged || secondStop.stop_requested()
            || !second.completed || second.cancelled || second.failed || second.retirementFailed
            || secondReply.transport_retirement_failed
            || secondReply.text.find(canary) == std::string::npos)
            return fail("FAIL: B did not return the requested canary independently of A's cancelled token.");
        if (!watchdog.finish())
            return fail("FAIL: transport result arrived after the probe deadline.", 70);
        evidence("PASS: A cancelled after actual HTTP dispatch; B returned its canary with a separate live request token.");
        evidence("Scope: production HTTP request ownership and cancellation only; not server-side generation-stop, global queue, native editor, candidate succession or OS-confinement proof.");
        return 0;
    }
}

int main(int argc, char** argv)
{
    log::enable_console(true);
    if (!explicit_run(argc, argv))
        return fail("No request sent. Explicit usage: EpochAiTransportContract --endpoint http://localhost:1234 --model qwen/qwen3.8-27b --run", 2);
    try
    {
        return run_probe();
    }
    catch (const std::exception&)
    {
        return fail("FAIL: production transport probe raised an exception; no successful cancellation or retirement is claimed.", 71);
    }
    catch (...)
    {
        return fail("FAIL: production transport probe raised an unknown exception.", 71);
    }
}
