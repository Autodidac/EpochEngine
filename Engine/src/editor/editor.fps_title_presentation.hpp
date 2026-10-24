#pragma once

#include <cmath>

namespace epochengine::editor::fps_title_presentation
{
    // The renderer is not the bottleneck when render-only throughput greatly
    // exceeds measured application FPS. Report this as an engine-limited status,
    // not a GPU failure. Unknown measurements are neutral.
    enum class Tone { neutral, success, warning, error };

    [[nodiscard]] inline Tone classify(double applicationFps,
        double renderOnlyFps) noexcept
    {
        if (!std::isfinite(applicationFps) || applicationFps <= 0.0)
            return Tone::neutral;
        if (std::isfinite(renderOnlyFps) && renderOnlyFps > 0.0
            && renderOnlyFps >= applicationFps * 2.0)
            return Tone::warning;
        if (applicationFps >= 50.0) return Tone::success;
        if (applicationFps >= 25.0) return Tone::warning;
        return Tone::error;
    }
}
