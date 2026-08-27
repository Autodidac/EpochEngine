/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>

module project.gui_library;

namespace epochengine::project_gui
{
    namespace
    {
        class ContractRoot final
        {
        public:
            ContractRoot() noexcept
            {
                try
                {
                    std::error_code error{};
                    path_ = std::filesystem::temp_directory_path(error)
                        / "epoch_gui_library_contract";
                    if (error)
                    {
                        path_.clear();
                        return;
                    }
                    std::filesystem::remove_all(path_, error);
                    error.clear();
                    std::filesystem::create_directories(path_, error);
                    if (error)
                        path_.clear();
                }
                catch (...)
                {
                    path_.clear();
                }
            }

            ~ContractRoot()
            {
                std::error_code error{};
                std::filesystem::remove_all(path_, error);
            }

            [[nodiscard]] bool valid() const noexcept
            {
                return !path_.empty();
            }

            [[nodiscard]] std::string string() const
            {
                return path_.generic_string();
            }

        private:
            std::filesystem::path path_{};
        };

        [[nodiscard]] asset::gui::CompiledGuiArtifact artifact(
            std::uint64_t revision)
        {
            asset::gui::CompiledGuiArtifact result{};
            for (std::size_t index = 0u;
                 index < result.identity.source_revision.content.bytes.size();
                 ++index)
            {
                result.identity.source_revision.content.bytes[index] =
                    static_cast<std::uint8_t>(index + revision);
            }
            result.identity.source_revision.sequence = revision;
            result.name = "ContractGui";
            result.root = 0u;
            asset::gui::CompiledWidget root{};
            root.id = {revision + 1u};
            root.kind = asset::gui::WidgetKind::canvas;
            root.name = "Root";
            root.layout.width = 1'280.0f;
            root.layout.height = 720.0f;
            result.widgets.push_back(std::move(root));
            result.identity.key = asset::gui::payload_content(result);
            return result;
        }
    }

    LibraryContractFailure run_library_contract() noexcept
    {
        ContractRoot root{};
        if (!root.valid())
            return LibraryContractFailure::temporary_root;

        ArtifactLibrary invalid{"", root.string()};
        if (invalid.valid())
            return LibraryContractFailure::construction;

        ArtifactLibrary library{"epoch.gui.contract", root.string()};
        if (!library.valid())
            return LibraryContractFailure::construction;

        auto first = artifact(1u);
        auto malformed = first;
        malformed.identity.key.bytes[0] ^= 0xffu;
        if (library.persist("Assets/Gui/main.epochgui", malformed).code
            != LibraryCode::invalid_artifact)
        {
            return LibraryContractFailure::invalid_rejection;
        }
        if (library.persist("../escape.epochgui", first).code
            != LibraryCode::invalid_path)
        {
            return LibraryContractFailure::traversal_rejection;
        }

        const ArtifactLocator persisted = library.persist(
            "Assets\\Gui//main.epochgui", first);
        if (!persisted || persisted.code != LibraryCode::ready
            || persisted.canonical_logical_path
                != "Assets/Gui/main.epochgui")
        {
            return LibraryContractFailure::first_persist;
        }
        const ArtifactLocator repeated = library.persist(
            "Assets/Gui/main.epochgui", first);
        if (!repeated || repeated.code != LibraryCode::unchanged
            || repeated.storage_path != persisted.storage_path)
        {
            return LibraryContractFailure::unchanged_persist;
        }

        const LoadedArtifact exact = library.load_exact(
            "Assets/Gui/main.epochgui", first.identity.key);
        if (!exact || exact.artifact != first)
            return LibraryContractFailure::exact_load;
        const LoadedArtifact latestFirst = library.load_latest(
            "Assets/Gui/main.epochgui");
        if (!latestFirst || latestFirst.artifact != first)
            return LibraryContractFailure::latest_load;

        auto second = artifact(2u);
        if (!library.persist("Assets/Gui/main.epochgui", second))
            return LibraryContractFailure::revision_advance;
        const LoadedArtifact latestSecond = library.load_latest(
            "Assets/Gui/main.epochgui");
        if (!latestSecond || latestSecond.artifact != second)
            return LibraryContractFailure::revision_advance;

        ArtifactLibrary foreign{"epoch.gui.foreign", root.string()};
        if (foreign.load_latest("Assets/Gui/main.epochgui").code
            != LibraryCode::not_found)
        {
            return LibraryContractFailure::project_isolation;
        }

        const LibraryMetrics metrics = library.metrics();
        if (metrics.persist_requests != 5u || metrics.writes != 2u
            || metrics.unchanged_writes != 1u || metrics.load_requests != 3u
            || metrics.loads != 3u || metrics.bytes_written == 0u
            || metrics.bytes_read == 0u || metrics.rejected_operations < 2u)
        {
            return LibraryContractFailure::metrics;
        }
        return LibraryContractFailure::none;
    }
}

#if defined(EPOCH_PROJECT_GUI_LIBRARY_CONTRACT_MAIN)
int main()
{
    return static_cast<int>(epochengine::project_gui::run_library_contract());
}
#endif
