// EPOCH_MANAGED_GENERATED_FILE: project_entry_v1
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#if defined(_WIN32)
#  include <stdlib.h>
#else
#  include <unistd.h>
#endif
#include <epoch.edition.hpp>
#include <epoch.engine.hpp>

#if defined(EPOCH_PROJECT_PLANETARY_RUNTIME)
extern "C" bool epoch_install_planetary_runtime(const char*, const char*) noexcept;
#endif

static_assert(EPOCH_SOURCE_EDITION_PUBLIC == 1, "Generated project source edition differs");

extern "C" void core_log_write(unsigned int lvl, const char* tag_utf8, const char* msg_utf8);

namespace
{
    bool has_arg(int argc, char** argv, const char* needle) noexcept
    {
        if (!needle)
            return false;
        for (int i = 1; i < argc; ++i)
        {
            if (argv && argv[i] && std::strcmp(argv[i], needle) == 0)
                return true;
        }
        return false;
    }

    epochengine::project::RuntimeProjectBinding boot_project_shell()
    {
        return epochengine::project::BindRuntimeProject("PlatformerExample2D");
    }

    void log_project_shell_self_test_line(const char* message) noexcept
    {
        core_log_write(1u, "Epoch.ChildProject", message ? message : "");
    }

    int log_project_shell_self_test(const epochengine::project::RuntimeProjectBinding& project) noexcept
    {
        log_project_shell_self_test_line("Epoch generated project shell self-test");
        log_project_shell_self_test_line("project_id=PlatformerExample2D");
        log_project_shell_self_test_line("project_name=2D Platformer Example");
        log_project_shell_self_test_line((std::string{"project_root="} + project.project_root).c_str());
        log_project_shell_self_test_line((std::string{"manifest="} + project.manifest_path).c_str());
        log_project_shell_self_test_line("engine_integration=repo-local static Epoch runtime child build across headers/modules/source/scripting/resources with project-selected runtime boot");
        log_project_shell_self_test_line("project_format=epoch-project-v1");
        log_project_shell_self_test_line("build_profile=epoch-runtime-static");
        const epochengine::project::ArtifactAcceptanceRequest request{
            .project_id = "PlatformerExample2D",
            .project_root = project.project_root,
            .scene_path = project.scene_path,
            .tilemap_path = "Assets/Maps/main.epochmap",
            .input_profile_path = "Assets/Config/input_profile.epochinput",
            .sprite_animation_path = "Assets/Animations/sprite_animations.epochanim",
            .audio_profile_path = "Assets/Audio/project_audio.epochaudio",
            .gui_path = "Assets/Gui/main.epochgui",
            .scene_requires_runtime = true,
            .progress_sink = log_project_shell_self_test_line};
        const auto acceptance = epochengine::project::VerifyArtifacts(request);
        log_project_shell_self_test_line((std::string{"artifact_acceptance.required_mask="} + std::to_string(acceptance.required_mask)).c_str());
        log_project_shell_self_test_line((std::string{"artifact_acceptance.verified_mask="} + std::to_string(acceptance.verified_mask)).c_str());
        log_project_shell_self_test_line((std::string{"artifact_acceptance.stage="} + acceptance.stage).c_str());
        log_project_shell_self_test_line((std::string{"artifact_acceptance.diagnostic="} + acceptance.diagnostic).c_str());
        const bool has_derived_artifacts = !request.tilemap_path.empty()
            || !request.input_profile_path.empty()
            || !request.sprite_animation_path.empty()
            || !request.audio_profile_path.empty()
            || !request.gui_path.empty();
        const auto regeneration = !acceptance.succeeded
            ? epochengine::project::ArtifactRegenerationReport{}
            : has_derived_artifacts
                ? epochengine::project::VerifyLibraryRegeneration(request)
                : epochengine::project::ArtifactRegenerationReport{
                    .succeeded = true, .stage = "not_required",
                    .diagnostic = "project declares no derived library artifacts"};
        log_project_shell_self_test_line((std::string{"library_regeneration.required_mask="} + std::to_string(regeneration.required_mask)).c_str());
        log_project_shell_self_test_line((std::string{"library_regeneration.regenerated_mask="} + std::to_string(regeneration.regenerated_mask)).c_str());
        log_project_shell_self_test_line((std::string{"library_regeneration.source_files_verified="} + std::to_string(regeneration.source_files_verified)).c_str());
        log_project_shell_self_test_line((std::string{"library_regeneration.artifact_files_quarantined="} + std::to_string(regeneration.artifact_files_quarantined)).c_str());
        log_project_shell_self_test_line((std::string{"library_regeneration.stage="} + regeneration.stage).c_str());
        log_project_shell_self_test_line((std::string{"library_regeneration.diagnostic="} + regeneration.diagnostic).c_str());
        const bool needs_playable_2d = true
            && !request.tilemap_path.empty() && !request.input_profile_path.empty();
        const auto gameplay = !acceptance.succeeded || !regeneration.succeeded
            ? epochengine::project::GameplayAcceptanceReport{}
            : needs_playable_2d
                ? epochengine::project::VerifyPlayable2D(request)
                : epochengine::project::GameplayAcceptanceReport{
                    .succeeded = true, .stage = "not_required",
                    .diagnostic = "profile does not declare playable-2D validation inputs"};
        log_project_shell_self_test_line((std::string{"gameplay_acceptance.input_frames="} + std::to_string(gameplay.input_frames)).c_str());
        log_project_shell_self_test_line((std::string{"gameplay_acceptance.fixed_steps="} + std::to_string(gameplay.fixed_steps)).c_str());
        log_project_shell_self_test_line((std::string{"gameplay_acceptance.animation_samples="} + std::to_string(gameplay.animation_samples)).c_str());
        log_project_shell_self_test_line((std::string{"gameplay_acceptance.audio_triggers="} + std::to_string(gameplay.audio_triggers)).c_str());
        log_project_shell_self_test_line((std::string{"gameplay_acceptance.canvas_hash="} + std::to_string(gameplay.canvas_hash)).c_str());
        log_project_shell_self_test_line((std::string{"gameplay_acceptance.logical_texture_bytes="} + std::to_string(gameplay.logical_texture_bytes)).c_str());
        log_project_shell_self_test_line((std::string{"gameplay_acceptance.emitted_sprites="} + std::to_string(gameplay.emitted_sprites)).c_str());
        log_project_shell_self_test_line((std::string{"gameplay_acceptance.emitted_batches="} + std::to_string(gameplay.emitted_batches)).c_str());
        log_project_shell_self_test_line((std::string{"gameplay_acceptance.collision_surfaces="} + std::to_string(gameplay.collision_surfaces)).c_str());
        log_project_shell_self_test_line((std::string{"gameplay_acceptance.peak_contacts_per_step="} + std::to_string(gameplay.peak_contacts_per_step)).c_str());
        log_project_shell_self_test_line((std::string{"gameplay_acceptance.audio_resident_bytes="} + std::to_string(gameplay.audio_resident_bytes)).c_str());
        log_project_shell_self_test_line((std::string{"gameplay_acceptance.canvas_rejections="} + std::to_string(gameplay.canvas_rejections)).c_str());
        log_project_shell_self_test_line("gameplay_acceptance.portable_budget_profile=T1-GLES/mobile_30");
        log_project_shell_self_test_line((std::string{"gameplay_acceptance.portable_budget_violations="} + std::to_string(gameplay.portable_budget_violations)).c_str());
        log_project_shell_self_test_line((std::string{"gameplay_acceptance.stage="} + gameplay.stage).c_str());
        log_project_shell_self_test_line((std::string{"gameplay_acceptance.diagnostic="} + gameplay.diagnostic).c_str());
        return acceptance.succeeded && regeneration.succeeded && gameplay.succeeded ? 0 : 2;
    }
}

int main(int argc, char** argv)
{
    if (has_arg(argc, argv, "--version") || has_arg(argc, argv, "-v"))
    {
        log_project_shell_self_test_line("Epoch generated project shell: 2D Platformer Example");
        return 0;
    }
    const auto project = boot_project_shell();
    if (!project.succeeded)
    {
        log_project_shell_self_test_line(project.summary.c_str());
        return 2;
    }
#if defined(EPOCH_PROJECT_PLANETARY_RUNTIME)
    if (!epoch_install_planetary_runtime(project.project_root.c_str(), "PlatformerExample2D"))
    {
        log_project_shell_self_test_line("The planetary runtime does not match this project's approved source and settings.");
        return 3;
    }
#endif
    if (has_arg(argc, argv, "--project-self-test"))
        return log_project_shell_self_test(project);
    epochengine::core::ParseCommandLine(argc, argv);
    epochengine::core::RunEngine();
    return 0;
}
