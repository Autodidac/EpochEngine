/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <string_view>

export module authoring.gui_compiler;

import asset.gui_artifact;
import authoring.gui_document;

export namespace epochengine::authoring::gui
{
    enum class CompileCode : std::uint8_t
    {
        ready,
        invalid_document,
        invalid_root,
        unresolved_reference,
        artifact_rejected,
        capacity_exceeded,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view compile_code_name(
        CompileCode code) noexcept
    {
        switch (code)
        {
        case CompileCode::ready: return "ready";
        case CompileCode::invalid_document: return "invalid_document";
        case CompileCode::invalid_root: return "invalid_root";
        case CompileCode::unresolved_reference: return "unresolved_reference";
        case CompileCode::artifact_rejected: return "artifact_rejected";
        case CompileCode::capacity_exceeded: return "capacity_exceeded";
        case CompileCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct CompileDiagnostics final
    {
        std::uint32_t source_slots{};
        std::uint32_t emitted_widgets{};
        std::uint32_t referenced_assets{};
        asset::gui::ArtifactCode artifact_code{
            asset::gui::ArtifactCode::invalid_artifact};
    };

    struct CompileResult final
    {
        CompileCode code{CompileCode::invalid_document};
        asset::gui::CompiledGuiArtifact artifact{};
        CompileDiagnostics diagnostics{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == CompileCode::ready
                && static_cast<bool>(artifact.identity);
        }
    };

    [[nodiscard]] CompileResult compile_gui_document(
        const GuiDocumentSnapshot& snapshot,
        const DocumentLimits& documentLimits = {},
        const asset::gui::ArtifactLimits& artifactLimits = {}) noexcept;

    enum class CompilerContractFailure : std::uint8_t
    {
        none,
        valid_document_rejected,
        deterministic_compilation,
        source_revision_mismatch,
        content_projection,
        artifact_round_trip,
        malformed_document_accepted
    };

    [[nodiscard]] CompilerContractFailure run_compiler_contract() noexcept;
}
