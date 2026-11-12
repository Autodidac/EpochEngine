// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <atomic>
#include <cmath>
#include <charconv>
#include <chrono>
#include <limits>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <stop_token>
#include <string>
#include <vector>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include <epoch.edition.hpp>
#include "../ai/ai.native_runtime.hpp"
#include "../../include/core.utf8.hpp"
#include "../assets/image.decode.hpp"
module editor.asset_generation;
import asset.texture_import;
import core.sha256;
import render.texture_artifact;
import render.canvas2d;
import render.device;
import platform.filesystem;

namespace epochengine::editor_asset_generation
{
    namespace
    {
        namespace jobs=ai::asset_jobs;
        std::optional<std::vector<std::byte>> captured_bytes(const std::filesystem::path& path,
            std::uint64_t limit=64ull*1024ull*1024ull)
        {
            try
            {
                if(!path.is_absolute())return {};
                auto current=path.root_path();
                for(const auto& part:path.relative_path())
                {
                    if(part==".."||part==".")return {};
                    current/=part;
                    const auto status=std::filesystem::symlink_status(current);
                    if(std::filesystem::is_symlink(status))return {};
                }
                if(!std::filesystem::is_regular_file(path))return {};
                std::ifstream in(path,std::ios::binary|std::ios::ate);
                const auto size=in.tellg();
                if(!in||size<=0||std::uint64_t(size)>limit)return {};
                std::vector<std::byte> bytes(static_cast<std::size_t>(size));in.seekg(0);
                if(!in.read(reinterpret_cast<char*>(bytes.data()),size)||in.peek()!=std::char_traits<char>::eof())return {};
                return bytes;
            }
            catch(...){return {};}
        }
        std::string digest(std::span<const std::byte> bytes)
        {
            return core::sha256::hex(core::sha256::hash(std::span{
                reinterpret_cast<const std::uint8_t*>(bytes.data()),bytes.size()}));
        }
        void decode_output(Completion& out)
        {
            if(!out.result)return;
            const auto bytes=captured_bytes(out.result.output);
            if(!bytes||digest(*bytes)!=out.result.output_sha256)
            {out.result.code=jobs::Code::invalid_output;out.result.diagnostic="Output changed after verification. Review it again.";return;}
            if(out.result.code==jobs::Code::awaiting_reference_approval)
            {
                auto decoded=asset::texture::import_texture_memory({.source=*bytes,.extension=".png"});
                if(!decoded){out.result.code=jobs::Code::invalid_output;out.result.diagnostic="Reference PNG could not be decoded.";return;}
                out.image=std::move(decoded.artifact);out.result.width=decoded.width;out.result.height=decoded.height;
            }
            else
            {
                out.model=asset::model_import::compile_base_color_glb(*bytes);
                if(!out.model)out.result.diagnostic+=" Model remains retained; this workspace cannot preview/import its unsupported materials: "+out.model.diagnostic;
            }
        }
        std::string stage_text(Stage stage)
        {
            switch(stage)
            {
            case Stage::waiting_for_models:return "Queued for a safe coding-model handoff";
            case Stage::generating:return "Generating and validating the owned output";
            case Stage::restoring_models:return "Checking worker retirement and restoring coding models";
            case Stage::inspecting_reference:return "Reading and validating the selected PNG reference";
            case Stage::restoring_outputs:return "Reading and validating retained generation outputs";
            case Stage::exporting_output:return "Saving a verified output copy";
            default:return "Ready";
            }
        }
        std::string path_text(const std::filesystem::path& value)
        {const auto u=value.generic_u8string();return {u.begin(),u.end()};}
    }
    bool load_settings(State& state,const std::filesystem::path& path,jobs::Action action)
    {
        const auto before=captured_bytes(path,1024u*1024u);
        if(!before){state.status="Choose an ordinary local backend settings file up to 1 MiB.";return false;}
        auto job=jobs::read_request_file(path);
        if(!job||job.action!=action)
        {state.status=job?"That settings file belongs to the other generation stage.":job.diagnostic;return false;}
        const auto after=captured_bytes(path,1024u*1024u);
        if(!after||digest(*before)!=digest(*after))
        {state.status="Backend settings changed while being read. Choose the file again.";return false;}
        if(action==jobs::Action::image){
            // A generator choice must not replace the request already typed in
            // this workspace. Empty workspaces can use the selected defaults.
            if(state.image_request.prompt.empty())
            {state.image_request=job.image;state.image_seed=std::to_string(job.image.seed);}
            state.image_settings=std::move(job);
            state.image_settings_path=path;state.image_settings_sha256=digest(*before);}
        else {state.model_request=job.model;state.model_seed=std::to_string(job.model.seed);
            if(!job.alternative_model_backend)job.model_backend.dinov3_terms_accepted=false;
            state.model_settings=std::move(job);state.model_settings_path=path;state.model_settings_sha256=digest(*before);}
        state.status="Pinned local backend settings loaded. Actual executable/model bytes and available memory are checked when generation starts. No download or inference was started.";
        return true;
    }
    namespace
    {
        constexpr std::uint64_t draft_limit=64u*1024u;
        bool draft_text(std::string_view text,std::size_t maximum)
        {return text.size()<=maximum&&text.find('\0')==std::string_view::npos&&core::utf8::valid(text);}
        bool draft_seed(std::string_view seed)
        {
            if(seed.empty())return true; // An unfinished input is a draft, not a generation request.
            std::uint32_t value{};const auto parsed=std::from_chars(seed.data(),seed.data()+seed.size(),value);
            return seed.size()<=10u&&parsed.ec==std::errc{}&&parsed.ptr==seed.data()+seed.size();
        }
        bool draft_source(const std::filesystem::path& path,std::string_view hash)
        {
            if(path.empty())return hash.empty();
            return path.is_absolute()&&path.lexically_normal()==path&&draft_text(path_text(path),8192u)
                &&hash.size()==64u&&std::all_of(hash.begin(),hash.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');});
        }
        bool valid_draft(const State& state)
        {
            const auto& image=state.image_request;const auto& model=state.model_request;
            return draft_text(image.prompt,8192u)&&draft_text(image.negative_prompt,4096u)
                &&draft_seed(state.image_seed)&&draft_seed(state.model_seed)
                &&image.width>=256u&&image.width<=2048u&&image.width%16u==0u
                &&image.height>=256u&&image.height<=2048u&&image.height%16u==0u
                &&image.steps>=1u&&image.steps<=8u&&model.resolution==512u
                &&model.steps>=1u&&model.steps<=24u&&model.face_budget>=1u&&model.face_budget<=1'000'000u
                &&model.atlas>=64u&&model.atlas<=4096u&&(model.atlas&(model.atlas-1u))==0u
                &&draft_source(state.image_settings_path,state.image_settings_sha256)
                &&draft_source(state.model_settings_path,state.model_settings_sha256);
        }
        bool draft_destination(const std::filesystem::path& path)
        {
            std::error_code error;
            if(!path.is_absolute()||path.lexically_normal()!=path||path.extension()!=".epoch-generation"
                ||std::filesystem::canonical(path.parent_path(),error)!=path.parent_path()||error)return false;
            const auto status=std::filesystem::symlink_status(path,error);
            return error==std::errc::no_such_file_or_directory
                ||(!error&&(!std::filesystem::exists(status)
                    ||(std::filesystem::is_regular_file(status)&&!std::filesystem::is_symlink(status))));
        }
    }
    bool save_draft(const State& state,const std::filesystem::path& path,std::string& diagnostic)
    {
        std::filesystem::path owned;
        try
        {
            if(!valid_draft(state)||!draft_destination(path))
            {diagnostic="Use a local .epoch-generation file in an ordinary folder, with bounded draft controls and seeds.";return false;}
            std::ostringstream out;
            out<<"EPOCH_GENERATION_DRAFT 1\nimage_backend "<<std::quoted(path_text(state.image_settings_path))<<' '<<std::quoted(state.image_settings_sha256)
                <<"\nmodel_backend "<<std::quoted(path_text(state.model_settings_path))<<' '<<std::quoted(state.model_settings_sha256)
                <<"\nimage_prompt "<<std::quoted(state.image_request.prompt)<<"\nimage_negative "<<std::quoted(state.image_request.negative_prompt)
                <<"\nimage_seed "<<std::quoted(state.image_seed)<<"\nimage_size "<<state.image_request.width<<' '<<state.image_request.height<<' '<<state.image_request.steps
                <<"\nmodel_seed "<<std::quoted(state.model_seed)<<"\nmodel_limits "<<state.model_request.resolution<<' '<<state.model_request.steps<<' '<<state.model_request.face_budget<<' '<<state.model_request.atlas<<"\nend\n";
            const auto encoded=out.str();if(encoded.size()>draft_limit){diagnostic="The generation draft exceeds 64 KiB.";return false;}
            static std::atomic<std::uint64_t> nextDraft{1u};std::error_code error;
            for(unsigned attempt{};attempt<16u;++attempt)
            {
                const auto temporary=path.parent_path()/(".epoch-generation-draft-write-"
                    +std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+'-'
                    +std::to_string(nextDraft.fetch_add(1u,std::memory_order_relaxed)));
                if(platform::filesystem::exclusive_create_and_write(temporary,std::as_bytes(std::span{encoded}),error))
                {owned=temporary;break;}
            }
            if(owned.empty()||!draft_destination(path)
                ||!platform::filesystem::atomic_replace_same_filesystem(owned,path,error))
            {if(!owned.empty())std::filesystem::remove(owned,error);diagnostic="The draft could not be published. Existing destination contents were preserved.";return false;}
            diagnostic="Generation draft saved. It contains controls and pinned backend references; outputs and approvals are not included.";return true;
        }
        catch(...){std::error_code ignored;if(!owned.empty())std::filesystem::remove(owned,ignored);
            diagnostic="The generation draft could not be saved.";return false;}
    }
    bool save_output_copy(const Completion& output,const std::filesystem::path& path,std::string& diagnostic,std::stop_token stop)
    {
        std::filesystem::path owned;
        try
        {
            std::error_code error;
            if(stop.stop_requested()){diagnostic="Save cancelled. No output copy was published.";return false;}
            const auto extension=output.result.output.extension();
            if(!output.result||(extension!=".png"&&extension!=".glb")||path.extension()!=extension
                ||!path.is_absolute()||path.lexically_normal()!=path
                ||std::filesystem::canonical(path.parent_path(),error)!=path.parent_path()||error)
            {diagnostic="Choose a new local file with the same PNG or GLB type in an ordinary folder.";return false;}
            const auto bytes=captured_bytes(output.result.output);
            if(!bytes||digest(*bytes)!=output.result.output_sha256)
            {diagnostic="The generated output changed or is unavailable. No copy was saved; review the output again.";return false;}
            const auto status=std::filesystem::symlink_status(path,error);
            if((error&&error!=std::errc::no_such_file_or_directory)||(!error&&std::filesystem::exists(status)))
            {diagnostic="That destination already exists or is unavailable. Choose a new filename; existing files are preserved.";return false;}
            static std::atomic<std::uint64_t> nextCopy{};
            for(unsigned attempt{};attempt<16u;++attempt)
            {
                const auto temporary=path.parent_path()/(".epoch-generation-output-"
                    +std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+'-'
                    +std::to_string(nextCopy.fetch_add(1u,std::memory_order_relaxed)));
                if(platform::filesystem::exclusive_create_and_write(temporary,*bytes,error))
                {owned=temporary;break;}
            }
            if(stop.stop_requested())
            {if(!owned.empty())std::filesystem::remove(owned,error);diagnostic="Save cancelled. No output copy was published.";return false;}
            if(owned.empty()||!platform::filesystem::atomic_publish_new_same_filesystem(owned,path,error))
            {if(!owned.empty())std::filesystem::remove(owned,error);diagnostic="The output copy could not be published. Existing files were preserved.";return false;}
            diagnostic="Saved a copy to "+path_text(path)+". Nothing was imported or approved.";
            return true;
        }
        catch(...){std::error_code ignored;if(!owned.empty())std::filesystem::remove(owned,ignored);
            diagnostic="The output copy could not be saved. Existing files were preserved.";return false;}
    }
    bool load_draft(State& state,const std::filesystem::path& path,std::string& diagnostic)
    {
        if(state.phase->load()!=Stage::idle)
        {diagnostic="Wait for generation and model restoration to settle before loading a draft.";return false;}
        try
        {
            const auto bytes=captured_bytes(path,draft_limit);
            if(!bytes){diagnostic="Choose an ordinary local generation draft up to 64 KiB.";return false;}
            const std::string encoded(reinterpret_cast<const char*>(bytes->data()),bytes->size());
            std::istringstream in(encoded);State restored;std::string key,imagePath,modelPath;
            const auto field=[&](std::string_view expected){return bool(in>>key)&&key==expected;};
            const auto text=[&](std::string& value){in>>std::ws;return in.peek()=='"'&&bool(in>>std::quoted(value));};
            const auto number=[&](std::uint32_t& value){std::string token;if(!(in>>token))return false;
                const auto parsed=std::from_chars(token.data(),token.data()+token.size(),value);
                return parsed.ec==std::errc{}&&parsed.ptr==token.data()+token.size();};
            std::uint32_t version{};
            if(!field("EPOCH_GENERATION_DRAFT")||!number(version)||version!=1u
                ||!field("image_backend")||!text(imagePath)||!text(restored.image_settings_sha256)
                ||!field("model_backend")||!text(modelPath)||!text(restored.model_settings_sha256)
                ||!field("image_prompt")||!text(restored.image_request.prompt)
                ||!field("image_negative")||!text(restored.image_request.negative_prompt)
                ||!field("image_seed")||!text(restored.image_seed)
                ||!field("image_size")||!number(restored.image_request.width)||!number(restored.image_request.height)||!number(restored.image_request.steps)
                ||!field("model_seed")||!text(restored.model_seed)
                ||!field("model_limits")||!number(restored.model_request.resolution)||!number(restored.model_request.steps)||!number(restored.model_request.face_budget)||!number(restored.model_request.atlas)
                ||!field("end")||!(in>>std::ws).eof()||!draft_text(encoded,draft_limit))
            {diagnostic="Malformed generation draft. Current controls and approvals were not changed.";return false;}
            restored.image_settings_path=std::filesystem::path{std::u8string{imagePath.begin(),imagePath.end()}};
            restored.model_settings_path=std::filesystem::path{std::u8string{modelPath.begin(),modelPath.end()}};
            if(!valid_draft(restored)){diagnostic="Generation draft controls or backend references are invalid. Current workspace was not changed.";return false;}
            std::string unavailable;
            const auto restoreBackend=[&](const std::filesystem::path& source,const std::string& hash,jobs::Action action)
            {
                if(source.empty())return;
                const auto captured=captured_bytes(source,1024u*1024u);
                if(!captured||digest(*captured)!=hash){unavailable+=" Reload the changed or missing "+std::string(action==jobs::Action::image?"2D":"3D")+" backend settings.";return;}
                State backend;
                if(!load_settings(backend,source,action)
                    ||(action==jobs::Action::image?backend.image_settings_sha256:backend.model_settings_sha256)!=hash)
                {unavailable+=" Backend settings changed or could not be restored. Choose them again.";return;}
                if(action==jobs::Action::image)restored.image_settings=std::move(backend.image_settings);
                else restored.model_settings=std::move(backend.model_settings);
            };
            restoreBackend(restored.image_settings_path,restored.image_settings_sha256,jobs::Action::image);
            restoreBackend(restored.model_settings_path,restored.model_settings_sha256,jobs::Action::model);
            // New state deliberately has no reference/output, pending task, native
            // model operation, import, license consent or previous human approval.
            diagnostic="Draft restored. Review a reference again before 3D generation. No output or approval was restored."+unavailable;
            restored.recall_path=state.recall_path;restored.recall_project=state.recall_project;restored.recall_attempted=true;
            restored.backend_selection_attempted=true;
            restored.status=diagnostic;state=std::move(restored);return true;
        }
        catch(...){diagnostic="The generation draft could not be read. Current workspace was not changed.";return false;}
    }
    bool remember_backend_selection(const State& state,const std::filesystem::path& path,std::string& diagnostic)
    {
        if(state.phase->load()!=Stage::idle||(!state.image_settings&&!state.model_settings))
        {diagnostic="No idle, explicitly selected generation backend could be remembered.";return false;}
        State selection;
        // Only the pinned source references enter the existing serializer.
        // Default draft controls deliberately omit the current prompt and any
        // completed output, imported asset, reference or approval.
        if(state.image_settings)
        {selection.image_settings_path=state.image_settings_path;selection.image_settings_sha256=state.image_settings_sha256;}
        if(state.model_settings)
        {selection.model_settings_path=state.model_settings_path;selection.model_settings_sha256=state.model_settings_sha256;}
        if(!save_draft(selection,path,diagnostic))return false;
        diagnostic="Selected generation backend references remembered locally. No prompts, outputs or approvals were saved.";
        return true;
    }
    bool restore_backend_selection(State& state,const std::filesystem::path& path,std::string& diagnostic)
    {
        diagnostic.clear();
        if(state.backend_selection_attempted||state.phase->load()!=Stage::idle)return false;
        state.backend_selection_attempted=true;
        std::error_code error;
        const auto status=std::filesystem::symlink_status(path,error);
        if(error==std::errc::no_such_file_or_directory||(!error&&!std::filesystem::exists(status)))return false;
        if(error||!draft_destination(path))
        {diagnostic="Remembered generation backend selection is unavailable or redirected. Choose the local backend settings again.";return false;}
        State restored;std::string restoredStatus;
        if(!load_draft(restored,path,restoredStatus))
        {diagnostic="Remembered generation backend selection could not be restored. "+restoredStatus;return false;}
        const auto ordinary_source=[](const std::filesystem::path& source){
            std::error_code failure;
            const auto canonical=std::filesystem::canonical(source,failure);
            return !failure&&canonical==source&&std::filesystem::is_regular_file(source,failure)&&!failure;
        };
        bool selected{};
        const auto merge=[&](auto& current,auto& candidate,auto& currentPath,auto& currentHash,
            const auto& candidatePath,const auto& candidateHash,std::string_view stage){
            if(current||candidatePath.empty())return;
            if(!candidate||!ordinary_source(candidatePath))
            {diagnostic+=" The remembered "+std::string{stage}+" backend settings changed, are missing or redirected. Choose them again.";return;}
            auto nextPath=candidatePath;auto nextHash=candidateHash;
            current=std::move(candidate);currentPath=std::move(nextPath);currentHash=std::move(nextHash);selected=true;
        };
        merge(state.image_settings,restored.image_settings,state.image_settings_path,state.image_settings_sha256,
            restored.image_settings_path,restored.image_settings_sha256,"2D");
        merge(state.model_settings,restored.model_settings,state.model_settings_path,state.model_settings_sha256,
            restored.model_settings_path,restored.model_settings_sha256,"3D");
        if(selected)diagnostic="Remembered generation backend settings restored. Executable/model bytes and memory will be checked when you explicitly generate; no model was loaded."+diagnostic;
        return selected;
    }
    namespace
    {
        using RecallJson=nlohmann::json;
        constexpr std::uint64_t recall_limit=64u*1024u;
        bool same_project(const ProjectIdentity& a,const ProjectIdentity& b) noexcept
        {return a.root==b.root&&a.id==b.id;}
        RecallJson read_recall_json(std::span<const std::byte> bytes)
        {
            std::vector<std::unordered_set<std::string>> objects;
            return RecallJson::parse(reinterpret_cast<const char*>(bytes.data()),
                reinterpret_cast<const char*>(bytes.data()+bytes.size()),
                [&](int depth,RecallJson::parse_event_t event,RecallJson& value)
                {
                    if(depth>16)throw std::invalid_argument{"Generation record is too deeply nested."};
                    if(event==RecallJson::parse_event_t::object_start)objects.emplace_back();
                    else if(event==RecallJson::parse_event_t::object_end)objects.pop_back();
                    else if(event==RecallJson::parse_event_t::key
                        &&!objects.back().insert(value.get<std::string>()).second)
                        throw std::invalid_argument{"Duplicate generation record field."};
                    return true;
                });
        }
        std::filesystem::path recall_source(const RecallJson& value,const char* key)
        {
            const auto text=value.at(key).get<std::string>();
            if(!draft_text(text,8192u))throw std::invalid_argument{"Invalid generation output path."};
            const std::filesystem::path path{std::u8string{text.begin(),text.end()}};
            if(!path.is_absolute()||path.lexically_normal()!=path)throw std::invalid_argument{"Generation output needs an ordinary absolute path."};
            return path;
        }
        Completion recalled_output(const RecallJson& value,bool model,ProjectIdentity project)
        {
            if(!value.is_object())throw std::invalid_argument{"Generation output record must be an object."};
            for(const auto& field:value.items())if(field.key()!="output"&&field.key()!="sha256"&&!(model&&field.key()=="reference_sha256"))
                throw std::invalid_argument{"Unknown generation output record field."};
            Completion output;output.project=std::move(project);output.retirement_verified=true;
            output.result.output=recall_source(value,"output");output.result.output_sha256=value.at("sha256").get<std::string>();
            if(!draft_source(output.result.output,output.result.output_sha256))throw std::invalid_argument{"Invalid generation output identity."};
            output.result.code=model?jobs::Code::awaiting_model_approval:jobs::Code::awaiting_reference_approval;
            if(model)output.reference_sha256=value.at("reference_sha256").get<std::string>();
            decode_output(output);
            if(!output.result||(model?!bool(output.model):!bool(output.image)))
                throw std::invalid_argument{"Retained generation output changed, is missing or cannot be decoded."};
            if(model)
            {
                for(const auto& primitive:output.model.model.primitives)
                {output.result.vertices+=primitive.geometry->vertices().size();output.result.faces+=primitive.geometry->indices().size()/3u;}
                output.result.materials=static_cast<std::uint32_t>(output.model.model.materials.size());
                output.result.textures=static_cast<std::uint32_t>(output.model.textures.size());
            }
            return output;
        }
        RecallJson output_record(const Completion& output,bool model)
        {
            RecallJson record{{"output",path_text(output.result.output)},{"sha256",output.result.output_sha256}};
            if(model)record["reference_sha256"]=output.reference_sha256;
            return record;
        }
        bool remember_completed(const State& state,std::string& diagnostic)
        {
            std::filesystem::path owned;
            try
            {
                if(state.recall_path.empty()||!state.recall_project)return true;
                if(!state.reference||!state.reference->result||!state.reference->image
                    ||!same_project(state.reference->project,*state.recall_project))return true;
                // Re-admit exact bytes before retaining their identity. A newer
                // image must not recall an older model made from another image.
                const auto& reference=*state.reference;
                const auto referenceBytes=captured_bytes(reference.result.output);
                if(!referenceBytes||digest(*referenceBytes)!=reference.result.output_sha256)
                    throw std::invalid_argument{"Reference changed before its recall record could be retained."};
                RecallJson record{{"schema","epoch.generation-recall.v1"},{"edition",EPOCH_SOURCE_EDITION_PUBLIC},
                    {"project_root",state.recall_project->root},{"project_id",state.recall_project->id},
                    {"reference",output_record(reference,false)}};
                if(state.generated_model&&state.generated_model->result&&state.generated_model->model
                    &&same_project(state.generated_model->project,*state.recall_project)
                    &&state.generated_model->reference_sha256==reference.result.output_sha256)
                {
                    const auto& model=*state.generated_model;const auto modelBytes=captured_bytes(model.result.output);
                    if(!modelBytes||digest(*modelBytes)!=model.result.output_sha256)
                        throw std::invalid_argument{"Model changed before its recall record could be retained."};
                    record["model"]=output_record(model,true);
                }
                const auto bytes=record.dump();if(bytes.size()>recall_limit)throw std::invalid_argument{"Generation recall record exceeds its limit."};
                auto path=state.recall_path;std::error_code error;
                if(!path.is_absolute()||path.lexically_normal()!=path
                    ||std::filesystem::canonical(path.parent_path(),error)!=path.parent_path()||error)
                    throw std::invalid_argument{"Generation recall folder is unavailable or redirected."};
                const auto status=std::filesystem::symlink_status(path,error);
                if(error&&error!=std::errc::no_such_file_or_directory)throw std::invalid_argument{"Generation recall destination is unavailable."};
                if(!error&&std::filesystem::exists(status)&&(!std::filesystem::is_regular_file(status)||std::filesystem::is_symlink(status)))
                    throw std::invalid_argument{"Generation recall destination is not an ordinary file."};
                static std::atomic<std::uint64_t> sequence{};
                for(unsigned attempt{};attempt<16u;++attempt)
                {
                    const auto temporary=path.parent_path()/('.'+path.filename().string()+"-write-"
                        +std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+'-'
                        +std::to_string(sequence.fetch_add(1u)));
                    if(platform::filesystem::exclusive_create_and_write(temporary,std::as_bytes(std::span{bytes}),error))
                    {owned=temporary;break;}
                }
                if(owned.empty()||!platform::filesystem::atomic_replace_same_filesystem(owned,path,error))
                    throw std::invalid_argument{"Generation recall record could not be published."};
                return true;
            }
            catch(const std::exception& error){std::error_code ignored;if(!owned.empty())std::filesystem::remove(owned,ignored);
                diagnostic=" Last-output recall was not saved: "+std::string{error.what()};return false;}
        }
    }
    bool begin_recall(State& state,const std::filesystem::path& path,ProjectIdentity project)
    {
        const bool changed=state.recall_path!=path||!state.recall_project||*state.recall_project!=project;
        state.recall_path=path;state.recall_project=std::move(project);
        if(changed)state.recall_attempted=false;
        if(state.recall_attempted||state.phase->load()!=Stage::idle)return false;
        state.recall_attempted=true;
        if(state.reference||state.generated_model)return false;
        std::error_code error;return std::filesystem::is_regular_file(path,error)&&!error;
    }
    Completion restore_last_completed(const std::filesystem::path& path,ProjectIdentity project,std::stop_token stop)
    {
        Completion result;result.project=project;result.recalled_workspace=true;
        try
        {
            if(stop.stop_requested()){result.result.code=jobs::Code::cancelled;return result;}
            const auto bytes=captured_bytes(path,recall_limit);if(!bytes)throw std::invalid_argument{"Last-output record is missing, redirected or too large."};
            const auto record=read_recall_json(*bytes);
            if(!record.is_object()||record.value("schema","")!="epoch.generation-recall.v1"
                ||!record.at("edition").is_number_integer()||record.at("edition").get<std::int64_t>()!=EPOCH_SOURCE_EDITION_PUBLIC
                ||record.at("project_root").get<std::string>()!=project.root||record.at("project_id").get<std::string>()!=project.id)
                throw std::invalid_argument{"Last-output record belongs to another project or edition."};
            for(const auto& item:record.items())if(item.key()!="schema"&&item.key()!="edition"&&item.key()!="project_root"&&item.key()!="project_id"&&item.key()!="reference"&&item.key()!="model")
                throw std::invalid_argument{"Unknown last-output record field."};
            auto reference=recalled_output(record.at("reference"),false,project);
            if(record.contains("model"))
            {
                result=recalled_output(record.at("model"),true,project);
                if(result.reference_sha256!=reference.result.output_sha256)throw std::invalid_argument{"Retained model/reference identities differ."};
                result.recalled_reference=std::make_shared<Completion>(std::move(reference));
            }
            else result=std::move(reference);
            if(stop.stop_requested()){result={};result.project=project;result.result.code=jobs::Code::cancelled;}
            result.recalled_workspace=true;result.result.diagnostic=result.result
                ?"Last completed outputs restored for review. Approvals were cleared; nothing was generated or imported."
                :"Output recall cancelled. Current workspace was preserved.";
        }
        catch(const std::exception& error){result={};result.project=project;result.recalled_workspace=true;
            result.result.diagnostic="Last outputs were not restored: "+std::string{error.what()};}
        return result;
    }
    Completion inspect_completed_generation(const std::filesystem::path& path,ProjectIdentity project,std::stop_token stop)
    {
        Completion result;result.project=project;result.recalled_workspace=true;
        try
        {
            if(stop.stop_requested()){result.result.code=jobs::Code::cancelled;return result;}
            const auto bytes=captured_bytes(path,1024u*1024u);if(!bytes)throw std::invalid_argument{"Choose an ordinary result.json up to1MiB."};
            const auto record=read_recall_json(*bytes);const auto schema=record.value("schema","");
            const bool model=schema=="epoch.asset_generation.model.v1";
            if((!model&&schema!="epoch.asset_generation.image.v1")
                ||record.value("state","")!=(model?"awaiting_model_approval":"awaiting_reference_approval"))
                throw std::invalid_argument{"This is not a completed supported generation result."};
            const auto output=recall_source(record,"output");
            if(output.parent_path()!=path.parent_path())throw std::invalid_argument{"Result output must be inside its own job folder."};
            RecallJson identity{{"output",path_text(output)},{"sha256",record.at("output_sha256")}};
            if(model)identity["reference_sha256"]=record.at("request").at("approved_reference_sha256");
            result=recalled_output(identity,model,project);
            if(model)
            {
                const auto reference=recalled_output(RecallJson{{"output",path_text(path.parent_path()/"approved-reference.png")},
                    {"sha256",result.reference_sha256}},false,project);
                result.recalled_reference=std::make_shared<Completion>(reference);
            }
            if(stop.stop_requested()){result={};result.project=project;result.result.code=jobs::Code::cancelled;}
            result.recalled_workspace=true;result.result.diagnostic=result.result
                ?"Completed generation opened for review. Its former approvals were not restored; nothing was imported."
                :"Opening completed generation was cancelled. Current workspace was preserved.";
        }
        catch(const std::exception& error){result={};result.project=project;result.recalled_workspace=true;
            result.result.diagnostic="Completed generation was not opened: "+std::string{error.what()};}
        return result;
    }
    std::string readiness_diagnostic(const State& state,jobs::Action action)
    {
        const auto& settings=action==jobs::Action::image?state.image_settings:state.model_settings;
        if(!settings)return action==jobs::Action::image
            ?"Load 2D Backend Settings first. Choose a local generator settings (.json) file; no download or model load starts here."
            :"Load 3D Backend Settings first. Choose a local generator settings (.json) file; no download or model load starts here.";
        const auto& seed=action==jobs::Action::image?state.image_seed:state.model_seed;
        std::uint32_t seedValue{};
        const auto parsed=std::from_chars(seed.data(),seed.data()+seed.size(),seedValue);
        if(seed.empty()||parsed.ec!=std::errc{}||parsed.ptr!=seed.data()+seed.size())
            return "Enter a whole-number seed from 0 to 4294967295.";
        if(action==jobs::Action::image)
        {
            if(state.image_request.prompt.empty())return "Enter a Main Prompt to enable Generate 2D.";
            return ai::image_generation::request_diagnostic(state.image_request);
        }
        if(!state.reference_approved||!state.reference||!state.reference->image)
            return "Choose a PNG reference and approve the visible image before Generate 3D.";
        const bool terms=settings->alternative_model_backend?settings->alternative_model_backend->terms_accepted
            :settings->model_backend.dinov3_terms_accepted;
        if(!terms)return "Review and accept this 3D backend's required terms before generation.";
        auto request=state.model_request;request.seed=seedValue;request.reference_path=state.reference->result.output;
        request.approved_reference_sha256=state.reference->result.output_sha256;
        return ai::model_generation::request_diagnostic(request);
    }
    std::string image_request_binding(const State& state,std::string_view prompt,std::string& diagnostic)
    {
        if(state.image_request.prompt!=prompt)
        {diagnostic="The chat request and visible Main Prompt differ. Review the current prompt before requesting generation.";return {};}
        State proposed;proposed.image_settings=state.image_settings;
        proposed.image_request=state.image_request;proposed.image_request.prompt=prompt;
        proposed.image_seed=state.image_seed;
        diagnostic=readiness_diagnostic(proposed,jobs::Action::image);
        if(!diagnostic.empty())return {};
        const auto& backend=state.image_settings->image_backend;
        std::ostringstream pinned;
        pinned<<"EPOCH_CHAT_IMAGE_REQUEST 1\n"<<std::quoted(prompt)<<' '
            <<std::quoted(state.image_request.prompt)<<' '
            <<std::quoted(state.image_request.negative_prompt)<<' '<<std::quoted(state.image_seed)<<' '
            <<state.image_request.width<<' '<<state.image_request.height<<' '<<state.image_request.steps<<' '
            <<std::quoted(path_text(state.image_settings_path))<<' '<<std::quoted(state.image_settings_sha256)<<' '
            <<std::quoted(path_text(backend.executable))<<' '<<backend.expected_executable.size_bytes<<' '
            <<std::quoted(backend.expected_executable.sha256)<<' '<<std::quoted(path_text(backend.models_cache_root));
        const auto bytes=pinned.str();return digest(std::as_bytes(std::span{bytes}));
    }
    std::string model_request_binding(const State& state,const ProjectIdentity& project,std::string& diagnostic)
    {
        diagnostic = readiness_diagnostic(state,jobs::Action::model);
        if (!diagnostic.empty()) return {};
        if (!current_project(*state.reference,project))
        {
            diagnostic = "The reference belongs to another project. Choose and approve it in this project.";
            return {};
        }
        const auto& settings = *state.model_settings;
        const auto& request = state.model_request;
        const auto& reference = state.reference->result;
        std::ostringstream pinned;
        pinned << "EPOCH_CHAT_MODEL_REQUEST 1\n"
            << std::quoted(path_text(reference.output)) << ' ' << std::quoted(reference.output_sha256) << ' '
            << std::quoted(state.model_seed) << ' ' << request.resolution << ' ' << request.steps << ' '
            << request.face_budget << ' ' << request.atlas << ' '
            << std::quoted(path_text(state.model_settings_path)) << ' ' << std::quoted(state.model_settings_sha256) << ' '
            << std::quoted(settings.backend_name);
        if (const auto& backend = settings.alternative_model_backend)
            pinned << ' ' << std::quoted(backend->id) << ' ' << std::quoted(backend->revision);
        else
        {
            const auto& nativeBackend = settings.model_backend;
            pinned << ' ' << std::quoted(path_text(nativeBackend.executable)) << ' '
                << nativeBackend.expected_executable.size_bytes << ' '
                << std::quoted(nativeBackend.expected_executable.sha256) << ' '
                << std::quoted(path_text(nativeBackend.models_cache_root));
        }
        // Both backends are loaded from the pinned settings file. The job
        // verifies the actual PNG bytes and executable before inference.
        const auto bytes = pinned.str();
        return digest(std::as_bytes(std::span{bytes}));
    }
    std::optional<jobs::PreparedJob> prepare_job(const State& state,jobs::Action action,
        const std::filesystem::path& work,std::string& error)
    {
        error=readiness_diagnostic(state,action);
        if(!error.empty())return {};
        auto request=action==jobs::Action::image?*state.image_settings:*state.model_settings;
        request.work_directory=work;
        const auto& seed=action==jobs::Action::image?state.image_seed:state.model_seed;
        std::uint32_t seedValue{};
        (void)std::from_chars(seed.data(),seed.data()+seed.size(),seedValue);
        if(action==jobs::Action::image){request.image=state.image_request;request.image.seed=seedValue;}
        else
        {
            request.model=state.model_request;request.model.seed=seedValue;
            request.model.reference_path=state.reference->result.output;
            request.model.approved_reference_sha256=state.reference->result.output_sha256;
        }
        return request;
    }
    Completion execute(jobs::PreparedJob job,ProjectIdentity project,
        std::shared_ptr<std::atomic<Stage>> progress,std::stop_token stop,bool foliageCutout)
    {
        const auto started=std::chrono::steady_clock::now();
        Completion result;result.project=std::move(project);result.backend=job.backend_name;
        result.prompt=job.image.prompt;result.reference_sha256=job.model.approved_reference_sha256;
        progress->store(Stage::waiting_for_models);
        std::string preflight;
        if(job.action==jobs::Action::image)
        {
            preflight=ai::image_generation::request_diagnostic(job.image);
            if(preflight.empty()&&(!job.image_backend.expected_executable.valid()
                ||!std::filesystem::is_regular_file(job.image_backend.executable)
                ||!std::filesystem::is_directory(job.image_backend.models_cache_root)))
                preflight="The selected local image runtime or model cache is unavailable.";
        }
        else if(job.alternative_model_backend)
        {
            if(!job.alternative_model_backend->terms_accepted||!job.alternative_model_backend->preflight)
                preflight="This local 3D backend is not admitted.";
            else preflight=job.alternative_model_backend->preflight(job.model);
        }
        else if(!job.model_backend.dinov3_terms_accepted)
            preflight="DINOv3 has separate terms. Read them and explicitly accept before this TRELLIS recipe can run.";
        if(!preflight.empty())
        {result.result.diagnostic=std::move(preflight);result.retirement_verified=true;progress->store(Stage::idle);return result;}
        const auto handoff=ai::native_runtime::run_asset_stage(
            [&](std::stop_token token,const ai::native_runtime::AssetStagePermit& permit)
            {
                progress->store(Stage::generating);
                result.result=jobs::execute(job,token,&permit);
                progress->store(Stage::restoring_models);
                return ai::native_runtime::AssetStageCompletion{bool(result.result),!result.result.worker.valid()||result.result.worker_retired};
            },stop);
        result.handoff_started=handoff.stage_started;result.coding_restored=handoff.restored;
        result.retirement_verified=handoff.workers_retired;result.handoff_diagnostic=handoff.diagnostic;
        if(!handoff.stage_started)
        {result.result.diagnostic=handoff.diagnostic;result.result.code=stop.stop_requested()?jobs::Code::cancelled:jobs::Code::model_work_active;}
        if(result.result&&stop.stop_requested())
        {result.result.code=jobs::Code::cancelled;result.result.diagnostic="Cancelled after output verification. Files are retained; nothing was approved or imported.";}
        if(result.result&&result.retirement_verified)decode_output(result);
        if (foliageCutout && result.result && result.retirement_verified && result.coding_restored)
        {
            auto& output = result.result;
            const auto fail = [&](std::string diagnostic) {
                output.code = stop.stop_requested() ? jobs::Code::cancelled : jobs::Code::invalid_output;
                output.diagnostic = std::move(diagnostic);
                result.image = {};
            };
            if (!result.image || output.output.parent_path() != output.work_directory)
                fail("The foliage image was not available in its verified work directory.");
            else
            {
                auto& mip = result.image.mips.front();
                if (mip.row_pitch_bytes != mip.width * 4u || !image_codec::make_magenta_cutout(mip.texels, mip.width, mip.height))
                    fail("Foliage generation needs a visible plant on a solid magenta background or an existing alpha cutout. The original image was retained.");
                else
                {
                    const auto bytes = image_codec::encode_rgba_png(mip.texels, mip.width, mip.height);
                    auto path = output.work_directory / "foliage-cutout.png";
                    std::error_code error;
                    if (bytes.empty() || stop.stop_requested() || !platform::filesystem::exclusive_create_and_write(path,bytes,error))
                        fail("The foliage cutout could not be retained. The original image was preserved.");
                    else
                    {
                        output.output = std::move(path);
                        output.output_sha256 = digest(bytes);
                        decode_output(result);
                        if (output) output.diagnostic += " Review the transparent foliage PNG before applying it.";
                    }
                }
            }
        }
        result.wall_ms=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count());
        progress->store(Stage::idle);return result;
    }
    Completion inspect_reference(const std::filesystem::path& path,ProjectIdentity project,std::stop_token stop)
    {
        Completion result;result.project=std::move(project);result.retirement_verified=true;result.coding_restored=true;
        if(stop.stop_requested()){result.result.code=jobs::Code::cancelled;return result;}
        const auto bytes=captured_bytes(path);
        if(!bytes){result.result.diagnostic="Choose an ordinary local PNG file up to 64 MiB.";return result;}
        result.result.output=path;result.result.output_sha256=digest(*bytes);
        result.result.code=jobs::Code::awaiting_reference_approval;result.result.worker_retired=true;
        decode_output(result);return result;
    }
    bool current_project(const Completion& result,const ProjectIdentity& current) noexcept
    {return result.project==current;}
    void accept_completion(State& state,Completion completed)
    {
        state.phase->store(Stage::idle);
        state.status=completed.result.diagnostic;
        if(completed.output_copy)return;
        if(!completed.handoff_diagnostic.empty())state.status+=" "+completed.handoff_diagnostic;
        if(!completed.result)return;
        if(completed.recalled_workspace)
        {
            state.reference.reset();state.generated_model.reset();state.reference_approved=false;
            state.image_sprite={};state.texture_sprite={};state.preview_markers.clear();state.preview_dirty=true;
            if(completed.recalled_reference)state.reference=std::move(*completed.recalled_reference);
            completed.recalled_reference.reset();
        }
        if(completed.result.code==jobs::Code::awaiting_reference_approval)
        {
            state.reference=std::move(completed);state.reference_approved=false;state.image_sprite={};
            state.status+=" PNG ready. Review it, then approve as the 3D reference or import it as a standalone texture.";
        }
        else
        {
            state.generated_model=std::move(completed);state.preview_dirty=true;state.texture_sprite={};
            state.status+=" 3D output retained for human review; nothing was imported.";
        }
        std::string recallDiagnostic;if(!remember_completed(state,recallDiagnostic))state.status+=recallDiagnostic;
    }
    bool reference_current(const State& state,std::string& error)
    {
        if(!state.reference||!state.reference->result||!state.reference->image)
        {error="No validated reference is visible.";return false;}
        const auto bytes=captured_bytes(state.reference->result.output);
        if(!bytes||digest(*bytes)!=state.reference->result.output_sha256)
        {error="The reference changed. Choose and review it again.";return false;}
        return true;
    }
    bool approve_reference(State& state,std::string& error)
    {
        if(!reference_current(state,error)){state.reference_approved=false;return false;}
        state.reference_approved=true;error="Approved this exact PNG as the 3D reference. No 3D generation or project import was started.";return true;
    }
    bool prepare_preview(State& state)
    {
        if(!state.preview_dirty&&state.published_view==state.view)return !state.preview_markers.empty();
        state.preview_markers.clear();state.preview_dirty=false;state.published_view=state.view;
        if(!state.generated_model||!state.generated_model->model)return false;
        const auto& compiled=state.generated_model->model;
        for(std::size_t i=0;i<compiled.model.primitives.size();++i)
        {
            const auto& primitive=compiled.model.primitives[i];
            previewgrid::ObjectMarker marker;marker.mesh=primitive.geometry;
            marker.primitive=previewgrid::ObjectPreviewPrimitive::IndexedMesh;
            const float units=static_cast<float>(compiled.model.meters_per_unit);marker.scale={units,units,units};
            marker.wireframe=state.view==View::wireframe;
            if(state.view==View::textured)
            {
                const auto& texture=compiled.textures.at(compiled.primitive_textures.at(i));
                const auto sealed=texture_artifact::seal_compiled_texture(texture.artifact);
                if(!sealed){state.status="Texture preview could not retain the validated artifact.";state.preview_markers.clear();return false;}
                const auto& material=compiled.model.materials.at(*primitive.material);
                const auto& source=material.base_color_texture->sampler;
                if((source.min_filter&&*source.min_filter>=9984u)
                    ||(source.min_filter&&source.mag_filter&&source.min_filter!=source.mag_filter))
                {state.status="This generated sampler needs unsupported mipmap or mixed filtering. The output remains retained.";state.preview_markers.clear();return false;}
                auto binding=std::make_shared<previewgrid::MeshBaseColorBinding>();binding->artifact=sealed.artifact;
                const auto key=texture.artifact.identity.key.words;
                binding->logical={key[0]?key[0]:1u,texture_artifact::compiled_artifact_revision(texture.artifact.identity)};
                const auto address=[](std::uint32_t value){return value==10497u?AddressMode::repeat
                    :value==33648u?AddressMode::mirrored_repeat:AddressMode::clamp_to_edge;};
                binding->sampler.address_u=address(source.wrap_s);binding->sampler.address_v=address(source.wrap_t);
                binding->sampler.min_filter=source.min_filter.value_or(9729u)==9728u?FilterMode::nearest:FilterMode::linear;
                binding->sampler.mag_filter=source.mag_filter.value_or(9729u)==9728u?FilterMode::nearest:FilterMode::linear;
                if(!previewgrid::valid_mesh_base_color_binding(*binding)){state.status="This texture sampler is not supported by the current 3D preview.";state.preview_markers.clear();return false;}
                marker.baseColorTexture=std::move(binding);
            }
            state.preview_markers.push_back(std::move(marker));
        }
        return !state.preview_markers.empty();
    }
    DrawResult draw(State& state,float width,float height,bool pending,DrawPermissions permissions)
    {
        DrawResult result;const auto workspaceOrigin=gui::cursor_position();
        width=(std::max)(1.0f,width);height=(std::max)(1.0f,height);
        const bool modelPreview=prepare_preview(state);
        const bool referencePreview=state.reference&&bool(state.reference->image);
        const bool columns=width>=640.0f&&(modelPreview||referencePreview);
        const float controls=columns?(std::min)(440.0f,width*0.5f):width;
        const auto row_options=[](float available)
        {return gui::InlineButtonRowOptions{.sizing=gui::InlineButtonSizing::FitContent,
            .alignment=gui::InlineButtonAlignment::Center,.wrap=true,.available_width=available};};
        const auto action_row=[&](auto buttons)
        {return gui::inline_button_row(buttons,30.0f,6.0f,row_options(controls-16.0f));};
        const std::array referenceApprovalButtons{gui::InlineButtonSpec{.label="Approve Reference",.width=0.0f,
            .enabled=!pending&&!state.reference_approved}};
        const auto reference_approval=[&](float available)
        {
            if(gui::inline_button_row(referenceApprovalButtons,30.0f,6.0f,row_options(available)))
                result.action=Action::approve_reference;
        };
        const std::array cancelButtons{gui::InlineButtonSpec{.label="Cancel Generation",.width=0.0f}};
        const float cancelHeight=pending?gui::inline_button_row_height(cancelButtons,30.0f,6.0f,row_options(width-16.0f)):0.0f;
        const float titleHeight=height>=160.0f?gui::measure_section_heading_height("Asset Generation",width-16.0f):0.0f;
        const auto status=pending?stage_text(state.phase->load()):state.status;
        const float statusBudget=(std::min)((std::max)(0.0f,height-cancelHeight-titleHeight-16.0f),
            (std::min)(96.0f,(std::max)(gui::line_height(),height-cancelHeight-titleHeight-80.0f)));
        const float statusHeight=(std::min)(gui::measure_wrapped_label_height(status,width-16.0f),statusBudget);
        const float headerHeight=(std::min)(height,cancelHeight+titleHeight+statusHeight+16.0f);
        gui::panel_rect(workspaceOrigin,{width,headerHeight});
        gui::set_cursor({workspaceOrigin.x+8.0f,workspaceOrigin.y+4.0f});
        if(pending&&gui::inline_button_row(cancelButtons,30.0f,6.0f,row_options(width-16.0f)))result.action=Action::cancel;
        if(titleHeight>0.0f)gui::section_heading("Asset Generation");
        if(statusHeight>0.0f)
        {
            (void)gui::begin_scroll_area({.id="asset-generation-status",.size={width-16.0f,statusHeight},.draw_background=false});
            gui::wrapped_label(status,width-16.0f);gui::end_scroll_area();
        }
        const gui::Vec2 origin{workspaceOrigin.x,workspaceOrigin.y+headerHeight};
        const float bodyHeight=(std::max)(0.0f,height-headerHeight);
        gui::set_cursor(origin);
        if(bodyHeight<64.0f)
        {gui::panel_rect(origin,{width,bodyHeight});return result;}
        if(!columns&&state.preview_expanded&&modelPreview&&bodyHeight>100.0f)
        {
            gui::panel_rect(origin,{width,(std::min)(bodyHeight,84.0f)});
            if(action_row(std::array{gui::InlineButtonSpec{.label="Back to Generation",.width=0.0f}}))state.preview_expanded=false;
            if(const auto mode=action_row(std::array{gui::InlineButtonSpec{.label="Textured",.width=0.0f},
                gui::InlineButtonSpec{.label="Solid",.width=0.0f},gui::InlineButtonSpec{.label="Wireframe",.width=0.0f}}))state.view=static_cast<View>(*mode);
            result.preview={{origin.x,origin.y+84.0f},{width,bodyHeight-84.0f}};result.preview_visible=true;return result;
        }
        // Own every pixel except the real admitted native mesh viewport.
        gui::panel_rect(origin,{columns&&modelPreview?controls+8.0f:width,bodyHeight});
        (void)gui::begin_scroll_area({.id="asset-generation-controls",.size={controls,bodyHeight},.draw_background=false});
        gui::wrapped_label("Create an image or texture. Approve a reference for 3D. Review before importing into your project.",controls-16.0f);
        if(const auto action=action_row(std::array{
            gui::InlineButtonSpec{.label="Save Generation Draft...",.width=0.0f,.enabled=!pending},
            gui::InlineButtonSpec{.label="Load Generation Draft...",.width=0.0f,.enabled=!pending}}))
            result.action=*action==0?Action::save_draft:Action::load_draft;
        if(action_row(std::array{gui::InlineButtonSpec{.label="Open Completed Generation...",.width=0.0f,.enabled=!pending}}))
            result.action=Action::open_completed_generation;
        gui::section_heading("1. Image / texture");
        gui::wrapped_label(state.image_settings?state.image_settings->backend_name:"No local 2D backend settings selected",controls-16.0f);
        if(action_row(std::array{gui::InlineButtonSpec{.label="Load 2D Backend Settings...",.width=0.0f,.enabled=!pending}}))result.action=Action::load_image_settings;
        const float promptWidth=(std::max)(48.0f,controls-16.0f);
        const auto promptHeight=[&](std::string_view text,float minimumLines,float maximumLines)
        {
            const float line=gui::line_height();
            const float minimum=minimumLines*line+16.0f;
            const float maximum=(std::max)(minimum,(std::min)(maximumLines*line+16.0f,bodyHeight*.45f));
            return (std::clamp)(gui::measure_wrapped_label_height(text,promptWidth-26.0f)+16.0f,minimum,maximum);
        };
        (void)gui::edit_box(state.image_request.prompt,{promptWidth,promptHeight(state.image_request.prompt,3.0f,12.0f)},
            8192u,true,gui::TextInputKeyboardPolicy::focused,false,"Main Prompt",true);
        (void)gui::edit_box(state.image_request.negative_prompt,{promptWidth,promptHeight(state.image_request.negative_prompt,2.0f,6.0f)},
            4096u,true,gui::TextInputKeyboardPolicy::focused,false,"Avoidance Prompt (optional)",true);
        const auto integer=[&](const char* id,const char* label,std::uint32_t& value,float low,float high,float step)
        {gui::label(std::string(label)+": "+std::to_string(value));const auto changed=gui::slider({.id=id,.minimum=low,.maximum=high,
            .value=static_cast<float>(value),.step=step,.size={controls-16.0f,26.0f}});if(changed.changed&&!pending)value=static_cast<std::uint32_t>(changed.value);};
        gui::label("Seed (same seed repeats the starting noise)");(void)gui::edit_box(state.image_seed,{controls-16.0f,28.0f},10u,false,
            gui::TextInputKeyboardPolicy::focused,false,{},true);
        integer("asset-width","Width",state.image_request.width,256,2048,64);
        integer("asset-height","Height",state.image_request.height,256,2048,64);
        integer("asset-image-steps","Steps",state.image_request.steps,1,8,1);
        const auto imageBlocked=readiness_diagnostic(state,jobs::Action::image);
        const bool imageSetupRequired=!state.image_settings;
        gui::wrapped_label(pending?"Generation is busy. Cancel Generation remains available above."
            :imageBlocked.empty()?"Ready to queue. Generator files, installed models and free memory are verified before work starts.":imageBlocked,controls-16.0f);
        if(const auto action=action_row(std::array{
            gui::InlineButtonSpec{.label=imageSetupRequired?"Set Up 2D Generation...":"Generate 2D",
                .width=0.0f,.enabled=!pending&&(imageSetupRequired||imageBlocked.empty())},
            gui::InlineButtonSpec{.label="Choose PNG Reference...",.width=0.0f,.enabled=!pending}}))
            result.action=*action==0?(imageSetupRequired?Action::load_image_settings:Action::generate_image):Action::choose_reference;
        if(referencePreview)
        {
            gui::wrapped_label("Working image: "+path_text(state.reference->result.output),controls-16.0f);
            gui::wrapped_label("Save a copy to keep it outside the working cache, or import it into your open project.",controls-16.0f);
            if(const auto action=action_row(std::array{
                gui::InlineButtonSpec{.label="Save Image As...",.width=0.0f,.enabled=!pending},
                gui::InlineButtonSpec{.label="Copy Image Location",.width=0.0f,.enabled=!pending}}))
                result.action=*action==0?Action::save_image:Action::copy_image_path;
            const auto& image=state.reference->image;const auto& mip=image.mips.front();
            if(!state.image_sprite.is_valid())state.image_sprite=gui::register_runtime_surface("asset-generation-reference-"+state.reference->result.output_sha256,
                std::span{reinterpret_cast<const std::uint8_t*>(mip.texels.data()),mip.texels.size()},mip.width,mip.height);
            if(!columns||modelPreview)
            {
                (void)gui::image_box({.id="asset-generation-reference",.sprite=state.image_sprite,.size={controls-16.0f,180.0f},
                    .source_size={static_cast<float>(mip.width),static_cast<float>(mip.height)},.caption=state.reference_approved?"Approved 3D reference":"Reference awaiting your approval"});
                reference_approval(controls-16.0f);
            }
            if(action_row(std::array{gui::InlineButtonSpec{.label="Import as Texture",.width=0.0f,
                .enabled=!pending&&permissions.project_open&&permissions.reference_project_current}}))
                result.action=Action::import_texture;
        }
        if(referencePreview)
        {
            if(!permissions.project_open)gui::wrapped_label("Open a project to import this image as a texture. Standalone image and 3D generation remain available.",controls-16.0f);
            else if(!permissions.reference_project_current)gui::wrapped_label("The project changed. Choose this PNG again to review it for the current project before importing.",controls-16.0f);
        }
        gui::section_heading("2. 3D model");
        gui::wrapped_label(state.model_settings?state.model_settings->backend_name:"No local 3D backend settings selected",controls-16.0f);
        if(action_row(std::array{gui::InlineButtonSpec{.label="Load 3D Backend Settings...",.width=0.0f,.enabled=!pending}}))result.action=Action::load_model_settings;
        gui::label("3D seed");(void)gui::edit_box(state.model_seed,{controls-16.0f,28.0f},10u,false,
            gui::TextInputKeyboardPolicy::focused,false,{},true);
        if(state.model_settings&&!state.model_settings->alternative_model_backend)
        {
            gui::wrapped_label("TRELLIS uses DINOv3 conditioning with separate access/license terms. Review the model terms before accepting. Settings files do not grant acceptance in this workspace.",controls-16.0f);
            if(!pending)(void)gui::toggle_switch("I accept the separate DINOv3 terms",state.model_settings->model_backend.dinov3_terms_accepted,{controls-16.0f,28.0f});
        }
        integer("asset-faces","Maximum faces",state.model_request.face_budget,1000,100000,1000);
        constexpr std::array<std::string_view,4> atlasLabels{"256","512","1024","2048"};
        const auto atlas=gui::select_box({.id="asset-atlas",.placeholder="Texture size",
            .selected=std::to_string(state.model_request.atlas),.options=atlasLabels,.size={controls-16.0f,28.0f}});
        if(!pending&&atlas.changed&&atlas.selected_index)state.model_request.atlas=256u<<*atlas.selected_index;
        if(state.model_settings&&state.model_settings->alternative_model_backend
            &&state.model_settings->alternative_model_backend->id=="triposr-pytorch-offline")
            gui::wrapped_label("Use a tightly framed object with a transparent background. TripoSR does not remove backgrounds.",controls-16.0f);
        const auto modelBlocked=readiness_diagnostic(state,jobs::Action::model);
        if(!modelBlocked.empty())gui::wrapped_label(modelBlocked,controls-16.0f);
        if(action_row(std::array{gui::InlineButtonSpec{.label="Generate 3D",.width=0.0f,
            .enabled=!pending&&modelBlocked.empty()}}))result.action=Action::generate_model;
        if(state.generated_model)
        {
            if(!columns&&action_row(std::array{gui::InlineButtonSpec{.label="Open 3D Preview",.width=0.0f,
                .enabled=bool(state.generated_model->model)}}))state.preview_expanded=true;
            const auto& out=state.generated_model->result;
            gui::wrapped_label("Working model: "+path_text(out.output),controls-16.0f);
            if(const auto action=action_row(std::array{
                gui::InlineButtonSpec{.label="Save Model As...",.width=0.0f,.enabled=!pending},
                gui::InlineButtonSpec{.label="Copy Model Location",.width=0.0f,.enabled=!pending}}))
                result.action=*action==0?Action::save_model:Action::copy_model_path;
            gui::wrapped_label(std::to_string(out.vertices)+" vertices / "+std::to_string(out.faces)+" faces. "+std::to_string(out.textures)+" textures.",controls-16.0f);
            gui::wrapped_label("Generated output is not automatically game-ready. Collision, LOD, rigging and animation require separate checks.",controls-16.0f);
            if(action_row(std::array{gui::InlineButtonSpec{.label=permissions.model_review_ready?"Approve Project Import":"Review Project Import",.width=0.0f,
                .enabled=!pending&&permissions.project_open&&permissions.model_project_current&&bool(state.generated_model->model)}}))result.action=permissions.model_review_ready?Action::import_model:Action::review_model;
            if(!permissions.project_open)gui::wrapped_label("Open a project to review and import this retained model.",controls-16.0f);
            else if(!permissions.model_project_current)gui::wrapped_label("The project changed. Review this retained model through Assets before importing it here.",controls-16.0f);
            if(const auto mode=action_row(std::array{
                gui::InlineButtonSpec{.label="Textured",.width=0.0f},gui::InlineButtonSpec{.label="Solid",.width=0.0f},gui::InlineButtonSpec{.label="Wireframe",.width=0.0f}}))
                state.view=static_cast<View>(*mode);
            gui::wrapped_label("3D reference SHA-256: "+state.generated_model->reference_sha256,controls-16.0f);
            if(state.generated_model->recalled_workspace)
                gui::wrapped_label("Restored output. Original generation and model-handoff timings are not retained in this workspace record.",controls-16.0f);
            else gui::wrapped_label("Generation: "+std::to_string(out.elapsed_ms)+" ms. Including model handoff and restoration: "+std::to_string(state.generated_model->wall_ms)+" ms.",controls-16.0f);
            gui::wrapped_label("Output: "+path_text(out.output),controls-16.0f);
        }
        gui::end_scroll_area();
        if(columns&&modelPreview)
        {result.preview={{origin.x+controls+8.0f,origin.y},{(std::max)(1.0f,width-controls-8.0f),bodyHeight}};result.preview_visible=true;}
        else if(columns&&referencePreview)
        {
            const auto& mip=state.reference->image.mips.front();
            const float imageWidth=(std::max)(1.0f,width-controls-24.0f);
            const float approvalHeight=gui::inline_button_row_height(referenceApprovalButtons,30.0f,6.0f,row_options(imageWidth));
            const float imageHeight=(std::max)(1.0f,bodyHeight-24.0f-approvalHeight);
            const float imageX=origin.x+controls+16.0f;
            gui::set_cursor({imageX,origin.y+8.0f});
            (void)gui::image_box({.id="asset-generation-reference-large",.sprite=state.image_sprite,
                .size={imageWidth,imageHeight},
                .source_size={static_cast<float>(mip.width),static_cast<float>(mip.height)},
                .caption=state.reference_approved?"Approved 3D reference":"Reference awaiting your approval"});
            gui::set_cursor({imageX,origin.y+16.0f+imageHeight});
            reference_approval(imageWidth);
        }
        return result;
    }
}
