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
#include <limits>
#include <map>
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
            std::string prefix{required_prefix};
            while (!prefix.empty() && prefix.back() == '/') prefix.pop_back();
            return normalized == prefix || normalized.starts_with(prefix + '/');
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

        // Preserve offsets/newlines while excluding comments and ordinary/raw
        // literals. Header operands stay visible for conservative include hints.
        // This is lexical navigation, not a compiler or preprocessor evaluation.
        [[nodiscard]] std::string indexed_code(std::string_view text)
        {
            std::string code{text};
            const auto blank = [&](std::size_t first, std::size_t last)
            {
                for (auto offset = first; offset < last; ++offset)
                    if (code[offset] != '\n' && code[offset] != '\r') code[offset] = ' ';
            };
            std::size_t lineBegin{};
            for (std::size_t offset{}; offset < text.size();)
            {
                if (text[offset] == '\n') { lineBegin = ++offset; continue; }
                if (text.substr(offset).starts_with("//"))
                {
                    auto end = text.find('\n', offset);
                    // A continued line comment must not reveal fake directives.
                    while (end != std::string_view::npos)
                    {
                        auto last = end;
                        if (last > offset && text[last - 1u] == '\r') --last;
                        if (last == offset || text[last - 1u] != '\\') break;
                        end = text.find('\n', end + 1u);
                    }
                    if (end == std::string_view::npos) end = text.size();
                    blank(offset, end);
                    offset = end;
                    continue;
                }
                if (text.substr(offset).starts_with("/*"))
                {
                    const auto close = text.find("*/", offset + 2u);
                    const auto end = close == std::string_view::npos ? text.size() : close + 2u;
                    blank(offset, end);
                    offset = end;
                    const auto newline = text.rfind('\n', end == 0u ? 0u : end - 1u);
                    if (newline != std::string_view::npos) lineBegin = newline + 1u;
                    continue;
                }
                if (text.substr(offset).starts_with("R\""))
                {
                    const auto open = text.find('(', offset + 2u);
                    if (open != std::string_view::npos && open - offset <= 18u)
                    {
                        const std::string ending = ")" + std::string{text.substr(offset + 2u, open - offset - 2u)} + '"';
                        const auto close = text.find(ending, open + 1u);
                        const auto end = close == std::string_view::npos ? text.size() : close + ending.size();
                        blank(offset, end);
                        offset = end;
                        const auto newline = text.rfind('\n', end - 1u);
                        if (newline != std::string_view::npos) lineBegin = newline + 1u;
                        continue;
                    }
                }
                if (text[offset] == '"' || text[offset] == '\'')
                {
                    // Digit separators are not character literals.
                    if (text[offset] == '\'' && offset > 0u && offset + 1u < text.size()
                        && std::isalnum(static_cast<unsigned char>(text[offset - 1u])) != 0
                        && std::isalnum(static_cast<unsigned char>(text[offset + 1u])) != 0)
                    { ++offset; continue; }
                    const auto prefix = trim(std::string_view{code}.substr(lineBegin, offset - lineBegin));
                    const bool header = (std::string_view{prefix}.starts_with('#')
                            && trim(std::string_view{prefix}.substr(1u)) == "include")
                        || prefix == "import" || prefix == "export import";
                    const char delimiter = text[offset];
                    std::size_t end = offset + 1u;
                    while (end < text.size())
                    {
                        if (text[end] == '\\') { end = (std::min)(end + 2u, text.size()); continue; }
                        if (text[end++] == delimiter) break;
                    }
                    if (!header) blank(offset, end);
                    offset = end;
                    const auto newline = text.rfind('\n', end - 1u);
                    if (newline != std::string_view::npos) lineBegin = newline + 1u;
                    continue;
                }
                ++offset;
            }
            return code;
        }

        [[nodiscard]] bool module_identity(std::string_view name)
        {
            return !name.empty() && name.size() <= 256u
                && std::ranges::all_of(name, [](unsigned char value)
                { return std::isalnum(value) != 0 || value == '_' || value == '.' || value == ':'; });
        }

        template<class Metadata>
        void collect_metadata(std::string_view text, Metadata& metadata)
        {
            std::size_t begin{};
            std::size_t lineNumber{1u};
            while (begin <= text.size())
            {
                const std::size_t end = text.find('\n', begin);
                std::string_view line = text.substr(begin,
                    end == std::string_view::npos ? text.size() - begin : end - begin);
                if (!line.empty() && line.back() == '\r') line.remove_suffix(1u);
                const std::string stripped = trim(line);
                std::string_view declarationText{stripped};
                const bool exported = declarationText.starts_with("export ");
                if (exported) declarationText.remove_prefix(7u);
                if (declarationText.starts_with("module "))
                {
                    const auto semicolon = declarationText.find(';');
                    if (semicolon != std::string_view::npos)
                    {
                        const auto name = trim(declarationText.substr(7u, semicolon - 7u));
                        if (module_identity(name) && name != ":private")
                        {
                            metadata.module_name = name;
                            metadata.module_line = lineNumber;
                            metadata.module_interface = exported;
                        }
                    }
                }
                std::string importName{};
                bool header{};
                if (declarationText.starts_with("import "))
                {
                    const auto semicolon = declarationText.find(';');
                    if (semicolon != std::string_view::npos)
                        importName = trim(declarationText.substr(7u, semicolon - 7u));
                }
                else if (declarationText.starts_with('#'))
                {
                    const auto directive = trim(declarationText.substr(1u));
                    if (std::string_view{directive}.starts_with("include")
                        && directive.size() > 7u
                        && (std::isspace(static_cast<unsigned char>(directive[7u])) != 0
                            || directive[7u] == '"' || directive[7u] == '<'))
                        importName = trim(std::string_view{directive}.substr(7u));
                }
                if (!importName.empty() && (importName.front() == '"' || importName.front() == '<'))
                {
                    const auto close = importName.find(importName.front() == '<' ? '>' : '"', 1u);
                    if (close != std::string::npos)
                    { importName = importName.substr(1u, close - 1u); header = true; }
                    else importName.clear();
                }
                if (!importName.empty() && importName.size() <= 512u
                    && (header || module_identity(importName)) && metadata.imports.size() < 128u)
                {
                    metadata.imports.push_back({std::move(importName), lineNumber, header});
                }

                const std::string lower = lower_ascii(stripped);
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
                    && metadata.symbols.size() < 256u)
                {
                    metadata.symbols.push_back(stripped);
                }
                if (end == std::string_view::npos) break;
                begin = end + 1u;
                ++lineNumber;
            }
        }

        [[nodiscard]] std::size_t identifier_offset(std::string_view text, std::string_view needle)
        {
            const auto identifierChar = [](unsigned char value)
            { return std::isalnum(value) != 0 || value == '_'; };
            std::size_t offset{};
            while ((offset = text.find(needle, offset)) != std::string_view::npos)
            {
                const auto end = offset + needle.size();
                if ((offset == 0u || !identifierChar(static_cast<unsigned char>(text[offset - 1u])))
                    && (end == text.size() || !identifierChar(static_cast<unsigned char>(text[end]))))
                    return offset;
                ++offset;
            }
            return std::string_view::npos;
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
                    || (ignored_component(entry.path().filename().generic_string())
                        && lower_ascii(relative.generic_string()) != "engine/src/build"))
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
                            const std::string code = indexed_code(indexed.text);
                            indexed.lower_text = lower_ascii(code);
                            collect_metadata(code, indexed);
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
        // Resolve only into the already admitted index. No header probing,
        // guessed filename-to-module mapping or traversal outside this scope.
        std::map<std::string, std::size_t> paths{};
        std::map<std::string, std::vector<std::size_t>> modules{};
        std::map<std::string, std::vector<std::size_t>> interfaces{};
        std::map<std::string, std::size_t> suffixes{};
        constexpr auto ambiguous = std::numeric_limits<std::size_t>::max();
        for (std::size_t i{}; i < entries_.size(); ++i)
        {
            const auto& indexed = entries_[i];
            paths.emplace(indexed.path, i);
            if (!indexed.module_name.empty())
            {
                modules[indexed.module_name].push_back(i);
                if (indexed.module_interface || (indexed.module_name.find(':') != std::string::npos
                    && std::string_view{indexed.path}.ends_with(".ixx")))
                    interfaces[indexed.module_name].push_back(i);
            }
            std::size_t begin{};
            while (begin < indexed.path.size())
            {
                auto [found, inserted] = suffixes.emplace(indexed.path.substr(begin), i);
                if (!inserted && found->second != i) found->second = ambiguous;
                const auto slash = indexed.path.find('/', begin);
                if (slash == std::string::npos) break;
                begin = slash + 1u;
            }
        }
        for (auto& [name, units] : modules)
            std::ranges::stable_sort(units, [&](std::size_t left, std::size_t right)
            { return entries_[left].module_interface && !entries_[right].module_interface; });
        for (std::size_t i{}; i < entries_.size(); ++i)
        {
            auto& indexed = entries_[i];
            if (const auto owner = modules.find(indexed.module_name); owner != modules.end())
            {
                for (const auto unit : owner->second)
                {
                    if (unit != i) indexed.module_units.push_back(unit);
                    if (indexed.module_units.size() == 64u) break;
                }
            }
            for (const auto& imported : indexed.imports)
            {
                auto target = ambiguous;
                if (!imported.header)
                {
                    std::string name = imported.name;
                    if (name.starts_with(':') && !indexed.module_name.empty())
                        name = indexed.module_name.substr(0u, indexed.module_name.find(':')) + name;
                    const auto interface = interfaces.find(name);
                    if (interface != interfaces.end() && interface->second.size() == 1u)
                        target = interface->second.front();
                }
                else
                {
                    const std::filesystem::path operand{imported.name};
                    if (!operand.is_absolute() && !operand.has_root_name())
                    {
                        const auto local = (std::filesystem::path{indexed.path}.parent_path()
                            / operand).lexically_normal().generic_string();
                        if (const auto found = paths.find(local); found != paths.end()) target = found->second;
                        else if (const auto rootPath = paths.find(operand.lexically_normal().generic_string());
                            rootPath != paths.end()) target = rootPath->second;
                        else if (std::ranges::none_of(operand, [](const auto& part) { return part == ".."; }))
                        {
                            const auto suffix = suffixes.find(operand.lexically_normal().generic_string());
                            if (suffix != suffixes.end()) target = suffix->second;
                        }
                    }
                }
                if (target != ambiguous && target != i
                    && std::ranges::none_of(indexed.dependencies,
                        [&](const Dependency& edge) { return edge.target == target; }))
                    indexed.dependencies.push_back({target, imported.line, imported.header});
            }
        }
        return {true, entries_.size(), symbolCount,
            "Repository source index is ready for bounded code search and scope-local module/include relationships."};
    }

    std::vector<SearchHit> RepositoryIndex::search(const SearchQuery& query) const
    {
        std::vector<SearchHit> hits{};
        if (query.text.empty() || entries_.empty()) return hits;
        const std::string needle = lower_ascii(query.text);
        if (query.maximum_hits == 0u) return hits;
        const std::size_t maximumHits = (std::min)(query.maximum_hits, std::size_t{64u});
        if (query.kind == SearchKind::imports || query.kind == SearchKind::importers)
        {
            std::set<std::size_t> targets{};
            for (std::size_t i{}; i < entries_.size(); ++i)
                if (entries_[i].path == query.text || entries_[i].module_name == query.text)
                    targets.insert(i);
            for (std::size_t i{}; i < entries_.size(); ++i)
            {
                const auto& entry = entries_[i];
                for (const auto& edge : entry.dependencies)
                {
                    const bool outgoing = query.kind == SearchKind::imports;
                    if (!(outgoing ? targets.contains(i) : targets.contains(edge.target))) continue;
                    const auto& hit = outgoing ? entries_[edge.target] : entry;
                    if (std::ranges::any_of(hits, [&](const SearchHit& found)
                        { return found.relative_path == hit.path; })) continue;
                    const auto line = outgoing ? hit.module_line : edge.line;
                    hits.push_back({hit.path, line, 700,
                        std::string{edge.header ? "include " : "import "} + entries_[edge.target].path});
                }
            }
        }
        else
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
            case SearchKind::importers:
                break;
            case SearchKind::references:
            case SearchKind::identifier:
            case SearchKind::text:
                offset = query.kind == SearchKind::text ? lower_ascii(entry.text).find(needle)
                    : identifier_offset(entry.lower_text, needle);
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

    std::vector<SearchHit> RepositoryIndex::related_sources(
        std::string_view relative_path, std::size_t maximum_hits) const
    {
        std::vector<SearchHit> hits{};
        if (maximum_hits == 0u) return hits;
        const auto found = std::ranges::lower_bound(entries_, relative_path, {}, &Entry::path);
        if (found == entries_.end() || found->path != relative_path) return hits;
        const auto add = [&](std::size_t target, int score, std::string_view relation)
        {
            const auto& entry = entries_[target];
            auto existing = std::ranges::find(hits, entry.path, &SearchHit::relative_path);
            if (existing == hits.end())
                hits.push_back({entry.path, entry.module_line, score, std::string{relation}});
            else if (existing->score < score) existing->score = score;
        };
        for (const auto unit : found->module_units)
            add(unit, entries_[unit].module_interface ? 900 : 600, "same module: " + found->module_name);
        for (const auto& edge : found->dependencies)
            add(edge.target, edge.header ? 700 : 800, edge.header ? "direct include" : "direct module import");
        std::ranges::sort(hits, [](const SearchHit& left, const SearchHit& right)
        {
            if (left.score != right.score) return left.score > right.score;
            return left.relative_path < right.relative_path;
        });
        if (hits.size() > (std::min)(maximum_hits, std::size_t{64u}))
            hits.resize((std::min)(maximum_hits, std::size_t{64u}));
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
            // Relevance orders the project; it must not hide unrelated paths
            // that a coding generation may discover are dependencies.
            ranked.emplace_back(score, &entry);
        }
        std::ranges::sort(ranked, [](const auto& a, const auto& b)
        {
            if (a.first != b.first) return a.first > b.first;
            return a.second->path < b.second->path;
        });
        // One-hop expansion from a small lexical seed, never a recursive graph
        // crawl. Declaration owners stay discoverable even without objective
        // keywords in their paths or text.
        std::vector<std::pair<int, const Entry*>> seeds{
            ranked.begin(), ranked.begin() + static_cast<std::ptrdiff_t>((std::min)(ranked.size(), std::size_t{8u}))};
        for (const auto& [score, seed] : seeds)
        {
            if (score <= 1) continue;
            for (const auto& hit : related_sources(seed->path, 8u))
            {
                const auto target = std::ranges::lower_bound(entries_, hit.relative_path, {}, &Entry::path);
                if (target == entries_.end()) continue;
                const int boost = (std::max)(1, score / 2) + hit.score / 32;
                auto existing = std::ranges::find_if(ranked,
                    [&](const auto& item) { return item.second == &*target; });
                if (existing == ranked.end()) ranked.emplace_back(boost, &*target);
                else existing->first = (std::max)(existing->first, boost);
            }
        }
        std::ranges::sort(ranked, [](const auto& a, const auto& b)
        {
            if (a.first != b.first) return a.first > b.first;
            return a.second->path < b.second->path;
        });
        constexpr std::string_view heading = "EPOCH_SOURCE_REPOSITORY_MAP_V2\n"
            "PATH lines are verified navigation authority. MAP/EDGE lines are lexical read-only hints, never edit authority.\n";
        constexpr std::string_view ending = "END_EPOCH_SOURCE_REPOSITORY_MAP_V2\n";
        if (maximum_bytes < heading.size() + ending.size()) return {};
        std::string out{heading};
        struct Block final
        {
            std::string path{};
            std::string body{};
            std::vector<std::pair<std::string, std::string>> edges{};
        };
        std::vector<Block> blocks{};
        std::size_t reserved = heading.size() + ending.size();
        for (const auto& [score, entry] : ranked)
        {
            if (blocks.size() >= maximum_files) break;
            const std::string pathLine = "PATH " + entry->path + "\n";
            if (pathLine.size() > maximum_bytes - reserved) continue;
            reserved += pathLine.size();
            out += pathLine;
            blocks.push_back({entry->path, {}, {}});
        }
        std::set<std::string> emittedPaths{};
        for (const auto& block : blocks) emittedPaths.insert(block.path);
        // Reserve navigation authority before optional verbose metadata. Large
        // symbol/edge blocks cannot evict small valid project path records.
        for (auto& block : blocks)
        {
            const auto found = std::ranges::lower_bound(entries_, block.path, {}, &Entry::path);
            if (found == entries_.end() || found->path != block.path) continue;
            const auto* entry = &*found;
            if (!entry->module_name.empty())
                block.body += "MAP " + entry->path + " :: "
                    + (entry->module_interface ? "interface " : "module unit ") + entry->module_name + "\n";
            std::size_t symbols{};
            for (const auto& symbol : entry->symbols)
            {
                if (symbols++ >= 4u) break;
                block.body += "MAP " + entry->path + " :: " + symbol + "\n";
            }
            for (const auto& hit : related_sources(entry->path, 4u))
            {
                if (!emittedPaths.contains(hit.relative_path)) continue;
                std::string edge = "EDGE " + entry->path + " -> " + hit.relative_path + " :: " + hit.preview + "\n";
                block.edges.emplace_back(hit.relative_path, std::move(edge));
            }
            if (block.body.size() <= maximum_bytes - reserved)
            {
                out += block.body;
                reserved += block.body.size();
            }
            // An EDGE must not advertise a target omitted from PATH authority.
            for (const auto& [target, edge] : block.edges)
                if (edge.size() <= maximum_bytes - reserved)
                {
                    out += edge;
                    reserved += edge.size();
                }
        }
        out += ending;
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
