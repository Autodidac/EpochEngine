/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.source_index;

namespace epochengine::ai::source_index
{
    namespace
    {
        [[nodiscard]] std::string lower_ascii(std::string_view text)
        {
            std::string out{text};
            std::ranges::transform(out, out.begin(), [](unsigned char value)
            { return static_cast<char>(std::tolower(value)); });
            return out;
        }

        [[nodiscard]] bool source_extension(const std::filesystem::path& path)
        {
            const std::string ext = lower_ascii(path.extension().string());
            return ext == ".cpp" || ext == ".ixx" || ext == ".hpp"
                || ext == ".h" || ext == ".cxx" || ext == ".cc";
        }

        [[nodiscard]] bool ignored_component(std::string_view value)
        {
            static constexpr std::array ignored{
                ".git", ".vs", "bin", "build", "built", "cache", "debug",
                "release", "logs", "vcpkg_installed", "x64", "node_modules"};
            const std::string lower = lower_ascii(value);
            return std::ranges::any_of(ignored, [&](std::string_view candidate)
            { return candidate.compare(lower) == 0; });
        }

        [[nodiscard]] bool path_allowed(
            const std::filesystem::path& relative,
            std::string_view required_prefix)
        {
            if (required_prefix.empty()) return true;
            const std::string normalized = relative.generic_string();
            return normalized.starts_with(required_prefix);
        }

        [[nodiscard]] std::vector<std::string> objective_terms(std::string_view objective)
        {
            std::vector<std::string> terms{};
            std::string current{};
            for (const unsigned char ch : objective)
            {
                if (std::isalnum(ch) != 0 || ch == '_' || ch == '.')
                {
                    current.push_back(static_cast<char>(std::tolower(ch)));
                    continue;
                }
                if (current.size() >= 3u
                    && std::ranges::find(terms, current) == terms.end())
                    terms.push_back(current);
                current.clear();
            }
            if (current.size() >= 3u
                && std::ranges::find(terms, current) == terms.end())
                terms.push_back(std::move(current));
            return terms;
        }

        [[nodiscard]] std::string trim(std::string_view text)
        {
            while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0)
                text.remove_prefix(1u);
            while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
                text.remove_suffix(1u);
            return std::string{text};
        }

        void collect_metadata(
            std::string_view text,
            std::vector<std::string>& symbols,
            std::vector<std::string>& imports)
        {
            std::size_t begin{};
            while (begin <= text.size())
            {
                const std::size_t end = text.find('\n', begin);
                std::string_view line = text.substr(begin,
                    end == std::string_view::npos ? text.size() - begin : end - begin);
                if (!line.empty() && line.back() == '\r') line.remove_suffix(1u);
                const std::string stripped = trim(line);
                const std::string lower = lower_ascii(stripped);
                if (lower.starts_with("import ") || lower.starts_with("export import ")
                    || lower.starts_with("#include"))
                {
                    if (imports.size() < 128u && stripped.size() <= 512u)
                        imports.push_back(stripped);
                }

                const bool declaration = lower.starts_with("class ")
                    || lower.starts_with("struct ") || lower.starts_with("enum ")
                    || lower.starts_with("enum class ") || lower.starts_with("namespace ")
                    || lower.starts_with("export class ") || lower.starts_with("export struct ")
                    || lower.starts_with("export enum ") || lower.starts_with("export namespace ")
                    || lower.starts_with("export module ") || lower.starts_with("module ")
                    || (stripped.find('(') != std::string::npos
                        && stripped.find(')') != std::string::npos
                        && stripped.find(';') != std::string::npos);
                if (declaration && stripped.size() >= 3u && stripped.size() <= 320u
                    && symbols.size() < 256u)
                {
                    symbols.push_back(stripped);
                }
                if (end == std::string_view::npos) break;
                begin = end + 1u;
            }
        }

        [[nodiscard]] std::size_t line_for_offset(std::string_view text, std::size_t offset)
        {
            return 1u + static_cast<std::size_t>(std::count(
                text.begin(), text.begin() + static_cast<std::ptrdiff_t>((std::min)(offset, text.size())), '\n'));
        }

        [[nodiscard]] std::string preview_for_offset(std::string_view text, std::size_t offset)
        {
            const std::size_t lineBegin = offset == 0u ? 0u : text.rfind('\n', offset - 1u) + 1u;
            const std::size_t lineEnd = text.find('\n', offset);
            std::string_view line = text.substr(lineBegin,
                lineEnd == std::string_view::npos ? text.size() - lineBegin : lineEnd - lineBegin);
            if (line.size() > 220u) line = line.substr(0u, 220u);
            return trim(line);
        }
    }

    BuildResult RepositoryIndex::build(
        const std::filesystem::path& root,
        std::string_view required_prefix)
    {
        clear();
        std::error_code error{};
        root_ = std::filesystem::weakly_canonical(root, error);
        if (error || root_.empty() || !root_.is_absolute()
            || !std::filesystem::is_directory(root_, error) || error)
            return {false, 0u, 0u, "Repository source index requires an existing absolute root."};

        constexpr std::size_t maximumFiles = 32'768u;
        constexpr std::uintmax_t maximumIndexedFileBytes = 8u * 1024u * 1024u;
        constexpr std::uintmax_t maximumIndexedBytes = 128u * 1024u * 1024u;
        std::uintmax_t indexedBytes{};
        std::size_t symbolCount{};
        std::filesystem::recursive_directory_iterator cursor{
            root_, std::filesystem::directory_options::skip_permission_denied, error};
        const std::filesystem::recursive_directory_iterator end{};
        while (!error && cursor != end && entries_.size() < maximumFiles)
        {
            const auto entry = *cursor;
            error.clear();
            if (entry.is_directory(error))
            {
                const auto canonical = std::filesystem::weakly_canonical(entry.path(), error);
                const auto relative = canonical.lexically_relative(root_);
                const bool outside = error || relative.empty() || relative.is_absolute()
                    || *relative.begin() == "..";
                if (outside || entry.is_symlink(error) || error
                    || ignored_component(entry.path().filename().generic_string()))
                    cursor.disable_recursion_pending();
            }
            else if (!error && entry.is_regular_file(error) && !error
                && !entry.is_symlink(error) && !error && source_extension(entry.path()))
            {
                const auto relative = entry.path().lexically_relative(root_);
                if (!relative.empty() && !relative.is_absolute()
                    && path_allowed(relative, required_prefix))
                {
                    error.clear();
                    const auto size = entry.file_size(error);
                    if (!error && size <= maximumIndexedFileBytes
                        && size <= maximumIndexedBytes - indexedBytes)
                    {
                        std::ifstream stream{entry.path(), std::ios::binary};
                        std::string bytes(static_cast<std::size_t>(size), '\0');
                        if (stream && (bytes.empty() || stream.read(bytes.data(),
                            static_cast<std::streamsize>(bytes.size()))))
                        {
                            Entry indexed{};
                            indexed.path = relative.generic_string();
                            indexed.lower_path = lower_ascii(indexed.path);
                            indexed.text = std::move(bytes);
                            indexed.lower_text = lower_ascii(indexed.text);
                            collect_metadata(indexed.text, indexed.symbols, indexed.imports);
                            symbolCount += indexed.symbols.size();
                            entries_.push_back(std::move(indexed));
                            indexedBytes += size;
                        }
                    }
                }
            }
            error.clear();
            cursor.increment(error);
            if (error) error.clear();
        }
        std::ranges::sort(entries_, {}, &Entry::path);
        return {true, entries_.size(), symbolCount,
            "Repository source index is ready for bounded path, symbol, text, import, and reference search."};
    }

    std::vector<SearchHit> RepositoryIndex::search(const SearchQuery& query) const
    {
        std::vector<SearchHit> hits{};
        if (query.text.empty() || entries_.empty()) return hits;
        const std::string needle = lower_ascii(query.text);
        const std::size_t maximumHits = (std::clamp)(query.maximum_hits, std::size_t{1u}, std::size_t{64u});
        for (const auto& entry : entries_)
        {
            int score{};
            std::size_t offset = std::string::npos;
            switch (query.kind)
            {
            case SearchKind::path:
                offset = entry.lower_path.find(needle);
                if (offset != std::string::npos) score = 600 - static_cast<int>((std::min)(offset, std::size_t{500u}));
                break;
            case SearchKind::imports:
                for (const auto& line : entry.imports)
                {
                    if (lower_ascii(line).find(needle) != std::string::npos)
                    { score = 520; offset = entry.lower_text.find(lower_ascii(line)); break; }
                }
                break;
            case SearchKind::importers:
            case SearchKind::references:
            case SearchKind::identifier:
            case SearchKind::text:
                offset = entry.lower_text.find(needle);
                if (offset != std::string::npos)
                {
                    score = query.kind == SearchKind::identifier ? 560
                        : query.kind == SearchKind::references ? 520 : 480;
                    if (entry.lower_path.find(needle) != std::string::npos) score += 120;
                }
                break;
            }
            if (score <= 0) continue;
            // A filename offset is not an offset into source contents (and can
            // exceed the contents of an empty or short source file).
            if (query.kind == SearchKind::path) offset = std::string::npos;
            hits.push_back(SearchHit{
                .relative_path = entry.path,
                .line = offset == std::string::npos ? 0u : line_for_offset(entry.text, offset),
                .score = score,
                .preview = offset == std::string::npos ? std::string{} : preview_for_offset(entry.text, offset)});
        }
        std::ranges::sort(hits, [](const SearchHit& a, const SearchHit& b)
        {
            if (a.score != b.score) return a.score > b.score;
            if (a.relative_path != b.relative_path) return a.relative_path < b.relative_path;
            return a.line < b.line;
        });
        if (hits.size() > maximumHits) hits.resize(maximumHits);
        return hits;
    }

    std::string RepositoryIndex::compact_map(
        std::string_view objective,
        std::size_t maximum_bytes,
        std::size_t maximum_files) const
    {
        std::vector<std::pair<int, const Entry*>> ranked{};
        const auto terms = objective_terms(objective);
        ranked.reserve(entries_.size());
        for (const auto& entry : entries_)
        {
            int score{};
            for (const auto& term : terms)
            {
                if (entry.lower_path.find(term) != std::string::npos) score += 48;
                if (entry.lower_text.find(term) != std::string::npos) score += 12;
                for (const auto& symbol : entry.symbols)
                    if (lower_ascii(symbol).find(term) != std::string::npos) { score += 24; break; }
            }
            if (score > 0) ranked.emplace_back(score, &entry);
        }
        if (ranked.empty())
            for (const auto& entry : entries_) ranked.emplace_back(1, &entry);
        std::ranges::sort(ranked, [](const auto& a, const auto& b)
        {
            if (a.first != b.first) return a.first > b.first;
            return a.second->path < b.second->path;
        });

        std::string out = "EPOCH_SOURCE_REPOSITORY_MAP_V2\n";
        out += "PATH lines are verified navigation authority. MAP lines are compact read-only hints, never edit authority.\n";
        std::size_t emitted{};
        for (const auto& [score, entry] : ranked)
        {
            if (emitted >= maximum_files) break;
            std::string block = "PATH " + entry->path + "\n";
            std::size_t symbols{};
            for (const auto& symbol : entry->symbols)
            {
                if (symbols++ >= 4u) break;
                block += "MAP " + entry->path + " :: " + symbol + "\n";
            }
            std::size_t imports{};
            for (const auto& importLine : entry->imports)
            {
                if (imports++ >= 2u) break;
                block += "EDGE " + entry->path + " :: " + importLine + "\n";
            }
            if (out.size() + block.size() > maximum_bytes) break;
            out += block;
            ++emitted;
        }
        out += "END_EPOCH_SOURCE_REPOSITORY_MAP_V2\n";
        return out;
    }

    std::size_t RepositoryIndex::file_count() const noexcept { return entries_.size(); }
    const std::filesystem::path& RepositoryIndex::root() const noexcept { return root_; }
    void RepositoryIndex::clear() noexcept { root_.clear(); entries_.clear(); }

    bool run_contract()
    {
        return SearchQuery{SearchKind::text, "theme", 8u}.maximum_hits == 8u;
    }
}
