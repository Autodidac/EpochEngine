// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once
#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

// Host-owned model file admission, not OS confinement or content classification.
namespace epochengine::ai::source_access {
[[nodiscard]] inline bool protected_path(std::string_view input) {
    std::string path;path.reserve(input.size()+2);path.push_back('/');
    for(unsigned char c:input)path.push_back(c=='\\'?'/' : c>='A'&&c<='Z'?char(c+32):char(c));
    path.push_back('/');
    constexpr std::array owners{
        "/engine/dep/epochselfcoding/", "/engine/src/updater/owner/", "/engine/src/ai/native/",
        "/engine/src/ai/implementation/", "/engine/src/editor/ai/",
        "/engine/include/ai/", "/engine/modules/ai/",
        "/engine/docs/security/", "/artifacts/", "/cache/", "/logs/",
        "/tools/ai/", "/tools/models/", "/.codex/", "/.agents/", "/.git/"};
    for(auto owner:owners)if(path.find(owner)!=path.npos)return true;
    constexpr std::array names{
        "agents.md", "ai_policy.md", "ai_source.md", "ai_plan.md", "ai_runtime.md",
        "active_pass.md", "mission_cache.md", "missioncache.md", "ai.source_access.hpp", "platform.ai_source_access.hpp",
        "ai.native_runtime.cpp", "ai.native_runtime.hpp", "ai.engine.cpp",
        "editor.application_models_attachments.inl", "editor.application_work_admission.inl",
        "platform.child_process", "platform.work_admission", "platform.filesystem",
        "context.admission", "editor.ai_file_scope", "ai.approved_patch",
        "ai.native", "ai.epoch_native", "ai.mcp", "ai.engine", "ai.source", "ai.development", "ai.policy",
        "editor.frame_assistant", "editor.frame_project_runtime", "editor.application_context_lifecycle"};
    for(std::size_t begin=1;begin<path.size();) {
        const auto end=path.find('/',begin);if(end==path.npos)break;
        auto component=std::string_view{path}.substr(begin,end-begin);
        if(component=="."||component==".."||(!component.empty()&&(component.back()=='.'||component.back()==' ')))return true;
        for(auto name:names)if(component==name||(component.starts_with(name)&&component.size()>std::string_view{name}.size()&&(component[std::string_view{name}.size()]=='.'||component[std::string_view{name}.size()]=='_'||component[std::string_view{name}.size()]==':')))return true;
        begin=end+1;
    }
    return false;
}
[[nodiscard]] inline bool permits_file(const std::filesystem::path& file, const std::filesystem::path& root = {}) noexcept try {
    const auto lexical = root.empty() ? file : file.lexically_relative(root);
    if(lexical.empty() || protected_path(lexical.generic_string()))return false;
    std::error_code ec;
    const auto resolved=std::filesystem::weakly_canonical(file,ec);
    if(ec||resolved.empty())return false;
    const auto canonicalRoot = root.empty() ? std::filesystem::path{} : std::filesystem::weakly_canonical(root,ec);
    if(ec)return false;
    const auto relative = root.empty() ? resolved : resolved.lexically_relative(canonicalRoot);
    if(relative.empty()||protected_path(relative.generic_string()))return false;
    if(!std::filesystem::is_regular_file(resolved,ec) || ec)return false;
    {
        const auto links=std::filesystem::hard_link_count(resolved,ec);
        if(ec||links!=1)return false;
    }
    return !ec;
} catch(...) {return false;}
}
