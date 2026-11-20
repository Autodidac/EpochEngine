#pragma once

#include <SDL3/SDL_system.h>
#include <jni.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>

#include "calculator.coach.hpp"

namespace calculator_example
{
    namespace android_coach
    {
        class JavaFrame final
        {
        public:
            JavaFrame() noexcept : env{static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv())}
            {
                if (!env) return;
                pushed_ = env->PushLocalFrame(16) == JNI_OK;
                if (!pushed_)
                {
                    (void)failed();
                    return;
                }
                activity = static_cast<jobject>(SDL_GetAndroidActivity());
                if (failed() || !activity) return;
                // The SDL native owner and the Coach worker have different Java class loaders.
                const jclass activity_type = env->GetObjectClass(activity);
                if (failed() || !activity_type) return;
                const auto get_loader = env->GetMethodID(activity_type, "getClassLoader", "()Ljava/lang/ClassLoader;");
                if (failed() || !get_loader) return;
                const auto loader = env->CallObjectMethod(activity, get_loader);
                if (failed() || !loader) return;
                const auto loader_type = env->GetObjectClass(loader);
                if (failed() || !loader_type) return;
                const auto load_class = env->GetMethodID(loader_type, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
                if (failed() || !load_class) return;
                const auto name = env->NewStringUTF("org.epochengine.runtime.EpochCalculatorCoach");
                if (failed() || !name) return;
                facade = static_cast<jclass>(env->CallObjectMethod(loader, load_class, name));
                if (failed()) facade = nullptr;
            }

            ~JavaFrame()
            {
                if (pushed_) env->PopLocalFrame(nullptr);
            }

            JavaFrame(const JavaFrame&) = delete;
            JavaFrame& operator=(const JavaFrame&) = delete;
            [[nodiscard]] bool failed() const noexcept
            {
                if (!env->ExceptionCheck()) return false;
                env->ExceptionClear();
                return true;
            }

            [[nodiscard]] jmethodID method(const char* name, const char* signature) const noexcept
            {
                const auto id = env->GetStaticMethodID(facade, name, signature);
                return failed() ? nullptr : id;
            }

            [[nodiscard]] jstring utf8(std::string_view input) const noexcept
            {
                const auto count = static_cast<jsize>(input.size());
                const auto data = env->NewByteArray(count);
                if (failed() || !data) return nullptr;
                env->SetByteArrayRegion(data, 0, count, reinterpret_cast<const jbyte*>(input.data()));
                if (failed()) return nullptr;
                const auto string_type = env->FindClass("java/lang/String");
                if (failed() || !string_type) return nullptr;
                const auto constructor = env->GetMethodID(string_type, "<init>", "([BLjava/lang/String;)V");
                if (failed() || !constructor) return nullptr;
                const auto encoding = env->NewStringUTF("UTF-8");
                if (failed() || !encoding) return nullptr;
                const auto value = static_cast<jstring>(env->NewObject(string_type, constructor, data, encoding));
                return failed() ? nullptr : value;
            }

            JNIEnv* env{};
            jobject activity{};
            jclass facade{};
        private:
            bool pushed_{};
        };

        [[nodiscard]] inline CoachQueryResult reply(JavaFrame& java, jstring value)
        {
            constexpr std::string_view prefix = "reply\n";
            const auto length = java.env->GetStringUTFLength(value);
            if (java.failed()) return {CoachCompletionCode::retirement_failed, {}, false};
            if (length <= 0 || static_cast<std::size_t>(length) > maximum_coach_reply_bytes + prefix.size())
                return {CoachCompletionCode::failed, {}, true};

            std::array<char, maximum_coach_reply_bytes + prefix.size() + 1u> bytes{};
            const auto chars = java.env->GetStringLength(value);
            if (java.failed()) return {CoachCompletionCode::retirement_failed, {}, false};
            java.env->GetStringUTFRegion(value, 0, chars, bytes.data());
            if (java.failed()) return {CoachCompletionCode::retirement_failed, {}, false};
            const std::string_view text{bytes.data(), static_cast<std::size_t>(length)};
            if (text.starts_with(prefix))
                return {CoachCompletionCode::reply, std::string{text.substr(prefix.size())}, true};
            if (text == "cancelled") return {CoachCompletionCode::cancelled, {}, true};
            if (text == "failed") return {CoachCompletionCode::failed, {}, true};
            if (text == "model_rejected") return {CoachCompletionCode::model_rejected, {}, true};
            return {CoachCompletionCode::retirement_failed, {}, false};
        }

        [[nodiscard]] inline CoachQueryResult run(void*, CoachQueryRequest request, std::stop_token stop)
        {
            using namespace std::chrono_literals;
            if (stop.stop_requested()) return {CoachCompletionCode::cancelled, {}, true};
            if (!valid_coach_question(request.question)
                || request.generation > static_cast<std::uint64_t>((std::numeric_limits<jlong>::max)()))
                return {CoachCompletionCode::failed, {}, true};
            JavaFrame java;
            if (!java.facade) return {CoachCompletionCode::failed, {}, true};
            const auto start = java.method("start", "(Landroid/app/Activity;JLjava/lang/String;Z)Z");
            const auto poll = java.method("poll", "(J)Ljava/lang/String;");
            const auto cancel = java.method("cancel", "(J)V");
            if (!start || !poll || !cancel) return {CoachCompletionCode::failed, {}, true};
            const auto question = java.utf8(request.question);
            if (!question) return {CoachCompletionCode::failed, {}, true};

            const auto generation = static_cast<jlong>(request.generation);
            const jboolean degrees = request.angle_unit == AngleUnit::degrees ? JNI_TRUE : JNI_FALSE;
            const auto started = java.env->CallStaticBooleanMethod(java.facade, start,
                java.activity, generation, question, degrees);
            if (java.failed()) return {CoachCompletionCode::retirement_failed, {}, false};
            if (!started) return {CoachCompletionCode::failed, {}, true};

            auto deadline = std::chrono::steady_clock::now() + 80s;
            bool cancelling = false;
            while (std::chrono::steady_clock::now() < deadline)
            {
                if (stop.stop_requested() && !cancelling)
                {
                    cancelling = true;
                    deadline = (std::min)(deadline, std::chrono::steady_clock::now() + 9s);
                    java.env->CallStaticVoidMethod(java.facade, cancel, generation);
                    if (java.failed()) return {CoachCompletionCode::retirement_failed, {}, false};
                }
                const auto value = static_cast<jstring>(java.env->CallStaticObjectMethod(java.facade, poll, generation));
                if (java.failed()) return {CoachCompletionCode::retirement_failed, {}, false};
                if (value) return reply(java, value);
                std::this_thread::sleep_for(50ms);
            }
            java.env->CallStaticVoidMethod(java.facade, cancel, generation);
            (void)java.failed();
            return {CoachCompletionCode::retirement_failed, {}, false};
        }
    }

    [[nodiscard]] inline CoachModelPicker android_coach_model_picker() noexcept
    {
        android_coach::JavaFrame java;
        if (!java.facade) return {};
        const auto installed = java.method("isModelInstalled", "(Landroid/app/Activity;)Z");
        if (!installed) return {};
        const auto available = java.env->CallStaticBooleanMethod(java.facade, installed, java.activity);
        if (java.failed()) return {};
        return {available == JNI_TRUE,
            []() noexcept {
                android_coach::JavaFrame frame;
                if (!frame.facade) return false;
                const auto open = frame.method("openModelPicker", "(Landroid/app/Activity;)Z");
                if (!open) return false;
                const auto opened = frame.env->CallStaticBooleanMethod(frame.facade, open, frame.activity);
                return !frame.failed() && opened == JNI_TRUE;
            },
            []() noexcept {
                android_coach::JavaFrame frame;
                if (!frame.facade) return CoachModelImport::failed;
                const auto result = frame.method("modelImportOutcome", "()I");
                if (!result) return CoachModelImport::failed;
                const auto state = frame.env->CallStaticIntMethod(frame.facade, result);
                if (frame.failed() || state < 0 || state > static_cast<int>(CoachModelImport::timed_out))
                    return CoachModelImport::failed;
                return static_cast<CoachModelImport>(state);
            }};
    }
}
