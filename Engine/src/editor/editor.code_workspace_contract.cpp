/*
 * This file is part of the Epoch Project.
 * epochengine - Modular C++ Framework
 *
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
module;

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module editor.code_workspace;

namespace epochengine::editor_code_workspace
{
    namespace
    {
        namespace fs = std::filesystem;

        struct Fixture final
        {
            fs::path root{};

            Fixture()
            {
                const auto stamp = std::chrono::steady_clock::now()
                    .time_since_epoch().count();
                root = fs::temp_directory_path()
                    / ("epoch-code-workspace-contract-"
                        + std::to_string(stamp));
                std::error_code error{};
                fs::create_directories(root / "Scripts", error);
                if (!error)
                    fs::create_directories(root / "Engine/src", error);
            }

            ~Fixture()
            {
                std::error_code error{};
                const fs::path temporary = fs::temp_directory_path(error);
                if (!error)
                {
                    const fs::path canonical_root = fs::weakly_canonical(root, error);
                    const fs::path canonical_temporary =
                        fs::weakly_canonical(temporary, error);
                    const fs::path contained =
                        canonical_root.lexically_relative(canonical_temporary);
                    if (!error
                        && !canonical_root.empty()
                        && canonical_root != canonical_temporary
                        && !contained.empty()
                        && !contained.is_absolute()
                        && *contained.begin() != "..")
                    {
                        fs::remove_all(canonical_root, error);
                    }
                }
            }

            [[nodiscard]] bool write(
                const std::string_view relative,
                const std::string_view bytes) const
            {
                const fs::path destination = root / fs::path{relative};
                std::error_code error{};
                fs::create_directories(destination.parent_path(), error);
                if (error)
                    return false;
                std::ofstream output(
                    destination,
                    std::ios::binary | std::ios::trunc);
                output.write(
                    bytes.data(),
                    static_cast<std::streamsize>(bytes.size()));
                return output.good();
            }

            [[nodiscard]] std::string read(
                const std::string_view relative) const
            {
                std::ifstream input(root / fs::path{relative}, std::ios::binary);
                return std::string{
                    std::istreambuf_iterator<char>{input},
                    std::istreambuf_iterator<char>{}};
            }
        };

        [[nodiscard]] WorkspaceAuthority authority(
            const std::uint64_t surface = 10u,
            const std::uint64_t project = 20u,
            const std::uint64_t async = 30u) noexcept
        {
            return WorkspaceAuthority{
                .surface_revision = surface,
                .project_revision = project,
                .async_generation = async};
        }

        [[nodiscard]] OpenRequest project_request(
            const Fixture& fixture)
        {
            return OpenRequest{
                .kind = WorkspaceKind::project_scripts,
                .workspace_id = "project-alpha",
                .root = fixture.root,
                .authority = authority(),
                .paths = {
                    {.relative_path = "Scripts/alpha.ascript.cpp", .writable = true},
                    {.relative_path = "Scripts/beta.ascript.cpp", .writable = true}},
                .maximum_file_bytes = 1'024u,
                .maximum_tabs = 4u};
        }

        [[nodiscard]] bool utf8_contract()
        {
            const std::string text = "a\xc3\xa9\n\xe7\x95\x8cz";
            if (!valid_utf8(text)
                || valid_utf8(std::string_view{"\xc0\x80", 2u})
                || valid_utf8(std::string_view{"x\0y", 3u}))
            {
                return false;
            }
            const auto first_line_end = byte_offset(text, {.line = 0u, .column = 2u});
            const auto second_column = byte_offset(text, {.line = 1u, .column = 1u});
            return first_line_end && *first_line_end == 3u
                && second_column && *second_column == 7u
                && text_position(text, 7u) == TextPosition{.line = 1u, .column = 1u}
                && !byte_offset(text, {.line = 1u, .column = 3u})
                && !byte_offset(text, {.line = 2u, .column = 0u});
        }

        [[nodiscard]] bool tabs_edit_selection_and_viewport_contract(
            const Fixture& fixture)
        {
            Controller controller{};
            const OpenRequest request = project_request(fixture);
            const OperationResult opened = controller.open(request);
            const WorkspaceSnapshot initial = controller.snapshot();
            if (!opened || initial.documents.size() != 2u
                || !initial.active_document
                || initial.documents[0].relative_path
                    != "Scripts/alpha.ascript.cpp"
                || initial.documents[1].relative_path
                    != "Scripts/beta.ascript.cpp")
            {
                return false;
            }

            const DocumentSnapshot alpha = initial.documents[0];
            const std::string edited = "line one\nwide \xe7\x95\x8c text\nlast\n";
            if (!controller.replace_text(
                    alpha.handle, request.authority, alpha.revision, edited))
            {
                return false;
            }
            const DocumentSnapshot changed = *controller.document(alpha.handle);
            if (!changed.dirty || changed.revision == alpha.revision)
                return false;
            if (!controller.set_selection(
                    changed.handle,
                    request.authority,
                    changed.revision,
                    TextRange{
                        .anchor = {.line = 1u, .column = 5u},
                        .caret = {.line = 1u, .column = 6u}}))
            {
                return false;
            }
            const OperationResult copied = controller.copy_selection(
                changed.handle, request.authority, changed.revision);
            if (!copied || copied.text != "\xe7\x95\x8c")
                return false;

            if (!controller.set_viewport(
                    changed.handle,
                    request.authority,
                    changed.revision,
                    Viewport{
                        .first_line = 999u,
                        .first_column = 999u,
                        .visible_lines = 2u,
                        .visible_columns = 4u}))
            {
                return false;
            }
            const DocumentSnapshot scrolled = *controller.document(changed.handle);
            if (scrolled.viewport.first_line != 2u
                || scrolled.viewport.first_column == 0u)
            {
                return false;
            }
            if (!controller.goto_line(
                    changed.handle,
                    request.authority,
                    changed.revision,
                    2u))
            {
                return false;
            }
            const DocumentSnapshot located = *controller.document(changed.handle);
            if (located.selection.caret
                    != TextPosition{.line = 1u, .column = 0u})
            {
                return false;
            }

            const DocumentSnapshot beta = initial.documents[1];
            if (!controller.activate(beta.handle, request.authority)
                || !controller.active_document()
                || controller.active_document()->handle != beta.handle
                || !controller.document(alpha.handle)->dirty)
            {
                return false;
            }
            if (!controller.open(request)
                || !controller.active_document()
                || controller.active_document()->handle != beta.handle
                || !controller.document(alpha.handle)->dirty)
            {
                return false;
            }
            if (controller.close(
                    alpha.handle,
                    request.authority,
                    changed.revision,
                    false).code != ResultCode::dirty_document)
            {
                return false;
            }
            if (!controller.close(
                    alpha.handle,
                    request.authority,
                    changed.revision,
                    true))
            {
                return false;
            }
            const WorkspaceSnapshot closed = controller.snapshot();
            if (closed.documents.size() != 1u
                || closed.documents.front().relative_path
                    != "Scripts/beta.ascript.cpp"
                || !closed.active_document
                || closed.active_document
                    != closed.documents.front().handle
                || controller.document(alpha.handle))
            {
                return false;
            }
            return true;
        }

        [[nodiscard]] bool bom_roundtrip_contract(
            const Fixture& fixture)
        {
            Controller controller{};
            const OpenRequest request{
                .kind = WorkspaceKind::project_scripts,
                .workspace_id = "project-bom",
                .root = fixture.root,
                .authority = authority(70u, 80u, 90u),
                .paths = {{
                    .relative_path = "Scripts/bom.ascript.cpp",
                    .writable = true}},
                .maximum_file_bytes = 1'024u,
                .maximum_tabs = 2u};
            if (!controller.open(request))
                return false;
            const DocumentSnapshot opened = *controller.active_document();
            if (!opened.utf8_bom
                || opened.text != "hello \xe2\x9c\x93\n"
                || opened.text.starts_with("\xEF\xBB\xBF"))
            {
                return false;
            }

            if (!controller.replace_text(
                    opened.handle,
                    request.authority,
                    opened.revision,
                    "saved \xe7\x95\x8c\n"))
            {
                return false;
            }
            const DocumentSnapshot changed = *controller.active_document();
            if (!controller.save(
                    changed.handle,
                    request.authority,
                    changed.revision)
                || fixture.read("Scripts/bom.ascript.cpp")
                    != "\xEF\xBB\xBF" "saved \xe7\x95\x8c\n")
            {
                return false;
            }

            if (!fixture.write("Scripts/bom.ascript.cpp", "plain utf-8\n")
                || !controller.reload(
                    changed.handle,
                    request.authority,
                    changed.revision,
                    true))
            {
                return false;
            }
            const DocumentSnapshot reloaded = *controller.active_document();
            return !reloaded.utf8_bom
                && reloaded.text == "plain utf-8\n";
        }

        [[nodiscard]] bool stale_and_atomic_save_contract(
            const Fixture& fixture)
        {
            Controller controller{};
            OpenRequest request = project_request(fixture);
            if (!controller.open(request))
                return false;
            const DocumentSnapshot original = controller.snapshot().documents[0];

            if (controller.replace_text(
                    original.handle,
                    authority(11u, 20u, 30u),
                    original.revision,
                    "changed").code != ResultCode::stale_surface
                || controller.replace_text(
                    original.handle,
                    authority(10u, 21u, 30u),
                    original.revision,
                    "changed").code != ResultCode::stale_project
                || controller.replace_text(
                    original.handle,
                    authority(10u, 20u, 31u),
                    original.revision,
                    "changed").code != ResultCode::stale_async_generation)
            {
                return false;
            }

            if (!controller.replace_text(
                    original.handle,
                    request.authority,
                    original.revision,
                    "updated \xe2\x9c\x93\n"))
            {
                return false;
            }
            const DocumentSnapshot changed = *controller.document(original.handle);
            if (controller.save(
                    changed.handle,
                    request.authority,
                    original.revision).code != ResultCode::stale_document)
            {
                return false;
            }

            if (!fixture.write("Scripts/alpha.ascript.cpp", "external\n")
                || controller.save(
                    changed.handle,
                    request.authority,
                    changed.revision).code != ResultCode::stale_disk)
            {
                return false;
            }
            if (!fixture.write("Scripts/alpha.ascript.cpp", "alpha\n"))
                return false;
            const OperationResult saved = controller.save(
                changed.handle, request.authority, changed.revision);
            if (!saved
                || fixture.read("Scripts/alpha.ascript.cpp")
                    != "updated \xe2\x9c\x93\n"
                || controller.document(changed.handle)->dirty)
            {
                return false;
            }
            return true;
        }

        [[nodiscard]] bool reload_and_isolation_contract(
            const Fixture& fixture)
        {
            Controller projects{};
            const OpenRequest request = project_request(fixture);
            if (!projects.open(request))
                return false;
            const DocumentSnapshot project_document = projects.snapshot().documents[0];
            if (!projects.replace_text(
                    project_document.handle,
                    request.authority,
                    project_document.revision,
                    "dirty project\n"))
            {
                return false;
            }
            const DocumentSnapshot dirty = *projects.document(project_document.handle);
            if (projects.reload(
                    dirty.handle,
                    request.authority,
                    dirty.revision,
                    false).code != ResultCode::dirty_document)
            {
                return false;
            }
            if (!projects.reload(
                    dirty.handle,
                    request.authority,
                    dirty.revision,
                    true)
                || projects.document(dirty.handle)->dirty)
            {
                return false;
            }

            Controller curated{};
            const OpenRequest curated_request{
                .kind = WorkspaceKind::curated_engine_source,
                .workspace_id = "curated-session",
                .root = fixture.root,
                .authority = authority(40u, 50u, 60u),
                .paths = {{
                    .relative_path = "Engine/src/reviewed.cpp",
                    .writable = false}},
                .maximum_file_bytes = 1'024u,
                .maximum_tabs = 4u};
            if (!curated.open(curated_request))
                return false;
            const DocumentSnapshot reviewed = curated.snapshot().documents[0];
            if (curated.replace_text(
                    reviewed.handle,
                    curated_request.authority,
                    reviewed.revision,
                    "not allowed").code != ResultCode::read_only
                || curated.snapshot().workspace_id
                    == projects.snapshot().workspace_id
                || curated.snapshot().documents[0].text
                    != "int reviewed = 1;\n")
            {
                return false;
            }
            return true;
        }

        [[nodiscard]] bool stable_tabs_and_session_contract(
            const Fixture& fixture)
        {
            if (!fixture.write("Scripts/alpha.ascript.cpp", "alpha\n")
                || !fixture.write("Scripts/beta.ascript.cpp", "beta\n"))
            {
                return false;
            }
            Controller source{};
            const OpenRequest request = project_request(fixture);
            if (!source.open(request))
                return false;
            const WorkspaceSnapshot initial = source.snapshot();
            const DocumentHandle alpha = initial.documents[0].handle;
            const DocumentHandle beta = initial.documents[1].handle;
            if (!source.activate(beta, request.authority)
                || !source.find(
                    beta,
                    request.authority,
                    initial.documents[1].revision,
                    "et",
                    FindOptions{},
                    FindDirection::forward)
                || !source.move_tab(beta, request.authority, 0u))
            {
                return false;
            }
            const WorkspaceSnapshot moved = source.snapshot();
            if (moved.documents[0].handle != beta
                || moved.documents[0].relative_path
                    != "Scripts/beta.ascript.cpp"
                || moved.documents[1].handle != alpha
                || !moved.active_document
                || *moved.active_document != beta)
            {
                return false;
            }
            if (!source.open(request)
                || source.snapshot().documents[0].handle != beta)
            {
                return false;
            }

            const std::string persisted = source.serialize_session();
            if (persisted.empty() || persisted != source.serialize_session())
                return false;
            Controller restored{};
            if (!restored.open(request)
                || !restored.restore_session(request.authority, persisted))
            {
                return false;
            }
            const WorkspaceSnapshot reopened = restored.snapshot();
            if (reopened.documents.size() != 2u
                || reopened.documents[0].relative_path
                    != "Scripts/beta.ascript.cpp"
                || reopened.documents[1].relative_path
                    != "Scripts/alpha.ascript.cpp"
                || !reopened.active_document
                || !reopened.documents[0].active
                || reopened.documents[0].find.query != "et"
                || reopened.documents[0].find.match_count != 1u
                || reopened.documents[0].find.current_match
                || reopened.documents[0].selection
                    != TextRange{
                        .anchor = {.line = 0u, .column = 1u},
                        .caret = {.line = 0u, .column = 3u}}
                || restored.serialize_session() != persisted)
            {
                return false;
            }
            const WorkspaceSnapshot before_refusal = restored.snapshot();
            if (restored.restore_session(
                    request.authority,
                    persisted + "x").code != ResultCode::invalid_request
                || restored.snapshot().revision != before_refusal.revision
                || restored.snapshot().documents[0].relative_path
                    != before_refusal.documents[0].relative_path)
            {
                return false;
            }

            const DocumentSnapshot alpha_document = *source.document(alpha);
            if (!source.close(
                    alpha,
                    request.authority,
                    alpha_document.revision,
                    false)
                || !source.document(beta)
                || !source.active_document()
                || source.active_document()->handle != beta)
            {
                return false;
            }
            return true;
        }

        [[nodiscard]] bool find_replace_and_revert_contract(
            const Fixture& fixture)
        {
            if (!fixture.write("Scripts/alpha.ascript.cpp", "alpha\n"))
                return false;
            Controller controller{};
            const OpenRequest request = project_request(fixture);
            if (!controller.open(request))
                return false;
            const DocumentSnapshot opened = controller.snapshot().documents[0];
            if (!controller.replace_text(
                    opened.handle,
                    request.authority,
                    opened.revision,
                    "alpha alpha1 Alpha\n\xe7\x95\x8c alpha\n"))
            {
                return false;
            }
            DocumentSnapshot document = *controller.document(opened.handle);
            const FindOptions identifier_search{
                .case_sensitive = false,
                .whole_identifier = true,
                .wrap = true};
            const OperationResult first = controller.find(
                document.handle,
                request.authority,
                document.revision,
                "alpha",
                identifier_search,
                FindDirection::forward);
            const OperationResult second = controller.find(
                document.handle,
                request.authority,
                document.revision,
                "alpha",
                identifier_search,
                FindDirection::forward);
            const OperationResult previous = controller.find(
                document.handle,
                request.authority,
                document.revision,
                "alpha",
                identifier_search,
                FindDirection::backward);
            const OperationResult wrapped = controller.find(
                document.handle,
                request.authority,
                document.revision,
                "alpha",
                identifier_search,
                FindDirection::backward);
            if (!first || first.match_count != 3u || first.match_index != 1u
                || !second || second.match_index != 2u
                || !previous || previous.match_index != 1u
                || !wrapped || wrapped.match_index != 3u || !wrapped.wrapped)
            {
                return false;
            }

            const OperationResult replaced = controller.replace_current(
                document.handle,
                request.authority,
                document.revision,
                "omega");
            if (!replaced || replaced.affected_count != 1u)
                return false;
            document = *controller.document(document.handle);
            const OperationResult all = controller.replace_all(
                document.handle,
                request.authority,
                document.revision,
                "alpha",
                identifier_search,
                "A");
            if (!all || all.affected_count != 2u)
                return false;
            document = *controller.document(document.handle);
            if (document.text != "A alpha1 A\n\xe7\x95\x8c omega\n"
                || !document.dirty)
            {
                return false;
            }
            const OperationResult range_edit = controller.replace_range(
                document.handle,
                request.authority,
                document.revision,
                TextRange{
                    .anchor = {.line = 0u, .column = 0u},
                    .caret = {.line = 0u, .column = 1u}},
                "\xce\xa9");
            if (!range_edit || range_edit.affected_count != 1u)
                return false;
            document = *controller.document(document.handle);
            if (!document.text.starts_with("\xce\xa9 alpha1")
                || document.selection.caret
                    != TextPosition{.line = 0u, .column = 1u}
                || !controller.revert(
                    document.handle,
                    request.authority,
                    document.revision))
            {
                return false;
            }
            const DocumentSnapshot reverted = *controller.document(
                document.handle);
            return reverted.text == "alpha\n"
                && !reverted.dirty
                && reverted.persisted_revision == opened.persisted_revision;
        }

        [[nodiscard]] bool bounded_undo_redo_contract(
            const Fixture& fixture)
        {
            if (!fixture.write(
                    "Scripts/alpha.ascript.cpp",
                    "alpha\nbeta\n"))
            {
                return false;
            }
            Controller controller{};
            const OpenRequest request = project_request(fixture);
            if (!controller.open(request))
                return false;
            const DocumentSnapshot opened =
                controller.snapshot().documents[0];
            if (opened.can_undo || opened.can_redo
                || opened.history_cursor != 0u
                || opened.history_entries != 0u)
            {
                return false;
            }

            if (!controller.replace_text(
                    opened.handle,
                    request.authority,
                    opened.revision,
                    "alpha\nchanged\n"))
            {
                return false;
            }
            DocumentSnapshot changed =
                *controller.document(opened.handle);
            if (!changed.can_undo || changed.can_redo
                || changed.history_cursor != 1u
                || changed.history_entries != 1u
                || changed.undo_label != "Edit text"
                || changed.retained_history_bytes == 0u
                || controller.undo(
                    changed.handle,
                    request.authority,
                    opened.revision).code
                    != ResultCode::stale_document)
            {
                return false;
            }

            if (!controller.undo(
                    changed.handle,
                    request.authority,
                    changed.revision))
            {
                return false;
            }
            DocumentSnapshot undone =
                *controller.document(opened.handle);
            if (undone.text != opened.text
                || undone.can_undo || !undone.can_redo
                || undone.history_cursor != 0u
                || undone.redo_label != "Edit text"
                || undone.revision == changed.revision)
            {
                return false;
            }

            if (!controller.redo(
                    undone.handle,
                    request.authority,
                    undone.revision))
            {
                return false;
            }
            DocumentSnapshot redone =
                *controller.document(opened.handle);
            if (redone.text != changed.text
                || !redone.can_undo || redone.can_redo
                || redone.history_cursor != 1u)
            {
                return false;
            }

            if (!controller.undo(
                    redone.handle,
                    request.authority,
                    redone.revision))
            {
                return false;
            }
            undone = *controller.document(opened.handle);
            const OperationResult branch = controller.replace_range(
                undone.handle,
                request.authority,
                undone.revision,
                TextRange{
                    .anchor = {.line = 0u, .column = 0u},
                    .caret = {.line = 0u, .column = 5u}},
                "omega");
            if (!branch)
                return false;
            const DocumentSnapshot diverged =
                *controller.document(opened.handle);
            return diverged.text == "omega\nbeta\n"
                && diverged.can_undo && !diverged.can_redo
                && diverged.history_cursor == 1u
                && diverged.history_entries == 1u
                && diverged.undo_label == "Replace text";
        }

        [[nodiscard]] bool diagnostics_contract(
            const Fixture& fixture)
        {
            if (!fixture.write("Scripts/alpha.ascript.cpp", "alpha\n"))
                return false;
            Controller controller{};
            const OpenRequest request = project_request(fixture);
            if (!controller.open(request))
                return false;
            DocumentSnapshot document = controller.snapshot().documents[0];
            std::vector<Diagnostic> diagnostics{
                Diagnostic{
                    .severity = DiagnosticSeverity::warning,
                    .source = "compiler",
                    .code = "W2",
                    .message = "Trailing token.",
                    .range = {
                        .anchor = {.line = 0u, .column = 4u},
                        .caret = {.line = 0u, .column = 5u}}},
                Diagnostic{
                    .severity = DiagnosticSeverity::error,
                    .source = "compiler",
                    .code = "E1",
                    .message = "Leading token.",
                    .range = {
                        .anchor = {.line = 0u, .column = 0u},
                        .caret = {.line = 0u, .column = 1u}}}};
            const OperationResult published = controller.publish_diagnostics(
                document.handle,
                request.authority,
                document.revision,
                diagnostics);
            document = *controller.document(document.handle);
            if (!published || published.affected_count != 2u
                || !document.diagnostics_current
                || document.diagnostics.size() != 2u
                || document.diagnostics[0].code != "E1"
                || !controller.navigate_diagnostic(
                    document.handle,
                    request.authority,
                    document.revision,
                    0u))
            {
                return false;
            }
            document = *controller.document(document.handle);
            if (document.selection != diagnostics[1].range
                || !controller.replace_range(
                    document.handle,
                    request.authority,
                    document.revision,
                    document.selection,
                    "A"))
            {
                return false;
            }
            document = *controller.document(document.handle);
            if (document.diagnostics_current
                || controller.navigate_diagnostic(
                    document.handle,
                    request.authority,
                    document.revision,
                    0u).code != ResultCode::stale_diagnostics)
            {
                return false;
            }
            const std::vector<Diagnostic> invalid{
                Diagnostic{
                    .severity = DiagnosticSeverity::error,
                    .source = "compiler",
                    .code = "E9",
                    .message = "Outside.",
                    .range = {
                        .anchor = {.line = 99u, .column = 0u},
                        .caret = {.line = 99u, .column = 1u}}}};
            return controller.publish_diagnostics(
                document.handle,
                request.authority,
                document.revision,
                invalid).code == ResultCode::invalid_request;
        }

        [[nodiscard]] bool path_refusal_contract(const Fixture& fixture)
        {
            Controller controller{};
            OpenRequest request = project_request(fixture);
            request.paths = {{.relative_path = "../outside.cpp", .writable = true}};
            if (controller.open(request).code != ResultCode::external_path)
                return false;

            request.paths = {{
                .relative_path = "Scripts/large.ascript.cpp",
                .writable = true}};
            request.maximum_file_bytes = 32u;
            if (controller.open(request).code != ResultCode::oversized_file)
                return false;

            request.paths = {{
                .relative_path = "Scripts/invalid.ascript.cpp",
                .writable = true}};
            request.maximum_file_bytes = 1'024u;
            if (controller.open(request).code != ResultCode::invalid_utf8)
                return false;

            request.paths = {{
                .relative_path = "Scripts/utf16.ascript.cpp",
                .writable = true}};
            const OperationResult utf16 = controller.open(request);
            if (utf16.code != ResultCode::invalid_utf8
                || utf16.reason.find("UTF-16 BOM") == std::string::npos)
            {
                return false;
            }

            request.paths = {
                {.relative_path = "Scripts/alpha.ascript.cpp", .writable = true},
                {.relative_path = "Scripts/alpha.ascript.cpp", .writable = true}};
            if (controller.open(request).code != ResultCode::duplicate_path)
                return false;
            return true;
        }
    }

    bool run_contract()
    {
        Fixture fixture{};
        if (!fixture.write("Scripts/alpha.ascript.cpp", "alpha\n")
            || !fixture.write("Scripts/beta.ascript.cpp", "beta\n")
            || !fixture.write(
                "Scripts/large.ascript.cpp",
                std::string(64u, 'x'))
            || !fixture.write(
                "Scripts/invalid.ascript.cpp",
                std::string_view{"\xc0\x80", 2u})
            || !fixture.write(
                "Scripts/bom.ascript.cpp",
                "\xEF\xBB\xBF" "hello \xe2\x9c\x93\n")
            || !fixture.write(
                "Scripts/utf16.ascript.cpp",
                std::string_view{"\xFF\xFEh\0i\0", 6u})
            || !fixture.write(
                "Engine/src/reviewed.cpp",
                "int reviewed = 1;\n"))
        {
            return false;
        }
        return utf8_contract()
            && tabs_edit_selection_and_viewport_contract(fixture)
            && bom_roundtrip_contract(fixture)
            && stale_and_atomic_save_contract(fixture)
            && reload_and_isolation_contract(fixture)
            && stable_tabs_and_session_contract(fixture)
            && find_replace_and_revert_contract(fixture)
            && bounded_undo_redo_contract(fixture)
            && diagnostics_contract(fixture)
            && path_refusal_contract(fixture);
    }
}

#if defined(EPOCH_EDITOR_CODE_WORKSPACE_CONTRACT_MAIN)
int main()
{
    return epochengine::editor_code_workspace::run_contract() ? 0 : 1;
}
#endif
