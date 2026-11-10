// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>
#include "../../include/epoch.edition.hpp"

module project.planetary;
import core.env;
import package.source_install;
import platform.child_process;
import platform.filesystem;
import platform.work_admission;
import asset.mesh_artifact;
import asset.texture_artifact;

namespace epochengine::project_planetary
{
    namespace
    {
        namespace fs = std::filesystem;
        namespace process = platform::child_process;
        using Clock = std::chrono::steady_clock;
        std::atomic_uint64_t sequence{};

        bool admit_preview_memory(const Settings& settings, Result& result)
        {
            // A prior larger mesh may remain visible while a smaller request
            // is prepared. Account for the full admitted prior-artifact bound;
            // callers need not destroy the old snapshot to free its memory.
            const auto footprint = preview_memory_footprint(settings, 64ull * 1024u * 1024u);
            namespace admission = platform::work_admission;
            const auto sample = admission::sample_host_resources();
            const auto verdict = footprint.cpu_upper_bytes == 0u
                ? admission::Reason::invalid_policy
                : admission::assess_memory_budget(sample, admission::now_milliseconds(),
                    footprint.cpu_upper_bytes);
            if (verdict == admission::Reason::ready) return true;
            result.message = "Terrain memory check: " + std::string{admission::reason_message(verdict)}
                + " The current terrain is retained.";
            return false;
        }

        std::string read_small(const fs::path& path, std::size_t limit)
        {
            if (!fs::is_regular_file(path) || fs::is_symlink(fs::symlink_status(path))) return {};
            std::ifstream input(path, std::ios::binary | std::ios::ate);
            if (!input || input.tellg() < 0 || std::uint64_t(input.tellg()) > limit) return {};
            std::string result(static_cast<std::size_t>(input.tellg()), '\0');
            input.seekg(0);
            if (!input.read(result.data(), static_cast<std::streamsize>(result.size()))) return {};
            if (input.peek() != std::char_traits<char>::eof()) return {};
            return result;
        }

        bool source_supports_lod_controls(const fs::path& source)
        {
            // The complete source tree was already verified against the exact
            // approved attachment. Older attached versions remain immutable.
            const auto exporter=read_small(source / "tools/planet.stream_export.cpp",64u*1024u);
            const auto options=read_small(source / "tools/planet.mesh_export.cpp",64u*1024u);
            return exporter.find("epoch.planet.stream.v4")!=std::string::npos
                &&options.find("--patch-pixels")!=std::string::npos
                &&options.find("--lod-hysteresis")!=std::string::npos;
        }

        bool source_supports_size_controls(const fs::path& source)
        {
            const auto exporter=read_small(source / "tools/planet.stream_export.cpp",64u*1024u);
            const auto options=read_small(source / "tools/planet.mesh_export.cpp",64u*1024u);
            return exporter.find("epoch.planet.stream.v5")!=std::string::npos
                &&options.find("--size-reference-body")!=std::string::npos
                &&options.find("--reference-radius-meters")!=std::string::npos;
        }

        bool source_supports_mesh_frame(const fs::path& source)
        {
            const auto exporter=read_small(source/"tools/planet.mesh_export.cpp",64u*1024u);
            return exporter.find("epoch.planet.mesh-frame.v1")!=std::string::npos;
        }

        bool source_supports_original_sunlight(const fs::path& source)
        {
            const auto runtime=read_small(source/"src/planet.exploration_runtime.cpp",64u*1024u);
            return runtime.find("epoch.planet.exploration.v3")!=std::string::npos
                &&runtime.find("binding_.original_sunlight")!=std::string::npos
                &&runtime.find("frame.sunlight=")!=std::string::npos;
        }

        bool source_supports_body_inventory(const fs::path& source)
        {
            return read_small(source/"tools/planet.mesh_export.cpp",64u*1024u)
                .find("epoch.planet.body-inventory.v1")!=std::string::npos;
        }

        bool source_supports_runtime_scope(const fs::path& source)
        {
            const auto runtime=read_small(source/"src/planet.exploration_runtime.cpp",64u*1024u);
            return runtime.find("epoch.planet.exploration.v4")!=std::string::npos
                &&runtime.find("binding_.universe")!=std::string::npos;
        }

        std::string real_text(double value)
        {
            std::array<char,64> bytes{};
            const auto written=std::to_chars(bytes.data(),bytes.data()+bytes.size(),value,
                std::chars_format::general,std::numeric_limits<double>::max_digits10);
            return written.ec==std::errc{}?std::string{bytes.data(),written.ptr}:std::string{};
        }

        void append_size_arguments(std::vector<std::string>& arguments,const Settings& settings)
        {
            if(requires_size_controls(settings))
                arguments.insert(arguments.end(),{"--size-reference-body",std::to_string(settings.size_reference_body),
                    "--reference-radius-meters",real_text(settings.reference_radius_meters)});
        }

        std::string stream_cache_identity(const Settings& settings)
        {
            return std::to_string(settings.seed)+"-"+std::to_string(settings.cells_per_tile)+"-"
                +std::to_string(settings.body_index)+(settings.ocean_surface?"-ocean":"")
                +(requires_size_controls(settings)?"-size-"+std::to_string(settings.size_reference_body)
                    +"-"+real_text(settings.reference_radius_meters):"");
        }

        std::optional<GeneratedMeshFrame> admit_generated_mesh_frame(std::string_view text,
            const Settings& settings,const asset::mesh::Artifact& artifact)
        {
            constexpr std::string_view header="epoch.planet.mesh-frame.v1\n";
            if(text.size()>4096u||!text.starts_with(header))return {};
            text.remove_prefix(header.size());
            const auto field=[&](std::string_view name)->std::optional<std::string_view> {
                if(!text.starts_with(name))return {};
                text.remove_prefix(name.size());const auto end=text.find('\n');
                if(end==std::string_view::npos||end==0u)return {};
                const auto value=text.substr(0,end);text.remove_prefix(end+1u);return value;
            };
            const auto integer=[&](std::string_view name,std::uint64_t expected) {
                const auto value=field(name);if(!value)return false;
                std::uint64_t parsed{};const auto converted=std::from_chars(value->data(),value->data()+value->size(),parsed);
                return converted.ec==std::errc{}&&converted.ptr==value->data()+value->size()&&parsed==expected;
            };
            const auto real=[&](std::string_view name,double& output) {
                const auto value=field(name);if(!value)return false;
                const auto converted=std::from_chars(value->data(),value->data()+value->size(),output);
                return converted.ec==std::errc{}&&converted.ptr==value->data()+value->size()&&std::isfinite(output);
            };
            const auto mode=field("mode=");
            double span{},seconds{},radius{},scale{};
            if(!mode||*mode!=settings.mode||!integer("seed=",settings.seed)
                ||!integer("body_index=",settings.body_index)||!integer("cells_per_tile=",settings.cells_per_tile)
                ||!integer("tiles_per_axis=",settings.tiles_per_axis)||!real("tile_span_meters=",span)
                ||span!=settings.tile_span_meters||!real("seconds=",seconds)||seconds!=0.0
                ||!integer("size_reference_body=",settings.size_reference_body)
                ||!real("reference_radius_meters=",radius)||radius!=settings.reference_radius_meters
                ||!real("meters_to_preview_units=",scale)||scale<=0.0)return {};
            GeneratedMeshFrame result;result.meters_per_unit=1.0/scale;
            if(!std::isfinite(result.meters_per_unit)||result.meters_per_unit<=0.0||result.meters_per_unit>1.0e12)return {};
            const auto origin=field("origin_meters=");if(!origin)return {};
            auto remaining=*origin;
            for(std::size_t axis=0u;axis<3u;++axis)
            {
                const auto comma=remaining.find(',');
                if((axis<2u)==(comma==std::string_view::npos))return {};
                const auto value=remaining.substr(0,comma);
                auto& output=result.source_origin_meters[axis];
                const auto converted=std::from_chars(value.data(),value.data()+value.size(),output);
                if(converted.ec!=std::errc{}||converted.ptr!=value.data()+value.size()
                    ||!std::isfinite(output)||std::abs(output)>1.0e15)return {};
                if(axis<2u)remaining.remove_prefix(comma+1u);
            }
            const auto key=field("content_key=");
            if(!key||*key!=asset::texture::content_hash_hex(artifact.key())
                ||!integer("vertices=",artifact.vertices().size())||!integer("indices=",artifact.indices().size())
                ||!text.empty())return {};
            return result;
        }

        std::vector<GeneratedBody> admit_generated_body_inventory(std::string_view text,
            const Settings& settings,const MeshGenerationIdentity& identity,const asset::mesh::Artifact& artifact)
        {
            constexpr std::string_view header="epoch.planet.body-inventory.v1\n";
            if(text.size()>16384u||!text.starts_with(header)||artifact.key().words!=identity.artifact_key
                ||artifact.vertices().size()!=identity.vertex_count||artifact.indices().size()!=identity.index_count)return {};
            text.remove_prefix(header.size());const auto split=text.find("\nbodies=");
            if(split==std::string_view::npos)return {};
            const auto frame=admit_generated_mesh_frame(text.substr(0,split+1u),settings,artifact);
            if(!frame||*frame!=identity.coordinates)return {};
            text.remove_prefix(split+1u);
            const auto field=[&](std::string_view name)->std::optional<std::string_view> {
                if(!text.starts_with(name))return {};
                text.remove_prefix(name.size());const auto end=text.find('\n');
                if(end==std::string_view::npos||end==0u)return {};
                const auto value=text.substr(0,end);text.remove_prefix(end+1u);return value;
            };
            const auto integer=[&](std::string_view name,auto& output) {
                const auto value=field(name);if(!value)return false;
                const auto parsed=std::from_chars(value->data(),value->data()+value->size(),output);
                return parsed.ec==std::errc{}&&parsed.ptr==value->data()+value->size();
            };
            const auto real=[&](std::string_view value,double& output) {
                const auto parsed=std::from_chars(value.data(),value.data()+value.size(),output);
                return parsed.ec==std::errc{}&&parsed.ptr==value.data()+value.size()&&std::isfinite(output);
            };
            const auto vector=[&](std::string_view name,auto& output) {
                const auto value=field(name);if(!value)return false;auto remaining=*value;
                for(std::size_t axis=0;axis<3u;++axis) {
                    const auto comma=remaining.find(',');if((axis<2u)==(comma==std::string_view::npos))return false;
                    double number{};if(!real(remaining.substr(0,comma),number)||std::abs(number)>1.0e15)return false;
                    output[axis]=static_cast<std::remove_reference_t<decltype(output[axis])>>(number);
                    if(!std::isfinite(output[axis]))return false;
                    if(axis<2u)remaining.remove_prefix(comma+1u);
                }return true;
            };
            std::uint32_t count{};if(!integer("bodies=",count)||count!=(settings.mode=="universe"?6u:1u))return {};
            std::vector<GeneratedBody> bodies;bodies.reserve(count);
            std::uint64_t vertexAt{},indexAt{};
            for(std::uint32_t at=0;at<count;++at) {
                GeneratedBody body;
                if(!integer("body=",body.id)||body.id>5u||!integer("parent=",body.parent)
                    ||body.parent< -1||body.parent>5||body.parent==static_cast<std::int32_t>(body.id)
                    ||std::any_of(bodies.begin(),bodies.end(),[&](const auto& prior){return prior.id==body.id;}))return {};
                if(settings.mode!="universe"&&body.id!=settings.body_index)return {};
                const auto name=field("name=");if(!name||name->size()>64u)return {};
                for(const auto character:*name)if(static_cast<unsigned char>(character)<32u||static_cast<unsigned char>(character)>126u)return {};
                body.name=*name;
                if(!vector("center_meters=",body.center_meters))return {};
                const auto radiusValue=field("radius_meters=");
                if(!radiusValue||!real(*radiusValue,body.radius_meters)||body.radius_meters<=0.0||body.radius_meters>1.0e12
                    ||!integer("first_vertex=",body.first_vertex)||!integer("vertices=",body.vertex_count)
                    ||!integer("first_index=",body.first_index)||!integer("indices=",body.index_count)
                    ||body.first_vertex!=vertexAt||body.first_index!=indexAt||body.vertex_count<3u
                    ||body.index_count<3u||body.index_count%3u||vertexAt+body.vertex_count>identity.vertex_count
                    ||indexAt+body.index_count>identity.index_count||!vector("minimum=",body.minimum)
                    ||!vector("maximum=",body.maximum))return {};
                auto minimum=artifact.vertices()[body.first_vertex].position,maximum=minimum;
                for(std::uint32_t i=0;i<body.vertex_count;++i)for(std::size_t axis=0;axis<3u;++axis) {
                    const auto value=artifact.vertices()[body.first_vertex+i].position[axis];
                    minimum[axis]=(std::min)(minimum[axis],value);maximum[axis]=(std::max)(maximum[axis],value);
                }
                if(minimum!=body.minimum||maximum!=body.maximum)return {};
                for(std::uint32_t i=0;i<body.index_count;++i) {
                    const auto index=artifact.indices()[body.first_index+i];
                    if(index<body.first_vertex||std::uint64_t(index)>=vertexAt+body.vertex_count)return {};
                }
                vertexAt+=body.vertex_count;indexAt+=body.index_count;bodies.push_back(std::move(body));
            }
            if(!text.empty()||vertexAt!=identity.vertex_count||indexAt!=identity.index_count)return {};
            for(const auto& body:bodies) {
                auto parent=body.parent;
                for(std::size_t step=0;parent>=0;++step) {
                    if(step>=bodies.size()||parent==static_cast<std::int32_t>(body.id))return {};
                    const auto found=std::find_if(bodies.begin(),bodies.end(),[&](const auto& value){return value.id==static_cast<std::uint32_t>(parent);});
                    if(found==bodies.end())break;parent=found->parent;
                }
            }
            return bodies;
        }

        // Validate every ancestor before creating a child. Existing Windows
        // directory spelling may differ in case; redirected paths stay denied.
        std::optional<fs::path> ordinary_directory(const fs::path& root,
            const fs::path& relative, bool create)
        {
            if (root.empty() || !root.is_absolute() || relative.is_absolute()) return {};
            const auto canonical = fs::canonical(root);
            if (!fs::is_directory(canonical)) return {};
            auto current = canonical;
            for (const auto& part : relative)
            {
                if (part.empty() || part == "." || part == "..") return {};
                current /= part;
                const auto status = fs::symlink_status(current);
                if (fs::is_symlink(status)) return {};
                if (!fs::exists(status) && (!create || !fs::create_directory(current))) return {};
                if (!fs::is_directory(current)) return {};
                // Windows junctions are not classified as symbolic links by
                // every filesystem implementation, but read_symlink exposes
                // their redirect. Do not accept even an equivalent target.
                std::error_code linkError;
                (void)fs::read_symlink(current, linkError);
                if (!linkError) return {};
                const auto resolved = fs::canonical(current);
                auto requestedName = current.native();
                auto resolvedName = resolved.native();
#if defined(_WIN32)
                // Relative owner names are ASCII. The parent is already in its
                // canonical spelling; no Unicode case approximation is needed.
                for (auto* name : {&requestedName, &resolvedName})
                    for (auto& ch : *name)
                        if (ch >= L'A' && ch <= L'Z') ch += L'a' - L'A';
#endif
                if (requestedName != resolvedName || !fs::equivalent(current, resolved)) return {};
                current = resolved;
            }
            return current;
        }

        std::optional<fs::path> project_directory(const fs::path& root,const fs::path& relative,bool create)
        {
            const auto base=ordinary_directory(root,{},false);
            if(!base||!fs::is_regular_file(*base/"project.epoch.json"))return {};
            return ordinary_directory(*base,relative,create);
        }

        bool write_owned(const fs::path& path, std::string_view text, std::string_view marker)
        {
            // Run/Build may refresh the same admitted dependency twice.
            // Identical managed bytes must not invalidate compiler freshness.
            if(!text.empty()&&read_small(path,32768u)==text)return true;
            if (fs::exists(path) && !read_small(path, 32768).starts_with(marker)) return false;
            const auto temporary = fs::path{path.string() + ".write-" + std::to_string(++sequence)};
            std::error_code error;
            const auto bytes = std::as_bytes(std::span{text.data(), text.size()});
            if (!platform::filesystem::exclusive_create_and_write(temporary, bytes, error)) return false;
            if (platform::filesystem::atomic_replace_same_filesystem(temporary, path, error)) return true;
            fs::remove(temporary, error);
            return false;
        }

        std::optional<fs::path> find_tool(std::string_view name)
        {
            const auto path = core::env::get("PATH");
            if (!path) return {};
            std::string_view remaining{path->data(), path->size()};
#ifdef _WIN32
            constexpr char separator = ';';
            const auto filename = std::string(name) + ".exe";
#else
            constexpr char separator = ':';
            const auto filename = std::string(name);
#endif
            while (!remaining.empty())
            {
                const auto end = remaining.find(separator);
                auto part = remaining.substr(0, end);
                if (part.size() > 1u && part.front() == '"' && part.back() == '"')
                    part = part.substr(1, part.size() - 2u);
                const fs::path directory{part};
                if (!part.empty() && directory.is_absolute() && fs::is_regular_file(directory / filename))
                    return fs::canonical(directory / filename);
                if (end == std::string_view::npos) break;
                remaining.remove_prefix(end + 1u);
            }
            return {};
        }

        bool execute(const fs::path& executable, std::vector<std::string> arguments,
            const fs::path& working, const fs::path& log, std::stop_token stop,
            const std::vector<process::EnvironmentVariable>& environment,
            std::uint64_t timeout, Result& result,
            std::optional<process::ExecutableIdentity> expected = {},
            std::shared_ptr<void> retirement_guard = {})
        {
            if (stop.stop_requested()) { result.cancelled = true; result.message = "Terrain generation cancelled."; return false; }
            const auto identity = process::inspect_executable(executable);
            if (!identity) { result.message = "The required build tool or terrain generator could not be verified."; return false; }
            if (expected && identity != expected)
            { result.message = "The terrain provider changed after its build receipt was checked."; return false; }
            process::LaunchRequest request;
            request.executable = executable;
            request.working_directory = working;
            request.merged_output_path = log;
            request.arguments = std::move(arguments);
            request.correlation_key = "epoch.project.planet." + std::to_string(++sequence);
            request.exclusive_group = "epoch.project.planet";
            request.display_name = "Planetary project generation";
            request.window_mode = process::WindowMode::hidden;
            request.append_output = true;
            request.expected_executable = identity;
            request.retirement_guard = std::move(retirement_guard);
            request.environment = environment;
            request.disconnect_standard_input = true;
            const auto launched = process::launch_or_focus(request);
            if (!launched.owns_new_process()) {
                result.worker_busy = process::active_in_group("epoch.project.planet").has_value();
                result.message = "Terrain worker did not start: " + launched.message; return false;
            }
            const auto waited = process::wait(launched.handle, stop, timeout);
            if (waited.code != process::WaitCode::exited)
            {
                (void)process::stop(launched.handle, process::StopMode::force);
                const auto retired = process::wait(launched.handle, {}, 10'000'000'000ull);
                if (retired.code != process::WaitCode::exited)
                {
                    result.message = "Terrain worker has not retired. Check Tasks before starting another build.";
                    return false; // Keep supervisor ownership/evidence; never release an active child.
                }
            }
            (void)process::release(launched.handle);
            result.cancelled = waited.code == process::WaitCode::cancelled;
            if (!waited || !waited.process || !waited.process->exit_code_valid || waited.process->exit_code != 0)
            {
                const std::string outcome = waited.code == process::WaitCode::timed_out ? "deadline exceeded"
                    : waited.code == process::WaitCode::cancelled ? "cancelled"
                    : waited.code == process::WaitCode::exited ? "exited"
                    : waited.code == process::WaitCode::stale_handle ? "stale process handle" : "wait failed";
                const std::string exit = waited.process && waited.process->exit_code_valid
                    ? std::to_string(waited.process->exit_code) : "unavailable";
                // Providers may fail before writing any output. Retain their exact
                // request and supervisor outcome in the existing local build log;
                // never include environment values or require another blind run.
                std::ofstream evidence(log, std::ios::binary | std::ios::app);
                if (evidence)
                {
                    evidence << "\n[Epoch terrain worker] outcome=" << outcome << "; exit=" << exit
                        << "; deadline_ns=" << timeout << "\nexecutable=" << executable.generic_string()
                        << "\nworking_directory=" << working.generic_string() << '\n';
                    for (std::size_t i = 0; i < request.arguments.size(); ++i)
                        evidence << "argument[" << i << "]=" << request.arguments[i] << '\n';
                    const auto providerFailure = read_small(log.parent_path() / "generated/terrain.failure", 4096u);
                    if (!providerFailure.empty()) evidence << providerFailure;
                }
                result.message = result.cancelled ? "Terrain generation cancelled. Saved scene is unchanged."
                    : waited.code == process::WaitCode::timed_out
                        ? "Terrain worker exceeded its " + std::to_string(timeout / 1'000'000'000ull)
                            + "-second deadline. Current terrain is retained. Details: " + log.generic_string()
                        : "Terrain worker " + outcome + " (exit " + exit
                            + "). Current terrain is retained. Details: " + log.generic_string();
                return false;
            }
            return true;
        }

        struct MeshGenerationRecord final { std::string filename, bytes; };
        std::optional<MeshGenerationRecord> mesh_generation_record(const SourceGeneration& source,
            const Settings& settings, const MeshGenerationIdentity& mesh)
        {
            const auto recipe = encode_settings(settings);
            const auto limits = artifact_limits(settings);
            if (recipe.empty() || recipe.size()>1024u || source.project_manifest.empty()
                || source.project_manifest.size()>65536u || source.content_hash.size()!=71u
                || mesh.artifact_key==std::array<std::uint64_t,4>{}
                || mesh.vertex_count<3u || mesh.vertex_count>limits.maximum_vertices
                || mesh.index_count<3u || mesh.index_count%3u || mesh.index_count>limits.maximum_indices
                || 48u+std::uint64_t(mesh.vertex_count)*44u+std::uint64_t(mesh.index_count)*4u>limits.maximum_bytes
                || !std::isfinite(mesh.coordinates.meters_per_unit) || mesh.coordinates.meters_per_unit<=0.0
                || mesh.coordinates.meters_per_unit>1.0e12) return {};
            for (const auto value:mesh.coordinates.source_origin_meters)
                if (!std::isfinite(value)||std::abs(value)>1.0e15) return {};
            const auto digest=[](std::string_view domain,std::string_view bytes) {
                return asset::texture::content_hash_hex(asset::texture::deterministic_content_hash(domain,
                    std::as_bytes(std::span{bytes.data(),bytes.size()})));
            };
            std::string bytes="epoch.planet.mesh-generation.v1\nsource="+source.content_hash
                +"\nmanifest="+digest("epoch.planet.manifest.v1",source.project_manifest)
                +"\nmesh="+asset::texture::content_hash_hex(asset::mesh::ContentHash{mesh.artifact_key})
                +"\nvertices="+std::to_string(mesh.vertex_count)+"\nindices="+std::to_string(mesh.index_count)
                +"\nmeters_per_unit="+real_text(mesh.coordinates.meters_per_unit)+"\norigin_meters=";
            for (std::size_t axis=0;axis<3u;++axis) {
                if(axis)bytes+=',';
                bytes+=real_text(mesh.coordinates.source_origin_meters[axis]);
            }
            bytes+='\n';bytes+=recipe;
            if(bytes.size()>4096u)return {};
            return MeshGenerationRecord{digest("epoch.planet.mesh-generation.v1",bytes)+".epoch_pgen",std::move(bytes)};
        }
        bool write_mesh_generation_record(const fs::path& root,const SourceGeneration& source,
            const Settings& settings,const MeshGenerationIdentity& mesh)
        {
            const auto record=mesh_generation_record(source,settings,mesh);
            const auto directory=record?project_directory(root,"Assets/Mesh",true):std::nullopt;
            if(!directory)return false;
            const auto path=*directory/record->filename;
            const auto existing=fs::symlink_status(path);
            if(existing.type()!=fs::file_type::not_found)
                return fs::is_regular_file(existing)&&!fs::is_symlink(existing)&&fs::hard_link_count(path)==1u
                    &&read_small(path,4096u)==record->bytes;
            std::error_code error;
            if(!platform::filesystem::exclusive_create_and_write(path,
                std::as_bytes(std::span{record->bytes.data(),record->bytes.size()}),error))return false;
            return read_small(path,4096u)==record->bytes;
        }

        bool write_mesh_generation_bodies(const fs::path& root,const SourceGeneration& source,
            const Settings& settings,const MeshGenerationIdentity& mesh,std::string_view bytes)
        {
            const auto record=mesh_generation_record(source,settings,mesh);
            const auto directory=record?project_directory(root,"Assets/Mesh",true):std::nullopt;
            if(!directory||bytes.empty()||bytes.size()>16384u)return false;
            auto filename=record->filename;filename.replace(filename.size()-10u,10u,"epoch_pbod");
            const auto path=*directory/filename;const auto status=fs::symlink_status(path);
            if(status.type()!=fs::file_type::not_found)
                return fs::is_regular_file(status)&&!fs::is_symlink(status)&&fs::hard_link_count(path)==1u
                    &&read_small(path,16384u)==bytes;
            std::error_code error;
            return platform::filesystem::exclusive_create_and_write(path,
                std::as_bytes(std::span{bytes.data(),bytes.size()}),error)&&read_small(path,16384u)==bytes;
        }

        std::optional<fs::path> generation_build_directory(const fs::path& root,std::string_view source_hash,
            const fs::path& engine_root,std::string_view water_hash={})
        {
            if(source_hash.size()!=71u||!source_hash.starts_with("sha256:"))return {};
            const auto project=project_directory(root,{},false),sdk=ordinary_directory(engine_root,{},false);
            if(!project||!sdk)return {};
            const std::string expected="epoch.planet.build.v1\nproject="+project->generic_string()
                +"\nsource="+std::string(source_hash)+"\nsdk="+sdk->generic_string()+"\nedition="
                +(EPOCH_EDITION_PUBLIC?"epoch.public.github.v1":"epoch.development.site.v1")+"\n"
                +(water_hash.empty()?std::string{}:"water_source="+std::string{water_hash}+"\n");
            if(expected.size()>8192u)return {};
            const auto key=asset::texture::content_hash_hex(asset::texture::deterministic_content_hash(
                "epoch.planet.build.v1",std::as_bytes(std::span{expected.data(),expected.size()})));
            const auto build=ordinary_directory(*sdk,fs::path{"Artifacts/Intermediate/planet"}/key.substr(0,12),true);
            if(!build)return {};
            const auto identity=*build/"build.identity";
            if(fs::exists(identity))return read_small(identity,8192u)==expected?build:std::nullopt;
            if(!fs::is_empty(*build))return {};
            std::error_code error;
            if(!platform::filesystem::exclusive_create_and_write(identity,std::as_bytes(std::span{expected.data(),expected.size()}),error))return {};
            return read_small(identity,8192u)==expected?build:std::nullopt;
        }

        std::string xml_escape(std::string_view text)
        {
            std::string out;
            for (const auto c : text)
                switch (c) {
                case '&': out += "&amp;"; break; case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break; case '"': out += "&quot;"; break;
                default: out += c;
                }
            return out;
        }

        bool source_has_runtime_module(const fs::path& project,
            const package_source::ProjectAttachment& source,const Settings& settings)
        {
            if(!supports_exploration(settings))return false;
            const auto status=fs::symlink_status(project/source.relative_source_root/"modules/planet.exploration_runtime.ixx");
            return fs::is_regular_file(status)&&!fs::is_symlink(status);
        }

        bool source_supports_water(const fs::path& planet,const fs::path& water)
        {
            for(const auto& path:{planet/"modules/water.planetary.ixx",water/"modules/water.fft.ixx"})
            {
                const auto status=fs::symlink_status(path);
                if(!fs::is_regular_file(status)||fs::is_symlink(status))return false;
            }
            return read_small(planet/"CMakeLists.txt",64u*1024u).find("EPOCH_PLANET_WATER_SOURCE_ROOT")!=std::string::npos;
        }

        bool write_dependency_fragments(const fs::path& project,
            const package_source::ProjectAttachment& dependency, const fs::path& cmake, const Settings& settings)
        {
            const auto packages = project_directory(project, "Packages", false);
            if (!packages) return false;
            const bool exploration = source_has_runtime_module(project,dependency,settings);
            const std::string runtimeOption = exploration ? "ON" : "OFF";
            package_source::ProjectAttachment water;
            if(settings.ocean_surface)
            {
                water=package_source::load_project_attachment(project,"fft_ocean");
                if(!water.source_attached())return false;
                if(!source_supports_water(project/dependency.relative_source_root,project/water.relative_source_root))return false;
            }
            const auto waterSource=settings.ocean_surface
                ?water.relative_source_root.lexically_relative("Packages").generic_string():std::string{};
            for(const auto c:waterSource)
                if(!(c>='a'&&c<='z')&&!(c>='0'&&c<='9')&&c!='/'&&c!='_'&&c!='-'&&c!='.')return false;
            // All variable paths here are derived from the verified, fixed
            // package ID/digest hierarchy, never from free-form UI text.
            const auto source = dependency.relative_source_root.lexically_relative("Packages").generic_string();
            for (const auto c : source)
                if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '/' && c != '_' && c != '-' && c != '.') return false;
            constexpr std::string_view physicalEdition = EPOCH_EDITION_PUBLIC ? "public" : "development";
            constexpr std::string_view editionId = EPOCH_EDITION_PUBLIC
                ? "epoch.public.github.v1" : "epoch.development.site.v1";
            const std::string stagingScript = "# EPOCH_MANAGED_GENERATED_FILE: planetary_provider_stage_v1\n"
                "set(expected_source \"" + dependency.content_hash + "\")\n"
                "set(expected_water \"" + water.content_hash + "\")\n"
                "set(expected_edition \"" + std::string{editionId} + "\")\n"
                "set(physical_edition \"" + std::string{physicalEdition} + "\")\n"
                + R"cmake(if(NOT EPOCH_PLANET_CONFIGURATION MATCHES "^(Debug|Release)$")
  message(FATAL_ERROR "The planetary provider configuration is invalid")
endif()
file(REAL_PATH "${CMAKE_CURRENT_LIST_DIR}/.." project_root)
function(require_ordinary path)
  get_filename_component(current "${path}" ABSOLUTE)
  while(NOT current STREQUAL "")
    if(IS_SYMLINK "${current}")
      message(FATAL_ERROR "The planetary runtime path is redirected")
    endif()
    if(EXISTS "${current}")
      file(REAL_PATH "${current}" resolved)
      set(requested "${current}")
      if(CMAKE_HOST_WIN32)
        string(TOLOWER "${requested}" requested)
        string(TOLOWER "${resolved}" resolved)
      endif()
      if(NOT requested STREQUAL resolved)
        message(FATAL_ERROR "The planetary runtime path is redirected")
      endif()
    endif()
    get_filename_component(parent "${current}" DIRECTORY)
    if(parent STREQUAL current)
      break()
    endif()
    set(current "${parent}")
  endwhile()
endfunction()
require_ordinary("${project_root}")
if(EPOCH_PLANET_BIND_LINK_CACHE OR EPOCH_PLANET_COPY_LINK_PROVIDER)
  require_ordinary("${EPOCH_PLANET_SDK}")
  file(REAL_PATH "${EPOCH_PLANET_SDK}" sdk)
  set(cache_parent "${sdk}/Artifacts/Intermediate/planet-link")
  get_filename_component(cache "${EPOCH_PLANET_BUILD}" ABSOLUTE)
  file(RELATIVE_PATH relative_cache "${cache_parent}" "${cache}")
  if(NOT relative_cache MATCHES "^[0-9A-Fa-f]+$" OR NOT IS_DIRECTORY "${sdk}/Engine")
    message(FATAL_ERROR "The planetary link cache is outside its owning SDK")
  endif()
  set(edition_header "${sdk}/Engine/include/epoch.edition.hpp")
  require_ordinary("${edition_header}")
  file(SIZE "${edition_header}" edition_size)
  if(edition_size GREATER 8192)
    message(FATAL_ERROR "The planetary SDK edition marker is invalid")
  endif()
  file(READ "${edition_header}" edition_bytes)
  if((physical_edition STREQUAL "public" AND NOT edition_bytes MATCHES "#define EPOCH_SOURCE_EDITION_PUBLIC 1")
     OR (physical_edition STREQUAL "development" AND NOT edition_bytes MATCHES "#define EPOCH_SOURCE_EDITION_PUBLIC 0"))
    message(FATAL_ERROR "The planetary link SDK belongs to another edition")
  endif()
  require_ordinary("${cache_parent}")
  file(MAKE_DIRECTORY "${cache_parent}")
  require_ordinary("${cache_parent}")
  require_ordinary("${cache_parent}/cache.lock")
  file(LOCK "${cache_parent}/cache.lock" GUARD PROCESS TIMEOUT 30)
  require_ordinary("${cache}")
  set(identity "epoch.planet.link-build.v1\nproject=${project_root}\nsdk=${sdk}\nsource=${expected_source}\nwater_source=${expected_water}\nedition=${expected_edition}\n")
  if(EXISTS "${cache}")
    if(NOT EXISTS "${cache}/build.identity")
      message(FATAL_ERROR "The planetary link cache has no owned identity")
    endif()
    require_ordinary("${cache}/build.identity")
    file(SIZE "${cache}/build.identity" identity_size)
    if(identity_size GREATER 8192)
      message(FATAL_ERROR "The planetary link cache identity is invalid")
    endif()
    file(READ "${cache}/build.identity" previous_identity)
    if(NOT previous_identity STREQUAL identity)
      message(FATAL_ERROR "The planetary link cache belongs to different project/source inputs")
    endif()
  else()
    file(MAKE_DIRECTORY "${cache}")
    require_ordinary("${cache}")
    file(CONFIGURE OUTPUT "${cache}/build.identity" CONTENT "${identity}" @ONLY NEWLINE_STYLE UNIX)
  endif()
  if(EPOCH_PLANET_COPY_LINK_PROVIDER)
    if(NOT EPOCH_PLANET_BUILD_CONFIGURATION MATCHES "^(Debug|Release|RelWithDebInfo)$")
      message(FATAL_ERROR "The planetary link configuration is invalid")
    endif()
    get_filename_component(input "${EPOCH_PLANET_STAGE_INPUT}" ABSOLUTE)
    file(RELATIVE_PATH relative_input "${project_root}" "${input}")
    if(NOT relative_input MATCHES "^build/pstage/[0-9a-f]+$" OR EXISTS "${input}")
      message(FATAL_ERROR "The planetary provider input is not a new owned project path")
    endif()
    set(completed "${cache}/${EPOCH_PLANET_BUILD_CONFIGURATION}/epoch_planet_mesh_export.exe")
    require_ordinary("${completed}")
    if(NOT EXISTS "${completed}" OR IS_DIRECTORY "${completed}")
      message(FATAL_ERROR "The completed planetary provider is unavailable")
    endif()
    require_ordinary("${input}")
    file(MAKE_DIRECTORY "${input}")
    require_ordinary("${input}")
    file(COPY_FILE "${completed}" "${input}/epoch_planet_mesh_export.exe")
  endif()
  return()
endif()
require_ordinary("${EPOCH_PLANET_PROVIDER}")
file(REAL_PATH "${EPOCH_PLANET_PROVIDER}" provider)
file(RELATIVE_PATH provider_relative "${project_root}" "${provider}")
if(IS_ABSOLUTE "${provider_relative}" OR provider_relative MATCHES "^\.\.(/|$)")
  message(FATAL_ERROR "The planetary provider must come from this project's build")
endif()
if(CMAKE_HOST_WIN32)
  set(platform "windows")
  set(filename "epoch_planet_mesh_export.exe")
elseif(CMAKE_HOST_APPLE)
  set(platform "macos")
  set(filename "epoch_planet_mesh_export")
else()
  set(platform "linux")
  set(filename "epoch_planet_mesh_export")
endif()
get_filename_component(actual_filename "${provider}" NAME)
if(NOT actual_filename STREQUAL filename OR IS_DIRECTORY "${provider}")
  message(FATAL_ERROR "The planetary provider has an unexpected filename")
endif()
file(SIZE "${provider}" producer_size)
if(producer_size LESS 1 OR producer_size GREATER 536870912)
  message(FATAL_ERROR "The planetary provider exceeds its executable limit")
endif()
string(SUBSTRING "${expected_source}" 7 12 source_version)
if(NOT expected_water STREQUAL "")
  string(SUBSTRING "${expected_water}" 7 12 water_version)
  string(APPEND source_version "-${water_version}")
endif()
set(runtime "${CMAKE_CURRENT_LIST_DIR}/.runtime/planetary_terrain/${physical_edition}/${EPOCH_PLANET_CONFIGURATION}-${source_version}")
require_ordinary("${runtime}")
if(expected_water STREQUAL "")
  set(prefix "epoch.planet.provider.v1\nsource=${expected_source}\n")
else()
  set(prefix "epoch.planet.provider.v2\nsource=${expected_source}\nwater_source=${expected_water}\n")
endif()
string(APPEND prefix "edition=${expected_edition}\nprotocol=epoch.planet.stream.v1\nconfiguration=${EPOCH_PLANET_CONFIGURATION}\nplatform=${platform}\nfile=${filename}\n")
if(EXISTS "${runtime}")
  file(GLOB existing RELATIVE "${runtime}" "${runtime}/*" "${runtime}/.*")
  foreach(name IN LISTS existing)
    if(NOT name STREQUAL filename AND NOT name STREQUAL "provider.identity")
      message(FATAL_ERROR "Unmanaged files exist in the planetary runtime directory")
    endif()
    require_ordinary("${runtime}/${name}")
  endforeach()
  if(existing)
    if(NOT EXISTS "${runtime}/provider.identity" OR NOT EXISTS "${runtime}/${filename}")
      message(FATAL_ERROR "The existing planetary runtime is incomplete")
    endif()
    file(SIZE "${runtime}/provider.identity" identity_size)
    if(identity_size GREATER 1024)
      message(FATAL_ERROR "The existing planetary runtime receipt is invalid")
    endif()
    file(SIZE "${runtime}/${filename}" old_size)
    file(SHA256 "${runtime}/${filename}" old_hash)
    file(READ "${runtime}/provider.identity" previous)
    set(expected_previous "${prefix}size=${old_size}\nsha256=${old_hash}\n")
    if(NOT previous STREQUAL expected_previous)
      message(FATAL_ERROR "The existing planetary runtime is not the admitted managed provider")
    endif()
  endif()
endif()
file(MAKE_DIRECTORY "${runtime}")
require_ordinary("${runtime}")
string(RANDOM LENGTH 24 ALPHABET 0123456789abcdef nonce)
set(temporary "${runtime}/provider-${nonce}.tmp")
set(receipt_temporary "${runtime}/receipt-${nonce}.tmp")
if(EXISTS "${temporary}" OR EXISTS "${receipt_temporary}")
  message(FATAL_ERROR "The planetary runtime staging paths are already occupied")
endif()
file(COPY_FILE "${provider}" "${temporary}")
file(SHA256 "${temporary}" producer_hash)
file(CONFIGURE OUTPUT "${receipt_temporary}" CONTENT "${prefix}size=${producer_size}\nsha256=${producer_hash}\n" @ONLY NEWLINE_STYLE UNIX)
file(RENAME "${temporary}" "${runtime}/${filename}")
file(RENAME "${receipt_temporary}" "${runtime}/provider.identity")
)cmake";
            if (!write_owned(*packages / "epoch.stage_planet.cmake", stagingScript,
                "# EPOCH_MANAGED_GENERATED_FILE: planetary_provider_stage_v1\n")) return false;
            const std::string cmakeText = "# EPOCH_MANAGED_GENERATED_FILE: planetary_dependency_v1\n"
                "set(EPOCH_PLANET_BUILD_MESH_EXPORT ON)\n"
                "set(EPOCH_PLANET_BUILD_RUNTIME_CONTROLLER " + runtimeOption + ")\n"
                "set(EPOCH_ENGINE_SOURCE_ROOT \"${EPOCH_REPO_ROOT}\")\n"
                "set(EPOCH_PLANET_BUILD_WATER " + (settings.ocean_surface?std::string{"ON"}:std::string{"OFF"}) + " CACHE BOOL \"\" FORCE)\n"
                "set(EPOCH_PLANET_WATER_SOURCE_ROOT \"" + (settings.ocean_surface?"${CMAKE_CURRENT_LIST_DIR}/"+waterSource:std::string{}) + "\" CACHE PATH \"\" FORCE)\n"
                "if(NOT TARGET epoch_extension_planetary_core)\n"
                "  add_subdirectory(\"${CMAKE_CURRENT_LIST_DIR}/" + source
                + "\" \"${CMAKE_BINARY_DIR}/planetary-extension\" EXCLUDE_FROM_ALL)\n"
                "endif()\n"
                "target_link_libraries(${target} PRIVATE EpochEngineExtensions::PlanetaryCore)\n"
                + (exploration ? std::string{
                    "target_link_libraries(${target} PRIVATE epoch_extension_planetary_runtime)\n"
                    "target_compile_definitions(${target} PRIVATE EPOCH_PROJECT_PLANETARY_RUNTIME=1)\n"} : std::string{}) +
                "add_dependencies(${target} epoch_planet_mesh_export)\n"
                "add_custom_command(TARGET ${target} POST_BUILD COMMAND \"${CMAKE_COMMAND}\"\n"
                "  \"-DEPOCH_PLANET_PROVIDER=$<TARGET_FILE:epoch_planet_mesh_export>\"\n"
                "  \"-DEPOCH_PLANET_CONFIGURATION=$<IF:$<CONFIG:Debug>,Debug,Release>\"\n"
                "  -P \"${CMAKE_CURRENT_LIST_DIR}/epoch.stage_planet.cmake\" VERBATIM)\n";
            if (!write_owned(*packages / "epoch.dependencies.cmake", cmakeText,
                "# EPOCH_MANAGED_GENERATED_FILE: planetary_dependency_v1\n")) return false;
#ifdef _WIN32
            // Project linking has its own compiler cache. A copied project's
            // first build must not create an unbound Editor-generation cache.
            const std::string linkInputs=dependency.content_hash+"|"+water.content_hash+"|"+std::string{editionId};
            const std::string props = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                "<!-- EPOCH_MANAGED_GENERATED_FILE: planetary_dependency_v1 -->\n"
                "<Project xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n"
                "  <PropertyGroup>\n"
                "    <EpochPlanetConfiguration>$(Configuration)</EpochPlanetConfiguration>\n"
                "    <EpochPlanetConfiguration Condition=\"'$(Configuration)'=='ReleaseWithDebugInfo'\">RelWithDebInfo</EpochPlanetConfiguration>\n"
                "    <EpochPlanetRuntimeConfiguration>Release</EpochPlanetRuntimeConfiguration>\n"
                "    <EpochPlanetRuntimeConfiguration Condition=\"'$(Configuration)'=='Debug'\">Debug</EpochPlanetRuntimeConfiguration>\n"
                "    <EpochPlanetLinkKey>$([MSBuild]::StableStringHash('$(MSBuildThisFileDirectory)|$(EpochRepoRoot)|"+xml_escape(linkInputs)+"').ToString('X8'))</EpochPlanetLinkKey>\n"
                "    <EpochPlanetBuild>$(EpochRepoRoot)Artifacts\\Intermediate\\planet-link\\$(EpochPlanetLinkKey)</EpochPlanetBuild>\n"
                "    <EpochPlanetStageInput>$(MSBuildThisFileDirectory)..\\build\\pstage\\$([System.Guid]::NewGuid().ToString('N'))</EpochPlanetStageInput>\n"
                "  </PropertyGroup>\n"
                "  <ItemDefinitionGroup>"
                + (exploration ? std::string{"<ClCompile><PreprocessorDefinitions>EPOCH_PROJECT_PLANETARY_RUNTIME=1;%(PreprocessorDefinitions)</PreprocessorDefinitions></ClCompile>"} : std::string{}) +
                "<Link><AdditionalDependencies>"
                + (exploration ? std::string{"$(EpochPlanetBuild)\\$(EpochPlanetConfiguration)\\epoch_extension_planetary_runtime.lib;"} : std::string{}) +
                (exploration && settings.ocean_surface ? std::string{
                    "$(EpochPlanetBuild)\\$(EpochPlanetConfiguration)\\epoch_extension_planetary_water.lib;"
                    "$(EpochPlanetBuild)\\water-extension\\$(EpochPlanetConfiguration)\\epoch_extension_water_fft.lib;"
                    "$(EpochPlanetBuild)\\water-extension\\$(EpochPlanetConfiguration)\\epoch_extension_fft_ocean.lib;"
                    "$(EpochPlanetBuild)\\$(EpochPlanetConfiguration)\\epoch_water_wave.lib;"} : std::string{}) +
                "$(EpochPlanetBuild)\\$(EpochPlanetConfiguration)\\epoch_extension_planetary_core.lib;%(AdditionalDependencies)</AdditionalDependencies></Link></ItemDefinitionGroup>\n"
                "  <Target Name=\"EpochBuildPlanetaryDependency\" BeforeTargets=\"Link\">\n"
                "    <Exec Command=\"&quot;"+xml_escape(cmake.generic_string())
                +"&quot; -DEPOCH_PLANET_BIND_LINK_CACHE=ON -DEPOCH_PLANET_CONFIGURATION=$(EpochPlanetRuntimeConfiguration) -DEPOCH_PLANET_SDK=&quot;$(EpochRepoRoot).&quot; -DEPOCH_PLANET_BUILD=&quot;$(EpochPlanetBuild)&quot; -P &quot;$(MSBuildThisFileDirectory)epoch.stage_planet.cmake&quot;\" />\n"
                "    <Exec Command=\"&quot;" + xml_escape(cmake.generic_string())
                + "&quot; -S &quot;$(MSBuildThisFileDirectory)" + xml_escape(source)
                + "&quot; -B &quot;$(EpochPlanetBuild)&quot; -G &quot;Visual Studio 17 2022&quot; -A x64 -DBUILD_TESTING=OFF -DEPOCH_PLANET_BUILD_MESH_EXPORT=ON -DEPOCH_PLANET_BUILD_RUNTIME_CONTROLLER="
                + runtimeOption + " -DEPOCH_ENGINE_SOURCE_ROOT=&quot;$(EpochRepoRoot).&quot; -DEPOCH_PLANET_BUILD_WATER="
                + (settings.ocean_surface?std::string{"ON"}:std::string{"OFF"})
                + " -DEPOCH_PLANET_WATER_SOURCE_ROOT=&quot;"
                + (settings.ocean_surface?"$(MSBuildThisFileDirectory)"+xml_escape(waterSource):std::string{}) + "&quot;\" />\n"
                "    <Exec Command=\"&quot;" + xml_escape(cmake.generic_string())
                + "&quot; --build &quot;$(EpochPlanetBuild)&quot; --config &quot;$(EpochPlanetConfiguration)&quot; --target "
                + (exploration ? std::string{"epoch_extension_planetary_runtime"} : std::string{"epoch_extension_planetary_core"})
                + " epoch_planet_mesh_export --parallel 1\" />\n"
                "    <Exec Command=\"&quot;"+xml_escape(cmake.generic_string())
                +"&quot; -DEPOCH_PLANET_COPY_LINK_PROVIDER=ON -DEPOCH_PLANET_CONFIGURATION=$(EpochPlanetRuntimeConfiguration) -DEPOCH_PLANET_SDK=&quot;$(EpochRepoRoot).&quot; -DEPOCH_PLANET_BUILD=&quot;$(EpochPlanetBuild)&quot; -DEPOCH_PLANET_BUILD_CONFIGURATION=$(EpochPlanetConfiguration) -DEPOCH_PLANET_STAGE_INPUT=&quot;$(EpochPlanetStageInput)&quot; -P &quot;$(MSBuildThisFileDirectory)epoch.stage_planet.cmake&quot;\" />\n"
                "    <Exec Command=\"&quot;" + xml_escape(cmake.generic_string())
                + "&quot; -DEPOCH_PLANET_PROVIDER=&quot;$(EpochPlanetStageInput)/epoch_planet_mesh_export.exe&quot;"
                " -DEPOCH_PLANET_CONFIGURATION=$(EpochPlanetRuntimeConfiguration) -P &quot;$(MSBuildThisFileDirectory)epoch.stage_planet.cmake&quot;\" ConsoleToMSBuild=\"true\" IgnoreExitCode=\"true\" IgnoreStandardErrorWarningFormat=\"true\">\n"
                "      <Output TaskParameter=\"ExitCode\" PropertyName=\"EpochPlanetStageExitCode\" />\n"
                "      <Output TaskParameter=\"ConsoleOutput\" ItemName=\"EpochPlanetStageOutput\" />\n"
                "    </Exec>\n"
                "    <Error Condition=\"'$(EpochPlanetStageExitCode)'!='0'\" Text=\"Planetary provider staging failed: @(EpochPlanetStageOutput, ' | ')\" />\n"
                "  </Target>\n</Project>\n";
            if (!write_owned(*packages / "epoch.dependencies.props", props,
                "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<!-- EPOCH_MANAGED_GENERATED_FILE: planetary_dependency_v1 -->")) return false;
#else
            (void)cmake;
#endif
            return true;
        }
    }

    std::optional<Settings> load_settings(const fs::path& project_root)
    {
        try {
            const auto directory = project_directory(project_root, "Assets/Worlds", false);
            return directory ? decode_settings(read_small(*directory / "Planet.epoch_planet", 1024u)) : std::nullopt;
        } catch (...) { return {}; }
    }
    namespace
    {
        // Observe optional authoring bytes without adopting redirected files.
        // A job may intentionally use a draft, but must not replace another
        // window's recipe edits made after its request was admitted.
        std::optional<std::string> recipe_guard(const fs::path& project_root)
        {
            const auto root=project_directory(project_root,{},false);
            if(!root)return {};
            for(const auto relative:{fs::path{"Assets"},fs::path{"Assets/Worlds"}})
            {
                const auto status=fs::symlink_status(*root/relative);
                if(status.type()==fs::file_type::not_found)return std::string{};
                if(!project_directory(*root,relative,false))return {};
            }
            const auto path=*root/recipe_relative_path;
            const auto status=fs::symlink_status(path);
            if(status.type()==fs::file_type::not_found)return std::string{};
            if(!fs::is_regular_file(status)||fs::is_symlink(status)||fs::hard_link_count(path)!=1u)return {};
            auto bytes=read_small(path,1024u);
            return bytes.empty()?std::nullopt:std::optional<std::string>{std::move(bytes)};
        }

        std::string water_source_requirement(const fs::path& project_root,const Settings& settings,
            std::stop_token cancellation={})
        {
            if(!settings.ocean_surface)return {};
            const auto source=package_source::load_project_attachment(project_root,"fft_ocean",cancellation);
            if(source.source_attached())return {};
            return "Ocean On requires approved Water (FFT Ocean) source in this project. Choose Save Settings or Build & Generate in Project to review and include Water automatically, or choose Ocean Off for a dry planet. "
                +source.message;
        }

        bool save_settings_observed(const fs::path& project_root,const Settings& settings,
            const std::optional<std::string>* observed=nullptr,std::stop_token cancellation={})
        {
            if(cancellation.stop_requested())return false;
            if(!water_source_requirement(project_root,settings,cancellation).empty())return false;
            const auto text=encode_settings(settings);
            if(text.empty())return false;
            const auto directory=project_directory(project_root,"Assets/Worlds",true);
            if(!directory)return false;
            auto lease=platform::filesystem::ExclusiveFileLease::acquire(*directory/"Planet.epoch_planet.lock");
            if(!lease||cancellation.stop_requested())return false;
            const auto currentRecipe=recipe_guard(project_root);
            if(!currentRecipe||(observed&&currentRecipe!=*observed))return false;
            return write_owned(*directory/"Planet.epoch_planet",text,"epoch.planet.recipe.v");
        }

        bool supported_session_envelope(std::string_view bytes) noexcept
        {
            return bytes.size()<=16u*1024u&&(bytes.starts_with("epoch.planet.exploration.v1\n")
                ||bytes.starts_with("epoch.planet.exploration.v2\n")||bytes.starts_with("epoch.planet.exploration.v3\n")
                ||bytes.starts_with("epoch.planet.exploration.v4\n"));
        }
    }

    bool save_settings(const fs::path& project_root,const Settings& settings)
    {
        try{return save_settings_observed(project_root,settings);}
        catch(...){return false;}
    }

    RuntimeRequirement runtime_requirement(const fs::path& project_root)
    {
        RuntimeRequirement result;
        try
        {
            const auto root=project_directory(project_root,{},false);
            if(!root){result.message="Run blocked: the active project folder is invalid.";return result;}
            for(const auto relative:{fs::path{"Assets"},fs::path{"Assets/Worlds"}})
            {
                const auto status=fs::symlink_status(*root/relative);
                if(status.type()==fs::file_type::not_found){result.ok=true;return result;}
                if(!project_directory(*root,relative,false))
                {result.message="Run blocked: the planetary recipe folder is redirected or invalid.";return result;}
            }
            const auto path=*root/recipe_relative_path;
            const auto status=fs::symlink_status(path);
            if(status.type()==fs::file_type::not_found){result.ok=true;return result;}
            if(!fs::is_regular_file(status)||fs::is_symlink(status)||fs::hard_link_count(path)!=1u)
            {result.message="Run blocked: the planetary recipe is redirected or invalid.";return result;}
            const auto settings=load_settings(*root);
            if(!settings){result.message="Run blocked: the planetary recipe cannot be read or validated.";return result;}
            if(const auto required=water_source_requirement(*root,*settings);!required.empty())
            {result.message="Run blocked: "+required;return result;}
            if(!supports_exploration(*settings)){result.ok=true;return result;}
            const auto source=package_source::load_project_attachment(*root,"planetary_terrain");
            if(!source.source_attached())
            {result.message="Run blocked: "+source.message;return result;}
            if(settings->ocean_surface)
            {
                const auto water=package_source::load_project_attachment(*root,"fft_ocean");
                if(!water.source_attached()||!source_supports_water(*root/source.relative_source_root,*root/water.relative_source_root))
                {result.message="Run blocked: animated oceans require compatible reviewed Planet and Water source. Explicitly review and attach both updates; saved source and terrain are retained.";return result;}
            }
            const auto generation=capture_source_generation(*root,source.content_hash);
            if(!generation||!source_generation_is_current(*root,*generation))
            {result.message="Run blocked: the approved planetary source changed.";return result;}
            if(settings->original_sunlight&&!source_supports_original_sunlight(*root/source.relative_source_root))
            {result.message="Run blocked: original planetary sunlight needs compatible reviewed Planetary source. Existing lighting and source are retained.";return result;}
            if(settings->mode=="planet"&&!source_supports_runtime_scope(*root/source.relative_source_root))
            {result.message="Run blocked: Planet mode needs compatible reviewed Planetary source. Review and attach the updated source so Run preserves the selected body; saved source and terrain are retained.";return result;}
            if(requires_size_controls(*settings)&&(!source_supports_size_controls(*root/source.relative_source_root)
                ||!source_supports_mesh_frame(*root/source.relative_source_root)))
            {result.message="Run blocked: the approved Planetary source predates world-size controls. Review and attach compatible source before building.";return result;}
            result.requires_project_runtime=source_has_runtime_module(*root,source,*settings);
            result.ok=true;
            if(result.requires_project_runtime)
                result.message="Planet exploration uses the compiled project runtime; building or launching that project now.";
            return result;
        }
        catch(...){result.message="Run blocked: the planetary runtime requirement could not be verified.";return result;}
    }

    Result prepare_runtime_dependency(const fs::path& project_root)
    {
        Result result;
        const auto required=runtime_requirement(project_root);
        if(!required.ok){result.message=required.message;return result;}
        if(!required.requires_project_runtime){result.ok=true;return result;}
        try
        {
            const auto settings=load_settings(project_root);
            if(!settings||!supports_exploration(*settings))
            {result.message="Planet exploration is no longer selected or its settings changed. No build files were changed.";return result;}
            const auto source=package_source::load_project_attachment(project_root,"planetary_terrain");
            if(!source.source_attached()||!source_has_runtime_module(project_root,source,*settings))
            {result.message="The approved planetary source no longer provides the exploration runtime. No build files were changed.";return result;}
            const auto generation=capture_source_generation(project_root,source.content_hash);
            const auto cmake=find_tool("cmake");
            if(!generation||!cmake||!source_generation_is_current(project_root,*generation))
            {result.message="The approved planetary source or CMake tool is unavailable for Run.";return result;}
            if(!write_dependency_fragments(project_root,source,*cmake,*settings)
                ||load_settings(project_root)!=settings||!source_generation_is_current(project_root,*generation))
            {result.message="The planetary link settings could not be refreshed safely. Existing authoring files were retained.";return result;}
            result.ok=true;
            result.message="The approved planetary exploration dependency is ready for the project build.";
        }
        catch(...){result.message="The planetary runtime dependency could not be prepared. No provider was launched.";}
        return result;
    }

    SessionFile load_exploration_session(const fs::path& project_root)
    {
        SessionFile result;
        try
        {
            const auto directory = project_directory(project_root, "Assets/Worlds", false);
            if (!directory) { result.message = "The world's saved-state folder is unavailable."; return result; }
            const auto path = *directory / "Planet.epoch_exploration";
            const auto status = fs::symlink_status(path);
            if (status.type() == fs::file_type::not_found)
            { result.ok = true; return result; }
            result.present = true;
            if (!fs::is_regular_file(status) || fs::is_symlink(status) || fs::hard_link_count(path) != 1u)
            { result.message = "The saved planetary session is redirected or is not an ordinary file."; return result; }
            result.bytes = read_small(path, 16u * 1024u);
            result.ok = supported_session_envelope(result.bytes);
            if (!result.ok) result.message = "The saved planetary session is damaged or uses an unsupported format. It was retained.";
        }
        catch (...) { result.message = "The planetary session could not be read. It was retained."; }
        return result;
    }

    SessionFile save_exploration_session(const fs::path& project_root,
        const SourceGeneration& admitted, std::string_view bytes, const SessionFile& previous)
    {
        SessionFile result;
        try
        {
            if (!previous.ok || !supported_session_envelope(bytes)
                || !source_generation_is_current(project_root, admitted))
            { result.message = "The planetary session or its approved source changed. The previous save was retained."; return result; }
            const auto directory = project_directory(project_root, "Assets/Worlds", false);
            if (!directory) { result.message = "The world's saved-state folder is unavailable."; return result; }
            auto lease = platform::filesystem::ExclusiveFileLease::acquire(*directory / "Planet.epoch_exploration.lock");
            if (!lease) { result.message = "Another window is saving this planetary session. Try Save again."; return result; }
            const auto current = load_exploration_session(project_root);
            if (!current.ok || current.present != previous.present || current.bytes != previous.bytes)
            { result.message = "The planetary session was changed by another window. Reopen it before saving."; return result; }
            if (!write_owned(*directory / "Planet.epoch_exploration", bytes, "epoch.planet.exploration.v"))
            { result.message = "The planetary session could not be saved. The previous save was retained."; return result; }
            result.ok = true;
            result.present = true;
            result.bytes = bytes;
            result.message = "Camera, world selection and simulation time saved.";
        }
        catch (...) { result.message = "The planetary session could not be saved. The previous save was retained."; }
        return result;
    }

    std::optional<SourceGeneration> capture_source_generation(const fs::path& project_root,
        std::string_view admitted_content_hash)
    {
        try
        {
            if (!admitted_content_hash.starts_with("sha256:") || admitted_content_hash.size() != 71u) return {};
            const auto root = project_directory(project_root, {}, false);
            if (!root) return {};
            auto manifest = read_small(*root / "project.epoch.json", 65536u);
            if (manifest.empty()) return {};
            return SourceGeneration{*root, std::move(manifest), std::string{admitted_content_hash}};
        }
        catch (...) { return {}; }
    }

    bool source_generation_is_current(const fs::path& project_root,
        const SourceGeneration& admitted, std::stop_token cancellation)
    {
        try
        {
            if (cancellation.stop_requested() || admitted.project_manifest.empty()) return false;
            const auto root = project_directory(project_root, {}, false);
            if (!root || *root != admitted.project_root
                || read_small(*root / "project.epoch.json", 65536u) != admitted.project_manifest) return false;
            const auto current = package_source::load_project_attachment(*root, "planetary_terrain", cancellation);
            return current.source_attached() && current.content_hash == admitted.content_hash;
        }
        catch (...) { return false; }
    }

    bool mesh_generation_is_current(const fs::path& project_root,const SourceGeneration& source,
        const Settings& settings,const MeshGenerationIdentity& mesh)
    {
        try {
            if(load_settings(project_root)!=settings||!source_generation_is_current(project_root,source))return false;
            const auto record=mesh_generation_record(source,settings,mesh);
            const auto directory=record?project_directory(project_root,"Assets/Mesh",false):std::nullopt;
            if(!directory)return false;
            const auto path=*directory/record->filename;
            const auto status=fs::symlink_status(path);
            return fs::is_regular_file(status)&&!fs::is_symlink(status)&&fs::hard_link_count(path)==1u
                &&read_small(path,4096u)==record->bytes;
        }catch(...){return false;}
    }

    std::optional<Settings> mesh_generation_settings(const fs::path& project_root,
        const SourceGeneration& source,const MeshGenerationIdentity& mesh)
    {
        try {
            if(!source_generation_is_current(project_root,source))return {};
            if(const auto saved=load_settings(project_root);
                saved&&mesh_generation_is_current(project_root,source,*saved,mesh))return saved;
            const auto directory=project_directory(project_root,"Assets/Mesh",false);
            if(!directory)return {};
            // Only refresh/reopen scans immutable generation receipts. Streaming
            // uses the already selected exact identity, never a directory scan.
            std::optional<Settings> selected;std::string selectedName;std::size_t count{};
            for(const auto& entry:fs::directory_iterator(*directory)) {
                if(++count>4096u)return {};
                if(entry.path().extension()!=".epoch_pgen")continue;
                const auto status=entry.symlink_status();
                if(!fs::is_regular_file(status)||fs::is_symlink(status)||fs::hard_link_count(entry.path())!=1u)continue;
                const auto bytes=read_small(entry.path(),4096u);
                const auto start=bytes.find("\nepoch.planet.recipe.");
                if(start==std::string::npos)continue;
                const auto settings=decode_settings(std::string_view{bytes}.substr(start+1u));
                const auto record=settings?mesh_generation_record(source,*settings,mesh):std::nullopt;
                const auto name=entry.path().filename().string();
                if(record&&record->filename==name&&record->bytes==bytes&&(!selected||name<selectedName))
                {selected=*settings;selectedName=name;}
            }
            return selected;
        }catch(...){return {};}
    }

    std::vector<GeneratedBody> mesh_generation_bodies(const fs::path& project_root,
        const SourceGeneration& source,const MeshGenerationIdentity& mesh,const asset::mesh::Artifact& artifact)
    {
        try {
            const auto settings=mesh_generation_settings(project_root,source,mesh);
            const auto record=settings?mesh_generation_record(source,*settings,mesh):std::nullopt;
            const auto directory=record?project_directory(project_root,"Assets/Mesh",false):std::nullopt;
            if(!directory)return {};
            auto filename=record->filename;filename.replace(filename.size()-10u,10u,"epoch_pbod");
            const auto path=*directory/filename;const auto status=fs::symlink_status(path);
            if(!fs::is_regular_file(status)||fs::is_symlink(status)||fs::hard_link_count(path)!=1u)return {};
            return admit_generated_body_inventory(read_small(path,16384u),*settings,mesh,artifact);
        }catch(...){return {};}
    }

    Result generate(const fs::path& project_root, const fs::path& provider_source_root,
        const fs::path& engine_source_root, Settings settings, std::stop_token cancellation)
    {
        Result result;
        try
        {
            if (!valid_settings(settings)) { result.message = "Terrain settings exceed the current preview budget."; return result; }
            if (cancellation.stop_requested()) { result.cancelled = true; result.message = "Terrain generation cancelled."; return result; }
            if(const auto required=water_source_requirement(project_root,settings,cancellation);!required.empty())
            {result.cancelled=cancellation.stop_requested();result.message=required;return result;}
            if (!admit_preview_memory(settings, result)) return result;
            const auto dependency = package_source::load_project_attachment(project_root, "planetary_terrain", cancellation);
            if (!dependency.source_attached()) {
                result.cancelled = dependency.code == package_source::Code::cancelled;
                result.message = dependency.message;
                return result;
            }
            const auto admitted = capture_source_generation(project_root, dependency.content_hash);
            if (!admitted) { result.message = "The terrain request has no stable project identity."; return result; }
            const auto observedRecipe=recipe_guard(project_root);
            if(!observedRecipe){result.message="The saved terrain settings are unavailable or redirected.";return result;}
            const auto source = fs::canonical(project_root / dependency.relative_source_root);
            if (source != fs::canonical(provider_source_root)) { result.message = "Planetary source differs from the approved project dependency."; return result; }
            package_source::ProjectAttachment water;
            fs::path waterRoot;
            if(settings.ocean_surface)
            {
                water=package_source::load_project_attachment(project_root,"fft_ocean",cancellation);
                if(!water.source_attached())
                {result.cancelled=cancellation.stop_requested();result.message="Approved Water source changed before the planetary build.";return result;}
                waterRoot=fs::canonical(project_root/water.relative_source_root);
                if(!source_supports_water(source,waterRoot))
                {result.message="Animated oceans require compatible reviewed Planet and Water source. Review and explicitly attach both updates; existing source and terrain are retained.";return result;}
            }
            if(requires_lod_controls(settings)&&!source_supports_lod_controls(source))
            { result.message="This attached Planetary version predates screen-detail controls. Review and explicitly attach the newer extension source, then rebuild; the existing version and terrain were retained.";return result; }
            if(requires_size_controls(settings)&&!source_supports_size_controls(source))
            {result.message="This attached Planetary version predates world-size controls. Review and explicitly attach compatible source before rebuilding; existing terrain was retained.";return result;}
            if(settings.original_sunlight&&!source_supports_original_sunlight(source))
            {result.message="Original planetary sunlight needs compatible reviewed Planetary source. Review and attach the updated extension; existing source and lighting were retained.";return result;}
            const bool meshFrame=source_supports_mesh_frame(source);
            if(settings.stream_lod&&settings.maximum_level>=20u
                &&read_small(source/"tools/planet.stream_export.cpp",64u*1024u).find("epoch.planet.stream-frame.v1")==std::string::npos)
            {result.message="Fine LOD20 requires the metric-coordinate Planetary update. Review and attach that source, then build again; existing terrain was retained.";return result;}
            if(requires_size_controls(settings)&&!meshFrame)
            {result.message="This attached Planetary version has no verified mesh-coordinate receipt. Existing terrain was retained.";return result;}
            const auto cmake = find_tool("cmake");
            auto python = find_tool("python");
            if (!python) python = find_tool("python3");
            if (!cmake || !python) { result.message = "CMake and Python build tools must be available before building this extension."; return result; }
            const auto jobs = project_directory(project_root, "build/planet-jobs", true);
            if (!jobs) { result.message = "The project job directory is unavailable or redirected."; return result; }
            const auto runDirectory = *jobs / (std::to_string(Clock::now().time_since_epoch().count()) + "-" + std::to_string(++sequence));
            if (!fs::create_directory(runDirectory)) { result.message = "Could not reserve terrain job output."; return result; }
            result.build_log = runDirectory / "build.log";
            const auto preflightEnvironment = process::prepare_workspace_environment(runDirectory);
            if (!preflightEnvironment) { result.message = "Could not prepare the terrain worker environment."; return result; }
            const auto started = Clock::now();
            // Same complete source/edition gate used by ordinary generated
            // projects. Public cannot borrow a Development codec/build tree.
            if (!execute(*python, {(engine_source_root / "Tools/build/verify_epoch_project_source.py").string(),
                "--root", engine_source_root.string(), "--edition",
#if EPOCH_EDITION_PUBLIC
                "epoch.public.github.v1"
#else
                "epoch.development.site.v1"
#endif
                }, runDirectory, result.build_log, cancellation, *preflightEnvironment, 60'000'000'000ull, result)) return result;
            // Validate the SDK/edition first. Both compiler intermediates and
            // compiler TEMP must be short, even for deeply nested projects.
            const auto build = generation_build_directory(project_root,dependency.content_hash,engine_source_root,water.content_hash);
            if (!build) { result.message = "The short planetary build cache is unavailable, redirected or bound to other inputs. Existing caches were retained."; return result; }
            const auto environment=process::prepare_workspace_environment(*build);
            if(!environment){result.message="Could not prepare the short terrain compiler environment.";return result;}
            std::vector<std::string> configure{"-S", source.string(), "-B", build->string(),
                "-DBUILD_TESTING=OFF", "-DEPOCH_PLANET_BUILD_MESH_EXPORT=ON",
                "-DEPOCH_ENGINE_SOURCE_ROOT=" + engine_source_root.generic_string(),
                std::string{"-DEPOCH_PLANET_BUILD_WATER="}+(settings.ocean_surface?"ON":"OFF"),
                "-DEPOCH_PLANET_WATER_SOURCE_ROOT="+waterRoot.generic_string()};
#ifdef _WIN32
            configure.insert(configure.end(), {"-G", "Visual Studio 17 2022", "-A", "x64"});
#else
            configure.insert(configure.end(), {"-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"});
#endif
            if (!execute(*cmake, std::move(configure), *build, result.build_log, cancellation,
                *environment, 120'000'000'000ull, result)) return result;
            if (!execute(*cmake, {"--build", build->string(), "--config", "Release", "--target", "epoch_planet_mesh_export", "--parallel", "1"},
                *build, result.build_log, cancellation, *environment, 600'000'000'000ull, result)) return result;
            result.build_seconds = std::chrono::duration<double>(Clock::now() - started).count();
            // Recheck source after compilation before executing its product.
            if (!source_generation_is_current(project_root, *admitted, cancellation))
            { result.cancelled = cancellation.stop_requested(); result.message = "The project or planetary source changed during the build. Review the dependency before retrying."; return result; }
            if(settings.ocean_surface)
            {
                const auto current=package_source::load_project_attachment(project_root,"fft_ocean",cancellation);
                if(!current.source_attached()||current.content_hash!=water.content_hash)
                {result.cancelled=cancellation.stop_requested();result.message="Approved Water source changed during the planetary build. No provider was staged.";return result;}
            }
#ifdef _WIN32
            const auto producer = *build / "Release/epoch_planet_mesh_export.exe";
#else
            const auto producer = *build / "epoch_planet_mesh_export";
#endif
            const auto generationStarted = Clock::now();
            if (!admit_preview_memory(settings, result)) return result;
            const auto destination = runDirectory / "generated";
            std::vector<std::string> generationArguments{settings.mode, destination.string(), std::to_string(settings.seed),
                std::to_string(settings.cells_per_tile), std::to_string(settings.tiles_per_axis),
                std::to_string(settings.body_index), [&settings] {
                    const auto text = encode_settings(settings);
                    const auto offset = text.find("tile_span_meters=") + 17u;
                    return text.substr(offset, text.find('\n', offset) - offset);
                }()};
            if (settings.ocean_surface) generationArguments.push_back("1");
            if (settings.snapshot_megabytes != 4u)
                generationArguments.insert(generationArguments.end(), {"--mesh-bytes",
                    std::to_string(artifact_limits(settings).maximum_bytes)});
            append_size_arguments(generationArguments,settings);
            if (!execute(producer, std::move(generationArguments),
                *build, result.build_log, cancellation, *environment, 60'000'000'000ull, result)) return result;
            result.generation_seconds = std::chrono::duration<double>(Clock::now() - generationStarted).count();
            result.artifact_path = destination / "terrain.epoch_mesh";
            const auto decoded=asset::mesh::read_artifact_file(result.artifact_path, artifact_limits(settings));
            if (!decoded)
            { result.message = "Terrain generator output failed mesh validation."; return result; }
            if(meshFrame)
            {
                const auto framePath=destination/"terrain.frame";
                const auto status=fs::symlink_status(framePath);
                if(fs::is_regular_file(status)&&!fs::is_symlink(status)&&fs::hard_link_count(framePath)==1u)
                    result.mesh_frame=admit_generated_mesh_frame(read_small(framePath,4096u),settings,*decoded.artifact);
                if(!result.mesh_frame){result.message="Terrain generator coordinate evidence is missing, invalid or belongs to another mesh/request. Saved terrain was retained.";return result;}
            }
            if (cancellation.stop_requested()) { result.cancelled = true; result.message = "Terrain generation cancelled."; return result; }
            if (!source_generation_is_current(project_root, *admitted, cancellation)||recipe_guard(project_root)!=observedRecipe)
            { result.cancelled = cancellation.stop_requested(); result.message = "The project or planetary dependency changed during generation. Output is retained; saved terrain is unchanged."; return result; }
            if(settings.ocean_surface)
            {
                const auto current=package_source::load_project_attachment(project_root,"fft_ocean",cancellation);
                if(!current.source_attached()||current.content_hash!=water.content_hash)
                {result.cancelled=cancellation.stop_requested();result.message="Approved Water source changed during generation. Output was retained; no provider was staged.";return result;}
            }
            const MeshGenerationIdentity generatedIdentity{decoded.artifact->key().words,
                static_cast<std::uint32_t>(decoded.artifact->vertices().size()),
                static_cast<std::uint32_t>(decoded.artifact->indices().size()),
                result.mesh_frame.value_or(GeneratedMeshFrame{})};
            std::string bodyBytes;
            if(source_supports_body_inventory(source))
            {
                const auto path=destination/"terrain.bodies";const auto status=fs::symlink_status(path);
                if(fs::is_regular_file(status)&&!fs::is_symlink(status)&&fs::hard_link_count(path)==1u)
                    bodyBytes=read_small(path,16384u);
                result.bodies=admit_generated_body_inventory(bodyBytes,settings,generatedIdentity,*decoded.artifact);
                if(result.bodies.empty())
                {result.message="Generated body inventory is missing or does not match its mesh/frame. Saved terrain was retained.";return result;}
            }
            // Immutable, unreferenced evidence may remain if the following CAS
            // refuses. It cannot authorize a different saved recipe/scene mesh.
            if (!write_mesh_generation_record(project_root,*admitted,settings,generatedIdentity))
            {result.message="Generated mesh retained, but its source/recipe/coordinate association could not be recorded.";return result;}
            if(!bodyBytes.empty()&&!write_mesh_generation_bodies(project_root,*admitted,settings,generatedIdentity,bodyBytes))
            {result.message="Generated mesh retained, but its verified body inventory could not be recorded.";return result;}
            if (!write_dependency_fragments(project_root, dependency, *cmake, settings))
            {result.message="Generated terrain retained, but its managed dependency/staging files could not be refreshed.";return result;}
            // Only the completed executable is copied, after compilation. Keep
            // the existing stager's project-owned-input guard and short compiler
            // paths; no compiler work runs in this project job directory.
            const auto stageInput=runDirectory/"provider";
            if(!fs::create_directory(stageInput)||!fs::copy_file(producer,stageInput/producer.filename()))
            {result.message="The completed provider could not be copied to its owned staging input. Previous terrain/provider retained.";return result;}
            if(!execute(*cmake,{"-DEPOCH_PLANET_PROVIDER="+(stageInput/producer.filename()).generic_string(),"-DEPOCH_PLANET_CONFIGURATION=Release",
                "-P",(project_root/"Packages/epoch.stage_planet.cmake").string()},*build,result.build_log,
                cancellation,*environment,60'000'000'000ull,result))return result;
            if (!save_settings_observed(project_root,settings,&observedRecipe,cancellation))
            { result.cancelled=cancellation.stop_requested();result.message = "Generated terrain is retained, but its project dependency or settings could not be saved. Settings may be busy or have changed in another window."; return result; }
            result.recipe_path = project_root / recipe_relative_path;
            result.ok = true;
            result.message = "Terrain generated. The saved recipe and optional extension belong to this project.";
        }
        catch (...) { result.message = "Terrain generation stopped because a project file or build path could not be accessed."; }
        return result;
    }

    namespace
    {
        struct AdmittedProvider final
        {
            fs::path executable{}, working{};
            std::optional<process::ExecutableIdentity> identity{};
        };
        // The same receipt-bound executable selection serves streaming and
        // one-shot queries. Neither path builds or borrows another source tree.
        std::optional<AdmittedProvider> admitted_provider(const fs::path& project_root,
            const std::string& content_hash, Result& operation, bool require_receipt=false,
            std::string_view water_hash={})
        {
            const auto build = project_directory(project_root, "build/planet", false);
            fs::path producer, working;
#ifdef _WIN32
            constexpr std::string_view producerName = "epoch_planet_mesh_export.exe", platformName = "windows";
#elif defined(__APPLE__)
            constexpr std::string_view producerName = "epoch_planet_mesh_export", platformName = "macos";
#else
            constexpr std::string_view producerName = "epoch_planet_mesh_export", platformName = "linux";
#endif
            if (water_hash.empty()&&!require_receipt && build && read_small(*build / "source.sha256", 128u) == content_hash + "\n")
            {
#ifdef _WIN32
                producer = *build / "Release" / producerName;
#else
                producer = *build / producerName;
#endif
                working = *build;
                if (!fs::is_regular_file(producer)) producer.clear();
            }
            std::optional<process::ExecutableIdentity> expectedProvider;
            if (producer.empty())
            {
                constexpr std::string_view configuration =
#if !defined(NDEBUG)
                    "Debug";
#else
                    "Release";
#endif
                constexpr std::string_view physicalEdition = EPOCH_EDITION_PUBLIC ? "public" : "development";
                constexpr std::string_view editionId = EPOCH_EDITION_PUBLIC
                    ? "epoch.public.github.v1" : "epoch.development.site.v1";
                const auto base=fs::path{"Packages/.runtime/planetary_terrain"}/physicalEdition;
                std::string selectedConfiguration{configuration};
                const auto version=content_hash.substr(7,12)
                    +(water_hash.empty()?std::string{}:"-"+std::string{water_hash.substr(7,12)});
                auto relative=base/(selectedConfiguration+'-'+version);
                if(fs::symlink_status(project_root/relative).type()==fs::file_type::not_found)
                {
                    const auto release=base/("Release-"+version);
                    if(configuration!="Release"&&fs::symlink_status(project_root/release).type()!=fs::file_type::not_found)
                    {relative=release;selectedConfiguration="Release";}
                    else if(water_hash.empty())relative=base/configuration;
                }
                const auto runtime = project_directory(project_root,relative,false);
                if (!runtime)
                { operation.message = "Build this project's approved planetary provider before streaming."; return {}; }
                std::size_t entries{};
                for (const auto& entry : fs::directory_iterator(*runtime))
                {
                    const auto name = entry.path().filename();
                    if (++entries > 2u || (name != producerName && name != "provider.identity")
                        || !fs::is_regular_file(entry.symlink_status()) || fs::is_symlink(entry.symlink_status()))
                    { operation.message = "The staged planetary provider contains unmanaged files."; return {}; }
                }
                producer = *runtime / producerName;
                expectedProvider = process::inspect_executable(producer);
                if (entries != 2u || !expectedProvider)
                { operation.message = "The staged planetary provider is incomplete or redirected."; return {}; }
                const std::string receipt = (water_hash.empty()?std::string{"epoch.planet.provider.v1\nsource="}
                    :std::string{"epoch.planet.provider.v2\nsource="}) + content_hash
                    +(water_hash.empty()?std::string{}:"\nwater_source="+std::string{water_hash})
                    + "\nedition=" + std::string{editionId} + "\nprotocol=epoch.planet.stream.v1\nconfiguration="
                    + selectedConfiguration + "\nplatform=" + std::string{platformName} + "\nfile="
                    + std::string{producerName} + "\nsize=" + std::to_string(expectedProvider->size_bytes)
                    + "\nsha256=" + expectedProvider->sha256 + "\n";
                if (read_small(*runtime / "provider.identity", 1024u) != receipt)
                { operation.message = "The staged planetary provider does not match its source, edition or build receipt."; return {}; }
                working = *runtime;
            }
            return AdmittedProvider{std::move(producer),std::move(working),std::move(expectedProvider)};
        }
    }

    namespace
    {
        bool source_supports_surface_query(const fs::path& source)
        {
            const auto bytes=read_small(source/"tools/planet.mesh_export.cpp",64u*1024u);
            return bytes.find("--surface-query")!=std::string::npos
                &&bytes.find("epoch.planet.surface.v1")!=std::string::npos;
        }
        bool source_supports_surface_batch(const fs::path& source)
        {
            const auto bytes=read_small(source/"tools/planet.mesh_export.cpp",64u*1024u);
            return bytes.find("--surface-batch")!=std::string::npos
                &&bytes.find("epoch.planet.surface-batch.v1")!=std::string::npos;
        }
        bool valid_surface_query(const SurfaceQuery& query)
        {
            if(!query.request_id||!query.artifact||!valid_settings(query.settings)
                ||!supports_exploration(query.settings)||query.body.id>5u
                ||query.mesh.artifact_key!=query.artifact->key().words
                ||query.mesh.vertex_count!=query.artifact->vertices().size()
                ||query.mesh.index_count!=query.artifact->indices().size())return false;
            double squared{};
            for(const auto value:query.direction){if(!std::isfinite(value))return false;squared+=value*value;}
            return std::abs(squared-1.0)<=1e-6;
        }
        bool surface_generation_is_current(const fs::path& root,const SurfaceQuery& query,
            std::stop_token cancellation)
        {
            if(!source_generation_is_current(root,query.source,cancellation)
                ||!mesh_generation_is_current(root,query.source,query.settings,query.mesh))return false;
            const auto bodies=mesh_generation_bodies(root,query.source,query.mesh,*query.artifact);
            return std::find(bodies.begin(),bodies.end(),query.body)!=bodies.end();
        }
        bool parse_surface_receipt(std::string_view bytes,const SurfaceQuery& query,SurfaceResult& result)
        {
            constexpr std::string_view header="epoch.planet.surface.v1\n";
            if(!bytes.starts_with(header)||bytes.size()>4096u)return false;
            bytes.remove_prefix(header.size());
            const auto field=[&bytes](std::string_view name)->std::optional<std::string_view> {
                if(!bytes.starts_with(name))return {};
                bytes.remove_prefix(name.size());const auto end=bytes.find('\n');
                if(end==std::string_view::npos)return {};
                const auto value=bytes.substr(0,end);bytes.remove_prefix(end+1u);return value;
            };
            const auto number=[&field](std::string_view name,auto& value) {
                const auto text=field(name);if(!text)return false;
                const auto parsed=std::from_chars(text->data(),text->data()+text->size(),value);
                return parsed.ec==std::errc{}&&parsed.ptr==text->data()+text->size();
            };
            std::uint64_t id{};if(!number("request_id=",id)||id!=query.request_id)return false;
            const auto status=field("status=");if(!status)return false;
            if(*status=="unsupported"&&bytes.empty())
            {result.code=SurfaceCode::unsupported;result.request_id=id;return true;}
            if(*status!="ready")return false;
            std::uint32_t seed{},body{},reference{};double referenceRadius{},radius{};
            SurfaceResult sampled;
            if(!number("seed=",seed)||!number("body=",body)||!number("size_reference_body=",reference)
                ||!number("reference_radius_meters=",referenceRadius)||!number("body_radius_meters=",radius)
                ||!number("height_meters=",sampled.height_meters)
                ||!number("point_x=",sampled.point_meters[0])||!number("point_y=",sampled.point_meters[1])
                ||!number("point_z=",sampled.point_meters[2])||!number("up_x=",sampled.radial_up[0])
                ||!number("up_y=",sampled.radial_up[1])||!number("up_z=",sampled.radial_up[2])
                ||!number("normal_x=",sampled.normal[0])||!number("normal_y=",sampled.normal[1])
                ||!number("normal_z=",sampled.normal[2])||!bytes.empty()
                ||seed!=query.settings.seed||body!=query.body.id||reference!=query.settings.size_reference_body
                ||referenceRadius!=query.settings.reference_radius_meters||!std::isfinite(radius)
                ||radius!=query.body.radius_meters||radius<=0.0||!std::isfinite(sampled.height_meters)
                ||radius+sampled.height_meters<=0.0)return false;
            double upSquared{},normalSquared{},outward{};
            const double directionLength=std::hypot(query.direction[0],query.direction[1],query.direction[2]);
            for(std::size_t i=0;i<3u;++i)
            {
                const auto point=sampled.point_meters[i],up=sampled.radial_up[i],normal=sampled.normal[i];
                if(!std::isfinite(point)||!std::isfinite(up)||!std::isfinite(normal)
                    ||std::abs(up-query.direction[i]/directionLength)>1e-9
                    ||std::abs(point-up*(radius+sampled.height_meters))>(std::max)(1e-6,radius*1e-12))return false;
                upSquared+=up*up;normalSquared+=normal*normal;outward+=up*normal;
            }
            if(std::abs(upSquared-1.0)>1e-6||std::abs(normalSquared-1.0)>1e-6||outward<=0.0)return false;
            result.point_meters=sampled.point_meters;result.radial_up=sampled.radial_up;
            result.normal=sampled.normal;result.height_meters=sampled.height_meters;
            result.request_id=id;result.code=SurfaceCode::ready;return true;
        }
    }

    namespace
    {
        bool parse_surface_batch_receipt(std::string_view bytes,const SurfaceBatchQuery& batch,
            SurfaceBatchResult& result)
        {
            constexpr std::string_view header="epoch.planet.surface-batch.v1\n";
            if(!bytes.starts_with(header)||bytes.size()>kMaximumSurfaceBatchSamples*4160u+128u)return false;
            bytes.remove_prefix(header.size());
            const auto number=[&bytes](std::string_view name,auto& value) {
                if(!bytes.starts_with(name))return false;
                bytes.remove_prefix(name.size());const auto end=bytes.find('\n');
                if(end==std::string_view::npos)return false;
                const auto parsed=std::from_chars(bytes.data(),bytes.data()+end,value);
                if(parsed.ec!=std::errc{}||parsed.ptr!=bytes.data()+end)return false;
                bytes.remove_prefix(end+1u);return true;
            };
            std::uint64_t request{};std::size_t count{};
            if(!number("request_id=",request)||request!=batch.query.request_id
                ||!number("samples=",count)||count!=batch.directions.size())return false;
            std::vector<SurfaceResult> samples;samples.reserve(count);
            auto query=batch.query;
            for(std::size_t index=0u;index<count;++index)
            {
                std::size_t length{};
                if(!number("sample_bytes=",length)||!length||length>4096u||length>bytes.size())return false;
                query.direction=batch.directions[index];SurfaceResult sample;
                if(!parse_surface_receipt(bytes.substr(0u,length),query,sample))return false;
                bytes.remove_prefix(length);samples.push_back(std::move(sample));
            }
            if(!bytes.empty())return false;
            result.request_id=request;
            result.code=std::all_of(samples.begin(),samples.end(),[](const auto& sample){return sample.code==SurfaceCode::ready;})
                ?SurfaceCode::ready:SurfaceCode::unsupported;
            if(result.code==SurfaceCode::ready)result.samples=std::move(samples);
            return true;
        }

    SurfaceBatchResult query_surface_samples(const fs::path& project_root,const SurfaceBatchQuery& batch,
        std::stop_token cancellation,bool batchTransport)
    {
        SurfaceBatchResult result;auto& operation=result.operation;auto query=batch.query;
        result.request_id=query.request_id;
        try
        {
            if(cancellation.stop_requested())
            {result.code=SurfaceCode::cancelled;operation.cancelled=true;operation.message="Surface query cancelled.";return result;}
            if(batch.directions.empty()||batch.directions.size()>kMaximumSurfaceBatchSamples)
            {operation.message="Surface query requires between one and 64 samples.";return result;}
            for(const auto& direction:batch.directions)
            {
                query.direction=direction;
                if(!valid_surface_query(query))
                {operation.message="Surface query requires an admitted mesh and finite unit body-local directions.";return result;}
            }
            query.direction=batch.directions.front();
            if(!surface_generation_is_current(project_root,query,cancellation))
            {result.code=SurfaceCode::stale;operation.message="The planetary source, mesh, recipe or selected body changed.";return result;}
            const auto observedRecipe=recipe_guard(project_root);
            const auto attached=package_source::load_project_attachment(project_root,"planetary_terrain",cancellation);
            if(!observedRecipe||!attached.source_attached()||attached.content_hash!=query.source.content_hash)
            {result.code=SurfaceCode::stale;operation.message="The admitted planetary source or recipe is unavailable.";return result;}
            if(!source_supports_surface_query(project_root/attached.relative_source_root)
                ||(batchTransport&&!source_supports_surface_batch(project_root/attached.relative_source_root)))
            {result.code=SurfaceCode::unsupported;operation.message="This attached Planetary provider predates surface queries. Review and explicitly attach compatible source, then rebuild; the current scene was retained.";return result;}
            if(const auto active=process::active_in_group("epoch.project.planet");active&&active->active())
            {result.code=SurfaceCode::busy;operation.worker_busy=true;operation.message="Surface query is waiting for the existing planetary worker.";return result;}
            result.code=SurfaceCode::provider_failure;
            package_source::ProjectAttachment water;
            if(query.settings.ocean_surface)
            {
                water=package_source::load_project_attachment(project_root,"fft_ocean",cancellation);
                if(!water.source_attached()){operation.message="Surface query requires the same approved Water dependency as this wet planet.";return result;}
            }
            const auto provider=admitted_provider(project_root,attached.content_hash,operation,true,water.content_hash);
            if(!provider)return result;
            if(!provider->identity)
            {operation.message="Surface queries require the source- and executable-bound staged provider receipt; rebuild this project's approved provider.";return result;}
            const auto jobs=project_directory(project_root,"build/planet-query",true);
            if(!jobs){operation.message="Surface query storage is unavailable or redirected.";return result;}
            const auto run=*jobs/(std::to_string(Clock::now().time_since_epoch().count())+"-"+std::to_string(++sequence));
            if(!fs::create_directory(run)){operation.message="Could not reserve a surface query.";return result;}
            const auto environment=process::prepare_workspace_environment(run);
            if(!environment){operation.message="Could not prepare the surface query worker.";return result;}
            operation.build_log=run/"query.log";
            const auto started=Clock::now();
            std::vector<std::string> arguments={batchTransport?"--surface-batch":"--surface-query",(run/"surface.receipt").string(),
                std::to_string(query.settings.seed),std::to_string(query.body.id),
                std::to_string(query.settings.size_reference_body),real_text(query.settings.reference_radius_meters)};
            if(batchTransport)
            {
                arguments.push_back(std::to_string(query.request_id));arguments.push_back(std::to_string(batch.directions.size()));
                for(const auto& direction:batch.directions)for(const auto value:direction)arguments.push_back(real_text(value));
            }
            else
            {
                for(const auto value:query.direction)arguments.push_back(real_text(value));
                arguments.push_back(std::to_string(query.request_id));
            }
            if(!execute(provider->executable,std::move(arguments),provider->working,operation.build_log,cancellation,
                *environment,2'000'000'000ull,operation,provider->identity))
            {result.code=operation.cancelled?SurfaceCode::cancelled:operation.worker_busy?SurfaceCode::busy:SurfaceCode::provider_failure;return result;}
            operation.generation_seconds=std::chrono::duration<double>(Clock::now()-started).count();
            if(cancellation.stop_requested())
            {result.code=SurfaceCode::cancelled;operation.cancelled=true;operation.message="Surface query cancelled.";return result;}
            if(recipe_guard(project_root)!=observedRecipe||!surface_generation_is_current(project_root,query,cancellation))
            {result.code=SurfaceCode::stale;operation.message="The planetary source, mesh, recipe or body changed during the query; its result was discarded.";return result;}
            bool parsed{};
            if(batchTransport)
                parsed=parse_surface_batch_receipt(read_small(run/"surface.receipt",kMaximumSurfaceBatchSamples*4160u+128u),batch,result);
            else
            {
                SurfaceResult sample;parsed=parse_surface_receipt(read_small(run/"surface.receipt",4096u),query,sample);
                if(parsed){result.code=sample.code;if(sample.code==SurfaceCode::ready)result.samples.push_back(std::move(sample));}
            }
            if(!parsed)
            {result.code=SurfaceCode::provider_failure;operation.message="The planetary provider returned an invalid surface receipt.";return result;}
            if(cancellation.stop_requested())
            {result.samples.clear();result.code=SurfaceCode::cancelled;operation.cancelled=true;operation.message="Surface query cancelled.";return result;}
            operation.ok=result.code==SurfaceCode::ready;
            operation.message=operation.ok?"Admitted procedural surface sampled; displayed triangle contact is not established."
                :"The selected planetary body has no supported terrain surface.";
        }
        catch(...){result.samples.clear();result.code=SurfaceCode::provider_failure;operation.message="Surface query stopped because its project or worker evidence could not be accessed.";}
        return result;
    }
    }

    SurfaceResult query_surface(const fs::path& project_root,const SurfaceQuery& query,
        std::stop_token cancellation)
    {
        auto batch=query_surface_samples(project_root,{query,{query.direction}},cancellation,false);
        SurfaceResult result;
        if(batch.code==SurfaceCode::ready)result=std::move(batch.samples.front());
        result.code=batch.code;result.request_id=query.request_id;result.operation=std::move(batch.operation);
        return result;
    }

    SurfaceBatchResult query_surfaces(const fs::path& project_root,const SurfaceBatchQuery& query,
        std::stop_token cancellation)
    {return query_surface_samples(project_root,query,cancellation,true);}

    ShorelineResult query_ocean_shoreline(const fs::path& project_root, const SurfaceQuery& query,
        double maximum_distance_meters, std::stop_token cancellation)
    {
        if (cancellation.stop_requested())
            return {SurfaceCode::cancelled, {.cancelled=true,.message="Shoreline search cancelled."}};
        if (!query.settings.ocean_surface || query.body.id != 0u)
            return {SurfaceCode::unsupported, {.message="The selected body has no admitted ocean shoreline."}};
        if (!std::isfinite(maximum_distance_meters) || maximum_distance_meters <= 0.0
            || !std::isfinite(query.body.radius_meters) || query.body.radius_meters <= 0.0)
            return {SurfaceCode::invalid, {.message="Shoreline search requires a valid distance and body radius."}};

        constexpr unsigned sampleCount = 48u;
        constexpr double goldenAngle = 2.39996322972865332;
        SurfaceBatchQuery batch{query, {}};
        batch.directions.reserve(sampleCount);
        for (unsigned index = 0u; index < sampleCount; ++index)
        {
            const double y = 1.0 - 2.0 * (index + 0.5) / sampleCount;
            const double radial = std::sqrt(1.0 - y * y);
            const double angle = goldenAngle * index;
            batch.directions.push_back({radial * std::cos(angle), y, radial * std::sin(angle)});
        }

        for (unsigned pass = 0u; pass < 5u; ++pass)
        {
            auto sampled = query_surface_samples(project_root, batch, cancellation, true);
            if (sampled.code != SurfaceCode::ready) return {sampled.code, std::move(sampled.operation)};
            const SurfaceResult* dry{};
            const SurfaceResult* wet{};
            double closestChord = 2.0;
            for (const auto& land : sampled.samples)
            {
                if (land.height_meters <= 0.0) continue;
                for (const auto& water : sampled.samples)
                {
                    if (water.height_meters >= 0.0) continue;
                    const auto& a = land.radial_up;
                    const auto& b = water.radial_up;
                    const auto chord = std::hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
                    if (chord < closestChord) { closestChord = chord; dry = &land; wet = &water; }
                }
            }
            if (!dry || !wet) break;
            const auto distance = 2.0 * query.body.radius_meters * std::asin(closestChord * 0.5);
            if (distance <= maximum_distance_meters)
            {
                sampled.operation.message = "Verified a dry surface point beside ocean water.";
                return {SurfaceCode::ready, std::move(sampled.operation), *dry, *wet};
            }
            // Keep both sides of the crossing and refine between them in
            // one batch. No sample from a different recipe can fill a gap.
            batch.directions.clear();
            for (unsigned index = 0u; index < sampleCount; ++index)
            {
                const double t = static_cast<double>(index) / (sampleCount - 1u);
                std::array<double,3> direction{};
                for (unsigned axis = 0u; axis < direction.size(); ++axis)
                    direction[axis] = std::lerp(dry->radial_up[axis], wet->radial_up[axis], t);
                const auto length = std::hypot(direction[0], direction[1], direction[2]);
                for (auto& value : direction) value /= length;
                batch.directions.push_back(direction);
            }
        }
        return {SurfaceCode::unsupported, {.message="The bounded terrain search found no verified ocean shoreline at that distance."}};
    }

    StreamResult stream_step(const fs::path& project_root, Settings settings,
        StreamView view, std::stop_token cancellation)
    {
        StreamResult result;
        auto& operation = result.operation;
        try
        {
            if (!valid_settings(settings) || settings.mode != "planet" || !settings.stream_lod
                || !view.request_id || view.viewport_height < 64u || view.viewport_height > 16384u
                || !std::isfinite(view.vertical_fov) || view.vertical_fov < 0.1 || view.vertical_fov > 3.0
                || (view.simulation_seconds && (!std::isfinite(*view.simulation_seconds)
                    || *view.simulation_seconds < 0.0 || *view.simulation_seconds >= 1.0e12)))
            { operation.message = "Planet streaming settings or camera are invalid."; return result; }
            for (const auto component : view.camera)
                if (!std::isfinite(component) || std::abs(component) > 1.0e10)
                { operation.message = "Planet streaming camera is outside its supported range."; return result; }
            if(view.forward)
            {
                const auto& forward=*view.forward;
                const double magnitude=std::hypot(forward[0],forward[1],forward[2]);
                if(!std::isfinite(magnitude)||magnitude<1e-12||!std::isfinite(view.aspect_ratio)
                    ||view.aspect_ratio<0.1||view.aspect_ratio>10.0)
                {operation.message="Planet streaming view direction or aspect is invalid.";return result;}
            }
            if(!valid_stream_up(view))
            {operation.message="Planet streaming camera up direction is invalid.";return result;}
            if (cancellation.stop_requested()) { operation.cancelled = true; operation.message = "Terrain streaming cancelled."; return result; }
            if (!admit_preview_memory(settings, operation))
            {
                result.waiting_for_worker = true;
                return result;
            }
            if (const auto active = process::active_in_group("epoch.project.planet"); active && active->active())
            {
                result.waiting_for_worker = true;
                operation.message = "Planet LOD is queued behind another terrain operation.";
                return result;
            }
            const auto attached = package_source::load_project_attachment(project_root, "planetary_terrain", cancellation);
            if (!attached.source_attached()) {
                operation.cancelled = attached.code == package_source::Code::cancelled;
                operation.message = attached.message; return result;
            }
            const auto admitted = capture_source_generation(project_root, attached.content_hash);
            if (!admitted) { operation.message = "The terrain request has no stable project identity."; return result; }
            const auto observedRecipe=recipe_guard(project_root);
            if(!observedRecipe){operation.message="The saved terrain settings are unavailable or redirected.";return result;}
            const auto authoredSettings=decode_settings(*observedRecipe);
            // The compiled provider belongs to the authored project's dependency
            // set. A dry selected body still uses that exact Water-linked binary.
            const bool providerWater=settings.ocean_surface||(authoredSettings&&authoredSettings->ocean_surface);
            package_source::ProjectAttachment water;
            if(providerWater)
            {
                water=package_source::load_project_attachment(project_root,"fft_ocean",cancellation);
                if(!water.source_attached()||!source_supports_water(project_root/attached.relative_source_root,
                    project_root/water.relative_source_root))
                {operation.message="Animated oceans need compatible admitted Planet and Water source. Review both updates; saved terrain is retained.";return result;}
            }
            const bool customLod=requires_lod_controls(settings);
            const bool customSize=requires_size_controls(settings);
            const bool meshFrame=source_supports_mesh_frame(project_root/attached.relative_source_root);
            const auto streamProviderSource=read_small(project_root/attached.relative_source_root/"tools/planet.stream_export.cpp",64u*1024u);
            const bool patchTransport=streamProviderSource.find("epoch.planet.stream.v6")!=std::string::npos;
            const bool metricTransport=patchTransport&&meshFrame&&streamProviderSource.find("epoch.planet.stream-frame.v1")!=std::string::npos;
            const bool boundedMetricOrigin=metricTransport&&streamProviderSource.find("epoch.planet.stream-origin.v2")!=std::string::npos;
            const bool densityTransport=metricTransport&&streamProviderSource.find("epoch.planet.cloud-density.v1")!=std::string::npos;
            const bool cloudTransport=densityTransport||(metricTransport&&streamProviderSource.find("epoch.planet.cloud-layer.v1")!=std::string::npos);
            const bool viewTransport=metricTransport&&view.forward
                &&streamProviderSource.find("epoch.planet.view-cone.v1")!=std::string::npos;
            const bool footprintTransport=viewTransport&&view.up
                &&streamProviderSource.find("epoch.planet.view-footprint.v1")!=std::string::npos;
            if(settings.maximum_level>=20u&&!metricTransport)
            {operation.message="Fine LOD20 requires the metric-coordinate Planetary update. Review and attach that source, then rebuild; current terrain was retained.";return result;}
            const auto recipeBytes=*observedRecipe+"\n"+encode_settings(settings);
            const auto recipeKey=asset::texture::content_hash_hex(asset::texture::deterministic_content_hash(
                "epoch.planet.stream-recipe.v1",std::as_bytes(std::span{recipeBytes.data(),recipeBytes.size()})));
            if(customLod&&!source_supports_lod_controls(project_root / attached.relative_source_root))
            { operation.message="This attached Planetary version predates screen-detail controls. Review and explicitly attach the newer extension source, then rebuild; the current terrain was retained.";return result; }
            if(customSize&&(!source_supports_size_controls(project_root/attached.relative_source_root)
                ||!meshFrame))
            {operation.message="This attached Planetary version predates world-size controls. Review and explicitly attach compatible source before rebuilding; current terrain was retained.";return result;}
            const auto streamingModule = fs::symlink_status(project_root / attached.relative_source_root
                / "modules/planet.streaming.ixx");
            if (!fs::is_regular_file(streamingModule) || fs::is_symlink(streamingModule))
            { operation.message = "Review and attach updated planetary source with streaming support before enabling Planet LOD."; return result; }
            if (!attached.content_hash.starts_with("sha256:") || attached.content_hash.size() != 71u)
            { operation.message = "Planet streaming dependency has no complete source identity."; return result; }
            const auto provider=admitted_provider(project_root,attached.content_hash,operation,false,water.content_hash);
            if(!provider)return result;
            const auto& producer=provider->executable;
            const auto& working=provider->working;
            const auto& expectedProvider=provider->identity;
            if (!attached.content_hash.starts_with("sha256:") || attached.content_hash.size() != 71u)
            { operation.message = "Planet streaming dependency has no complete source identity."; return result; }
            const auto jobs = project_directory(project_root, "build/planet-stream", true);
            if (!jobs) { operation.message = "Planet streaming storage is unavailable or redirected."; return result; }
            auto lease = platform::filesystem::ExclusiveFileLease::acquire(*jobs / "cache.lock");
            if (!lease)
            {
                result.waiting_for_worker = lease.code() == platform::filesystem::FileLeaseCode::busy;
                operation.worker_busy = result.waiting_for_worker;
                operation.message = result.waiting_for_worker
                    ? "Another application is updating this project's planetary cache."
                    : "The planetary cache lease is unavailable or redirected.";
                return result;
            }
            auto retainedLease = std::make_shared<platform::filesystem::ExclusiveFileLease>(std::move(lease));
            const auto cache = project_directory(project_root,
                fs::path{"cache/planet"}/attached.content_hash.substr(7u)
                    /(water.content_hash.empty()?fs::path{}:fs::path{water.content_hash.substr(7u)})/stream_cache_identity(settings),true);
            if (!cache) { operation.message = "Planet streaming storage is unavailable or redirected."; return result; }
            const auto run = *jobs / (std::to_string(Clock::now().time_since_epoch().count()) + "-" + std::to_string(++sequence));
            if (!fs::create_directory(run)) { operation.message = "Could not reserve a terrain streaming job."; return result; }
            const auto environment = process::prepare_workspace_environment(run);
            if (!environment) { operation.message = "Could not prepare the terrain streaming worker."; return result; }
            operation.build_log = run / "stream.log";
            const auto real = [](double value) {
                std::array<char, 64> bytes{};
                const auto parsed = std::to_chars(bytes.data(), bytes.data() + bytes.size(), value,
                    std::chars_format::general, std::numeric_limits<double>::max_digits10);
                return parsed.ec == std::errc{} ? std::string(bytes.data(), parsed.ptr) : std::string{};
            };
            const auto started = Clock::now();
            // Amortize cache restore and snapshot work without increasing the
            // provider's four concurrent jobs or admitted memory budget. Older
            // admitted providers retain their original 32-patch request limit.
            const std::uint32_t maximumNewPatches =
                streamProviderSource.find("epoch.planet.stream-batch.v2")!=std::string::npos ? 128u : 32u;
            std::vector<std::string> streamArguments{"--stream-step-preview", (run / "generated").string(), cache->string(),
                std::to_string(settings.seed), std::to_string(settings.cells_per_tile), std::to_string(settings.body_index),
                real(view.camera[0]), real(view.camera[1]), real(view.camera[2]), std::to_string(view.viewport_height),
                real(view.vertical_fov), std::to_string(settings.maximum_level), std::to_string(settings.maximum_leaves),
                std::to_string(std::uint64_t{settings.resident_megabytes} * 1024u * 1024u),
                std::to_string(maximumNewPatches), std::to_string(view.request_id)};
            if (settings.ocean_surface) streamArguments.push_back("1");
            if (settings.snapshot_megabytes != 4u)
                streamArguments.insert(streamArguments.end(), {"--mesh-bytes",
                    std::to_string(artifact_limits(settings).maximum_bytes)});
            if (view.simulation_seconds)
                streamArguments.insert(streamArguments.end(), {"--seconds", real(*view.simulation_seconds)});
            if(customLod)
                streamArguments.insert(streamArguments.end(),{"--patch-pixels",real(settings.patch_pixels),
                    "--lod-hysteresis",real(settings.lod_hysteresis)});
            append_size_arguments(streamArguments,settings);
            if(patchTransport)streamArguments.insert(streamArguments.end(),{"--patch-transport-v1",attached.content_hash+":"+recipeKey});
            if(metricTransport)streamArguments.push_back("--metric-frame-v1");
            if(viewTransport)
            {
                const auto& forward=*view.forward;
                auto orientation=real(forward[0])+","+real(forward[1])+","+real(forward[2])+","+real(view.aspect_ratio);
                if(footprintTransport)
                    orientation+=","+real((*view.up)[0])+","+real((*view.up)[1])+","+real((*view.up)[2]);
                streamArguments.insert(streamArguments.end(),{footprintTransport?"--view-footprint-v1":"--view-cone-v1",std::move(orientation)});
            }
            if(cloudTransport)streamArguments.push_back(densityTransport?"--cloud-density-v1":"--cloud-layer-v1");
            if (!execute(producer, std::move(streamArguments),
                working, operation.build_log, cancellation, *environment, 30'000'000'000ull, operation, expectedProvider, retainedLease))
            { result.waiting_for_worker = operation.worker_busy; return result; }
            operation.generation_seconds = std::chrono::duration<double>(Clock::now() - started).count();
            // Decode only bounded worker output here. One full source check
            // below binds the completed decode/materialization to the captured
            // project, rather than rehashing the same package on both sides.
            if (cancellation.stop_requested()||recipe_guard(project_root)!=observedRecipe)
            { operation.cancelled = cancellation.stop_requested(); operation.message = "The project or planetary dependency changed during streaming. Worker evidence is retained; this snapshot was not accepted."; return result; }
            const auto receipt = read_small(run / "generated/terrain.stream", 4096u);
            std::string_view remaining{receipt};
            const std::string_view header = footprintTransport?"epoch.planet.stream.v9\n":viewTransport?"epoch.planet.stream.v8\n":metricTransport?"epoch.planet.stream.v7\n":patchTransport ? "epoch.planet.stream.v6\n" : customSize ? "epoch.planet.stream.v5\n" : customLod ? "epoch.planet.stream.v4\n"
                :view.simulation_seconds ? "epoch.planet.stream.v3\n"
                : settings.ocean_surface ? "epoch.planet.stream.v2\n" : "epoch.planet.stream.v1\n";
            if (!remaining.starts_with(header)) { operation.message = "The terrain worker returned no valid streaming receipt."; return result; }
            remaining.remove_prefix(header.size());
            const auto field = [&remaining](std::string_view name) -> std::optional<std::string_view> {
                if (!remaining.starts_with(name)) return {};
                remaining.remove_prefix(name.size());
                const auto end = remaining.find('\n');
                if (end == std::string_view::npos) return {};
                const auto value = remaining.substr(0, end); remaining.remove_prefix(end + 1u); return value;
            };
            const auto integer = [&field](std::string_view name, auto& output) {
                const auto value = field(name); if (!value) return false;
                const auto parsed = std::from_chars(value->data(), value->data() + value->size(), output);
                return parsed.ec == std::errc{} && parsed.ptr == value->data() + value->size();
            };
            std::uint32_t ready{}, vertices{}, indices{};
            double scale{};
            bool valid = integer("request_id=", result.request_id) && integer("snapshot_ready=", ready)
                && integer("built=", result.built) && integer("reused=", result.reused)
                && integer("resident=", result.resident) && integer("pending=", result.pending)
                && integer("draw=", result.draw) && integer("min_level=", result.minimum_level)
                && integer("max_level=", result.maximum_level) && integer("resident_bytes=", result.resident_bytes)
                && integer("maximum_bytes=", result.maximum_bytes);
            const auto scaleText = field("meters_to_preview_units=");
            if (scaleText) {
                const auto parsed = std::from_chars(scaleText->data(), scaleText->data() + scaleText->size(), scale);
                valid = valid && parsed.ec == std::errc{} && parsed.ptr == scaleText->data() + scaleText->size()
                    && std::isfinite(scale) && scale > 0.0&&std::isfinite(1.0/scale)&&1.0/scale>0.0&&1.0/scale<=1.0e12;
            } else valid = false;
            std::array<double,3> sourceOrigin{};
            double bodyRadius{};
            if(metricTransport)
            {
                const auto realField=[&](std::string_view name,double& output){
                    const auto value=field(name);if(!value)return false;
                    const auto parsed=std::from_chars(value->data(),value->data()+value->size(),output);
                    return parsed.ec==std::errc{}&&parsed.ptr==value->data()+value->size()&&std::isfinite(output);};
                valid=valid&&scale==1.0&&realField("body_radius_meters=",bodyRadius)
                    &&bodyRadius>0.0&&bodyRadius<=1.0e12
                    &&realField("source_origin_x_meters=",sourceOrigin[0])
                    &&realField("source_origin_y_meters=",sourceOrigin[1])
                    &&realField("source_origin_z_meters=",sourceOrigin[2]);
                const auto expectedOrigin=project_planetary_snapshot::metric_source_origin(
                    {view.camera[0]*(bodyRadius/10.0),view.camera[1]*(bodyRadius/10.0),
                        view.camera[2]*(bodyRadius/10.0)},boundedMetricOrigin);
                valid=valid&&expectedOrigin.has_value();
                for(std::size_t axis=0;axis<3u&&expectedOrigin;++axis)
                {
                    const double expected=(*expectedOrigin)[axis];
                    valid=valid&&std::abs(sourceOrigin[axis])<=1.0e12
                        &&std::abs(sourceOrigin[axis]-expected)<=1e-8*(std::max)(1.0,std::abs(expected));
                }
                if(settings.body_index==settings.size_reference_body)
                    valid=valid&&bodyRadius==settings.reference_radius_meters;
            }
            const auto meshFile = field("mesh_file=");
            const auto key = field("content_key=");
            valid = valid && integer("vertices=", vertices) && integer("indices=", indices);
            if (customSize || customLod || settings.ocean_surface || view.simulation_seconds)
            {
                std::uint32_t ocean{};
                valid = valid && integer("ocean_surface=", ocean) && ocean == (settings.ocean_surface ? 1u : 0u);
            }
            if (customSize || customLod || view.simulation_seconds)
            {
                const auto secondsText = field("simulation_seconds=");
                double seconds{};
                if (!secondsText) valid = false;
                else
                {
                    const auto parsed = std::from_chars(secondsText->data(), secondsText->data() + secondsText->size(), seconds);
                    valid = valid && parsed.ec == std::errc{} && parsed.ptr == secondsText->data() + secondsText->size()
                        && std::isfinite(seconds) && seconds == view.simulation_seconds.value_or(0.0);
                }
            }
            if(customSize||customLod)
            {
                const auto matches=[&](std::string_view name,double expected) {
                    const auto text=field(name);if(!text)return false;
                    double value{};const auto parsed=std::from_chars(text->data(),text->data()+text->size(),value);
                    return parsed.ec==std::errc{}&&parsed.ptr==text->data()+text->size()
                        &&std::isfinite(value)&&value==expected;
                };
                valid=valid&&matches("patch_pixels=",settings.patch_pixels)
                    &&matches("lod_hysteresis=",settings.lod_hysteresis);
            }
            if(customSize)
            {
                std::uint32_t body{};
                valid=valid&&integer("size_reference_body=",body)&&body==settings.size_reference_body;
                const auto radiusText=field("reference_radius_meters=");
                double radius{};
                if(!radiusText)valid=false;
                else
                {
                    const auto parsed=std::from_chars(radiusText->data(),radiusText->data()+radiusText->size(),radius);
                    valid=valid&&parsed.ec==std::errc{}&&parsed.ptr==radiusText->data()+radiusText->size()
                        &&std::isfinite(radius)&&radius==settings.reference_radius_meters;
                }
            }
            const auto patchFile=patchTransport?field("patch_file="):std::optional<std::string_view>{};
            valid=valid&&(!patchTransport||(patchFile&&*patchFile==(ready?"terrain.epoch_planet_patch":"")));
            if(viewTransport)
            {
                const auto matches=[&](std::string_view name,double expected) {
                    const auto text=field(name);if(!text)return false;
                    double value{};const auto decoded=std::from_chars(text->data(),text->data()+text->size(),value);
                    return decoded.ec==std::errc{}&&decoded.ptr==text->data()+text->size()
                        &&std::isfinite(value)&&value==expected;
                };
                valid=valid&&matches("view_forward_x=",(*view.forward)[0])&&matches("view_forward_y=",(*view.forward)[1])
                    &&matches("view_forward_z=",(*view.forward)[2])&&matches("view_aspect=",view.aspect_ratio);
                if(footprintTransport)
                    valid=valid&&matches("view_up_x=",(*view.up)[0])&&matches("view_up_y=",(*view.up)[1])
                        &&matches("view_up_z=",(*view.up)[2]);
            }
            valid = valid && remaining.empty() && result.request_id == view.request_id && ready <= 1u
                && result.resident_bytes <= result.maximum_bytes
                && result.maximum_bytes == std::uint64_t{settings.resident_megabytes} * 1024u * 1024u;
            if (!valid) { operation.message = "Terrain streaming evidence is invalid or belongs to another request."; return result; }
            if (ready)
            {
                if (!meshFile || *meshFile != "terrain.epoch_mesh" || !key)
                { operation.message = "Terrain streaming mesh identity is missing."; return result; }
                operation.artifact_path = run / "generated/terrain.epoch_mesh";
                if (!admit_preview_memory(settings, operation))
                {
                    result.waiting_for_worker = true;
                    return result;
                }
                const auto decoded = asset::mesh::read_artifact_file(operation.artifact_path, artifact_limits(settings));
                if (!decoded || decoded.artifact->vertices().size() != vertices || decoded.artifact->indices().size() != indices
                    || asset::texture::content_hash_hex(decoded.artifact->key()) != *key)
                { operation.message = "Terrain streaming mesh failed content validation."; return result; }
                result.snapshot = decoded.artifact;
                if(patchTransport)
                {
                    const auto patchLimits=snapshot_transition_limits(settings);
                    auto encoded=read_small(run/"generated/terrain.epoch_planet_patch",
                        static_cast<std::size_t>(patchLimits.maximum_metadata_bytes));
                    const project_planetary_snapshot::Identity expected{decoded.artifact->key(),attached.content_hash,recipeKey,view.request_id};
                    auto metadata=project_planetary_snapshot::decode(std::as_bytes(std::span{encoded.data(),encoded.size()}),
                        *decoded.artifact,expected,patchLimits,cancellation);
                    if(!metadata){operation.message="Terrain patch hierarchy failed identity or bounded-content admission.";result.snapshot.reset();return result;}
                    if(metadata->cloud&&(!cloudTransport||bool(metadata->cloud->density)!=densityTransport||metadata->cloud->body!=settings.body_index
                        ||metadata->cloud->radius_meters!=bodyRadius))
                    {operation.message="Cloud layer does not match the admitted planet frame.";result.snapshot.reset();return result;}
                    if(metricTransport&&settings.ocean_surface&&(metadata->water_radius_meters!=bodyRadius
                        ||metadata->water_meters_to_artifact!=scale||metadata->water_origin_meters!=sourceOrigin))
                    {operation.message="Terrain water coordinates do not match the admitted stream frame.";result.snapshot.reset();return result;}
                    std::string{}.swap(encoded);
                    auto admitted=std::make_shared<const project_planetary_snapshot::Snapshot>(
                        project_planetary_snapshot::Snapshot{decoded.artifact,std::move(*metadata)});
                    project_planetary_snapshot::Transition initial{patchLimits};
                    // This worker returns a complete static fallback. Temporal
                    // scene owners may retain patch_snapshot and advance their
                    // own transition, but displaying fresh roots here would
                    // discard every requested fine patch on every worker turn.
                    if(!initial.admit(admitted,cancellation,project_planetary_snapshot::InitialCut::selected_patches)
                        ||(result.snapshot=initial.materialize(cancellation))==nullptr)
                    {operation.message="Terrain patch hierarchy exceeds the retained snapshot budget.";result.snapshot.reset();return result;}
                    result.patch_snapshot=std::move(admitted);
                }
                if(meshFrame)operation.mesh_frame=GeneratedMeshFrame{1.0/scale,sourceOrigin};
            }
            result.snapshot_ready = ready != 0u;
            if(cancellation.stop_requested()||!source_generation_is_current(project_root,*admitted,cancellation)
                ||recipe_guard(project_root)!=observedRecipe)
            {
                operation.cancelled=cancellation.stop_requested();result.snapshot.reset();result.patch_snapshot.reset();result.snapshot_ready=false;
                operation.message="Terrain source or settings changed during patch admission. The previous terrain was retained.";return result;
            }
            if(providerWater)
            {
                const auto current=package_source::load_project_attachment(project_root,"fft_ocean",cancellation);
                if(!current.source_attached()||current.content_hash!=water.content_hash||cancellation.stop_requested())
                {
                    operation.cancelled=cancellation.stop_requested();result.snapshot.reset();result.patch_snapshot.reset();result.snapshot_ready=false;
                    operation.message="Approved Water source changed during patch admission. The previous terrain was retained.";return result;
                }
            }
            result.verified_source = *admitted;
            operation.ok = true;
            operation.message = "Planet LOD " + std::to_string(result.minimum_level) + "–" + std::to_string(result.maximum_level)
                + " | " + std::to_string(result.draw) + " visible patches | " + std::to_string(result.pending) + " loading";
            constexpr std::uint64_t mib = 1024u * 1024u;
            operation.message += " | patch cache " + std::to_string(result.resident_bytes / mib)
                + "." + std::to_string((result.resident_bytes % mib) * 10u / mib)
                + " / " + std::to_string(result.maximum_bytes / mib) + " MiB";
            if(cloudTransport&&!densityTransport)
                operation.message+="\nClouds use the project's older geometric source. In Package Manager, review Planetary Terrain and choose Update Project Source, then Build & Generate for volumetric clouds. Saved settings and terrain are retained.";
            // Provider jobs use a separate scheduler from the Editor toolbar.
            // Show measured completed work, never configured threads as active.
            const auto performance=read_small(run/"generated/terrain.performance",4096u);
            if(performance.starts_with("epoch.planet.stream-performance.v1\n"))
            {
                const auto number=[&](std::string_view name,std::uint64_t& value) {
                    const auto prefix="\n"+std::string{name};
                    const auto start=performance.find(prefix);if(start==std::string::npos)return false;
                    const auto begin=start+prefix.size(),end=performance.find('\n',begin);
                    if(end==std::string::npos||performance.find(prefix,end)!=std::string::npos)return false;
                    const auto parsed=std::from_chars(performance.data()+begin,performance.data()+end,value);
                    return parsed.ec==std::errc{}&&parsed.ptr==performance.data()+end;
                };
                std::uint64_t id{},limit{},peak{},jobsStarted{};
                if(number("request_id=",id)&&id==view.request_id&&number("worker_limit=",limit)
                    &&number("peak_active_workers=",peak)&&number("worker_jobs_started=",jobsStarted)
                    &&limit>=1u&&limit<=4u&&peak<=limit&&((jobsStarted==0u)==(peak==0u)))
                {
                    operation.message+="\nLast LOD build: "+std::to_string(peak)+"/"+std::to_string(limit)+" workers";
                    if(std::isfinite(operation.generation_seconds)&&operation.generation_seconds>=0.0
                        &&operation.generation_seconds<=3600.0)
                        operation.message+=", "+std::to_string(static_cast<std::uint64_t>(
                            operation.generation_seconds*1000.0+0.5))+" ms provider";
                }
            }
            // Advisory bounded evidence survives successful output cleanup.
            // It never admits a provider, artifact or camera publication.
            {
                std::ofstream evidence{run/"stream.evidence",std::ios::binary|std::ios::noreplace};
                if(evidence)evidence<<std::setprecision(17)<<"epoch.planet.host-stream-performance.v1\nrequest_id="<<view.request_id
                    <<"\nbody="<<settings.body_index<<"\ncamera_x="<<view.camera[0]<<"\ncamera_y="<<view.camera[1]<<"\ncamera_z="<<view.camera[2]
                    <<"\nprovider_seconds="<<operation.generation_seconds
                    <<"\nconsume_seconds="<<std::chrono::duration<double>(Clock::now()-started).count()-operation.generation_seconds
                    <<"\nbuilt="<<result.built<<"\nreused="<<result.reused<<"\nresident="<<result.resident
                    <<"\npending="<<result.pending<<"\ndraw="<<result.draw<<"\nvertices="<<vertices<<"\nindices="<<indices
                    <<"\n"<<performance
                    <<read_small(run/"generated/terrain.snapshot-performance",4096u);
            }
            // Successful step outputs are disposable. The validated immutable
            // snapshot is retained in memory, and only the provider's bounded
            // patch cache survives. Failures retain their actual diagnostic log.
            std::error_code cleanupError;
            for (const auto* name : {"terrain.epoch_mesh", "terrain.epoch_planet_patch", "terrain.preview.json", "terrain.stream"})
                fs::remove(run / "generated" / name, cleanupError);
            fs::remove(run / "generated", cleanupError);
            fs::remove(operation.build_log, cleanupError);
            for (const auto* directory : {"cache/process/profile/AppData/Local", "cache/process/profile/AppData/Roaming",
                "cache/process/profile/AppData", "cache/process/profile", "cache/process/temp",
                "cache/process/packages", "cache/process", "cache"})
                fs::remove(run / directory, cleanupError);
            // prepare_workspace_environment may own additional temporary files;
            // remove an empty directory only, never recurse through worker data.
            fs::remove(run, cleanupError);
            operation.artifact_path.clear();
            operation.build_log.clear();
        }
        catch (...) { operation.message = "Terrain streaming stopped because a project path or worker output was unavailable."; }
        return result;
    }
}
