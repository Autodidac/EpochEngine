/*
 * This file is part of the Epoch Project.
 * epochengine - Modular C++ Framework
 *
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
module;

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module editor.code_workspace;

export namespace epochengine::editor_code_workspace
{
    enum class WorkspaceKind : std::uint8_t
    {
        project_scripts = 0,
        curated_engine_source
    };

    enum class ResultCode : std::uint8_t
    {
        success = 0,
        invalid_request,
        unavailable,
        external_path,
        symlink_path,
        missing_file,
        oversized_file,
        invalid_utf8,
        duplicate_path,
        tab_limit,
        stale_surface,
        stale_project,
        stale_document,
        stale_async_generation,
        stale_disk,
        dirty_workspace,
        dirty_document,
        read_only,
        write_failed,
        verification_failed
    };

    [[nodiscard]] std::string_view to_string(ResultCode code) noexcept;

    struct WorkspaceAuthority final
    {
        std::uint64_t surface_revision{};
        std::uint64_t project_revision{};
        std::uint64_t async_generation{};

        friend bool operator==(
            const WorkspaceAuthority&,
            const WorkspaceAuthority&) = default;
    };

    struct DocumentHandle final
    {
        std::uint32_t index{};
        std::uint32_t generation{};

        [[nodiscard]] bool valid() const noexcept
        {
            return generation != 0u;
        }

        friend bool operator==(
            const DocumentHandle&,
            const DocumentHandle&) = default;
    };

    struct TextPosition final
    {
        std::size_t line{};
        std::size_t column{};

        friend bool operator==(
            const TextPosition&,
            const TextPosition&) = default;
    };

    struct TextRange final
    {
        TextPosition anchor{};
        TextPosition caret{};

        friend bool operator==(
            const TextRange&,
            const TextRange&) = default;
    };

    struct Viewport final
    {
        std::size_t first_line{};
        std::size_t first_column{};
        std::size_t visible_lines{1u};
        std::size_t visible_columns{1u};

        friend bool operator==(
            const Viewport&,
            const Viewport&) = default;
    };

    struct PathRequest final
    {
        std::string relative_path{};
        bool writable{};
    };

    struct OpenRequest final
    {
        WorkspaceKind kind{WorkspaceKind::project_scripts};
        std::string workspace_id{};
        std::filesystem::path root{};
        WorkspaceAuthority authority{};
        std::vector<PathRequest> paths{};
        std::size_t maximum_file_bytes{256u * 1024u};
        std::size_t maximum_tabs{32u};
    };

    struct DocumentSnapshot final
    {
        DocumentHandle handle{};
        std::string relative_path{};
        std::string label{};
        std::string text{};
        std::uint64_t revision{};
        std::uint64_t persisted_revision{};
        std::size_t byte_count{};
        std::size_t line_count{};
        std::size_t maximum_line_columns{};
        TextRange selection{};
        Viewport viewport{};
        bool active{};
        bool dirty{};
        bool writable{};
        bool utf8_bom{};
    };

    struct WorkspaceSnapshot final
    {
        WorkspaceKind kind{WorkspaceKind::project_scripts};
        std::string workspace_id{};
        std::string canonical_root{};
        WorkspaceAuthority authority{};
        std::vector<DocumentSnapshot> documents{};
        std::optional<DocumentHandle> active_document{};
        std::uint64_t revision{};
        bool configured{};
        bool dirty{};
    };

    struct OperationResult final
    {
        ResultCode code{ResultCode::unavailable};
        std::string reason{};
        std::optional<DocumentHandle> document{};
        std::string text{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    class Controller final
    {
    public:
        struct Implementation;

        Controller();
        ~Controller();
        Controller(Controller&&) noexcept;
        Controller& operator=(Controller&&) noexcept;
        Controller(const Controller&) = delete;
        Controller& operator=(const Controller&) = delete;

        [[nodiscard]] OperationResult open(const OpenRequest& request);
        [[nodiscard]] OperationResult activate(
            DocumentHandle document,
            const WorkspaceAuthority& expected);
        [[nodiscard]] OperationResult close(
            DocumentHandle document,
            const WorkspaceAuthority& expected,
            std::uint64_t expected_document_revision,
            bool discard_dirty);
        [[nodiscard]] OperationResult replace_text(
            DocumentHandle document,
            const WorkspaceAuthority& expected,
            std::uint64_t expected_document_revision,
            std::string text);
        [[nodiscard]] OperationResult set_selection(
            DocumentHandle document,
            const WorkspaceAuthority& expected,
            std::uint64_t expected_document_revision,
            TextRange selection);
        [[nodiscard]] OperationResult copy_selection(
            DocumentHandle document,
            const WorkspaceAuthority& expected,
            std::uint64_t expected_document_revision) const;
        [[nodiscard]] OperationResult set_viewport(
            DocumentHandle document,
            const WorkspaceAuthority& expected,
            std::uint64_t expected_document_revision,
            Viewport viewport);
        [[nodiscard]] OperationResult goto_line(
            DocumentHandle document,
            const WorkspaceAuthority& expected,
            std::uint64_t expected_document_revision,
            std::size_t one_based_line);
        [[nodiscard]] OperationResult reload(
            DocumentHandle document,
            const WorkspaceAuthority& expected,
            std::uint64_t expected_document_revision,
            bool discard_dirty);
        [[nodiscard]] OperationResult save(
            DocumentHandle document,
            const WorkspaceAuthority& expected,
            std::uint64_t expected_document_revision);

        [[nodiscard]] WorkspaceSnapshot snapshot() const;
        [[nodiscard]] std::optional<DocumentSnapshot> document(
            DocumentHandle document) const;
        [[nodiscard]] std::optional<DocumentSnapshot> active_document() const;
        [[nodiscard]] bool has_dirty_documents() const noexcept;

    private:
        std::unique_ptr<Implementation> implementation_{};
    };

    [[nodiscard]] bool valid_utf8(std::string_view text) noexcept;
    [[nodiscard]] std::optional<std::size_t> byte_offset(
        std::string_view text,
        TextPosition position) noexcept;
    [[nodiscard]] TextPosition text_position(
        std::string_view text,
        std::size_t byte_offset) noexcept;
    [[nodiscard]] bool run_contract();
}
