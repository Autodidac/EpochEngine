/*
 * This file is part of the Epoch Project.
 * epochengine - Modular C++ Framework
 *
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

module editor.code_workspace;

import platform.filesystem;

namespace epochengine::editor_code_workspace
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr std::size_t kAbsoluteMaximumFileBytes = 512u * 1024u;
        constexpr std::size_t kAbsoluteMaximumTabs = 64u;

        struct LoadedFile final
        {
            fs::path canonical_path{};
            std::string relative_path{};
            std::string text{};
        };

        [[nodiscard]] OperationResult result(
            const ResultCode code,
            std::string reason,
            const std::optional<DocumentHandle> document = std::nullopt,
            std::string text = {})
        {
            return OperationResult{
                .code = code,
                .reason = std::move(reason),
                .document = document,
                .text = std::move(text)};
        }

        [[nodiscard]] bool authority_valid(
            const WorkspaceAuthority& authority) noexcept
        {
            return authority.surface_revision != 0u
                && authority.project_revision != 0u
                && authority.async_generation != 0u;
        }

        [[nodiscard]] bool has_parent_component(const fs::path& path)
        {
            return std::any_of(
                path.begin(), path.end(),
                [](const fs::path& component)
                {
                    return component == "." || component == "..";
                });
        }

        [[nodiscard]] bool path_is_symlink(
            const fs::path& path,
            std::error_code& error)
        {
            const fs::file_status status = fs::symlink_status(path, error);
            return !error && fs::is_symlink(status);
        }

        [[nodiscard]] ResultCode validate_root(
            const fs::path& requested,
            fs::path& canonical,
            std::string& reason)
        {
            if (requested.empty() || !requested.is_absolute())
            {
                reason = "The code-workspace root must be an absolute path.";
                return ResultCode::invalid_request;
            }

            std::error_code error{};
            if (path_is_symlink(requested, error))
            {
                reason = "The code-workspace root is a symbolic link.";
                return ResultCode::symlink_path;
            }
            if (error)
            {
                reason = "The code-workspace root could not be inspected.";
                return ResultCode::unavailable;
            }

            canonical = fs::canonical(requested, error);
            if (error || canonical.empty()
                || !fs::is_directory(canonical, error) || error)
            {
                reason = "The code-workspace root is unavailable.";
                return ResultCode::unavailable;
            }
            return ResultCode::success;
        }

        [[nodiscard]] ResultCode resolve_file(
            const fs::path& root,
            const std::string_view relative_text,
            const std::size_t maximum_bytes,
            LoadedFile& loaded,
            std::string& reason)
        {
            const fs::path relative{relative_text};
            if (relative.empty() || relative.is_absolute()
                || has_parent_component(relative))
            {
                reason = "The requested code path is not a canonical relative path.";
                return ResultCode::external_path;
            }

            fs::path inspected = root;
            std::error_code error{};
            for (const fs::path& component : relative)
            {
                inspected /= component;
                if (path_is_symlink(inspected, error))
                {
                    reason = "The requested code path crosses a symbolic link.";
                    return ResultCode::symlink_path;
                }
                if (error)
                {
                    reason = "The requested code path could not be inspected.";
                    return ResultCode::missing_file;
                }
            }

            const fs::path candidate = fs::canonical(root / relative, error);
            if (error || candidate.empty()
                || !fs::is_regular_file(candidate, error) || error)
            {
                reason = "The requested code file does not exist as a regular file.";
                return ResultCode::missing_file;
            }

            const fs::path contained = candidate.lexically_relative(root);
            if (contained.empty() || contained.is_absolute()
                || has_parent_component(contained))
            {
                reason = "The requested code file resolves outside the workspace root.";
                return ResultCode::external_path;
            }

            const std::uintmax_t size = fs::file_size(candidate, error);
            if (error)
            {
                reason = "The requested code file size could not be read.";
                return ResultCode::unavailable;
            }
            if (size > maximum_bytes
                || size > static_cast<std::uintmax_t>(
                    (std::numeric_limits<std::size_t>::max)()))
            {
                reason = "The requested code file exceeds the workspace byte limit.";
                return ResultCode::oversized_file;
            }

            std::ifstream input(candidate, std::ios::binary);
            if (!input)
            {
                reason = "The requested code file could not be opened.";
                return ResultCode::unavailable;
            }

            std::string text(static_cast<std::size_t>(size), '\0');
            if (!text.empty())
            {
                input.read(text.data(), static_cast<std::streamsize>(text.size()));
                if (!input || static_cast<std::size_t>(input.gcount()) != text.size())
                {
                    reason = "The requested code file changed while it was read.";
                    return ResultCode::stale_disk;
                }
            }
            if (!valid_utf8(text))
            {
                reason = "The requested code file is not valid UTF-8 text.";
                return ResultCode::invalid_utf8;
            }

            loaded.canonical_path = candidate;
            loaded.relative_path = contained.generic_string();
            loaded.text = std::move(text);
            return ResultCode::success;
        }

        [[nodiscard]] std::vector<std::size_t> line_starts(
            const std::string_view text)
        {
            std::vector<std::size_t> result{0u};
            result.reserve(1u + static_cast<std::size_t>(
                std::count(text.begin(), text.end(), '\n')));
            for (std::size_t index = 0u; index < text.size(); ++index)
            {
                if (text[index] == '\n')
                    result.push_back(index + 1u);
            }
            return result;
        }

        [[nodiscard]] std::size_t next_codepoint(
            const std::string_view text,
            const std::size_t index) noexcept
        {
            if (index >= text.size())
                return text.size();
            const unsigned char lead = static_cast<unsigned char>(text[index]);
            if (lead < 0x80u)
                return index + 1u;
            if (lead < 0xe0u)
                return (std::min)(text.size(), index + 2u);
            if (lead < 0xf0u)
                return (std::min)(text.size(), index + 3u);
            return (std::min)(text.size(), index + 4u);
        }

        [[nodiscard]] std::size_t codepoint_count(
            const std::string_view text,
            std::size_t begin,
            const std::size_t end) noexcept
        {
            std::size_t count{};
            begin = (std::min)(begin, text.size());
            while (begin < (std::min)(end, text.size()))
            {
                begin = next_codepoint(text, begin);
                ++count;
            }
            return count;
        }

        [[nodiscard]] std::size_t maximum_columns(
            const std::string_view text,
            const std::vector<std::size_t>& starts) noexcept
        {
            std::size_t maximum{};
            for (std::size_t line = 0u; line < starts.size(); ++line)
            {
                const std::size_t begin = starts[line];
                std::size_t end = line + 1u < starts.size()
                    ? starts[line + 1u] - 1u
                    : text.size();
                if (end > begin && text[end - 1u] == '\r')
                    --end;
                maximum = (std::max)(
                    maximum,
                    codepoint_count(text, begin, end));
            }
            return maximum;
        }

        [[nodiscard]] Viewport clamp_viewport(
            Viewport viewport,
            const std::size_t lines,
            const std::size_t columns) noexcept
        {
            viewport.visible_lines = (std::max)(std::size_t{1u}, viewport.visible_lines);
            viewport.visible_columns = (std::max)(std::size_t{1u}, viewport.visible_columns);
            const std::size_t maximum_first_line = lines > viewport.visible_lines
                ? lines - viewport.visible_lines
                : 0u;
            const std::size_t maximum_first_column = columns > viewport.visible_columns
                ? columns - viewport.visible_columns
                : 0u;
            viewport.first_line = (std::min)(viewport.first_line, maximum_first_line);
            viewport.first_column = (std::min)(viewport.first_column, maximum_first_column);
            return viewport;
        }

        [[nodiscard]] bool read_exact(
            const fs::path& path,
            std::string& bytes)
        {
            std::error_code error{};
            const std::uintmax_t size = fs::file_size(path, error);
            if (error || size > static_cast<std::uintmax_t>(
                    (std::numeric_limits<std::size_t>::max)()))
            {
                return false;
            }
            std::ifstream input(path, std::ios::binary);
            if (!input)
                return false;
            bytes.assign(static_cast<std::size_t>(size), '\0');
            if (!bytes.empty())
            {
                input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
                return input
                    && static_cast<std::size_t>(input.gcount()) == bytes.size();
            }
            return true;
        }
    }

    struct Controller::Implementation final
    {
        struct Document final
        {
            DocumentHandle handle{};
            fs::path canonical_path{};
            std::string relative_path{};
            std::string label{};
            std::string text{};
            std::string persisted_text{};
            std::vector<std::size_t> line_starts{0u};
            std::uint64_t revision{1u};
            std::uint64_t persisted_revision{1u};
            TextRange selection{};
            Viewport viewport{};
            std::size_t maximum_bytes{256u * 1024u};
            bool writable{};
        };

        WorkspaceKind kind{WorkspaceKind::project_scripts};
        std::string workspace_id{};
        fs::path canonical_root{};
        WorkspaceAuthority authority{};
        std::vector<Document> documents{};
        std::optional<DocumentHandle> active{};
        std::uint64_t revision{};
        std::uint32_t next_generation{1u};
        bool configured{};

        [[nodiscard]] Document* find(const DocumentHandle handle) noexcept
        {
            if (!handle.valid() || handle.index >= documents.size())
                return nullptr;
            Document& document = documents[handle.index];
            return document.handle == handle ? &document : nullptr;
        }

        [[nodiscard]] const Document* find(
            const DocumentHandle handle) const noexcept
        {
            if (!handle.valid() || handle.index >= documents.size())
                return nullptr;
            const Document& document = documents[handle.index];
            return document.handle == handle ? &document : nullptr;
        }

        [[nodiscard]] bool dirty() const noexcept
        {
            return std::any_of(
                documents.begin(), documents.end(),
                [](const Document& document)
                {
                    return document.text != document.persisted_text;
                });
        }
    };

    namespace
    {
        [[nodiscard]] OperationResult validate_operation(
            const Controller::Implementation& state,
            const WorkspaceAuthority& expected)
        {
            if (!state.configured)
                return result(ResultCode::unavailable, "No code workspace is configured.");
            if (expected.surface_revision != state.authority.surface_revision)
                return result(ResultCode::stale_surface, "The editor surface revision changed.");
            if (expected.project_revision != state.authority.project_revision)
                return result(ResultCode::stale_project, "The project/source authority revision changed.");
            if (expected.async_generation != state.authority.async_generation)
                return result(ResultCode::stale_async_generation, "The asynchronous workspace generation changed.");
            return result(ResultCode::success, "Workspace authority is current.");
        }

        [[nodiscard]] OperationResult validate_document_operation(
            const Controller::Implementation& state,
            const DocumentHandle handle,
            const WorkspaceAuthority& expected,
            const std::uint64_t expected_revision,
            const Controller::Implementation::Document*& document)
        {
            if (const OperationResult authority = validate_operation(state, expected);
                !authority)
            {
                return authority;
            }
            document = state.find(handle);
            if (!document)
                return result(ResultCode::stale_document, "The code document handle is stale.");
            if (document->revision != expected_revision)
                return result(ResultCode::stale_document, "The code document revision changed.", handle);
            return result(ResultCode::success, "Code document authority is current.", handle);
        }

        [[nodiscard]] DocumentSnapshot make_snapshot(
            const Controller::Implementation::Document& document,
            const std::optional<DocumentHandle> active)
        {
            return DocumentSnapshot{
                .handle = document.handle,
                .relative_path = document.relative_path,
                .label = document.label,
                .text = document.text,
                .revision = document.revision,
                .persisted_revision = document.persisted_revision,
                .byte_count = document.text.size(),
                .line_count = document.line_starts.size(),
                .maximum_line_columns = maximum_columns(
                    document.text, document.line_starts),
                .selection = document.selection,
                .viewport = document.viewport,
                .active = active && *active == document.handle,
                .dirty = document.text != document.persisted_text,
                .writable = document.writable};
        }
    }

    std::string_view to_string(const ResultCode code) noexcept
    {
        switch (code)
        {
        case ResultCode::success: return "success";
        case ResultCode::invalid_request: return "invalid_request";
        case ResultCode::unavailable: return "unavailable";
        case ResultCode::external_path: return "external_path";
        case ResultCode::symlink_path: return "symlink_path";
        case ResultCode::missing_file: return "missing_file";
        case ResultCode::oversized_file: return "oversized_file";
        case ResultCode::invalid_utf8: return "invalid_utf8";
        case ResultCode::duplicate_path: return "duplicate_path";
        case ResultCode::tab_limit: return "tab_limit";
        case ResultCode::stale_surface: return "stale_surface";
        case ResultCode::stale_project: return "stale_project";
        case ResultCode::stale_document: return "stale_document";
        case ResultCode::stale_async_generation: return "stale_async_generation";
        case ResultCode::stale_disk: return "stale_disk";
        case ResultCode::dirty_workspace: return "dirty_workspace";
        case ResultCode::dirty_document: return "dirty_document";
        case ResultCode::read_only: return "read_only";
        case ResultCode::write_failed: return "write_failed";
        case ResultCode::verification_failed: return "verification_failed";
        }
        return "unknown";
    }

    bool valid_utf8(const std::string_view text) noexcept
    {
        std::size_t index{};
        while (index < text.size())
        {
            const unsigned char lead = static_cast<unsigned char>(text[index]);
            if (lead == 0u)
                return false;
            if (lead < 0x80u)
            {
                ++index;
                continue;
            }

            std::size_t count{};
            std::uint32_t value{};
            std::uint32_t minimum{};
            if (lead >= 0xc2u && lead <= 0xdfu)
            {
                count = 2u;
                value = lead & 0x1fu;
                minimum = 0x80u;
            }
            else if (lead >= 0xe0u && lead <= 0xefu)
            {
                count = 3u;
                value = lead & 0x0fu;
                minimum = 0x800u;
            }
            else if (lead >= 0xf0u && lead <= 0xf4u)
            {
                count = 4u;
                value = lead & 0x07u;
                minimum = 0x1'0000u;
            }
            else
            {
                return false;
            }
            if (index + count > text.size())
                return false;
            for (std::size_t offset = 1u; offset < count; ++offset)
            {
                const unsigned char continuation =
                    static_cast<unsigned char>(text[index + offset]);
                if ((continuation & 0xc0u) != 0x80u)
                    return false;
                value = (value << 6u) | (continuation & 0x3fu);
            }
            if (value < minimum || value > 0x10'ffffu
                || (value >= 0xd800u && value <= 0xdfffu))
            {
                return false;
            }
            index += count;
        }
        return true;
    }

    std::optional<std::size_t> byte_offset(
        const std::string_view text,
        const TextPosition position) noexcept
    {
        if (!valid_utf8(text))
            return std::nullopt;
        const std::vector<std::size_t> starts = line_starts(text);
        if (position.line >= starts.size())
            return std::nullopt;
        const std::size_t begin = starts[position.line];
        std::size_t end = position.line + 1u < starts.size()
            ? starts[position.line + 1u] - 1u
            : text.size();
        if (end > begin && text[end - 1u] == '\r')
            --end;
        std::size_t cursor = begin;
        for (std::size_t column = 0u; column < position.column; ++column)
        {
            if (cursor >= end)
                return std::nullopt;
            cursor = next_codepoint(text, cursor);
        }
        return cursor <= end ? std::optional<std::size_t>{cursor} : std::nullopt;
    }

    TextPosition text_position(
        const std::string_view text,
        const std::size_t requested_offset) noexcept
    {
        if (!valid_utf8(text))
            return {};
        const std::size_t offset = (std::min)(requested_offset, text.size());
        const std::vector<std::size_t> starts = line_starts(text);
        const auto upper = std::upper_bound(starts.begin(), starts.end(), offset);
        const std::size_t line = upper == starts.begin()
            ? 0u
            : static_cast<std::size_t>(std::distance(starts.begin(), upper) - 1);
        return TextPosition{
            .line = line,
            .column = codepoint_count(text, starts[line], offset)};
    }

    Controller::Controller()
        : implementation_(std::make_unique<Implementation>())
    {
    }

    Controller::~Controller() = default;
    Controller::Controller(Controller&&) noexcept = default;
    Controller& Controller::operator=(Controller&&) noexcept = default;

    OperationResult Controller::open(const OpenRequest& request)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        if (request.workspace_id.empty() || !authority_valid(request.authority)
            || request.paths.empty() || request.maximum_file_bytes == 0u
            || request.maximum_file_bytes > kAbsoluteMaximumFileBytes
            || request.maximum_tabs == 0u
            || request.maximum_tabs > kAbsoluteMaximumTabs)
        {
            return result(ResultCode::invalid_request, "The code-workspace request is incomplete or exceeds a hard bound.");
        }
        if (request.paths.size() > request.maximum_tabs)
            return result(ResultCode::tab_limit, "The code-workspace request exceeds its tab limit.");

        fs::path root{};
        std::string reason{};
        if (const ResultCode root_code = validate_root(request.root, root, reason);
            root_code != ResultCode::success)
        {
            return result(root_code, std::move(reason));
        }

        Implementation& current = *implementation_;
        const bool same_identity = current.configured
            && current.kind == request.kind
            && current.workspace_id == request.workspace_id
            && current.canonical_root == root
            && current.authority == request.authority;
        if (current.dirty() && !same_identity)
            return result(ResultCode::dirty_workspace, "The current code workspace has unsaved documents.");

        std::vector<LoadedFile> loaded{};
        loaded.reserve(request.paths.size());
        for (const PathRequest& path : request.paths)
        {
            LoadedFile file{};
            const ResultCode code = resolve_file(
                root, path.relative_path, request.maximum_file_bytes, file, reason);
            if (code != ResultCode::success)
                return result(code, std::move(reason));
            if (std::any_of(
                    loaded.begin(), loaded.end(),
                    [&](const LoadedFile& existing)
                    {
                        return existing.canonical_path == file.canonical_path;
                    }))
            {
                return result(ResultCode::duplicate_path, "The code-workspace request contains the same file more than once.");
            }
            loaded.push_back(std::move(file));
        }

        if (same_identity && loaded.size() == current.documents.size())
        {
            bool same_paths = true;
            for (std::size_t index = 0u; index < loaded.size(); ++index)
            {
                same_paths = same_paths
                    && loaded[index].canonical_path
                        == current.documents[index].canonical_path;
            }
            if (same_paths)
            {
                return result(
                    ResultCode::success,
                    "The requested code workspace is already open.",
                    current.active);
            }
        }
        if (current.dirty())
            return result(ResultCode::dirty_workspace, "The current code workspace has unsaved documents.");

        Implementation replacement{};
        replacement.kind = request.kind;
        replacement.workspace_id = request.workspace_id;
        replacement.canonical_root = std::move(root);
        replacement.authority = request.authority;
        replacement.revision = current.revision + 1u;
        replacement.next_generation = current.next_generation;
        replacement.configured = true;
        replacement.documents.reserve(loaded.size());
        for (std::size_t index = 0u; index < loaded.size(); ++index)
        {
            const PathRequest& path_request = request.paths[index];
            LoadedFile& file = loaded[index];
            const DocumentHandle handle{
                .index = static_cast<std::uint32_t>(index),
                .generation = replacement.next_generation++};
            if (replacement.next_generation == 0u)
                replacement.next_generation = 1u;
            std::vector<std::size_t> starts = line_starts(file.text);
            replacement.documents.push_back(Implementation::Document{
                .handle = handle,
                .canonical_path = std::move(file.canonical_path),
                .relative_path = std::move(file.relative_path),
                .label = fs::path{path_request.relative_path}.filename().string(),
                .text = file.text,
                .persisted_text = std::move(file.text),
                .line_starts = std::move(starts),
                .revision = 1u,
                .persisted_revision = 1u,
                .selection = {},
                .viewport = {},
                .maximum_bytes = request.maximum_file_bytes,
                .writable = path_request.writable});
        }
        replacement.active = replacement.documents.front().handle;
        *implementation_ = std::move(replacement);
        return result(
            ResultCode::success,
            "Opened the bounded code workspace.",
            implementation_->active);
    }

    OperationResult Controller::activate(
        const DocumentHandle document,
        const WorkspaceAuthority& expected)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        if (const OperationResult authority = validate_operation(*implementation_, expected);
            !authority)
        {
            return authority;
        }
        if (!implementation_->find(document))
            return result(ResultCode::stale_document, "The requested code tab is stale.");
        implementation_->active = document;
        ++implementation_->revision;
        return result(ResultCode::success, "Activated the requested code tab.", document);
    }

    OperationResult Controller::replace_text(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision,
        std::string text)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        if (!checked->writable)
            return result(ResultCode::read_only, "The reviewed code document is read-only.", handle);
        if (text.size() > checked->maximum_bytes)
            return result(ResultCode::oversized_file, "The edited code exceeds the workspace byte limit.", handle);
        if (!valid_utf8(text))
            return result(ResultCode::invalid_utf8, "The edited code is not valid UTF-8 text.", handle);

        Implementation::Document& document = *implementation_->find(handle);
        if (text == document.text)
            return result(ResultCode::success, "The code document is unchanged.", handle);
        document.text = std::move(text);
        document.line_starts = line_starts(document.text);
        ++document.revision;
        document.selection = {};
        document.viewport = clamp_viewport(
            document.viewport,
            document.line_starts.size(),
            maximum_columns(document.text, document.line_starts));
        ++implementation_->revision;
        return result(ResultCode::success, "Updated the in-memory code document.", handle);
    }

    OperationResult Controller::set_selection(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision,
        const TextRange selection)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        if (!byte_offset(checked->text, selection.anchor)
            || !byte_offset(checked->text, selection.caret))
        {
            return result(ResultCode::invalid_request, "The UTF-8 selection is outside the code document.", handle);
        }
        implementation_->find(handle)->selection = selection;
        return result(ResultCode::success, "Updated the code selection.", handle);
    }

    OperationResult Controller::copy_selection(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision) const
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* document{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, document);
            !validation)
        {
            return validation;
        }
        const auto anchor = byte_offset(document->text, document->selection.anchor);
        const auto caret = byte_offset(document->text, document->selection.caret);
        if (!anchor || !caret)
            return result(ResultCode::invalid_request, "The UTF-8 selection is outside the code document.", handle);
        const std::size_t begin = (std::min)(*anchor, *caret);
        const std::size_t end = (std::max)(*anchor, *caret);
        return result(
            ResultCode::success,
            "Copied the selected UTF-8 code range.",
            handle,
            document->text.substr(begin, end - begin));
    }

    OperationResult Controller::set_viewport(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision,
        const Viewport viewport)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        Implementation::Document& document = *implementation_->find(handle);
        document.viewport = clamp_viewport(
            viewport,
            document.line_starts.size(),
            maximum_columns(document.text, document.line_starts));
        return result(ResultCode::success, "Clamped the independent code viewport.", handle);
    }

    OperationResult Controller::goto_line(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision,
        const std::size_t one_based_line)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        if (one_based_line == 0u)
            return result(ResultCode::invalid_request, "Go To Line uses one-based line numbers.", handle);
        Implementation::Document& document = *implementation_->find(handle);
        const std::size_t target = (std::min)(
            one_based_line - 1u,
            document.line_starts.size() - 1u);
        document.selection = TextRange{
            .anchor = {.line = target, .column = 0u},
            .caret = {.line = target, .column = 0u}};
        document.viewport.first_line = target;
        document.viewport.first_column = 0u;
        document.viewport = clamp_viewport(
            document.viewport,
            document.line_starts.size(),
            maximum_columns(document.text, document.line_starts));
        return result(ResultCode::success, "Moved to the requested code line.", handle);
    }

    OperationResult Controller::reload(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision,
        const bool discard_dirty)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        if (checked->text != checked->persisted_text && !discard_dirty)
            return result(ResultCode::dirty_document, "Reload requires explicit permission to discard local edits.", handle);

        LoadedFile loaded{};
        std::string reason{};
        const ResultCode code = resolve_file(
            implementation_->canonical_root,
            checked->relative_path,
            checked->maximum_bytes,
            loaded,
            reason);
        if (code != ResultCode::success)
            return result(code, std::move(reason), handle);

        Implementation::Document& document = *implementation_->find(handle);
        if (loaded.canonical_path != document.canonical_path)
            return result(ResultCode::stale_disk, "The code file identity changed before reload.", handle);
        document.text = std::move(loaded.text);
        document.persisted_text = document.text;
        document.line_starts = line_starts(document.text);
        ++document.revision;
        document.persisted_revision = document.revision;
        document.selection = {};
        document.viewport = {};
        ++implementation_->revision;
        return result(ResultCode::success, "Reloaded the verified code document.", handle);
    }

    OperationResult Controller::save(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        if (!checked->writable)
            return result(ResultCode::read_only, "The reviewed code document is read-only.", handle);
        if (checked->text == checked->persisted_text)
            return result(ResultCode::success, "The code document is already saved.", handle);

        LoadedFile current{};
        std::string reason{};
        ResultCode code = resolve_file(
            implementation_->canonical_root,
            checked->relative_path,
            checked->maximum_bytes,
            current,
            reason);
        if (code != ResultCode::success)
            return result(code, std::move(reason), handle);
        if (current.canonical_path != checked->canonical_path
            || current.text != checked->persisted_text)
        {
            return result(ResultCode::stale_disk, "The code file changed on disk after it was opened.", handle);
        }

        const fs::path temporary = checked->canonical_path.parent_path()
            / (checked->canonical_path.filename().string()
                + ".epoch-workspace-"
                + std::to_string(handle.generation)
                + "-" + std::to_string(checked->revision) + ".tmp");
        std::error_code error{};
        fs::remove(temporary, error);
        error.clear();
        const std::span<const char> characters{
            checked->text.data(), checked->text.size()};
        if (!platform::filesystem::exclusive_create_and_write(
                temporary, std::as_bytes(characters), error))
        {
            return result(ResultCode::write_failed, "The verified temporary code file could not be written.", handle);
        }

        std::string temporary_bytes{};
        if (!read_exact(temporary, temporary_bytes)
            || temporary_bytes != checked->text)
        {
            fs::remove(temporary, error);
            return result(ResultCode::verification_failed, "The temporary code file failed byte verification.", handle);
        }

        std::string precommit_bytes{};
        if (!read_exact(checked->canonical_path, precommit_bytes)
            || precommit_bytes != checked->persisted_text)
        {
            fs::remove(temporary, error);
            return result(ResultCode::stale_disk, "The code file changed immediately before atomic replacement.", handle);
        }

        if (!platform::filesystem::atomic_replace_same_filesystem(
                temporary, checked->canonical_path, error))
        {
            fs::remove(temporary, error);
            return result(ResultCode::write_failed, "Atomic code-file replacement failed.", handle);
        }

        std::string committed{};
        if (!read_exact(checked->canonical_path, committed)
            || committed != checked->text)
        {
            return result(ResultCode::verification_failed, "The committed code file failed byte verification.", handle);
        }

        Implementation::Document& document = *implementation_->find(handle);
        document.persisted_text = document.text;
        document.persisted_revision = document.revision;
        ++implementation_->revision;
        return result(ResultCode::success, "Saved the code document by verified atomic replacement.", handle);
    }

    WorkspaceSnapshot Controller::snapshot() const
    {
        WorkspaceSnapshot output{};
        if (!implementation_)
            return output;
        output.kind = implementation_->kind;
        output.workspace_id = implementation_->workspace_id;
        output.canonical_root = implementation_->canonical_root.generic_string();
        output.authority = implementation_->authority;
        output.active_document = implementation_->active;
        output.revision = implementation_->revision;
        output.configured = implementation_->configured;
        output.dirty = implementation_->dirty();
        output.documents.reserve(implementation_->documents.size());
        for (const Implementation::Document& document : implementation_->documents)
            output.documents.push_back(make_snapshot(document, implementation_->active));
        return output;
    }

    std::optional<DocumentSnapshot> Controller::document(
        const DocumentHandle handle) const
    {
        if (!implementation_)
            return std::nullopt;
        const Implementation::Document* document = implementation_->find(handle);
        if (!document)
            return std::nullopt;
        return make_snapshot(*document, implementation_->active);
    }

    std::optional<DocumentSnapshot> Controller::active_document() const
    {
        if (!implementation_ || !implementation_->active)
            return std::nullopt;
        return document(*implementation_->active);
    }

    bool Controller::has_dirty_documents() const noexcept
    {
        return implementation_ && implementation_->dirty();
    }
}
