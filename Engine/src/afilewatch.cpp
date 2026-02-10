// src modules/afilewatch.cppm

import <algorithm>;
import <cstdint>;
import <filesystem>;
import <fstream>;
import <system_error>;
import <vector>;

import autility.filewatch;

namespace almondnamespace::filewatch
{
    static std::uint64_t compute_file_hash(const std::filesystem::path& p)
    {
        std::ifstream in(p, std::ios::binary);
        if (!in.is_open())
            return 0;

        constexpr std::uint64_t FNV_offset = 1469598103934665603ull;
        constexpr std::uint64_t FNV_prime = 1099511628211ull;

        std::uint64_t h = FNV_offset;
        char buf[64 * 1024];

        while (in.good())
        {
            in.read(buf, sizeof(buf));
            const std::streamsize n = in.gcount();
            for (std::streamsize i = 0; i < n; ++i)
            {
                h ^= static_cast<std::uint8_t>(buf[i]);
                h *= FNV_prime;
            }
        }
        return h;
    }

    static file_state snapshot_state(const std::filesystem::path& p)
    {
        file_state s{};
        s.dirty = false;

        std::error_code ec{};

        if (!std::filesystem::exists(p, ec) || ec)
            return s;

        s.exists = true;
        s.last_write = std::filesystem::last_write_time(p, ec);
        if (ec) s.last_write = file_time{};

        s.size = std::filesystem::file_size(p, ec);
        if (ec) s.size = 0;

        s.hash = compute_file_hash(p);
        return s;
    }

    void watcher::track(std::filesystem::path p)
    {
        std::error_code ec{};
        auto canon = std::filesystem::weakly_canonical(p, ec);
        if (!ec) p = std::move(canon);

        if (is_tracked(p))
            return;

        m_files.emplace_back(p);
        m_states.emplace_back(snapshot_state(p));
    }

    void watcher::untrack(const std::filesystem::path& p)
    {
        std::error_code ec{};
        auto canon = std::filesystem::weakly_canonical(p, ec);
        const auto& key = ec ? p : canon;

        for (std::size_t i = 0; i < m_files.size(); ++i)
        {
            if (m_files[i] == key)
            {
                m_files.erase(m_files.begin() + static_cast<std::ptrdiff_t>(i));
                m_states.erase(m_states.begin() + static_cast<std::ptrdiff_t>(i));
                return;
            }
        }
    }

    bool watcher::is_tracked(const std::filesystem::path& p) const
    {
        std::error_code ec{};
        auto canon = std::filesystem::weakly_canonical(p, ec);
        const auto& key = ec ? p : canon;

        return std::find(m_files.begin(), m_files.end(), key) != m_files.end();
    }

    const std::vector<std::filesystem::path>& watcher::files() const noexcept
    {
        return m_files;
    }

    poll_result watcher::poll()
    {
        poll_result out{};

        for (std::size_t i = 0; i < m_files.size(); ++i)
        {
            const auto& p = m_files[i];
            auto& st = m_states[i];

            std::error_code ec{};
            const bool exists = std::filesystem::exists(p, ec) && !ec;

            if (!exists)
            {
                const bool was_present = st.exists;
                if (was_present)
                    out.removed.push_back(p);

                st = file_state{};
                st.dirty = was_present;
                continue;
            }

            const auto new_write = std::filesystem::last_write_time(p, ec);
            const auto new_size = std::filesystem::file_size(p, ec);

            if (!st.exists)
            {
                std::uint64_t new_hash = ec ? 0 : compute_file_hash(p);

                st.last_write = ec ? file_time{} : new_write;
                st.size = ec ? 0 : new_size;
                st.hash = new_hash;
                st.exists = true;
                st.dirty = true;
                out.added.push_back(p);
                continue;
            }

            bool changed = false;

            if (!ec)
            {
                if (st.last_write != new_write) changed = true;
                if (st.size != new_size)        changed = true;
            }
            else
            {
                changed = true;
            }

            std::uint64_t new_hash = st.hash;
            if (changed)
                new_hash = compute_file_hash(p);

            if (changed || (new_hash != st.hash))
            {
                st.last_write = ec ? file_time{} : new_write;
                st.size = ec ? 0 : new_size;
                st.hash = new_hash;
                st.dirty = true;
                st.exists = true;

                out.changed.push_back(p);
            }
            else
            {
                st.dirty = false;
                st.exists = true;
            }
        }

        return out;
    }
}
