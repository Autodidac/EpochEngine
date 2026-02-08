module;

export module aengine.telemetry;

import <atomic>;
import <cstdint>;
import <string_view>;

import aengine.context.type;

export namespace almondnamespace::telemetry
{
    struct RendererTelemetryTags
    {
        core::ContextType backend = core::ContextType::None;
        std::uintptr_t window_id = 0;
        std::string_view detail{};
    };

    struct RendererTelemetrySink
    {
        virtual ~RendererTelemetrySink() = default;
        virtual void emit_counter(std::string_view name,
            std::int64_t value,
            const RendererTelemetryTags& tags) = 0;
        virtual void emit_gauge(std::string_view name,
            std::int64_t value,
            const RendererTelemetryTags& tags) = 0;
        virtual void emit_histogram_ms(std::string_view name,
            double value_ms,
            const RendererTelemetryTags& tags) = 0;
    };

    namespace detail
    {
        inline std::atomic<RendererTelemetrySink*> g_renderer_sink{ nullptr };
    }

    inline void set_renderer_telemetry_sink(RendererTelemetrySink* sink) noexcept
    {
        detail::g_renderer_sink.store(sink, std::memory_order_release);
    }

    inline RendererTelemetrySink* get_renderer_telemetry_sink() noexcept
    {
        return detail::g_renderer_sink.load(std::memory_order_acquire);
    }

    inline void emit_counter(std::string_view name,
        std::int64_t value,
        const RendererTelemetryTags& tags) noexcept
    {
        if (auto* sink = get_renderer_telemetry_sink())
            sink->emit_counter(name, value, tags);
    }

    inline void emit_gauge(std::string_view name,
        std::int64_t value,
        const RendererTelemetryTags& tags) noexcept
    {
        if (auto* sink = get_renderer_telemetry_sink())
            sink->emit_gauge(name, value, tags);
    }

    inline void emit_histogram_ms(std::string_view name,
        double value_ms,
        const RendererTelemetryTags& tags) noexcept
    {
        if (auto* sink = get_renderer_telemetry_sink())
            sink->emit_histogram_ms(name, value_ms, tags);
    }
}
