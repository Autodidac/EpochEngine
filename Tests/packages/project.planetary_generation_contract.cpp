// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <chrono>
#include <compare>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include "../../Engine/include/epoch.edition.hpp"

import asset.mesh_artifact;
import package.source_install;
import platform.child_process;
import platform.filesystem;
import project.planetary;

namespace
{
    namespace fs = std::filesystem;
    std::string read(const fs::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    }
    void write(const fs::path& path, std::string_view bytes)
    {
        fs::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
}

#define CHECK(condition) do { if (!(condition)) { write(base / "failure-check.txt", std::to_string(__LINE__)); return __LINE__; } } while (false)

int main(int argc, char** argv)
{
    const std::string_view action = argc == 5 ? argv[4] : "";
    if (argc != 4 && (argc != 5 || (action != "--fresh-props" && action != "--verify-copy"
        && action != "--settings-only" && action != "--refuse-assets" && action != "--verify-runtime"
        && action != "--refresh-hooks" && action != "--ocean" && action != "--timed" && action != "--run-routing"
        && action != "--generation-identity"))) return 1;
    const bool freshProps = action == "--fresh-props";
    const bool oceanSurface = action == "--ocean";
    const bool timedStream = action == "--timed";
    namespace planet = epochengine::project_planetary;
    namespace source = epochengine::package_source;
    namespace process = epochengine::platform::child_process;
    const auto engine = fs::canonical(argv[1]);
    const auto manifest = fs::canonical(argv[2]);
    const auto base = fs::absolute(argv[3]);
    if (action == "--refresh-hooks")
    {
        const auto dependency = source::load_project_attachment(base, "planetary_terrain");
        CHECK(dependency.source_attached());
        const auto settings = planet::load_settings(base);
        CHECK(settings);
        const auto generated = planet::generate(base, base / dependency.relative_source_root, engine, *settings);
        if (!generated.ok) write(base / "refresh-hooks-failure.txt", generated.message);
        CHECK(generated.ok && epochengine::asset::mesh::read_artifact_file(generated.artifact_path));
        CHECK(planet::load_settings(base) == settings);
        write(base / "refresh-hooks-result.txt", "PASS: actual approved-source generator refreshed managed dependency hooks; saved scene unchanged.\n");
        return 0;
    }
    if (action == "--verify-runtime")
    {
        CHECK(!fs::exists(base / "build/planet"));
        CHECK(source::load_project_attachment(base, "planetary_terrain").source_attached());
        auto settings = planet::load_settings(base);
        CHECK(settings);
        settings->mode = "planet";
        CHECK(planet::save_settings(base, *settings));
        planet::StreamView view{.camera={0.0, 2500.0, 0.0}, .viewport_height=720, .request_id=1};
        fs::create_directories(base / "build/planet-stream");
        auto held = epochengine::platform::filesystem::ExclusiveFileLease::acquire(base / "build/planet-stream/cache.lock");
        CHECK(held);
        const auto waiting = planet::stream_step(base, *settings, view);
        CHECK(!waiting.operation.ok && waiting.waiting_for_worker && waiting.operation.build_log.empty());
        held.reset();
        const auto streamed = planet::stream_step(base, *settings, view);
        if (!streamed.operation.ok) write(base / "runtime-failure.txt", streamed.operation.message);
        CHECK(streamed.operation.ok && streamed.snapshot_ready && streamed.snapshot && streamed.draw == 6u);
        CHECK(!process::active_in_group("epoch.project.planet"));
        const fs::path relative = fs::path{"Packages/.runtime/planetary_terrain"}
            / (EPOCH_EDITION_PUBLIC ? "public" : "development") /
#if !defined(NDEBUG)
            "Debug";
#else
            "Release";
#endif
        const auto identity = base / relative / "provider.identity";
        const auto original = read(identity);
        for (const auto field : {"source=", "edition=", "protocol=", "sha256="})
        {
            auto changed = original;
            const auto at = changed.find(field) + std::string_view{field}.size();
            CHECK(at < changed.size());
            changed[at] = changed[at] == '0' ? '1' : '0';
            write(identity, changed);
            view.request_id++;
            const auto rejected = planet::stream_step(base, *settings, view);
            CHECK(!rejected.operation.ok && rejected.operation.build_log.empty());
            CHECK(!process::active_in_group("epoch.project.planet"));
            write(identity, original);
        }
        write(base / relative / "unmanaged.txt", "untouched");
        view.request_id++;
        CHECK(!planet::stream_step(base, *settings, view).operation.ok);
        CHECK(fs::remove(base / relative / "unmanaged.txt"));
#ifdef _WIN32
        const auto producer = base / relative / "epoch_planet_mesh_export.exe";
#else
        const auto producer = base / relative / "epoch_planet_mesh_export";
#endif
        const auto producerBytes = read(producer);
        write(producer, producerBytes + "changed");
        view.request_id++;
        CHECK(!planet::stream_step(base, *settings, view).operation.ok);
        CHECK(!process::active_in_group("epoch.project.planet"));
        write(producer, producerBytes);
        CHECK(!fs::exists(base / "build/planet"));
        write(base / "runtime-result.txt", "PASS: exact staged provider streams with no generation cache; source/edition/protocol/executable identity and unmanaged-file mismatches reject before launch.\n");
        return 0;
    }
    if (action == "--refuse-assets")
        return !planet::save_settings(base, planet::Settings{}) && !planet::load_settings(base) ? 0 : 3;
    if (action == "--verify-copy")
    {
        const auto dependency = source::load_project_attachment(base, "planetary_terrain");
        const auto expected = source::inspect_local_source(manifest);
        const auto settings = planet::load_settings(base);
        return dependency.source_attached() && expected.review_ready()
            && dependency.content_hash == expected.content_hash && settings
            && !fs::exists(base / "build/planet")
            && !process::active_in_group("epoch.project.planet") ? 0 : 3;
    }
    if (fs::exists(base)) return 2;
    fs::create_directories(base);
    try
    {
        const auto project = base / "ordinary project";
        write(project / "project.epoch.json", "{\"id\":\"planetary-contract\"}\n");
        const auto scene = project / "Scenes/main.epoch";
        write(scene, "Unrelated scene must remain unchanged.\n");
#ifdef _WIN32
        // Real Game starters already have this lowercase directory. It must be
        // reused instead of failing the canonical spelling check.
        fs::create_directories(project / "assets/worlds");
#endif
        planet::Settings settings;
        settings.mode = oceanSurface || timedStream || action=="--run-routing" ? "planet" : "classic";
        settings.ocean_surface = oceanSurface;
        settings.cells_per_tile = 4;
        if(oceanSurface)
        {
            const auto water=source::inspect_local_source(manifest.parent_path().parent_path()/"fft_ocean/package.epoch-package.json");
            CHECK(water.review_ready());
            const auto stagedWater=source::stage_local_source(water,water.content_hash,base/"water-cache");
            CHECK(stagedWater.source_staged());
            CHECK(source::attach_project_source(water,stagedWater,water.content_hash,project).source_attached());
        }
        CHECK(planet::save_settings(project, settings));
        CHECK(planet::load_settings(project) == settings);
        CHECK(!planet::save_settings(base / "not a project", settings));
        if(action=="--run-routing")
        {
            const auto ordinary=base/"ordinary";
            write(ordinary/"project.epoch.json","{\"id\":\"ordinary\"}\n");
            const auto plain=planet::runtime_requirement(ordinary);
            CHECK(plain.ok&&!plain.requires_project_runtime);
            const auto missing=planet::runtime_requirement(project);
            CHECK(!missing.ok&&!missing.message.empty());
            const auto recipe=project/planet::recipe_relative_path;
            const auto saved=read(recipe);
            write(recipe,"corrupt recipe\n");
            CHECK(!planet::runtime_requirement(project).ok);
            write(recipe,saved);
        }
        if (action == "--settings-only")
        {
#ifdef _WIN32
            CHECK(fs::is_regular_file(project / "assets/worlds/Planet.epoch_planet"));
#endif
            const auto outside = base / "outside";
            write(outside / "Worlds/sentinel.txt", "untouched\n");
            const auto linked = base / "linked project";
            write(linked / "project.epoch.json", "{}\n");
            std::error_code linkError;
            fs::create_directory_symlink(outside, linked / "Assets", linkError);
            CHECK(!linkError);
            CHECK(!planet::save_settings(linked, settings));
            CHECK(!planet::load_settings(linked));
            CHECK(read(outside / "Worlds/sentinel.txt") == "untouched\n");
            CHECK(!fs::exists(outside / "Worlds/Planet.epoch_planet"));
            CHECK(read(scene) == "Unrelated scene must remain unchanged.\n");
            write(base / "result.txt", "PASS: lowercase project assets settings save/reopen; symbolic-link redirect refused without outside writes.\n");
            return 0;
        }

        const auto review = source::inspect_local_source(manifest);
        CHECK(review.review_ready());
        const auto staged = source::stage_local_source(review, review.content_hash, base / "cache");
        if(!staged.source_staged())write(base/"stage-failure.txt",staged.message);
        CHECK(staged.source_staged());
        const auto dependency = source::attach_project_source(review, staged, review.content_hash, project);
        CHECK(dependency.source_attached());
        const auto provider = project / dependency.relative_source_root;
        if(action=="--generation-identity")
        {
            const auto admitted=planet::capture_source_generation(project,dependency.content_hash);
            CHECK(admitted);
            const auto generated=planet::generate(project,provider,engine,settings);
            if (!generated.ok)
                write(base / "generation-failure.txt", generated.message + "\n" + generated.build_log.generic_string());
            CHECK(generated.ok);
            const auto artifact=epochengine::asset::mesh::read_artifact_file(generated.artifact_path,planet::artifact_limits(settings));
            CHECK(artifact);
            const planet::MeshGenerationIdentity identity{artifact.artifact->key().words,
                static_cast<std::uint32_t>(artifact.artifact->vertices().size()),
                static_cast<std::uint32_t>(artifact.artifact->indices().size()),
                generated.mesh_frame.value_or(planet::GeneratedMeshFrame{})};
            CHECK(planet::mesh_generation_is_current(project,*admitted,settings,identity));
            CHECK(planet::mesh_generation_settings(project,*admitted,identity)==settings);
            CHECK(generated.bodies.size()==1u&&generated.bodies.front().id==settings.body_index);
            CHECK(planet::mesh_generation_bodies(project,*admitted,identity,*artifact.artifact)==generated.bodies);
            const auto originalMesh=read(generated.artifact_path);
            auto changed=settings;changed.mode="universe";changed.reference_radius_meters=25000;changed.seed=42;
            CHECK(planet::save_settings(project,changed));
            CHECK(!planet::mesh_generation_is_current(project,*admitted,settings,identity));
            CHECK(!planet::mesh_generation_is_current(project,*admitted,changed,identity));
            CHECK(planet::mesh_generation_settings(project,*admitted,identity)==settings);
            CHECK(read(generated.artifact_path)==originalMesh);
            const auto regenerated=planet::generate(project,provider,engine,changed);
            CHECK(regenerated.ok);
            const auto replacement=epochengine::asset::mesh::read_artifact_file(regenerated.artifact_path,planet::artifact_limits(changed));
            CHECK(replacement);
            const auto replacementBytes=read(regenerated.artifact_path);
            const planet::MeshGenerationIdentity next{replacement.artifact->key().words,
                static_cast<std::uint32_t>(replacement.artifact->vertices().size()),
                static_cast<std::uint32_t>(replacement.artifact->indices().size()),
                regenerated.mesh_frame.value_or(planet::GeneratedMeshFrame{})};
            CHECK(planet::mesh_generation_is_current(project,*admitted,changed,next));
            CHECK(!planet::mesh_generation_is_current(project,*admitted,changed,identity));
            CHECK(planet::mesh_generation_settings(project,*admitted,next)==changed);
            CHECK(regenerated.bodies.size()==6u);
            CHECK(planet::mesh_generation_bodies(project,*admitted,next,*replacement.artifact)==regenerated.bodies);
            std::uint32_t vertices{},indices{};
            for(std::size_t i=0;i<regenerated.bodies.size();++i) {
                const auto& body=regenerated.bodies[i];CHECK(body.id==i&&!body.name.empty()&&body.radius_meters>0.0);
                CHECK(body.first_vertex==vertices&&body.first_index==indices);
                vertices+=body.vertex_count;indices+=body.index_count;
            }
            CHECK(vertices==next.vertex_count&&indices==next.index_count);
            fs::path bodyFile;
            for(const auto& entry:fs::directory_iterator(project/"Assets/Mesh"))
                if(entry.path().extension()==".epoch_pbod"&&read(entry.path()).find("\nmode=universe\n")!=std::string::npos)bodyFile=entry.path();
            CHECK(!bodyFile.empty());const auto bodyBytes=read(bodyFile);
            const auto replace=[](std::string text,std::string_view from,std::string_view to) {
                const auto at=text.find(from);if(at==std::string::npos)return std::string{};
                text.replace(at,from.size(),to);return text;
            };
            for(const auto& bad:std::vector<std::string>{bodyBytes+"bodies=6\n",
                replace(bodyBytes,"bodies=6\n","bodies=17\n"),
                replace(bodyBytes,"body=1\n","body=0\n"),
                replace(bodyBytes,"first_vertex=0\n","first_vertex=1\n"),
                replace(bodyBytes,"first_index=0\n","first_index=1\n"),
                replace(bodyBytes,"radius_meters=","radius_meters=nan"),
                replace(bodyBytes,"minimum=","minimum=nan"),
                replace(bodyBytes,"reference_radius_meters=25000\n","reference_radius_meters=50000\n")}) {
                CHECK(!bad.empty()&&bad!=bodyBytes);write(bodyFile,bad);
                CHECK(planet::mesh_generation_bodies(project,*admitted,next,*replacement.artifact).empty());
            }
            write(bodyFile,bodyBytes);
            CHECK(planet::mesh_generation_bodies(project,*admitted,next,*replacement.artifact)==regenerated.bodies);
            CHECK(read(generated.artifact_path)==originalMesh);
            auto foreign=next;foreign.coordinates.meters_per_unit*=2;
            CHECK(!planet::mesh_generation_settings(project,*admitted,foreign));
            foreign=next;foreign.coordinates.source_origin_meters[0]+=1;
            CHECK(!planet::mesh_generation_settings(project,*admitted,foreign));
            foreign=next;--foreign.index_count;
            CHECK(!planet::mesh_generation_settings(project,*admitted,foreign));
            auto stale=*admitted;stale.project_manifest+=" ";
            CHECK(!planet::mesh_generation_settings(project,stale,next));
            bool corrupted{};
            for(const auto& entry:fs::directory_iterator(project/"Assets/Mesh"))
                if(entry.path().extension()==".epoch_pgen"&&read(entry.path()).ends_with(planet::encode_settings(changed)))
                {write(entry.path(),"corrupt\n");corrupted=true;}
            CHECK(corrupted&&!planet::mesh_generation_settings(project,*admitted,next));
            CHECK(!planet::mesh_generation_is_current(project,*admitted,changed,next));
            CHECK(read(regenerated.artifact_path)==replacementBytes);
            CHECK(read(scene)=="Unrelated scene must remain unchanged.\n");
            CHECK(!process::active_in_group("epoch.project.planet"));
            write(base/"generation-identity-result.txt","PASS: real source attachment, generation receipt, Save mismatch retains prior mesh/settings evidence; matching regeneration, altered units/counts/source and corrupt receipt refusal. No Editor/GPU runtime.\n");
            return 0;
        }
        if(action=="--run-routing")
        {
            const auto expected=planet::runtime_requirement(project);
            CHECK(expected.ok&&expected.requires_project_runtime&&!expected.message.empty());
            auto universeSettings=settings;universeSettings.mode="universe";
            CHECK(planet::save_settings(project,universeSettings));
            const auto universeRequirement=planet::runtime_requirement(project);
            CHECK(universeRequirement.ok&&universeRequirement.requires_project_runtime);
            CHECK(planet::load_settings(project)==universeSettings);
            const auto cmakeHook=project/"Packages/epoch.dependencies.cmake";
            const auto propsHook=project/"Packages/epoch.dependencies.props";
            write(cmakeHook,"# EPOCH_MANAGED_GENERATED_FILE: planetary_dependency_v1\nset(EPOCH_PLANET_BUILD_RUNTIME_CONTROLLER OFF)\n");
            write(propsHook,"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<!-- EPOCH_MANAGED_GENERATED_FILE: planetary_dependency_v1 -->\n<Project/>\n");
            const auto recipeBefore=read(project/planet::recipe_relative_path);
            const auto prepared=planet::prepare_runtime_dependency(project);
            CHECK(prepared.ok);
            CHECK(read(cmakeHook).find("set(EPOCH_PLANET_BUILD_RUNTIME_CONTROLLER ON)")!=std::string::npos);
            CHECK(read(cmakeHook).find("epoch_extension_planetary_runtime")!=std::string::npos);
#ifdef _WIN32
            CHECK(read(propsHook).find("EPOCH_PROJECT_PLANETARY_RUNTIME=1")!=std::string::npos);
            CHECK(read(propsHook).find("epoch_extension_planetary_runtime.lib")!=std::string::npos);
            CHECK(read(propsHook).find("-DEPOCH_PLANET_BUILD_RUNTIME_CONTROLLER=ON")!=std::string::npos);
#endif
            const auto cmakeTime=fs::last_write_time(cmakeHook),propsTime=fs::last_write_time(propsHook);
            CHECK(planet::prepare_runtime_dependency(project).ok);
            CHECK(fs::last_write_time(cmakeHook)==cmakeTime&&fs::last_write_time(propsHook)==propsTime);
            CHECK(read(project/planet::recipe_relative_path)==recipeBefore);
            CHECK(planet::save_settings(project,settings));
            auto ordinarySettings=settings;ordinarySettings.mode="classic";
            CHECK(planet::save_settings(project,ordinarySettings));
            const auto planar=planet::runtime_requirement(project);
            CHECK(planar.ok&&!planar.requires_project_runtime);
            CHECK(planet::save_settings(project,settings));
            const auto module=provider/"modules/planet.exploration_runtime.ixx";
            const auto original=read(module);
            CHECK(!original.empty());
            const auto hookBytes=read(cmakeHook);
            write(module,original+"\n// changed after explicit admission\n");
            CHECK(!planet::runtime_requirement(project).ok);
            CHECK(!planet::prepare_runtime_dependency(project).ok&&read(cmakeHook)==hookBytes);
            write(module,original);
            CHECK(planet::runtime_requirement(project).requires_project_runtime);
            CHECK(fs::remove(module));
            CHECK(!planet::runtime_requirement(project).ok);
            CHECK(!planet::prepare_runtime_dependency(project).ok&&read(cmakeHook)==hookBytes);
            write(module,original);
            const auto restored=planet::runtime_requirement(project);
            CHECK(restored.ok&&restored.requires_project_runtime);
            CHECK(!fs::exists(project/"build/planet")&&!process::active_in_group("epoch.project.planet"));
            CHECK(read(scene)=="Unrelated scene must remain unchanged.\n");
            write(base/"run-routing-result.txt","PASS: actual ordinary/planar/verified-runtime classification; missing, corrupt and changed source block; no provider build or launch.\n");
            return 0;
        }
        if(timedStream)
        {
            const auto admitted=planet::capture_source_generation(project,dependency.content_hash);
            CHECK(admitted&&planet::source_generation_is_current(project,*admitted));
            const auto missing=planet::load_exploration_session(project);
            CHECK(missing.ok&&!missing.present&&missing.bytes.empty());
            // Storage/CAS bytes are opaque here; provider codec validity has
            // its own contract. No provider is activated by these operations.
            const std::string first="epoch.planet.exploration.v1\nfixture_state=1\n";
            const std::string next="epoch.planet.exploration.v1\nfixture_state=2\n";
            const auto saved=planet::save_exploration_session(project,*admitted,first,missing);
            CHECK(saved.ok&&saved.present&&saved.bytes==first);
            const auto reloaded=planet::load_exploration_session(project);
            CHECK(reloaded.ok&&reloaded.bytes==first);
            CHECK(!planet::save_exploration_session(project,*admitted,next,missing).ok);
            const auto second=planet::save_exploration_session(project,*admitted,next,reloaded);
            CHECK(second.ok&&planet::load_exploration_session(project).bytes==next);
            CHECK(!planet::save_exploration_session(project,*admitted,first,reloaded).ok);
            const auto statePath=project/"Assets/Worlds/Planet.epoch_exploration";
            write(statePath,"corrupt session\n");
            const auto corrupt=planet::load_exploration_session(project);
            CHECK(corrupt.present&&!corrupt.ok);
            CHECK(!planet::save_exploration_session(project,*admitted,first,corrupt).ok);
            CHECK(read(statePath)=="corrupt session\n");
            write(statePath,next);
            auto foreign=*admitted;foreign.project_manifest+=" ";
            CHECK(!planet::save_exploration_session(project,foreign,first,second).ok);
            CHECK(read(statePath)==next);
            write(base/"session-storage-result.txt","PASS: missing/save/reload, stale-window CAS, corrupt storage and foreign-source refusal preserve observed bytes; opaque provider state only.\n");
        }
        const auto originalRecipe = read(project / planet::recipe_relative_path);
        const auto wrongSource = planet::generate(project, review.repository_root, engine, settings);
        CHECK(!wrongSource.ok && wrongSource.build_log.empty());
        CHECK(read(project / planet::recipe_relative_path) == originalRecipe);

        std::stop_source beforeAdmission;
        beforeAdmission.request_stop();
        const auto stoppedBefore = planet::generate(project, provider, engine, settings, beforeAdmission.get_token());
        CHECK(!stoppedBefore.ok && stoppedBefore.cancelled && stoppedBefore.build_log.empty());
        auto overBudget = settings;
        overBudget.mode = "universe";
        overBudget.cells_per_tile = 64;
        const auto refusedBudget = planet::generate(project, provider, engine, overBudget);
        CHECK(!refusedBudget.ok && refusedBudget.build_log.empty());
        CHECK(!fs::exists(project / "build/planet"));

        const auto generated = planet::generate(project, provider, engine, settings);
        if (!generated.ok) write(base / "generation-failure.txt", generated.message);
        CHECK(generated.ok && !generated.cancelled);
        CHECK(fs::is_regular_file(generated.build_log));
        CHECK(fs::is_regular_file(generated.artifact_path));
        CHECK(epochengine::asset::mesh::read_artifact_file(generated.artifact_path));
        if(oceanSurface)
        {
            const auto decoded=epochengine::asset::mesh::read_artifact_file(generated.artifact_path);
            std::size_t sea{};
            for(const auto& vertex:decoded.artifact->vertices())
                if(std::abs(vertex.color[0]-0.015f)<1e-7f && std::abs(vertex.color[1]-0.085f)<1e-7f
                    &&std::abs(vertex.color[2]-0.12f)<1e-7f) ++sea;
            CHECK(sea>0u);
            CHECK(read(project/planet::recipe_relative_path).starts_with("epoch.planet.recipe.v3\n"));
        }
        CHECK(planet::load_settings(project) == settings);
        CHECK(generated.recipe_path == project / planet::recipe_relative_path);
        const auto cacheIdentity = project / "build/planet/source.sha256";
        CHECK(read(cacheIdentity) == dependency.content_hash + "\n");
        write(cacheIdentity, "sha256:" + std::string(64, '0') + "\n");
        const auto foreignCache = planet::generate(project, provider, engine, settings);
        CHECK(!foreignCache.ok && foreignCache.build_log.empty());
        CHECK(read(cacheIdentity) == "sha256:" + std::string(64, '0') + "\n");
        write(cacheIdentity, dependency.content_hash + "\n");
        const auto cmake = read(project / "Packages/epoch.dependencies.cmake");
        CHECK(cmake.find("EpochEngineExtensions::PlanetaryCore") != std::string::npos);
        CHECK(cmake.find(dependency.relative_source_root.lexically_relative("Packages").generic_string()) != std::string::npos);
#ifdef _WIN32
        const auto props = read(project / "Packages/epoch.dependencies.props");
        CHECK(props.find("epoch_extension_planetary_core.lib") != std::string::npos);
        CHECK(props.find("planetary\\sha256:") == std::string::npos);
#endif
        if (freshProps)
        {
            write(base / "result.txt", "PASS: production generation wrote project dependency hooks; source remains approved for fresh-copy build tests.\n");
            return 0;
        }
        const auto oldArtifact = read(generated.artifact_path);
        auto changed = settings;
        changed.mode = "planet";
        changed.seed = 42;
        changed.cells_per_tile = 8;
        const auto regenerated = planet::generate(project, provider, engine, changed);
        if (!regenerated.ok) write(base / "regeneration-failure.txt", regenerated.message);
        CHECK(regenerated.ok && regenerated.artifact_path != generated.artifact_path);
        CHECK(read(generated.artifact_path) == oldArtifact);
        CHECK(epochengine::asset::mesh::read_artifact_file(regenerated.artifact_path));
        CHECK(planet::load_settings(project) == changed);

        // Actual supervised provider/cache transport, not a mocked receipt.
        planet::StreamView view{.camera={0.0, 2500.0, 0.0}, .viewport_height=720, .request_id=1};
        if(timedStream)view.simulation_seconds=123.25;
        auto streamed = planet::stream_step(project, changed, view);
        if (!streamed.operation.ok) write(base / "stream-failure.txt", streamed.operation.message);
        CHECK(streamed.operation.ok && streamed.snapshot_ready && streamed.snapshot);
        CHECK(streamed.draw == 6u && streamed.maximum_level == 0u);
        if(timedStream)
        {
            CHECK(streamed.operation.artifact_path.empty()&&streamed.operation.build_log.empty());
        }
        const auto distantKey = streamed.snapshot->key();
        view.request_id++;
        streamed = planet::stream_step(project, changed, view);
        CHECK(streamed.operation.ok && streamed.snapshot && streamed.built == 0u && streamed.reused >= 6u);
        CHECK(streamed.snapshot->key() == distantKey);
        if(timedStream)
        {
            const auto previous=streamed.snapshot;
            const auto previousFile=regenerated.artifact_path;
            const auto previousBytes=read(previousFile);
            for(const double invalid:{-1.0,std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::quiet_NaN(),1.0e12+1.0})
            {
                auto invalidView=view;invalidView.simulation_seconds=invalid;++invalidView.request_id;
                const auto rejected=planet::stream_step(project,changed,invalidView);
                CHECK(!rejected.operation.ok&&!rejected.snapshot&&rejected.operation.build_log.empty());
                CHECK(!process::active_in_group("epoch.project.planet"));
                CHECK(previous->key()==distantKey&&read(previousFile)==previousBytes);
            }
            view.simulation_seconds=124.5;++view.request_id;
            const auto advanced=planet::stream_step(project,changed,view);
            CHECK(advanced.operation.ok&&advanced.snapshot_ready&&advanced.snapshot);
            CHECK(advanced.operation.artifact_path.empty()&&advanced.operation.build_log.empty());
            CHECK(read(previousFile)==previousBytes&&planet::load_settings(project)==changed);
            // Coherent, explicitly reviewed disposable package variant. The
            // real producer emits a wrong echo or stale header; the host must
            // reject its receipt rather than trusting successful execution.
            const auto variant=base/"wrong-time-source";
            CHECK(source::verify_staged_source(review,base/"cache").source_staged());
            const auto frozenSource=staged.receipt_path.parent_path()/"source";
            for(const auto& entry:review.files)write(variant/entry.relative_path,read(frozenSource/entry.relative_path));
            const auto writer=variant/review.source_path/"tools/planet.stream_export.cpp";
            auto writerBytes=read(writer);
            const std::string header="(simulation_seconds?\"epoch.planet.stream.v3\\n\"";
            const auto headerAt=writerBytes.find(header);CHECK(headerAt!=std::string::npos);
            writerBytes.replace(headerAt,header.size(),"(simulation_seconds&&*simulation_seconds==124.5?\"epoch.planet.stream.v2\\n\":simulation_seconds?\"epoch.planet.stream.v3\\n\"");
            const std::string echo="<<*simulation_seconds<<'\\n'";
            const auto echoAt=writerBytes.find(echo);CHECK(echoAt!=std::string::npos);
            writerBytes.replace(echoAt,echo.size(),"<<(*simulation_seconds+1.0)<<'\\n'");write(writer,writerBytes);
            const auto wrongReview=source::inspect_local_source(variant/review.source_path/"package.epoch-package.json");
            if(!wrongReview.review_ready())write(base/"wrong-time-inspection-failure.txt",wrongReview.message);
            CHECK(wrongReview.review_ready()&&wrongReview.content_hash!=review.content_hash);
            const auto wrongStage=source::stage_local_source(wrongReview,wrongReview.content_hash,base/"cache");CHECK(wrongStage.source_staged());
            const auto wrongProject=base/"wrong-time-project";write(wrongProject/"project.epoch.json","{\"id\":\"wrong-time-fixture\"}\n");
            const auto wrongAttach=source::attach_project_source(wrongReview,wrongStage,wrongReview.content_hash,wrongProject);CHECK(wrongAttach.source_attached());
            const auto wrongGenerated=planet::generate(wrongProject,wrongProject/wrongAttach.relative_source_root,engine,changed);CHECK(wrongGenerated.ok);
            const auto wrongPrior=read(wrongGenerated.artifact_path);
            auto wrongView=view;wrongView.simulation_seconds=123.25;++wrongView.request_id;
            const auto wrongEcho=planet::stream_step(wrongProject,changed,wrongView);
            CHECK(!wrongEcho.operation.ok&&!wrongEcho.snapshot&&wrongEcho.operation.message.contains("evidence is invalid"));
            CHECK(read(wrongGenerated.artifact_path)==wrongPrior&&read(previousFile)==previousBytes&&previous->key()==distantKey);
            wrongView.simulation_seconds=124.5;++wrongView.request_id;
            const auto staleHeader=planet::stream_step(wrongProject,changed,wrongView);
            CHECK(!staleHeader.operation.ok&&!staleHeader.snapshot&&staleHeader.operation.message.contains("no valid streaming receipt"));
            CHECK(read(wrongGenerated.artifact_path)==wrongPrior&&read(previousFile)==previousBytes&&!process::active_in_group("epoch.project.planet"));
            CHECK(planet::load_settings(wrongProject)==changed&&planet::load_settings(project)==changed);
            write(base/"timed-result.txt","PASS: actual timed v3 request/echo, repeat cache reuse, advanced time and invalid time refusal preserve prior output.\n");
        }
        if(oceanSurface)
        {
            auto terrainOnly=changed;terrainOnly.ocean_surface=false;
            CHECK(planet::save_settings(project,terrainOnly));
            CHECK(read(project/planet::recipe_relative_path).starts_with("epoch.planet.recipe.v2\n"));
            view.request_id++;
            const auto dry=planet::stream_step(project,terrainOnly,view);
            CHECK(dry.operation.ok&&dry.snapshot_ready&&dry.snapshot&&dry.snapshot->key()!=distantKey);
            const auto cacheName=std::to_string(changed.seed)+"-"+std::to_string(changed.cells_per_tile)+"-"+std::to_string(changed.body_index);
            const auto cacheRoot=project/"cache/planet"/dependency.content_hash.substr(7u);
            CHECK(fs::is_directory(cacheRoot/cacheName)&&fs::is_directory(cacheRoot/(cacheName+"-ocean")));
            CHECK(planet::save_settings(project,changed)&&planet::load_settings(project)==changed);
            view.request_id++;
            streamed=planet::stream_step(project,changed,view);
            CHECK(streamed.operation.ok&&streamed.snapshot&&streamed.snapshot->key()==distantKey&&streamed.built==0u);
            write(base/"ocean-result.txt","PASS: actual blue sea-level geometry; v3 save/reopen; wet v2 receipt and legacy dry v1 transport; distinct caches and unchanged prior artifacts.\n");
        }
        view.camera = {0.0, 10.01, 0.0};
        for (unsigned attempt=0; attempt<16u; ++attempt)
        {
            view.request_id++;
            streamed = planet::stream_step(project, changed, view);
            CHECK(streamed.operation.ok && streamed.snapshot_ready && streamed.snapshot);
            CHECK(streamed.resident_bytes <= streamed.maximum_bytes);
            if (!streamed.pending) break;
        }
        CHECK(streamed.maximum_level > 0u && streamed.snapshot->key() != distantKey);
        CHECK(planet::load_settings(project) == changed);
        std::stop_source cancelledStream; cancelledStream.request_stop();
        view.request_id++;
        const auto stoppedStream = planet::stream_step(project, changed, view, cancelledStream.get_token());
        CHECK(!stoppedStream.operation.ok && !stoppedStream.snapshot);
        CHECK(!process::active_in_group("epoch.project.planet"));

        std::stop_source duringWorker;
        bool observedWorker{};
        std::jthread cancelWorker([&](std::stop_token threadStop) {
            const auto end = std::chrono::steady_clock::now() + std::chrono::seconds{15};
            while (!threadStop.stop_requested() && std::chrono::steady_clock::now() < end)
            {
                if (const auto worker = process::active_in_group("epoch.project.planet"); worker && worker->active())
                {
                    observedWorker = true;
                    duringWorker.request_stop();
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            }
        });
        const auto cancelled = planet::generate(project, provider, engine, settings, duringWorker.get_token());
        cancelWorker.request_stop();
        cancelWorker.join();
        CHECK(observedWorker && !cancelled.ok && cancelled.cancelled);
        CHECK(!process::active_in_group("epoch.project.planet"));
        CHECK(planet::load_settings(project) == changed);
        CHECK(read(scene) == "Unrelated scene must remain unchanged.\n");
        for (const auto& worker : process::snapshots())
            CHECK(worker.exclusive_group != "epoch.project.planet" || !worker.active());
        write(provider / "CMakeLists.txt", read(provider / "CMakeLists.txt") + "\n# changed source\n");
        const auto modified = planet::generate(project, provider, engine, settings);
        CHECK(!modified.ok && modified.build_log.empty());
        CHECK(planet::load_settings(project) == changed);
        write(base / "result.txt", "PASS: project source admission, native compiler/producer, repeated generation, recipe persistence and owned cancellation.\n");
    }
    catch (const std::exception& error)
    {
        write(base / "exception.txt", error.what());
        return 255;
    }
    return 0;
}
