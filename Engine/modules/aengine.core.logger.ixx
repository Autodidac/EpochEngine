/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
 **************************************************************/
module;

export module aengine.core.logger;

import <atomic>;
import <cctype>;
import <filesystem>;
import <fstream>;
import <format>;
import <iostream>;
import <memory>;
import <mutex>;
import <source_location>;
import <string>;
import <string_view>;
import <system_error>;
import <unordered_map>;
import <utility>;

import aengine.core.time;

export namespace almondnamespace::logger
{
    // ---------------------------------------------------------------------
    // Levels (keep legacy names compiling)
    // ---------------------------------------------------------------------
    enum class LogLevel : int
    {
        INFO = 0,
        WARN = 1,
        ALMOND_ERROR = 2,
        OFF = 3
    };

    // ---------------------------------------------------------------------
    // Config
    // ---------------------------------------------------------------------
    struct LogConfig
    {
        std::filesystem::path root_dir = "logs";
        LogLevel level = LogLevel::INFO;

        bool flush_each_write = true;
        bool include_source = true;

        bool console_enabled = false;
        bool file_enabled = true;
    };

    namespace detail
    {
        constexpr std::string_view level_text(LogLevel lvl) noexcept
        {
            switch (lvl)
            {
            case LogLevel::INFO:         return "INFO";
            case LogLevel::WARN:         return "WARN";
            case LogLevel::ALMOND_ERROR: return "ERROR";
            case LogLevel::OFF:          return "OFF";
            }
            return "UNKNOWN";
        }

        inline std::string_view filename_only(std::string_view path) noexcept
        {
            const auto p = path.find_last_of("/\\");
            return (p == std::string_view::npos) ? path : path.substr(p + 1);
        }

        inline std::string sanitize_system_name(std::string_view sys)
        {
            std::string out;
            out.reserve(sys.size());
            for (char c : sys)
            {
                const unsigned char uc = static_cast<unsigned char>(c);
                const bool ok = std::isalnum(uc) || c == '.' || c == '_' || c == '-';
                out.push_back(ok ? c : '_');
            }
            if (out.empty())
                out = "system";
            return out;
        }

        inline bool should_log(LogLevel msg, LogLevel cur) noexcept
        {
            if (cur == LogLevel::OFF) return false;
            return static_cast<int>(msg) >= static_cast<int>(cur);
        }

        inline std::mutex& console_mutex()
        {
            static std::mutex m{};
            return m;
        }
    } // namespace detail

    // ---------------------------------------------------------------------
    // SystemLogger (loc-first logf)
    // ---------------------------------------------------------------------
    class SystemLogger final
    {
    public:
        explicit SystemLogger(std::string systemName)
            : m_system(std::move(systemName))
        {
        }

        ~SystemLogger() { close(); }

        SystemLogger(const SystemLogger&) = delete;
        SystemLogger& operator=(const SystemLogger&) = delete;
        SystemLogger(SystemLogger&&) = delete;
        SystemLogger& operator=(SystemLogger&&) = delete;

        void configure(const LogConfig& cfg)
        {
            std::scoped_lock lock(m_mutex);

            m_level.store(cfg.level, std::memory_order_relaxed);
            m_flush_each.store(cfg.flush_each_write, std::memory_order_relaxed);
            m_include_source.store(cfg.include_source, std::memory_order_relaxed);
            m_console_enabled.store(cfg.console_enabled, std::memory_order_relaxed);
            m_file_enabled.store(cfg.file_enabled, std::memory_order_relaxed);

            close_locked();

            if (!cfg.file_enabled)
                return;

            std::error_code ec{};
            std::filesystem::create_directories(cfg.root_dir, ec);
            if (ec)
            {
                throw std::runtime_error(std::format(
                    "Logger: could not create log directory '{}': {}",
                    cfg.root_dir.string(), ec.message()));
            }

            const auto safe = detail::sanitize_system_name(m_system);
            m_path = cfg.root_dir / (safe + ".log");

            m_file.open(m_path, std::ios::out | std::ios::app);
            if (!m_file.is_open())
            {
                throw std::runtime_error(std::format(
                    "Logger: could not open log file '{}'",
                    m_path.string()));
            }
        }

        void log(LogLevel lvl,
            std::string_view msg,
            std::source_location loc)
        {
            const auto cur = m_level.load(std::memory_order_relaxed);
            if (!detail::should_log(lvl, cur))
                return;

            const bool includeSrc = m_include_source.load(std::memory_order_relaxed);

            std::string line;
            if (includeSrc)
            {
                line = std::format(
                    "{} [{}] [{}] ({}:{} {}) - {}",
                    timing::getCurrentTimeString(),
                    detail::level_text(lvl),
                    m_system,
                    detail::filename_only(loc.file_name()),
                    loc.line(),
                    loc.function_name(),
                    msg
                );
            }
            else
            {
                line = std::format(
                    "{} [{}] [{}] - {}",
                    timing::getCurrentTimeString(),
                    detail::level_text(lvl),
                    m_system,
                    msg
                );
            }

            if (m_console_enabled.load(std::memory_order_relaxed))
            {
                std::scoped_lock lock(detail::console_mutex());
                std::cout << line << "\n";
                if (m_flush_each.load(std::memory_order_relaxed))
                    std::cout.flush();
            }

            if (m_file_enabled.load(std::memory_order_relaxed))
            {
                std::scoped_lock lock(m_mutex);
                if (m_file.is_open())
                {
                    m_file << line << "\n";
                    if (m_flush_each.load(std::memory_order_relaxed))
                        m_file.flush();
                }
            }
        }

        // -----------------------------
        // LOC-FIRST logf (required)
        // -----------------------------
        template <class... Args>
        void logf(LogLevel lvl,
            std::source_location loc,
            std::format_string<Args...> fmt,
            Args&&... args)
        {
            log(lvl, std::format(fmt, std::forward<Args>(args)...), loc);
        }

        std::string_view system_name() const noexcept { return m_system; }

        std::filesystem::path file_path() const
        {
            std::scoped_lock lock(m_mutex);
            return m_path;
        }

        void close()
        {
            std::scoped_lock lock(m_mutex);
            close_locked();
        }

    private:
        void close_locked()
        {
            if (m_file.is_open())
                m_file.close();
            m_path.clear();
        }

        std::string m_system;

        mutable std::mutex m_mutex{};
        std::ofstream m_file{};
        std::filesystem::path m_path{};

        std::atomic<LogLevel> m_level{ LogLevel::INFO };
        std::atomic<bool> m_flush_each{ true };
        std::atomic<bool> m_include_source{ true };
        std::atomic<bool> m_console_enabled{ true };
        std::atomic<bool> m_file_enabled{ true };
    };

    // ---------------------------------------------------------------------
    // Hub
    // ---------------------------------------------------------------------
    class LoggerHub final
    {
    public:
        void init(LogConfig cfg)
        {
            std::scoped_lock lock(m_mutex);
            m_cfg = std::move(cfg);

            for (auto& [_, ptr] : m_systems)
                ptr->configure(m_cfg);
        }

        SystemLogger& system(std::string_view name)
        {
            std::scoped_lock lock(m_mutex);

            const std::string key{ name };
            auto it = m_systems.find(key);
            if (it != m_systems.end())
                return *it->second;

            auto ptr = std::make_unique<SystemLogger>(key);
            ptr->configure(m_cfg);

            auto [insIt, _] = m_systems.emplace(key, std::move(ptr));
            return *insIt->second;
        }

        LogConfig config() const
        {
            std::scoped_lock lock(m_mutex);
            return m_cfg;
        }

    private:
        mutable std::mutex m_mutex{};
        LogConfig m_cfg{};
        std::unordered_map<std::string, std::unique_ptr<SystemLogger>> m_systems{};
    };

    inline LoggerHub& hub()
    {
        static LoggerHub h{};
        return h;
    }

    // ---------------------------------------------------------------------
    // API
    // ---------------------------------------------------------------------
    inline void init(LogConfig cfg) { hub().init(std::move(cfg)); }
    inline SystemLogger& get(std::string_view system_name) { return hub().system(system_name); }

    inline void info(std::string_view sys, std::string_view msg,
        std::source_location loc = std::source_location::current())
    {
        hub().system(sys).log(LogLevel::INFO, msg, loc);
    }

    inline void warn(std::string_view sys, std::string_view msg,
        std::source_location loc = std::source_location::current())
    {
        hub().system(sys).log(LogLevel::WARN, msg, loc);
    }

    inline void error(std::string_view sys, std::string_view msg,
        std::source_location loc = std::source_location::current())
    {
        hub().system(sys).log(LogLevel::ALMOND_ERROR, msg, loc);
    }

    // ---------------------------------------------------------------------
    // LOC-FIRST formatted API (callers SHOULD use macros to capture loc)
    // ---------------------------------------------------------------------
    template <class... Args>
    inline void infof_loc(std::string_view sys,
        std::source_location loc,
        std::format_string<Args...> fmt,
        Args&&... args)
    {
        hub().system(sys).logf(LogLevel::INFO, loc, fmt, std::forward<Args>(args)...);
    }

    template <class... Args>
    inline void warnf_loc(std::string_view sys,
        std::source_location loc,
        std::format_string<Args...> fmt,
        Args&&... args)
    {
        hub().system(sys).logf(LogLevel::WARN, loc, fmt, std::forward<Args>(args)...);
    }

    template <class... Args>
    inline void errorf_loc(std::string_view sys,
        std::source_location loc,
        std::format_string<Args...> fmt,
        Args&&... args)
    {
        hub().system(sys).logf(LogLevel::ALMOND_ERROR, loc, fmt, std::forward<Args>(args)...);
    }

    // ---------------------------------------------------------------------
    // Legacy compatibility (minimal)
    // ---------------------------------------------------------------------
    class Logger final
    {
    public:
        Logger(const std::string& system_name_or_file,
            LogLevel level = LogLevel::INFO)
            : m_system(system_name_or_file)
        {
            (void)level;
        }

        ~Logger() = default;

        inline static Logger& GetInstance(const std::string& system_name_or_file)
        {
            static Logger instance(system_name_or_file);
            return instance;
        }

        void log(std::string_view message,
            LogLevel level = LogLevel::INFO,
            std::source_location loc = std::source_location::current())
        {
            hub().system(m_system).log(level, message, loc);
        }

        void log(const std::string& message,
            LogLevel level = LogLevel::INFO,
            std::source_location loc = std::source_location::current())
        {
            log(std::string_view{ message }, level, loc);
        }

        std::string getLogFileName() const
        {
            return hub().system(m_system).file_path().string();
        }

    private:
        std::string m_system;
    };

} // namespace almondnamespace::logger
