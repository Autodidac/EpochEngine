module;
// modules/autility.filewatch.ixx
export module autility.filewatch;

import <cstdint>;
import <filesystem>;
import <string>;
import <vector>;

export namespace almondnamespace::filewatch
{
    using file_time = std::filesystem::file_time_type;

    struct file_state
    {
        file_time last_write{};      // correct type for last_write_time()
        std::uintmax_t size{};       // file size snapshot
        std::uint64_t hash{};        // content hash snapshot (optional but useful)
        bool exists{};               // last known existence state
        bool dirty{};                // mark-dirty flag for consumers
    };

    // Result for a single poll tick
    struct poll_result
    {
        std::vector<std::filesystem::path> changed;
        std::vector<std::filesystem::path> removed;
        std::vector<std::filesystem::path> added;
    };

    // Watches a set of files (not directories) for change/remove/add by polling.
    export class watcher
    {
    public:
        watcher() = default;

        void track(std::filesystem::path p);
        void untrack(const std::filesystem::path& p);
        bool is_tracked(const std::filesystem::path& p) const;

        // Poll once. Marks file_state::dirty for changed files.
        poll_result poll();

        // Read-only access (debug/diagnostics)
        const std::vector<std::filesystem::path>& files() const noexcept;

    private:
        std::vector<std::filesystem::path> m_files;
        std::vector<file_state> m_states; // parallel to m_files
    };
}
