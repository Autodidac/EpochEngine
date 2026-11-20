#pragma once
// SPDX-License-Identifier: LicenseRef-MIT-NoSell

#include <SDL3/SDL_system.h>
#include <jni.h>
#include <string>
#include <string_view>

namespace epochengine::platform::android
{
    [[nodiscard]] inline std::string read_project_transfer_token(JNIEnv& env, const jstring value)
    {
        if (!value || env.ExceptionCheck()) return {};
        const auto length = env.GetStringLength(value);
        if (length <= 0 || length > 192) return {};

        // The host protocol contains only ASCII status codes and project IDs.
        jchar characters[192]{};
        env.GetStringRegion(value, 0, length, characters);
        if (env.ExceptionCheck()) return {};
        std::string result;
        result.reserve(static_cast<std::size_t>(length));
        for (jsize i = 0; i < length; ++i)
        {
            if (characters[i] > 127) return {};
            result.push_back(static_cast<char>(characters[i]));
        }
        return result;
    }

    [[nodiscard]] inline bool valid_archive_project_id(const std::string_view id) noexcept
    {
        return !id.empty() && id.find_first_not_of(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == std::string_view::npos;
    }

    // Each call binds to the current SDL Activity. Never retain Activity or JNI
    // references across Android pause, resume or configuration changes.
    class ProjectTransferActivity final
    {
    public:
        ProjectTransferActivity()
        {
            env_ = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
            if (!env_) return;
            has_frame_ = env_->PushLocalFrame(4) == JNI_OK;
            if (!has_frame_) return;
            activity_ = static_cast<jobject>(SDL_GetAndroidActivity());
            if (activity_ && !env_->ExceptionCheck())
                class_ = env_->GetObjectClass(activity_);
        }

        ProjectTransferActivity(const ProjectTransferActivity&) = delete;
        ProjectTransferActivity& operator=(const ProjectTransferActivity&) = delete;

        ~ProjectTransferActivity()
        {
            if (!env_) return;
            if (env_->ExceptionCheck()) env_->ExceptionClear();
            if (has_frame_) env_->PopLocalFrame(nullptr);
        }

        [[nodiscard]] bool request_import() const
        {
            if (!class_) return false;
            const auto method = env_->GetMethodID(class_, "requestProjectArchiveImport", "()Z");
            return method && env_->CallBooleanMethod(activity_, method) == JNI_TRUE
                && !env_->ExceptionCheck();
        }

        [[nodiscard]] bool request_export(const std::string& id, const std::string& root) const
        {
            if (!class_) return false;
            const auto method = env_->GetMethodID(class_, "requestProjectArchiveExport",
                "(Ljava/lang/String;Ljava/lang/String;)Z");
            if (!method) return false;
            // The caller binds this to an ASCII project ID under the app's fixed storage path.
            const auto project_id = env_->NewStringUTF(id.c_str());
            if (!project_id || env_->ExceptionCheck()) return false;
            const auto project_root = env_->NewStringUTF(root.c_str());
            return project_root && !env_->ExceptionCheck()
                && env_->CallBooleanMethod(activity_, method, project_id, project_root) == JNI_TRUE
                && !env_->ExceptionCheck();
        }

        [[nodiscard]] std::string poll_transfer() const
        {
            if (!class_) return {};
            const auto method = env_->GetMethodID(class_, "pollProjectArchiveImport", "()Ljava/lang/String;");
            if (!method) return {};
            return read_project_transfer_token(*env_,
                static_cast<jstring>(env_->CallObjectMethod(activity_, method)));
        }

    private:
        JNIEnv* env_{};
        jobject activity_{};
        jclass class_{};
        bool has_frame_{};
    };
}
