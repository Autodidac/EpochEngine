/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

export module ai.source_index;

export namespace epochengine::ai::source_index
{
    enum class SearchKind : std::uint8_t
    {
        text,
        identifier,
        path,
        // Exact indexed path or module identity, not a source-text substring.
        imports,
        importers,
        references
    };

    struct SearchQuery final
    {
        SearchKind kind{SearchKind::text};
        std::string text{};
        std::size_t maximum_hits{12u};
    };

    struct SearchHit final
    {
        std::string relative_path{};
        std::size_t line{};
        int score{};
        std::string preview{};
    };

    struct BuildResult final
    {
        bool accepted{};
        std::size_t file_count{};
        std::size_t symbol_count{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept { return accepted; }
    };

    class RepositoryIndex final
    {
    public:
        [[nodiscard]] BuildResult build(
            const std::filesystem::path& root,
            std::string_view required_prefix = {});
        [[nodiscard]] std::vector<SearchHit> search(const SearchQuery& query) const;
        // Direct dependencies and same-module units only. No recursive source
        // loading, filesystem access or edit authority is implied by a hit.
        [[nodiscard]] std::vector<SearchHit> related_sources(
            std::string_view relative_path, std::size_t maximum_hits = 8u) const;
        [[nodiscard]] std::string compact_map(
            std::string_view objective,
            std::size_t maximum_bytes = 48u * 1024u,
            std::size_t maximum_files = 256u) const;
        [[nodiscard]] std::size_t file_count() const noexcept;
        [[nodiscard]] const std::filesystem::path& root() const noexcept;
        void clear() noexcept;

    private:
        struct Import final
        {
            std::string name{};
            std::size_t line{};
            bool header{};
        };

        struct Dependency final
        {
            std::size_t target{};
            std::size_t line{};
            bool header{};
        };

        struct Entry final
        {
            std::string path{};
            std::string lower_path{};
            std::string text{};
            std::string lower_text{};
            std::vector<std::string> symbols{};
            std::string module_name{};
            std::size_t module_line{};
            bool module_interface{};
            std::vector<Import> imports{};
            std::vector<Dependency> dependencies{};
            std::vector<std::size_t> module_units{};
        };

        std::filesystem::path root_{};
        std::vector<Entry> entries_{};
    };

    [[nodiscard]] bool run_contract();
}
