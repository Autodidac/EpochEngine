// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>
export module editor.asset_generation;
export import ai.asset_jobs;
import asset.model_import;
import asset.texture_artifact;
import gui.engine;
import render.preview_grid;
import sprite.handle;

export namespace epochengine::editor_asset_generation
{
    enum class Stage : std::uint8_t { idle, waiting_for_models, generating, restoring_models, inspecting_reference, restoring_outputs, exporting_output };
    enum class Action : std::uint8_t { none, load_image_settings, load_model_settings, choose_reference,
        generate_image, approve_reference, generate_model, import_texture, review_model, import_model, cancel,
        save_draft, load_draft, open_completed_generation, save_image, save_model,
        copy_image_path, copy_model_path };
    enum class View : std::uint8_t { textured, solid, wireframe };
    struct ProjectIdentity final
    {
        std::string root{}, id{};
        std::uint64_t generation{};
        friend bool operator==(const ProjectIdentity&,const ProjectIdentity&)=default;
    };
    struct Completion final
    {
        ai::asset_jobs::Result result{};
        ProjectIdentity project{};
        asset::texture::CompiledTextureArtifact image{};
        asset::model_import::BaseColorCompilation model{};
        bool handoff_started{}, coding_restored{}, retirement_verified{};
        std::string handoff_diagnostic{}, reference_sha256{}, prompt{}, backend{};
        std::uint64_t wall_ms{};
        bool recalled_workspace{}, output_copy{};
        std::shared_ptr<Completion> recalled_reference{};
    };
    struct State final
    {
        std::optional<ai::asset_jobs::PreparedJob> image_settings{}, model_settings{};
        std::filesystem::path image_settings_path{}, model_settings_path{};
        std::string image_settings_sha256{}, model_settings_sha256{};
        ai::image_generation::Request image_request{};
        ai::model_generation::Request model_request{};
        std::optional<Completion> reference{}, generated_model{};
        bool reference_approved{};
        std::string status{"Load your local 2D or 3D backend settings. No models are downloaded or loaded by opening this workspace."};
        std::shared_ptr<std::atomic<Stage>> phase{std::make_shared<std::atomic<Stage>>(Stage::idle)};
        SpriteHandle image_sprite{}, texture_sprite{};
        std::vector<previewgrid::ObjectMarker> preview_markers{};
        View view{View::textured}, published_view{View::textured};
        bool preview_dirty{true}, preview_expanded{};
        std::string image_seed{"42"}, model_seed{"42"};
        float scroll{};
        std::filesystem::path recall_path{};
        std::filesystem::path active_work_directory{};
        std::optional<ProjectIdentity> recall_project{};
        bool recall_attempted{};
        bool backend_selection_attempted{};
    };
    struct DrawPermissions final
    {
        bool project_open{}, reference_project_current{}, model_project_current{}, model_review_ready{};
    };
    struct DrawResult final
    {
        Action action{Action::none};
        gui::WidgetBounds preview{};
        bool preview_visible{};
    };
    [[nodiscard]] bool load_settings(State&,const std::filesystem::path&,ai::asset_jobs::Action);
    // Explicit local document operations only. Drafts contain controls and pinned
    // settings-file references, never outputs, callbacks, credentials or approvals.
    [[nodiscard]] bool save_draft(const State&,const std::filesystem::path&,std::string& diagnostic);
    [[nodiscard]] bool load_draft(State&,const std::filesystem::path&,std::string& diagnostic);
    // The existing draft codec stores only explicitly selected backend pins.
    // Restore merges unselected stages; prompts, outputs and current approvals
    // remain owned by the live workspace. Neither operation launches a model.
    [[nodiscard]] bool remember_backend_selection(const State&,const std::filesystem::path&,std::string& diagnostic);
    [[nodiscard]] bool restore_backend_selection(State&,const std::filesystem::path&,std::string& diagnostic);
    // Explicit export of verified bytes only; never imports, approves or replaces
    // an existing destination. Generation working files remain separately owned.
    [[nodiscard]] bool save_output_copy(const Completion&,const std::filesystem::path&,std::string& diagnostic,std::stop_token = {});
    // Exact local output recall only; no backend settings, consent or import.
    [[nodiscard]] bool begin_recall(State&,const std::filesystem::path&,ProjectIdentity);
    [[nodiscard]] Completion restore_last_completed(const std::filesystem::path&,ProjectIdentity,std::stop_token);
    [[nodiscard]] Completion inspect_completed_generation(const std::filesystem::path&,ProjectIdentity,std::stop_token);
    // Pure control validation for visible guidance and the dispatch gate. It
    // never scans model files, queries hardware, loads models or starts work.
    [[nodiscard]] std::string readiness_diagnostic(const State&,ai::asset_jobs::Action);
    // Pins a literal chat image request to the visible controls and selected
    // backend. Pure input binding only; no file/model/hardware access.
    [[nodiscard]] std::string image_request_binding(const State&,std::string_view prompt,std::string& diagnostic);
    [[nodiscard]] std::string model_request_binding(const State&,const ProjectIdentity&,std::string& diagnostic);
    [[nodiscard]] std::optional<ai::asset_jobs::PreparedJob> prepare_job(const State&,
        ai::asset_jobs::Action,const std::filesystem::path& new_work,std::string& diagnostic);
    [[nodiscard]] Completion execute(ai::asset_jobs::PreparedJob,ProjectIdentity,
        std::shared_ptr<std::atomic<Stage>>,std::stop_token,bool foliageCutout = false);
    [[nodiscard]] Completion inspect_reference(const std::filesystem::path&,ProjectIdentity,std::stop_token);
    void accept_completion(State&,Completion);
    [[nodiscard]] bool reference_current(const State&,std::string& diagnostic);
    [[nodiscard]] bool approve_reference(State&,std::string& diagnostic);
    [[nodiscard]] bool current_project(const Completion&,const ProjectIdentity&) noexcept;
    [[nodiscard]] bool prepare_preview(State&);
    [[nodiscard]] DrawResult draw(State&,float width,float height,bool pending,DrawPermissions);
}
