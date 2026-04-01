/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 ***********************************************/
module;

#include <atomic>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <format>
#include <memory>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>

export module core.logger;

import core.log;
import core.time;

export namespace epochnamespace::logger
{
    enum class LogLevel : int
    {
        INFO = 0,
        WARN = 1,
        Error = 2,
        OFF = 3
    };

    struct LogConfig
    {
        std::filesystem::path root_dir = "logs";
        LogLevel level = LogLevel::INFO;

        bool flush_each_write = true;
        bool include_source = true;

        bool console_enabled = true;
        bool file_enabled = true;
    };

    namespace detail
    {
        [[nodiscard]] constexpr std::string_view level_text(const LogLevel lvl) noexcept
        {
            switch (lvl)
            {
            case LogLevel::INFO:  return "INFO";
            case LogLevel::WARN:  return "WARN";
            case LogLevel::Error: return "ERROR";
            case LogLevel::OFF:   return "OFF";
            }
            return "UNKNOWN";
        }

        [[nodiscard]] inline std::string_view filename_only(const std::string_view path) noexcept
        {
            const auto pos = path.find_last_of("/\\");
            return (pos == std::string_view::npos) ? path : path.substr(pos + 1);
        }

        [[nodiscard]] inline std::string sanitize_system_name(const std::string_view sys)
        {
            std::string out;
            out.reserve(sys.size());

            for (const char c : sys)
            {
                const auto uc = static_cast<unsigned char>(c);
                const bool ok = std::isalnum(uc) != 0 || c == '.' || c == '_' || c == '-';
                out.push_back(ok ? c : '_');
            }

            if (out.empty())
                out = "system";

            return out;
        }

        [[nodiscard]] constexpr bool should_log(const LogLevel msg, const LogLevel cur) noexcept
        {
            if (cur == LogLevel::OFF)
                return false;

            return static_cast<int>(msg) >= static_cast<int>(cur);
        }

        [[nodiscard]] constexpr epoch::core::log::level map_level(const LogLevel lvl) noexcept
        {
            switch (lvl)
            {
            case LogLevel::INFO:  return epoch::core::log::level::info;
            case LogLevel::WARN:  return epoch::core::log::level::warn;
            case LogLevel::Error: return epoch::core::log::level::error;
            case LogLevel::OFF:   return epoch::core::log::level::off;
            }
            return epoch::core::log::level::info;
        }
    }

    class SystemLogger final
    {
    public:
        explicit SystemLogger(std::string system_name)
            : m_system(std::move(system_name))
        {
        }

        ~SystemLogger()
        {
            close();
        }

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
                    cfg.root_dir.string(),
                    ec.message()));
            }

            const auto safe_name = detail::sanitize_system_name(m_system);
            m_path = cfg.root_dir / (safe_name + ".log");

            m_file.open(m_path, std::ios::out | std::ios::app);
            if (!m_file.is_open())
            {
                throw std::runtime_error(std::format(
                    "Logger: could not open log file '{}'",
                    m_path.string()));
            }
        }

        void log(
            const LogLevel lvl,
            const std::string_view msg,
            const std::source_location loc = std::source_location::current())
        {
            const auto cur = m_level.load(std::memory_order_relaxed);
            if (!detail::should_log(lvl, cur))
                return;

            const bool include_src = m_include_source.load(std::memory_order_relaxed);

            const std::string message{ msg };
            std::string console_message = message;

            std::scoped_lock lock(m_mutex);

            const auto file_name = std::string(detail::filename_only(loc.file_name()));
            const auto line = loc.line();
            const auto repeat_key = std::format(
                "{}|{}|{}|{}|{}",
                static_cast<int>(lvl),
                include_src ? 1 : 0,
                file_name,
                line,
                message);

            if (m_repeat.active && m_repeat.key == repeat_key)
            {
                ++m_repeat.suppressed;
                return;
            }

            flush_repeat_locked();

            m_repeat.active = true;
            m_repeat.key = repeat_key;
            m_repeat.level = lvl;
            m_repeat.include_source = include_src;
            m_repeat.file_name = file_name;
            m_repeat.line = line;
            m_repeat.message = message;
            m_repeat.suppressed = 0;

            write_line_locked(lvl, include_src, file_name, line, message);
        }

        template <class... Args>
        void logf(
            const LogLevel lvl,
            const std::source_location loc,
            std::format_string<Args...> fmt,
            Args&&... args)
        {
            log(lvl, std::format(fmt, std::forward<Args>(args)...), loc);
        }

        [[nodiscard]] std::string_view system_name() const noexcept
        {
            return m_system;
        }

        [[nodiscard]] std::filesystem::path file_path() const
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
        struct RepeatState
        {
            bool active = false;
            LogLevel level = LogLevel::INFO;
            bool include_source = true;
            std::string key{};
            std::string file_name{};
            std::uint_least32_t line = 0;
            std::string message{};
            std::size_t suppressed = 0;
        };

        void write_line_locked(
            const LogLevel lvl,
            const bool include_src,
            const std::string_view file_name,
            const std::uint_least32_t line,
            const std::string_view message)
        {
            std::string console_message{ message };
            std::string file_line;

            if (include_src)
            {
                file_line = std::format(
                    "{} [{}] [{}] ({}:{}) - {}",
                    epoch::core::time::system_time_string(),
                    detail::level_text(lvl),
                    m_system,
                    file_name,
                    line,
                    message);

                console_message = std::format(
                    "({}:{}) - {}",
                    file_name,
                    line,
                    message);
            }
            else
            {
                file_line = std::format(
                    "{} [{}] [{}] - {}",
                    epoch::core::time::system_time_string(),
                    detail::level_text(lvl),
                    m_system,
                    message);
            }

            if (m_console_enabled.load(std::memory_order_relaxed))
            {
                epoch::core::log::core_log_write(
                    static_cast<std::uint32_t>(detail::map_level(lvl)),
                    m_system.c_str(),
                    console_message.c_str());
            }

            if (m_file_enabled.load(std::memory_order_relaxed) && m_file.is_open())
            {
                m_file << file_line << '\n';
                if (m_flush_each.load(std::memory_order_relaxed))
                    m_file.flush();
            }
        }

        void flush_repeat_locked()
        {
            if (!m_repeat.active || m_repeat.suppressed == 0)
                return;

            write_line_locked(
                m_repeat.level,
                m_repeat.include_source,
                m_repeat.file_name,
                m_repeat.line,
                std::format(
                    "{} (repeated {} more times)",
                    m_repeat.message,
                    m_repeat.suppressed));

            m_repeat.suppressed = 0;
        }

        void close_locked()
        {
            flush_repeat_locked();
            if (m_file.is_open())
                m_file.close();
            m_path.clear();
            m_repeat = {};
        }

        std::string m_system;

        mutable std::mutex m_mutex{};
        std::ofstream m_file{};
        std::filesystem::path m_path{};

        std::atomic<LogLevel> m_level{ LogLevel::INFO };
        std::atomic<bool> m_flush_each{ true };
        std::atomic<bool> m_include_source{ true };
        std::atomic<bool> m_console_enabled{ false };
        std::atomic<bool> m_file_enabled{ true };
        RepeatState m_repeat{};
    };

    class LoggerHub final
    {
    public:
        void init(LogConfig cfg)
        {
            std::scoped_lock lock(m_mutex);
            m_cfg = std::move(cfg);
            apply_runtime_sink_locked();

            for (auto& [_, ptr] : m_systems)
                ptr->configure(m_cfg);
        }

        SystemLogger& system(const std::string_view name)
        {
            std::scoped_lock lock(m_mutex);
            apply_runtime_sink_locked();

            const std::string key{ name };
            if (const auto it = m_systems.find(key); it != m_systems.end())
                return *it->second;

            auto ptr = std::make_unique<SystemLogger>(key);
            ptr->configure(m_cfg);

            auto [it, _] = m_systems.emplace(key, std::move(ptr));
            return *it->second;
        }

        [[nodiscard]] LogConfig config() const
        {
            std::scoped_lock lock(m_mutex);
            return m_cfg;
        }

    private:
        void apply_runtime_sink_locked() const
        {
            epoch::core::log::enable_console(m_cfg.console_enabled);
            epoch::core::log::set_level(detail::map_level(m_cfg.level));
        }

        mutable std::mutex m_mutex{};
        LogConfig m_cfg{};
        std::unordered_map<std::string, std::unique_ptr<SystemLogger>> m_systems{};
    };

    inline LoggerHub& hub()
    {
        static LoggerHub h{};
        return h;
    }

    inline void init(LogConfig cfg)
    {
        hub().init(std::move(cfg));
    }

    [[nodiscard]] inline LogConfig config()
    {
        return hub().config();
    }

    inline SystemLogger& get(const std::string_view system_name)
    {
        return hub().system(system_name);
    }

    inline void info(
        const std::string_view sys,
        const std::string_view msg,
        const std::source_location loc = std::source_location::current())
    {
        hub().system(sys).log(LogLevel::INFO, msg, loc);
    }

    inline void warn(
        const std::string_view sys,
        const std::string_view msg,
        const std::source_location loc = std::source_location::current())
    {
        hub().system(sys).log(LogLevel::WARN, msg, loc);
    }

    inline void error(
        const std::string_view sys,
        const std::string_view msg,
        const std::source_location loc = std::source_location::current())
    {
        hub().system(sys).log(LogLevel::Error, msg, loc);
    }

    template <class... Args>
    inline void infof_loc(
        const std::string_view sys,
        const std::source_location loc,
        std::format_string<Args...> fmt,
        Args&&... args)
    {
        hub().system(sys).logf(LogLevel::INFO, loc, fmt, std::forward<Args>(args)...);
    }

    template <class... Args>
    inline void warnf_loc(
        const std::string_view sys,
        const std::source_location loc,
        std::format_string<Args...> fmt,
        Args&&... args)
    {
        hub().system(sys).logf(LogLevel::WARN, loc, fmt, std::forward<Args>(args)...);
    }

    template <class... Args>
    inline void errorf_loc(
        const std::string_view sys,
        const std::source_location loc,
        std::format_string<Args...> fmt,
        Args&&... args)
    {
        hub().system(sys).logf(LogLevel::Error, loc, fmt, std::forward<Args>(args)...);
    }

    class Logger final
    {
    public:
        Logger(const std::string& system_name_or_file, const LogLevel level = LogLevel::INFO)
            : m_system(system_name_or_file)
        {
            (void)level;
        }

        inline static Logger& GetInstance(const std::string& system_name_or_file)
        {
            static Logger instance(system_name_or_file);
            return instance;
        }

        void log(
            const std::string_view message,
            const LogLevel level = LogLevel::INFO,
            const std::source_location loc = std::source_location::current())
        {
            hub().system(m_system).log(level, message, loc);
        }

        void log(
            const std::string& message,
            const LogLevel level = LogLevel::INFO,
            const std::source_location loc = std::source_location::current())
        {
            log(std::string_view{ message }, level, loc);
        }

        [[nodiscard]] std::string getLogFileName() const
        {
            return hub().system(m_system).file_path().string();
        }

    private:
        std::string m_system;
    };
}
