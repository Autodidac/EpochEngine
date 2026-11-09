// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

module package.source_install;
import core.sha256;
import package.catalog;
import package.registry;
import platform.filesystem;

namespace epochengine::package_source
{
    namespace
    {
        namespace fs = std::filesystem;
        constexpr std::uint64_t kMaximumFileBytes = 16u * 1024u * 1024u;
        constexpr std::uint64_t kMaximumTotalBytes = 64u * 1024u * 1024u;
        constexpr std::size_t kMaximumManifestBytes = 256u * 1024u;
        struct Failure { Code code; std::string message; };

        void require(bool condition, Code code, std::string_view message)
        {
            if (!condition) throw Failure{code, std::string{message}};
        }
        void check_cancelled(std::stop_token token)
        { require(!token.stop_requested(), Code::cancelled, "Local source operation cancelled."); }
        std::string folded(std::string value)
        {
            for (char& ch : value)
                if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch + ('a' - 'A'));
            return value;
        }
        bool same_path(const fs::path& left, const fs::path& right)
        {
            if (left == right) return true;
            std::error_code error;
            return fs::equivalent(left, right, error) && !error;
        }
        bool within(const fs::path& path, const fs::path& root)
        {
            auto p = path.lexically_normal().generic_string();
            auto r = root.lexically_normal().generic_string();
#if defined(_WIN32)
            p = folded(std::move(p));
            r = folded(std::move(r));
#endif
            if (!r.ends_with('/')) r += '/';
            if (p.starts_with(r)) return true;
            // Filesystem identity, rather than the host OS, decides whether
            // an existing ancestor aliases the reviewed source (for example
            // Linux on a case-insensitive mounted volume).
            for (auto ancestor = path.parent_path(); !ancestor.empty();)
            {
                if (same_path(ancestor, root)) return true;
                auto parent = ancestor.parent_path();
                if (parent == ancestor) break;
                ancestor = std::move(parent);
            }
            return false;
        }
        bool portable_relative(std::string_view text)
        {
            if (text.empty() || text.size() > 1024u || text.front() == '/'
                || text.find("//") != std::string_view::npos) return false;
            for (const unsigned char ch : text)
                if (ch < 0x20u || ch > 0x7eu || std::string_view{"\\:\"<>|?*"}.find(ch) != std::string_view::npos)
                    return false;
            const fs::path path{std::string{text}};
            if (path.is_absolute() || path.has_root_name()) return false;
            for (const auto& part : path)
            {
                const auto name = folded(part.string());
                if (name.empty() || name == "." || name == ".." || name.ends_with('.') || name.ends_with(' ')
                    || name == ".git" || name == ".vs" || name == "node_modules" || name == "__pycache__"
                    || name.starts_with(".env") || name.starts_with("credentials") || name.starts_with("secrets")) return false;
                const auto stem = name.substr(0u, name.find('.'));
                if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul"
                    || (stem.size() == 4u && (stem.starts_with("com") || stem.starts_with("lpt"))
                        && stem.back() >= '1' && stem.back() <= '9')) return false;
            }
            const auto extension = folded(path.extension().string());
            constexpr std::array rejected{".exe", ".dll", ".so", ".dylib", ".lib", ".a", ".obj", ".pdb",
                ".ifc", ".pyc", ".gguf", ".jsonl", ".pem", ".key", ".pfx", ".p12", ".db", ".sqlite",
                ".zip", ".gz", ".tgz", ".tar", ".7z"};
            return std::find(rejected.begin(), rejected.end(), extension) == rejected.end();
        }
        void check_chain(const fs::path& path)
        {
            require(path.native().size() < 4096u, Code::unsafe_path,
                "The package path exceeds the supported staging length.");
            std::error_code error;
            fs::path current;
            for (const auto& component : path)
            {
                require(component != ".." && component != ".", Code::unsafe_path, "Source paths cannot contain parent traversal.");
                current /= component;
                const auto status = fs::symlink_status(current, error);
                if (error == std::errc::no_such_file_or_directory) { error.clear(); continue; }
                require(!error && !fs::is_symlink(status), Code::unsafe_path, "Source and cache paths must not contain symbolic links.");
            }
        }
        fs::path absolute_plain(const fs::path& input)
        {
            require(!input.empty() && input.is_absolute(), Code::unsafe_path, "Choose an absolute local file path.");
            require(!input.generic_string().starts_with("//"), Code::unsafe_path, "Network paths are not admitted by local source staging.");
            check_chain(input);
            std::error_code error;
            auto result = fs::weakly_canonical(input, error);
            require(!error, Code::unsafe_path, "The local path could not be resolved safely.");
            return result;
        }
        std::string read_small(const fs::path& path, std::size_t maximum)
        {
            check_chain(path);
            std::error_code error;
            require(fs::is_regular_file(path, error) && !error, Code::missing_file, "A declared source or license file is missing.");
            const auto size = fs::file_size(path, error);
            require(!error && size > 0u && size <= maximum, Code::budget_exceeded, "The manifest, license or receipt exceeds its local inspection limit.");
            std::ifstream input(path, std::ios::binary);
            std::string bytes(static_cast<std::size_t>(size), '\0');
            input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            require(input && input.peek() == std::char_traits<char>::eof(), Code::source_changed, "A source file changed while it was read.");
            return bytes;
        }
        SourceFile hash_file(const fs::path& root, const std::string& relative, std::stop_token token)
        {
            require(portable_relative(relative), Code::unsafe_path, "A package source path is unsafe or not portable.");
            const auto path = root / relative;
            check_chain(path);
            std::error_code error;
            require(fs::is_regular_file(path, error) && !error, Code::missing_file, "A declared package source file is missing.");
            const auto expected = fs::file_size(path, error);
            require(!error && expected <= kMaximumFileBytes, Code::budget_exceeded, "A source file exceeds the 16 MiB limit.");
            std::ifstream input(path, std::ios::binary);
            require(static_cast<bool>(input), Code::io_failure, "A package source file could not be opened.");
            std::array<char, 64u * 1024u> buffer{};
            core::sha256::Hasher hash;
            std::uint64_t bytes{};
            while (input)
            {
                check_cancelled(token);
                input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                const auto count = static_cast<std::size_t>(input.gcount());
                require(count <= kMaximumFileBytes - bytes, Code::budget_exceeded, "A source file grew beyond its inspection limit.");
                bytes += count;
                hash.update(std::string_view{buffer.data(), count});
            }
            require(input.eof() && bytes == expected, Code::source_changed, "A package source file changed during inspection.");
            return {relative, bytes, core::sha256::hex(hash.finish())};
        }
        std::string tree_hash(const Inspection& inspection)
        {
            core::sha256::Hasher hash;
            hash.update("epoch.local-source-package.v1\n");
            hash.update(inspection.registry_package_id + "\n" + inspection.manifest_id + "\n");
            for (const auto& file : inspection.files)
                hash.update(file.relative_path + "\n" + std::to_string(file.bytes) + "\n" + file.sha256 + "\n");
            return "sha256:" + core::sha256::hex(hash.finish());
        }
        void check_review(const Inspection& reviewed)
        {
            const auto* binding = package_registry::source_manifest_binding(reviewed.manifest_id);
            require(reviewed.review_ready() && binding && binding->registry_id == reviewed.registry_package_id
                && binding->source_path == reviewed.source_path && package_registry::source_manifest_repository_matches(*binding, reviewed.declared_repository)
                && !reviewed.files.empty() && reviewed.files.size() <= 192u
                && package_registry::content_hash_identity(reviewed.content_hash)
                && tree_hash(reviewed) == reviewed.content_hash,
                Code::invalid_manifest, "The reviewed source snapshot is invalid.");
        }
        std::string receipt_text(const Inspection& reviewed)
        {
            // All emitted strings are exact reviewed identifiers, fixed URLs or
            // portable paths (quotes, slashes and controls are rejected above).
            std::string text = "{\n\"schema\":\"epoch.local-source-package.v1\",\n\"registry_package_id\":\""
                + reviewed.registry_package_id + "\",\n\"manifest_id\":\"" + reviewed.manifest_id
                + "\",\n\"content_hash\":\"" + reviewed.content_hash
                + "\",\n\"declared_repository\":\"" + reviewed.declared_repository
                + "\",\n\"snapshot_kind\":\"local_sha256\",\n\"automatic_execution\":false,\n\"activated\":false,\n\"files\":[";
            bool first = true;
            for (const auto& file : reviewed.files)
            {
                require(portable_relative(file.relative_path) && core::sha256::from_hex(file.sha256).has_value(),
                    Code::unsafe_path, "The source inventory is invalid.");
                if (!first) text += ',';
                first = false;
                text += "{\"path\":\"" + file.relative_path + "\",\"bytes\":" + std::to_string(file.bytes)
                    + ",\"sha256\":\"" + file.sha256 + "\"}";
            }
            return text + "]\n}\n";
        }
        fs::path version_path(const Inspection& reviewed, const fs::path& cache)
        {
            check_review(reviewed);
            const auto base = absolute_plain(cache);
            require(!within(base, reviewed.repository_root) && !within(reviewed.repository_root, base)
                && !same_path(base, reviewed.repository_root), Code::unsafe_path, "Source and package cache directories must be separate.");
            const auto result = base / reviewed.manifest_id / "versions" / reviewed.content_hash.substr(7u);
            check_chain(result);
            require(within(result, base), Code::unsafe_path, "The package destination escaped its cache.");
            return result;
        }
        StageResult verify_version(const Inspection& reviewed, const fs::path& version, std::stop_token token,
            bool project_dependency = false)
        {
            require(read_small(version / "source.receipt.json", kMaximumManifestBytes) == receipt_text(reviewed),
                Code::cache_collision, "This cache version already exists with a different or incomplete receipt.");
            std::set<std::string> expectedFiles{"source.receipt.json"};
            if (project_dependency) expectedFiles.insert("project.dependency.json");
            std::set<std::string> expectedDirectories{"source"};
            for (const auto& file : reviewed.files)
            {
                const auto relative = fs::path{"source"} / file.relative_path;
                expectedFiles.insert(relative.generic_string());
                for (auto directory = relative.parent_path(); !directory.empty(); directory = directory.parent_path())
                    expectedDirectories.insert(directory.generic_string());
            }
            std::error_code error;
            fs::recursive_directory_iterator cursor(version, error), end;
            require(!error, Code::io_failure, "The source cache could not be inventoried.");
            for (; cursor != end; cursor.increment(error))
            {
                check_cancelled(token);
                require(!error, Code::io_failure, "The source cache inventory changed while it was read.");
                const auto status = cursor->symlink_status(error);
                const auto relative = cursor->path().lexically_relative(version).generic_string();
                require(!error && !fs::is_symlink(status)
                    && ((fs::is_regular_file(status) && expectedFiles.contains(relative))
                        || (fs::is_directory(status) && expectedDirectories.contains(relative))),
                    Code::cache_collision, "The cached source contains a path outside the approved inventory.");
            }
            require(!error, Code::io_failure, "The source cache inventory could not finish.");
            for (const auto& expected : reviewed.files)
            {
                check_cancelled(token);
                const auto actual = hash_file(version / "source", expected.relative_path, token);
                require(actual.bytes == expected.bytes && actual.sha256 == expected.sha256,
                    Code::source_changed, "Cached package source changed; no build or activation is allowed.");
            }
            return {Code::already_staged, "Source staged and verified. It has not been built or activated.",
                reviewed.content_hash, version / "source" / reviewed.source_path, version / "source.receipt.json"};
        }
        void copy_snapshot(const Inspection& current, const fs::path& source_root,
            const fs::path& staging, std::stop_token token)
        {
            std::error_code error;
            for (const auto& file : current.files)
            {
                check_cancelled(token);
                const auto destination = staging / "source" / file.relative_path;
                fs::create_directories(destination.parent_path(), error);
                require(!error, Code::io_failure, "A source staging directory could not be created.");
                check_chain(destination.parent_path());
                check_chain(source_root / file.relative_path);
                std::ifstream input(source_root / file.relative_path, std::ios::binary);
                std::ofstream output(destination, std::ios::binary | std::ios::out | std::ios::noreplace);
                require(input && output, Code::io_failure, "A source file could not be copied without replacing existing data.");
                std::array<char, 64u * 1024u> bytes{};
                std::uint64_t copiedBytes{};
                while (input)
                {
                    check_cancelled(token);
                    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
                    const auto count = input.gcount();
                    require(static_cast<std::uint64_t>(count) <= file.bytes - copiedBytes,
                        Code::source_changed, "Source grew after review; the copy was cancelled.");
                    copiedBytes += static_cast<std::uint64_t>(count);
                    output.write(bytes.data(), count);
                }
                output.flush();
                require(input.eof() && output && copiedBytes == file.bytes, Code::io_failure, "A package source copy failed.");
                output.close();
                const auto copied = hash_file(staging / "source", file.relative_path, token);
                require(copied.bytes == file.bytes && copied.sha256 == file.sha256,
                    Code::source_changed, "Source changed while it was copied; the snapshot was not published.");
            }
            const auto receipt = receipt_text(current);
            {
                std::ofstream output(staging / "source.receipt.json", std::ios::binary | std::ios::out | std::ios::noreplace);
                output.write(receipt.data(), static_cast<std::streamsize>(receipt.size()));
                output.flush();
                require(static_cast<bool>(output), Code::io_failure, "The source receipt could not be committed.");
            }
        }
        fs::path project_root_path(const fs::path& project)
        {
            const auto root = absolute_plain(project);
            std::error_code error;
            require(fs::is_directory(root, error) && !error, Code::unsafe_path, "Choose an existing project directory.");
            check_chain(root / "project.epoch.json");
            require(fs::is_regular_file(root / "project.epoch.json", error) && !error,
                Code::missing_file, "The project manifest is missing.");
            return root;
        }
        fs::path project_package_relative(const Inspection& reviewed)
        { return fs::path{"Packages"} / reviewed.manifest_id; }
        fs::path project_source_relative(const Inspection& reviewed)
        { return project_package_relative(reviewed) / "source" / reviewed.source_path; }
        std::string project_receipt_text(const Inspection& reviewed)
        {
            return "{\n\"schema\":\"epoch.project-source-dependency.v1\",\n\"registry_package_id\":\""
                + reviewed.registry_package_id + "\",\n\"manifest_id\":\"" + reviewed.manifest_id
                + "\",\n\"content_hash\":\"" + reviewed.content_hash + "\",\n\"provider_source\":\""
                + project_source_relative(reviewed).generic_string()
                + "\",\n\"source_attached\":true,\n\"automatic_execution\":false\n}\n";
        }
        ProjectAttachment verify_project(const Inspection& reviewed, const fs::path& root, std::stop_token token,
            Code receipt_mismatch = Code::cache_collision)
        {
            check_cancelled(token);
            check_review(reviewed);
            const auto relative = project_package_relative(reviewed);
            const auto folder = root / relative;
            check_chain(folder);
            require(read_small(folder / "project.dependency.json", kMaximumManifestBytes) == project_receipt_text(reviewed),
                receipt_mismatch, "The project's immutable dependency receipt does not match this approved source.");
            std::error_code error;
            fs::directory_iterator cursor(folder, error), end;
            require(!error, Code::io_failure, "The project dependency could not be inspected.");
            std::size_t count{};
            for (; cursor != end; cursor.increment(error))
            {
                check_cancelled(token);
                require(!error && ++count <= 3u, Code::cache_collision, "Unexpected files exist in this project dependency.");
                const auto name = cursor->path().filename().string();
                const auto status = cursor->symlink_status(error);
                require(!error && !fs::is_symlink(status)
                    && (((name == "project.dependency.json" || name == "source.receipt.json") && fs::is_regular_file(status))
                        || (name == "source" && fs::is_directory(status))),
                    Code::cache_collision, "The project dependency contains an unapproved path.");
            }
            require(!error && count == 3u, Code::cache_collision, "The project dependency is incomplete.");
            (void)verify_version(reviewed, folder, token, true);
            return {Code::already_staged, "Project source dependency verified; no producer was executed.",
                reviewed.registry_package_id, reviewed.manifest_id, reviewed.content_hash,
                project_source_relative(reviewed), relative / "project.dependency.json"};
        }
        void remove_owned(const fs::path& path, const fs::path& parent) noexcept
        {
            try
            {
                if (!path.empty() && within(path, parent))
                {
                    check_chain(path);
                    std::error_code error;
                    (void)fs::remove_all(path, error);
                }
            }
            catch (...) {}
        }
        platform::filesystem::ExclusiveFileLease project_source_lease(const fs::path& root,std::string_view id)
        {
            const auto parent=root/"Packages";check_chain(parent);
            fs::create_directories(parent);check_chain(parent);
            auto lease=platform::filesystem::ExclusiveFileLease::acquire(parent/(".source-"+std::string(id)+".lock"));
            require(static_cast<bool>(lease),Code::cache_collision,"Another source operation owns this project/package, or its lease path is invalid. Retry after it finishes.");
            return lease;
        }
        enum class UpdateCommit { published, unchanged, rolled_back, recovery_required };
        template<class Rename>
        UpdateCommit commit_project_source_update(const fs::path& staging,const fs::path& destination,
            const fs::path& previous,Rename&& rename)
        {
            std::error_code error;rename(destination,previous,error);
            if(error)return UpdateCommit::unchanged;
            std::error_code publicationError;rename(staging,destination,publicationError);
            if(!publicationError)return UpdateCommit::published;
            std::error_code restorationError;rename(previous,destination,restorationError);
            return restorationError?UpdateCommit::recovery_required:UpdateCommit::rolled_back;
        }
    }

    Inspection inspect_local_source(const fs::path& manifest, std::stop_token token)
    {
        Inspection result;
        try
        {
            check_cancelled(token);
            result.manifest_path = absolute_plain(manifest);
            require(result.manifest_path.filename() == "package.epoch-package.json", Code::invalid_manifest,
                "Choose a package.epoch-package.json source manifest.");
            const auto parsed = package_catalog::parse_local_source_manifest(read_small(result.manifest_path, kMaximumManifestBytes));
            require(parsed.ok, Code::invalid_manifest, parsed.message);
            const auto* binding = package_registry::source_manifest_binding(parsed.id);
            require(binding && package_registry::is_research_or_downloadable(binding->registry_id),
                Code::unknown_package, "This source manifest has no reviewed optional-package identity.");
            require(package_registry::source_manifest_repository_matches(*binding, parsed.repository) && parsed.source_path == binding->source_path,
                Code::invalid_manifest, "The package's declared repository or source path does not match its reviewed identity.");
            const auto* descriptor = package_registry::find(binding->registry_id);
            require(descriptor && !package_registry::is_network_sensitive(*descriptor), Code::invalid_manifest,
                "Server and listener packages are not admitted by local source staging.");
            result.registry_package_id = std::string{binding->registry_id};
            result.manifest_id = parsed.id;
            result.source_path = parsed.source_path;
            result.declared_repository = parsed.repository;
            result.display_name = parsed.display_name;
            result.summary = parsed.summary;
            result.license = parsed.license;
            auto repository = result.manifest_path.parent_path();
            for (const auto& ignored : fs::path{result.source_path}) { (void)ignored; repository = repository.parent_path(); }
            result.repository_root = absolute_plain(repository);
            require(result.repository_root / result.source_path == result.manifest_path.parent_path(),
                Code::unsafe_path, "The source folder does not match the package manifest.");
            require(!read_small(repository / "LICENSE", kMaximumManifestBytes).empty(), Code::missing_file,
                "The source repository's license file is required for review.");
            std::map<std::string, std::string> inventory;
            auto add = [&](std::string relative, bool declared)
            {
                require(portable_relative(relative), Code::unsafe_path, "A declared source path is unsafe.");
                const auto key = folded(relative);
                const auto found = inventory.find(key);
                require(found == inventory.end() || (!declared && found->second == relative),
                    Code::unsafe_path, "The source manifest contains duplicate or case-colliding paths.");
                inventory.emplace(key, std::move(relative));
            };
            for (const auto& relative : parsed.source_files) add(result.source_path + '/' + relative, true);
            add(result.source_path + "/package.epoch-package.json", false);
            add(result.source_path + '/' + parsed.script_path, false);
            add("packages/_api/epoch_package_script_api.hpp", false);
            add("LICENSE", false);
            for (const auto& [key, relative] : inventory)
            {
                (void)key;
                const auto file = hash_file(repository, relative, token);
                require(file.bytes <= kMaximumTotalBytes - result.total_bytes, Code::budget_exceeded,
                    "This source package exceeds the 64 MiB staging limit.");
                result.total_bytes += file.bytes;
                result.files.push_back(file);
            }
            result.content_hash = tree_hash(result);
            result.code = Code::review_ready;
            result.message = "Local source and license are ready for review. Nothing has been copied, built or run.";
        }
        catch (const Failure& failure) { result.code = failure.code; result.message = failure.message; }
        catch (...) { result.code = Code::io_failure; result.message = "Local source inspection failed without changing the project."; }
        return result;
    }

    StageResult verify_staged_source(const Inspection& reviewed, const fs::path& cache, std::stop_token token)
    {
        try { return verify_version(reviewed, version_path(reviewed, cache), token); }
        catch (const Failure& failure) { return {failure.code, failure.message}; }
        catch (...) { return {Code::io_failure, "The staged source could not be verified."}; }
    }

    CachedSource inspect_cached_source(const fs::path& cache, std::string_view packageId,
        std::stop_token token)
    {
        try
        {
            check_cancelled(token);
            const package_registry::SourceManifestBinding* binding{};
            for (const auto& candidate : package_registry::kSourceManifestBindings)
                if (candidate.registry_id == packageId) binding = &candidate;
            require(binding != nullptr, Code::unknown_package, "This package has no admitted local source binding.");
            const auto base = absolute_plain(cache);
            const auto versions = base / binding->manifest_id / "versions";
            check_chain(versions);
            std::error_code error;
            const bool exists = fs::exists(versions, error);
            require(!error, Code::io_failure, "The local source cache is unavailable.");
            require(exists, Code::missing_file, "This extension has not been downloaded to the local package cache. No project files were added.");
            require(fs::is_directory(versions, error) && !error, Code::unsafe_path, "The package versions path is not an ordinary directory.");
            CachedSource found;
            std::size_t count{};
            for (fs::directory_iterator cursor{versions, error}, end; !error && cursor != end; cursor.increment(error))
            {
                check_cancelled(token);
                require(++count <= 64u, Code::budget_exceeded, "Too many local package versions. Choose a specific source manifest with Browse.");
                const auto name = cursor->path().filename().generic_string();
                if (name.starts_with(".staging-")) continue;
                require(core::sha256::from_hex(name).has_value(), Code::cache_collision, "The package cache contains an unrecognized version. Choose a specific source manifest with Browse.");
                check_chain(cursor->path());
                require(cursor->is_directory(error) && !error, Code::unsafe_path, "A package cache version is not an ordinary directory.");
                auto inspection = inspect_local_source(cursor->path() / "source" / binding->source_path / "package.epoch-package.json", token);
                require(inspection.review_ready() && inspection.registry_package_id == binding->registry_id
                    && inspection.manifest_id == binding->manifest_id && inspection.content_hash == "sha256:" + name,
                    inspection.code == Code::cancelled ? Code::cancelled : Code::source_changed,
                    "The cached package source no longer matches its exact version identity. Review a known source manifest instead.");
                auto staged = verify_version(inspection, cursor->path(), token);
                require(!found.review_ready(), Code::cache_collision,
                    "Several cached source versions are available. Choose the desired package manifest with Browse; no version was selected automatically.");
                found.inspection = std::move(inspection);
                found.staged = std::move(staged);
            }
            require(!error, Code::io_failure, "The local source cache inventory could not finish.");
            require(found.review_ready(), Code::missing_file, "No complete local package is available. No project files were added.");
            return found;
        }
        catch (const Failure& failure) { return {{failure.code, failure.message}, {failure.code, failure.message}}; }
        catch (...) { return {{Code::io_failure, "The local source cache could not be inspected."}, {Code::io_failure, "The local source cache could not be inspected."}}; }
    }

    CachedSource inspect_available_source(const fs::path& cache,
        const fs::path& repository, std::string_view packageId, std::stop_token token)
    {
        auto found = inspect_cached_source(cache, packageId, token);
        // An explicit review can select the caller's current durable source,
        // but never conceal a corrupt cache or replace its immutable versions.
        if (repository.empty() || (!found.review_ready()
            && found.inspection.code != Code::missing_file
            && found.inspection.code != Code::cache_collision)) return found;
        try
        {
            check_cancelled(token);
            const package_registry::SourceManifestBinding* binding{};
            for (const auto& candidate : package_registry::kSourceManifestBindings)
                if (candidate.registry_id == packageId || candidate.manifest_id == packageId)
                { binding = &candidate; break; }
            require(binding, Code::unknown_package, "This extension has no reviewed local source identity.");
            const auto root = absolute_plain(repository);
            const auto manifest = root / binding->source_path / "package.epoch-package.json";
            std::error_code error;
            const auto status = fs::symlink_status(manifest, error);
            if (error == std::errc::no_such_file_or_directory
                || (!error && !fs::exists(status))) return found;
            require(!error, Code::io_failure, "The durable extension source could not be inspected.");
            auto inspection = inspect_local_source(manifest, token);
            if (!inspection.review_ready()) return {std::move(inspection), {}};
            require(inspection.registry_package_id == binding->registry_id
                && same_path(inspection.repository_root, root), Code::invalid_manifest,
                "The discovered source does not belong to the selected extension.");
            // Review Extension permits a disposable local source copy. The
            // separate Add/Create decision still owns project attachment/build.
            auto staged = stage_local_source(inspection, inspection.content_hash, cache, token);
            if (!staged.source_staged()) return {std::move(inspection), std::move(staged)};
            // Return the exact reviewed identity, not an arbitrary cache member
            // when both the previous and current versions are now retained.
            return {std::move(inspection), std::move(staged)};
        }
        catch (const Failure& failure) { return {{failure.code, failure.message}, {failure.code, failure.message}}; }
        catch (...) { return {{Code::io_failure, "The extension source could not be discovered safely."}, {}}; }
    }

    StageResult stage_local_source(const Inspection& reviewed, std::string_view approved,
        const fs::path& cache, std::stop_token token)
    {
        fs::path staging, version, parent;
        bool reserved = false;
        bool stagingOwned = false;
        try
        {
            check_cancelled(token);
            check_review(reviewed);
            require(approved == reviewed.content_hash, Code::approval_required, "Approve this exact source snapshot before staging it.");
            const auto current = inspect_local_source(reviewed.manifest_path, token);
            require(current.review_ready() && current.content_hash == reviewed.content_hash,
                current.code == Code::cancelled ? Code::cancelled : Code::source_changed,
                "The source changed after review. Inspect and approve it again.");
            version = version_path(current, cache);
            parent = version.parent_path();
            std::error_code error;
            if (fs::exists(version, error)) return verify_version(current, version, token);
            require(!error, Code::io_failure, "The package cache could not be inspected.");
            fs::create_directories(parent, error);
            require(!error, Code::io_failure, "The package cache directory could not be created.");
            check_chain(parent);
            staging = parent / (".staging-" + current.content_hash.substr(7u) + '-'
                + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            const bool stagingCreated = fs::create_directory(staging, error);
            if (error) throw Failure{Code::io_failure, "The source staging directory could not be created: " + error.message()};
            require(stagingCreated, Code::cache_collision, "A source staging transaction already exists; nothing was replaced.");
            stagingOwned = true;
            copy_snapshot(current, current.repository_root, staging, token);
            (void)verify_version(current, staging, token);
            check_cancelled(token);
            const bool versionCreated = fs::create_directory(version, error);
            if (error) throw Failure{Code::io_failure, "The source cache version could not be reserved: " + error.message()};
            require(versionCreated, Code::cache_collision, "This cache version appeared during staging; it was not replaced.");
            reserved = true;
            check_chain(version);
            fs::rename(staging / "source", version / "source", error);
            require(!error, Code::io_failure, "The reviewed source snapshot could not be published.");
            check_cancelled(token);
            fs::rename(staging / "source.receipt.json", version / "source.receipt.json", error);
            require(!error, Code::io_failure, "The verified source receipt could not be published.");
            reserved = false;
            remove_owned(staging, parent);
            stagingOwned = false;
            return {Code::staged, "Source staged and verified. It has not been built, run or activated.",
                current.content_hash, version / "source" / current.source_path, version / "source.receipt.json"};
        }
        catch (const Failure& failure)
        {
            if (stagingOwned) remove_owned(staging, parent);
            if (reserved) remove_owned(version, parent);
            return {failure.code, failure.message};
        }
        catch (...)
        {
            if (stagingOwned) remove_owned(staging, parent);
            if (reserved) remove_owned(version, parent);
            return {Code::io_failure, "Local source staging failed; no project or source package was activated."};
        }
    }

    ProjectAttachment verify_project_source(const Inspection& reviewed, const fs::path& project, std::stop_token token)
    {
        try { return verify_project(reviewed, project_root_path(project), token); }
        catch (const Failure& failure) { return {failure.code, failure.message}; }
        catch (...) { return {Code::io_failure, "The project source dependency could not be verified."}; }
    }

    ProjectAttachment attach_project_source(const Inspection& reviewed, const StageResult& staged,
        std::string_view approved, const fs::path& project, std::stop_token token)
    {
        fs::path staging, destination, parent;
        bool stagingOwned{}, reserved{};
        try
        {
            check_cancelled(token);
            check_review(reviewed);
            require(approved == reviewed.content_hash, Code::approval_required,
                "Approve this exact source snapshot for this project before attaching it.");
            require(staged.source_staged() && staged.content_hash == reviewed.content_hash,
                Code::source_changed, "The staged source does not match the approved snapshot.");
            const auto version = absolute_plain(staged.receipt_path).parent_path();
            require(absolute_plain(staged.source_root) == version / "source" / reviewed.source_path,
                Code::unsafe_path, "The staged provider source does not match its receipt.");
            (void)verify_version(reviewed, version, token);
            const auto root = project_root_path(project);
            const auto sourceLease=project_source_lease(root,reviewed.manifest_id);
            require(root != version && !within(root, version) && !within(version, root),
                Code::unsafe_path, "Project and staging source directories must be separate.");
            parent = root / "Packages";
            destination = root / project_package_relative(reviewed);
            check_chain(destination);
            std::error_code error;
            if (fs::exists(destination, error)) return verify_project(reviewed, root, token);
            require(!error, Code::io_failure, "The project dependency path could not be inspected.");
            fs::create_directories(parent, error);
            require(!error, Code::io_failure, "The project package directory could not be created.");
            check_chain(parent);
            staging = parent / (".staging-" + reviewed.manifest_id + '-'
                + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            require(fs::create_directory(staging, error) && !error,
                Code::cache_collision, "A project source transaction already exists.");
            stagingOwned = true;
            copy_snapshot(reviewed, version / "source", staging, token);
            (void)verify_version(reviewed, staging, token);
            const auto receipt = project_receipt_text(reviewed);
            {
                std::ofstream output(staging / "project.dependency.json", std::ios::binary | std::ios::noreplace);
                output.write(receipt.data(), static_cast<std::streamsize>(receipt.size()));
                output.flush();
                require(static_cast<bool>(output), Code::io_failure, "The project dependency receipt could not be saved.");
            }
            check_cancelled(token);
            require(fs::create_directory(destination, error) && !error,
                Code::cache_collision, "This project dependency appeared during attachment; it was not replaced.");
            reserved = true;
            check_chain(destination);
            fs::rename(staging / "source", destination / "source", error);
            require(!error, Code::io_failure, "The project source snapshot could not be published (filesystem "
                + std::to_string(error.value()) + ": " + error.message() + ").");
            check_cancelled(token);
            fs::rename(staging / "source.receipt.json", destination / "source.receipt.json", error);
            require(!error, Code::io_failure, "The project source receipt could not be published.");
            check_cancelled(token);
            fs::rename(staging / "project.dependency.json", destination / "project.dependency.json", error);
            require(!error, Code::io_failure, "The project dependency receipt could not be published.");
            auto result = verify_project(reviewed, root, token);
            reserved = false;
            remove_owned(staging, parent);
            stagingOwned = false;
            result.code = Code::staged;
            result.message = "Approved source attached to this project; compilation remains a project build step.";
            return result;
        }
        catch (const Failure& failure)
        {
            if (stagingOwned) remove_owned(staging, parent);
            if (reserved) remove_owned(destination, parent);
            return {failure.code, failure.message};
        }
        catch (...)
        {
            if (stagingOwned) remove_owned(staging, parent);
            if (reserved) remove_owned(destination, parent);
            return {Code::io_failure, "Project source attachment failed; no producer was executed."};
        }
    }

    ProjectAttachment update_project_source(const Inspection& reviewed,const StageResult& staged,
        std::string_view approved,const fs::path& project,std::string_view expectedPrevious,std::stop_token token)
    {
        fs::path staging,parent,previous;bool stagingOwned{},published{};
        try {
            check_cancelled(token);check_review(reviewed);
            require(approved==reviewed.content_hash,Code::approval_required,"Approve this exact replacement source snapshot.");
            require(!expectedPrevious.empty()&&expectedPrevious!=approved,Code::approval_required,
                "An update requires the explicitly observed previous source identity and a different reviewed version.");
            require(staged.source_staged()&&staged.content_hash==approved,Code::source_changed,"The staged replacement differs from the reviewed source.");
            const auto version=absolute_plain(staged.receipt_path).parent_path();
            require(absolute_plain(staged.source_root)==version/"source"/reviewed.source_path,Code::unsafe_path,"Replacement source has no matching staged receipt.");
            (void)verify_version(reviewed,version,token);
            const auto root=project_root_path(project);
            require(root!=version&&!within(root,version)&&!within(version,root),Code::unsafe_path,"Project and staged source must be separate.");
            const auto lease=project_source_lease(root,reviewed.manifest_id);
            const auto old=load_project_attachment(root,reviewed.manifest_id,token);
            require(old.source_attached()&&old.content_hash==expectedPrevious,Code::source_changed,
                "The installed source differs from the version reviewed for replacement. Review the current project source again.");
            const auto destination=root/project_package_relative(reviewed);parent=destination.parent_path();
            const auto manifestBytes=read_small(root/"project.epoch.json",kMaximumManifestBytes);
            const auto oldReceipt=read_small(destination/"project.dependency.json",kMaximumManifestBytes);
            const auto oldSourceReceipt=read_small(destination/"source.receipt.json",kMaximumManifestBytes);
            const auto stamp=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            staging=parent/(".update-"+reviewed.manifest_id+'-'+stamp);
            require(fs::create_directory(staging),Code::cache_collision,"A replacement transaction already exists.");stagingOwned=true;
            copy_snapshot(reviewed,version/"source",staging,token);(void)verify_version(reviewed,staging,token);
            const auto receipt=project_receipt_text(reviewed);
            {std::ofstream output(staging/"project.dependency.json",std::ios::binary|std::ios::noreplace);
                output.write(receipt.data(),static_cast<std::streamsize>(receipt.size()));output.flush();
                require(static_cast<bool>(output),Code::io_failure,"The replacement project receipt could not be saved.");}
            const auto current=load_project_attachment(root,reviewed.manifest_id,token);
            require(current.source_attached()&&current.content_hash==expectedPrevious
                &&read_small(root/"project.epoch.json",kMaximumManifestBytes)==manifestBytes
                &&read_small(destination/"project.dependency.json",kMaximumManifestBytes)==oldReceipt
                &&read_small(destination/"source.receipt.json",kMaximumManifestBytes)==oldSourceReceipt,
                Code::source_changed,"Project/source receipts changed during replacement preparation. No source was replaced.");
            const auto history=parent/".source-history"/reviewed.manifest_id;check_chain(history);
            fs::create_directories(history);check_chain(history);
            previous=history/(std::string(expectedPrevious.substr(7,12))+'-'+stamp);check_chain(previous);
            require(!fs::exists(previous),Code::cache_collision,"The retained-source destination already exists.");
            check_cancelled(token);
            // No cancellation between these renames and rollback. The OS does
            // not provide a portable atomic exchange of nonempty directories;
            // a crash can leave the old source safely retained in history.
            const auto committed=commit_project_source_update(staging,destination,previous,
                [](const fs::path& from,const fs::path& to,std::error_code& error){fs::rename(from,to,error);});
            if(committed==UpdateCommit::recovery_required) {
                stagingOwned=false;
                return {Code::io_failure,"Source publication and rollback failed. Previous source retained at Packages/"+previous.lexically_relative(parent).generic_string()
                    +"; replacement retained at Packages/"+staging.lexically_relative(parent).generic_string()+". The active dependency must be re-reviewed.",
                    {},{},{},{},{},previous/"source"/reviewed.source_path};
            }
            require(committed==UpdateCommit::published,Code::io_failure,
                committed==UpdateCommit::rolled_back?"Replacement publication failed; previous project source was restored."
                    :"The installed source could not be retained; project source was not replaced.");
            published=true;stagingOwned=false;
            auto result=verify_project(reviewed,root,{});result.code=Code::staged;
            result.retained_previous_source=previous/"source"/reviewed.source_path;
            result.message="Reviewed project source updated. Previous source was retained in Packages/.source-history. Scenes and settings were not changed; choose Build & Generate to use this version.";
            return result;
        }catch(const Failure& failure) {
            if(stagingOwned)remove_owned(staging,parent);
            return {failure.code,published?"Replacement published but verification failed. Previous source retained at Packages/"+previous.lexically_relative(parent).generic_string()+". "+failure.message:failure.message,
                {},{},{},{},{},published?previous/"source"/reviewed.source_path:fs::path{}};
        }catch(...) {
            if(stagingOwned)remove_owned(staging,parent);
            return {Code::io_failure,published?"Replacement verification failed. Previous source retained at Packages/"+previous.lexically_relative(parent).generic_string()+".":"Source update failed; no scene or settings were changed.",
                {},{},{},{},{},published?previous/"source"/reviewed.source_path:fs::path{}};
        }
    }

    ProjectAttachment load_project_attachment(const fs::path& project, std::string_view packageId, std::stop_token token)
    {
        try
        {
            check_cancelled(token);
            const package_registry::SourceManifestBinding* binding{};
            for (const auto& candidate : package_registry::kSourceManifestBindings)
                if (candidate.registry_id == packageId || candidate.manifest_id == packageId) binding = &candidate;
            require(binding, Code::unknown_package, "This project dependency has no reviewed package identity.");
            const auto root = project_root_path(project);
            const auto folder = root / "Packages" / binding->manifest_id;
            check_chain(folder);
            std::error_code error;
            fs::directory_iterator cursor(folder, error), end;
            require(!error, Code::missing_file, "No source dependency is attached for this package.");
            std::size_t count{};
            for (; cursor != end; cursor.increment(error))
            {
                check_cancelled(token);
                require(!error && ++count <= 3u, Code::cache_collision, "The project dependency contains unexpected paths.");
                const auto name = cursor->path().filename().string();
                const auto status = cursor->symlink_status(error);
                require(!error && !fs::is_symlink(status), Code::unsafe_path, "Project dependency paths must not be linked.");
                require(((name == "project.dependency.json" || name == "source.receipt.json") && fs::is_regular_file(status))
                    || (name == "source" && fs::is_directory(status)),
                    Code::cache_collision, "The project source path is invalid.");
            }
            require(!error && count == 3u, Code::cache_collision, "The project dependency is incomplete.");
            const auto reviewed = inspect_local_source(folder / "source" / binding->source_path / "package.epoch-package.json", token);
            require(reviewed.review_ready(),
                reviewed.code == Code::cancelled ? Code::cancelled : Code::source_changed,
                "Project source changed; inspect and approve the replacement explicitly.");
            return verify_project(reviewed, root, token, Code::source_changed);
        }
        catch (const Failure& failure) { return {failure.code, failure.message}; }
        catch (...) { return {Code::io_failure, "The saved project dependency could not be loaded."}; }
    }

    namespace
    {
        ProjectAttachment current_feature_owner(const ProjectFeatureReview& review, std::stop_token token)
        {
            const auto requirements = package_registry::source_feature_dependencies(
                review.owner.registry_package_id, review.feature, review.enabled);
            require(requirements.valid(), Code::unknown_package, "This package feature has no reviewed source dependency rule.");
            const auto root = project_root_path(review.project_root);
            require(root == review.project_root, Code::unsafe_path, "The reviewed feature project path changed.");
            const auto owner = load_project_attachment(root, requirements.owner->registry_id, token);
            require(owner.source_attached(), owner.code, owner.message);
            require(owner.registry_package_id == review.owner.registry_package_id
                && owner.manifest_id == review.owner.manifest_id && owner.content_hash == review.owner.content_hash
                && hash_file(root, "project.epoch.json", token).sha256 == review.project_manifest_sha256,
                Code::source_changed, "The reviewed project or parent source changed. Review the feature again.");
            return owner;
        }
    }

    ProjectFeatureReview review_project_feature(const fs::path& project, std::string_view ownerPackage,
        SourceFeature feature, bool enabled, const fs::path& cache, const fs::path& extensions,
        std::stop_token token)
    {
        try
        {
            check_cancelled(token);
            const auto requirements = package_registry::source_feature_dependencies(ownerPackage, feature, enabled);
            require(requirements.valid() && requirements.required.size() <= 1u, Code::unknown_package,
                "This package feature has no bounded reviewed source dependency rule.");
            ProjectFeatureReview result;
            result.project_root = project_root_path(project);
            result.project_manifest_sha256 = hash_file(result.project_root, "project.epoch.json", token).sha256;
            result.feature = feature;
            result.enabled = enabled;
            result.owner = load_project_attachment(result.project_root, requirements.owner->registry_id, token);
            require(result.owner.source_attached(), result.owner.code, result.owner.message);
            if (requirements.required.empty())
            {
                (void)current_feature_owner(result, token);
                result.code = Code::already_staged;
                result.message = "This feature is disabled; no optional dependency was inspected or attached.";
                return result;
            }
            const auto& dependency = requirements.required.front();
            const auto folder = result.project_root / "Packages" / dependency.manifest_id;
            check_chain(folder);
            std::error_code error;
            const auto status = fs::symlink_status(folder, error);
            if (error == std::errc::no_such_file_or_directory) error.clear();
            require(!error, Code::io_failure, "The optional dependency path could not be inspected.");
            if (status.type() != fs::file_type::not_found)
            {
                result.attached_dependency = load_project_attachment(result.project_root, dependency.registry_id, token);
                require(result.attached_dependency.source_attached(), result.attached_dependency.code,
                    result.attached_dependency.message);
                result.code = Code::already_staged;
                result.message = "The feature's exact optional source is already approved in this project; rendering is not activated.";
            }
            else
            {
                result.dependency = inspect_available_source(cache, extensions, dependency.registry_id, token);
                require(result.dependency.inspection.review_ready(), result.dependency.inspection.code,
                    result.dependency.inspection.message);
                require(result.dependency.staged.source_staged(), result.dependency.staged.code,
                    result.dependency.staged.message);
                result.code = Code::review_ready;
                result.message = "Planetary ocean requires Water (FFT Ocean) source. Review and approve this exact source snapshot; no project settings were changed.";
            }
            (void)current_feature_owner(result, token);
            return result;
        }
        catch (const Failure& failure) { return {failure.code, failure.message}; }
        catch (...) { return {Code::io_failure, "The package feature source could not be reviewed safely."}; }
    }

    ProjectFeatureAdmission admit_project_feature(const ProjectFeatureReview& review,
        std::string_view approved, std::stop_token token)
    {
        try
        {
            check_cancelled(token);
            const auto requirements = package_registry::source_feature_dependencies(
                review.owner.registry_package_id, review.feature, review.enabled);
            require(requirements.valid() && requirements.required.size() <= 1u, Code::unknown_package,
                "This package feature has no bounded reviewed source dependency rule.");
            require(review.needs_approval() || review.sources_admitted(), review.code,
                "The package feature needs a current successful source review.");
            const auto root = project_root_path(review.project_root);
            // Existing parent-source writers use this same lease. The nested
            // attachment owns only the distinct FFT source lease; no callbacks,
            // compiler or renderer work runs while either lease is held.
            const auto parentLease = project_source_lease(root, requirements.owner->manifest_id);
            const auto owner = current_feature_owner(review, token);
            if (requirements.required.empty())
                return {Code::already_staged, "No optional source is required while this feature is disabled.", owner};
            const auto& required = requirements.required.front();
            ProjectAttachment dependency;
            if (review.attached_dependency.source_attached())
            {
                dependency = load_project_attachment(root, required.registry_id, token);
                require(dependency.source_attached(), dependency.code, dependency.message);
                require(dependency.content_hash == review.attached_dependency.content_hash, Code::source_changed,
                    "The optional source version changed after feature review. Review it again.");
            }
            else
            {
                require(review.dependency.inspection.registry_package_id == required.registry_id
                    && review.dependency.inspection.manifest_id == required.manifest_id, Code::source_changed,
                    "The reviewed source is not this feature's exact registered dependency.");
                require(approved == review.dependency.inspection.content_hash, Code::approval_required,
                    "Approve this exact optional dependency source before enabling the feature.");
                dependency = attach_project_source(review.dependency.inspection, review.dependency.staged,
                    approved, root, token);
                require(dependency.source_attached(), dependency.code, dependency.message);
            }
            // Direct external edits are not OS-confined by the source lease.
            // Refuse activation if observed after attachment; keep the explicitly
            // approved source for recovery, never erase an existing installation.
            try { (void)current_feature_owner(review, token); }
            catch (const Failure& failure)
            {
                return {failure.code, "Optional source retained at " + dependency.relative_source_root.generic_string()
                    + "; feature admission refused: " + failure.message, owner, dependency};
            }
            return {dependency.code, "Feature source dependencies verified; settings, build and water rendering remain caller-owned.",
                owner, dependency};
        }
        catch (const Failure& failure) { return {failure.code, failure.message}; }
        catch (...) { return {Code::io_failure, "The package feature dependency could not be admitted safely."}; }
    }
}
