// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include "../../include/epoch.engine.hpp"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

import project.audio_profile;
import project.gui_library;
import project.gui_runtime;
import project.input_profile;
import project.sprite_animation;
import project.tilemap_runtime;
import scene.document;
import scene.persistence;
import scene.runtime;
import scene.snapshot;

namespace epochengine::project
{
    [[nodiscard]] std::string scene_acceptance_diagnostic(
        const scene::SceneSnapshot& snapshot, bool requiresRuntime,
        AuthoringSceneKind authoringKind)
    {
        if (!requiresRuntime)
        {
            const bool editor = authoringKind == AuthoringSceneKind::editor;
            if (snapshot.document_kind != (editor ? "editor" : "tool"))
                return editor ? "editor profile requires an editor scene document"
                              : "tool profile requires a tool scene document";
            if (!scene::validate_scene_document(snapshot,
                    scene::SceneDocumentRequirement::authoring))
                return editor ? "editor scene document is invalid"
                              : "tool scene document is invalid";
            return {};
        }
        scene_runtime::SceneRuntime runtime{};
        const auto compiled = runtime.replace(snapshot);
        return compiled.committed() ? std::string{}
            : std::string{"scene source is not runnable"};
    }

    ArtifactAcceptanceReport VerifyArtifacts(
        const ArtifactAcceptanceRequest& request) noexcept
    {
        namespace fs = std::filesystem;
        ArtifactAcceptanceReport report{};

        const auto artifact_bit = [](ArtifactKind kind) noexcept
        {
            return static_cast<std::uint32_t>(kind);
        };
        const auto require = [&](ArtifactKind kind, std::string_view path)
        {
            if (!path.empty())
                report.required_mask |= artifact_bit(kind);
        };
        const auto fail = [&](std::string stage, std::string diagnostic)
        {
            report.stage = std::move(stage);
            report.diagnostic = std::move(diagnostic);
            return report;
        };
        const auto accept = [&](ArtifactKind kind)
        {
            report.verified_mask |= artifact_bit(kind);
        };

        require(ArtifactKind::scene, request.scene_path);
        require(ArtifactKind::tilemap, request.tilemap_path);
        require(ArtifactKind::input_profile, request.input_profile_path);
        require(ArtifactKind::sprite_animation, request.sprite_animation_path);
        require(ArtifactKind::audio_profile, request.audio_profile_path);
        require(ArtifactKind::gui, request.gui_path);

        if (request.project_id.empty() || request.project_root.empty())
            return fail("request", "project identity and root are required");
        if (report.required_mask == 0u)
            return fail("request", "no project artifacts were declared");

        try
        {
            std::error_code error{};
            fs::path root = fs::canonical(
                fs::path{request.project_root}, error);
            if (error || !fs::is_directory(root, error) || error)
                return fail("project_root", "project root is not an accessible directory");

            if (!request.scene_path.empty())
            {
                fs::path scenePath{request.scene_path};
                if (scenePath.is_relative())
                    scenePath = root / scenePath;
                scenePath = fs::canonical(scenePath, error);
                if (error)
                    return fail("scene", "scene path could not be resolved");

                const fs::path relative = scenePath.lexically_relative(root);
                if (relative.empty() || relative == fs::path{"."}
                    || *relative.begin() == fs::path{".."})
                {
                    return fail("scene", "scene path is outside the project root");
                }

                const auto loaded =
                    epochengine::scene::persistence::load_scene_snapshot(
                        scenePath);
                if (!loaded.result)
                {
                    return fail(
                        "scene",
                        std::string{"scene load "}
                            + std::string{
                                epochengine::scene::persistence::
                                    scene_persistence_status_name(
                                        loaded.result.status)}
                            + ": " + loaded.result.error);
                }
                if (loaded.snapshot.project_id != request.project_id)
                    return fail("scene", "scene project identity does not match the request");

                const auto sceneDiagnostic = scene_acceptance_diagnostic(
                    loaded.snapshot, request.scene_requires_runtime, request.authoring_scene_kind);
                if (!sceneDiagnostic.empty())
                    return fail("scene", sceneDiagnostic);
                accept(ArtifactKind::scene);
            }

            if (!request.tilemap_path.empty())
            {
                epochengine::project_tilemap_runtime::ProjectTileMapRuntime
                    sourceRuntime{std::string{request.project_id}, root.string()};
                auto prepared = sourceRuntime.prepare({
                    .logical_path = std::string{request.tilemap_path},
                    .source_policy =
                        epochengine::project_tilemap_runtime::SourcePolicy::
                            prefer_source});
                if (!prepared)
                {
                    return fail(
                        "tilemap",
                        std::string{
                            epochengine::project_tilemap_runtime::
                                runtime_code_name(prepared.code)}
                            + ": " + prepared.diagnostic);
                }

                epochengine::project_tilemap_runtime::ProjectTileMapRuntime
                    artifactRuntime{
                        std::string{request.project_id}, root.string()};
                auto restored = artifactRuntime.prepare({
                    .logical_path = std::string{request.tilemap_path},
                    .source_policy =
                        epochengine::project_tilemap_runtime::SourcePolicy::
                            compiled_only});
                if (!restored
                    || restored.provenance
                        != epochengine::project_tilemap_runtime::Provenance::
                            library_restored)
                {
                    return fail(
                        "tilemap_artifact",
                        std::string{
                            epochengine::project_tilemap_runtime::
                                runtime_code_name(restored.code)}
                            + ": " + restored.diagnostic);
                }
                accept(ArtifactKind::tilemap);
            }

            if (!request.input_profile_path.empty())
            {
                const std::string normalized = fs::path{
                    request.input_profile_path}.lexically_normal().generic_string();
                if (normalized
                    != epochengine::project_input::canonical_source_path)
                {
                    return fail(
                        "input_profile",
                        "input profile does not use the canonical logical path");
                }

                epochengine::project_input::ProjectInputProfileStore store{
                    std::string{request.project_id}, root};
                if (!store.valid())
                    return fail("input_profile", "project input store is invalid");

                error.clear();
                const bool sourceExists =
                    fs::exists(store.source_path(), error) && !error;
                if (error)
                    return fail("input_profile", "input source existence check failed");
                if (sourceExists)
                {
                    const auto source = store.load_source();
                    if (!source)
                    {
                        return fail(
                            "input_profile",
                            std::string{"source "}
                                + std::string{
                                    epochengine::project_input::
                                        store_code_name(source.code)});
                    }
                    const auto compiled =
                        epochengine::project_input::compile_profile(
                            request.project_id, source.source);
                    if (!compiled)
                    {
                        return fail(
                            "input_profile",
                            std::string{"compile "}
                                + std::string{
                                    epochengine::project_input::
                                        validation_code_name(compiled.code)});
                    }
                    const auto published =
                        store.publish_artifact(compiled.artifact);
                    if (!published)
                    {
                        return fail(
                            "input_profile_artifact",
                            std::string{"publish "}
                                + std::string{
                                    epochengine::project_input::
                                        store_code_name(published.code)});
                    }
                }

                const auto loaded = store.load_artifact();
                if (!loaded)
                {
                    return fail(
                        "input_profile_artifact",
                        std::string{"load "}
                            + std::string{
                                epochengine::project_input::
                                    store_code_name(loaded.code)});
                }
                const auto validation =
                    epochengine::project_input::validate_compiled_profile(
                        loaded.artifact);
                if (validation
                    != epochengine::project_input::ValidationCode::ready)
                {
                    return fail(
                        "input_profile_artifact",
                        std::string{"validation "}
                            + std::string{
                                epochengine::project_input::
                                    validation_code_name(validation)});
                }
                accept(ArtifactKind::input_profile);
            }

            if (!request.sprite_animation_path.empty())
            {
                auto prepared =
                    epochengine::project_sprite_animation::
                        prepare_project_sprite_animations(
                            request.project_id,
                            root,
                            request.sprite_animation_path,
                            nullptr);
                if (!prepared)
                {
                    return fail(
                        "sprite_animation",
                        std::string{
                            epochengine::project_sprite_animation::
                                preparation_code_name(prepared.code)}
                            + ": " + prepared.diagnostic);
                }

                epochengine::project_sprite_animation::
                    ProjectSpriteAnimationStore store{
                        std::string{request.project_id}, root};
                const auto loaded = store.load_artifact();
                if (!loaded)
                {
                    return fail(
                        "sprite_animation_artifact",
                        std::string{"load "}
                            + std::string{
                                epochengine::project_sprite_animation::
                                    store_code_name(loaded.code)});
                }
                const auto validation =
                    epochengine::project_sprite_animation::validate_artifact(
                        loaded.artifact);
                if (validation
                    != epochengine::project_sprite_animation::
                        ValidationCode::ready)
                {
                    return fail(
                        "sprite_animation_artifact",
                        std::string{"validation "}
                            + std::string{
                                epochengine::project_sprite_animation::
                                    validation_code_name(validation)});
                }
                accept(ArtifactKind::sprite_animation);
            }

            if (!request.audio_profile_path.empty())
            {
                const std::string normalized = fs::path{
                    request.audio_profile_path}.lexically_normal().generic_string();
                if (normalized
                    != epochengine::project_audio::canonical_source_path)
                {
                    return fail(
                        "audio_profile",
                        "audio profile does not use the canonical logical path");
                }

                epochengine::project_audio::ProjectAudioProfileStore store{
                    std::string{request.project_id}, root};
                auto prepared = store.prepare(false);
                if (!prepared)
                {
                    return fail(
                        "audio_profile",
                        std::string{
                            epochengine::project_audio::
                                preparation_code_name(prepared.code)}
                            + ": " + prepared.diagnostic);
                }
                const auto loaded = store.load_artifact(false);
                if (!loaded)
                {
                    return fail(
                        "audio_profile_artifact",
                        std::string{"load "}
                            + std::string{
                                epochengine::project_audio::
                                    store_code_name(loaded.code)});
                }
                accept(ArtifactKind::audio_profile);
            }

            if (!request.gui_path.empty())
            {
                epochengine::project_gui::ArtifactLibrary library{
                    std::string{request.project_id}, root.string()};
                auto loaded = library.load_or_compile(request.gui_path);
                if (!loaded)
                {
                    return fail(
                        "gui_artifact",
                        std::string{"load "}
                            + std::string{
                                epochengine::project_gui::
                                    library_code_name(loaded.code)});
                }
                epochengine::project_gui_runtime::RuntimeSession runtime{
                    std::move(loaded.artifact)};
                if (!runtime.valid())
                    return fail("gui_runtime", "GUI runtime rejected the artifact");
                const auto frame = runtime.build_frame({1'280.0f, 720.0f});
                if (!frame)
                {
                    return fail(
                        "gui_runtime",
                        std::string{"frame "}
                            + std::string{
                                epochengine::project_gui_runtime::
                                    runtime_code_name(frame.code)});
                }
                accept(ArtifactKind::gui);
            }

            report.succeeded =
                report.required_mask == report.verified_mask;
            report.stage = report.succeeded ? "complete" : "incomplete";
            report.diagnostic = report.succeeded
                ? "all declared project artifacts were accepted"
                : "one or more declared project artifacts were not verified";
            return report;
        }
        catch (const std::exception& exception)
        {
            return fail("exception", exception.what());
        }
        catch (...)
        {
            return fail(
                "exception",
                "project artifact acceptance failed with an unknown exception");
        }
    }
}
