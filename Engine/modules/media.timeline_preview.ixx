// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

export module media.timeline_preview;

export namespace epochengine::media::timeline_preview
{
    struct Rational final
    {
        std::uint32_t numerator{};
        std::uint32_t denominator{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return numerator > 0u && denominator > 0u;
        }

        [[nodiscard]] constexpr double value() const noexcept
        {
            return valid()
                ? static_cast<double>(numerator) / static_cast<double>(denominator)
                : 0.0;
        }

        friend constexpr bool operator==(const Rational&, const Rational&) noexcept = default;
    };

    enum class Rotation : std::uint16_t
    {
        degrees_0 = 0u,
        degrees_90 = 90u,
        degrees_180 = 180u,
        degrees_270 = 270u
    };

    [[nodiscard]] inline constexpr bool valid_rotation(Rotation rotation) noexcept
    {
        switch (rotation)
        {
        case Rotation::degrees_0:
        case Rotation::degrees_90:
        case Rotation::degrees_180:
        case Rotation::degrees_270:
            return true;
        default:
            return false;
        }
    }

    struct RevisionStamp final
    {
        std::uint64_t generation{};
        std::uint64_t revision{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return generation > 0u && revision > 0u;
        }

        friend constexpr bool operator==(
            const RevisionStamp&,
            const RevisionStamp&) noexcept = default;
    };

    [[nodiscard]] inline constexpr bool newer_than(
        RevisionStamp candidate,
        RevisionStamp current) noexcept
    {
        return candidate.valid()
            && current.valid()
            && (candidate.generation > current.generation
                || (candidate.generation == current.generation
                    && candidate.revision > current.revision));
    }

    struct SourceDescriptor final
    {
        std::string logical_path{};
        std::string content_sha256{};
        std::string provider_revision{};
        std::uint32_t source_width{};
        std::uint32_t source_height{};
        Rational pixel_aspect{1u, 1u};
        Rotation rotation = Rotation::degrees_0;
        std::uint64_t frame_count{};
        Rational frame_rate{};
        double duration_seconds{};
    };

    enum class AdmissionError : std::uint8_t
    {
        none = 0u,
        invalid_revision,
        stale_revision,
        invalid_logical_path,
        invalid_content_hash,
        invalid_provider_revision,
        invalid_dimensions,
        invalid_pixel_aspect,
        invalid_rotation,
        invalid_timing
    };

    struct AdmissionResult final
    {
        AdmissionError error = AdmissionError::none;

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return error == AdmissionError::none;
        }
    };

    [[nodiscard]] inline bool project_relative_logical_path(
        std::string_view path) noexcept
    {
        if (path.empty() || path.size() > 512u || path.front() == '/')
            return false;

        std::size_t segment_begin = 0u;
        for (std::size_t index = 0u; index <= path.size(); ++index)
        {
            if (index < path.size())
            {
                const unsigned char character =
                    static_cast<unsigned char>(path[index]);
                if (character < 0x20u
                    || path[index] == '\\'
                    || path[index] == ':'
                    || path[index] == '?'
                    || path[index] == '#')
                {
                    return false;
                }
                if (path[index] != '/')
                    continue;
            }

            const std::string_view segment = path.substr(
                segment_begin,
                index - segment_begin);
            if (segment.empty() || segment == "." || segment == "..")
                return false;
            segment_begin = index + 1u;
        }
        return true;
    }

    [[nodiscard]] inline bool lowercase_sha256(std::string_view value) noexcept
    {
        if (value.size() != 64u)
            return false;
        for (const char character : value)
        {
            if (!((character >= '0' && character <= '9')
                || (character >= 'a' && character <= 'f')))
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] inline bool valid_provider_revision(
        std::string_view value) noexcept
    {
        if (value.empty() || value.size() > 128u)
            return false;
        for (const char character : value)
        {
            const bool accepted =
                (character >= 'a' && character <= 'z')
                || (character >= 'A' && character <= 'Z')
                || (character >= '0' && character <= '9')
                || character == '.'
                || character == '_'
                || character == '+'
                || character == '-';
            if (!accepted)
                return false;
        }
        return true;
    }

    [[nodiscard]] inline AdmissionResult validate(
        const SourceDescriptor& source) noexcept
    {
        if (!project_relative_logical_path(source.logical_path))
            return {AdmissionError::invalid_logical_path};
        if (!lowercase_sha256(source.content_sha256))
            return {AdmissionError::invalid_content_hash};
        if (!valid_provider_revision(source.provider_revision))
            return {AdmissionError::invalid_provider_revision};
        if (source.source_width == 0u
            || source.source_height == 0u
            || source.source_width > 1'048'576u
            || source.source_height > 1'048'576u)
        {
            return {AdmissionError::invalid_dimensions};
        }
        if (!source.pixel_aspect.valid()
            || source.pixel_aspect.numerator > 1'000'000u
            || source.pixel_aspect.denominator > 1'000'000u)
        {
            return {AdmissionError::invalid_pixel_aspect};
        }
        if (!valid_rotation(source.rotation))
            return {AdmissionError::invalid_rotation};
        if (source.frame_count == 0u
            || !source.frame_rate.valid()
            || source.frame_rate.numerator > 1'000'000u
            || source.frame_rate.denominator > 1'000'000u
            || !std::isfinite(source.duration_seconds)
            || source.duration_seconds <= 0.0)
        {
            return {AdmissionError::invalid_timing};
        }

        const double frames_per_second = source.frame_rate.value();
        const double frame_seconds = 1.0 / frames_per_second;
        const double derived_duration =
            static_cast<double>(source.frame_count) / frames_per_second;
        if (!std::isfinite(derived_duration)
            || std::abs(derived_duration - source.duration_seconds)
                > frame_seconds + 1.0e-9)
        {
            return {AdmissionError::invalid_timing};
        }
        return {};
    }

    struct Rect final
    {
        double x{};
        double y{};
        double width{};
        double height{};

        [[nodiscard]] bool valid() const noexcept
        {
            return std::isfinite(x)
                && std::isfinite(y)
                && std::isfinite(width)
                && std::isfinite(height)
                && width > 0.0
                && height > 0.0;
        }
    };

    struct DisplaySize final
    {
        double width{};
        double height{};

        [[nodiscard]] bool valid() const noexcept
        {
            return std::isfinite(width)
                && std::isfinite(height)
                && width > 0.0
                && height > 0.0;
        }
    };

    [[nodiscard]] inline std::optional<DisplaySize> oriented_display_size(
        const SourceDescriptor& source) noexcept
    {
        if (!validate(source))
            return std::nullopt;

        const double pixel_width = source.pixel_aspect.value();
        const double unrotated_width =
            static_cast<double>(source.source_width) * pixel_width;
        const double unrotated_height =
            static_cast<double>(source.source_height);
        const bool swaps_axes = source.rotation == Rotation::degrees_90
            || source.rotation == Rotation::degrees_270;
        DisplaySize result{
            swaps_axes ? unrotated_height : unrotated_width,
            swaps_axes ? unrotated_width : unrotated_height};
        return result.valid()
            ? std::optional<DisplaySize>{result}
            : std::nullopt;
    }

    [[nodiscard]] inline std::optional<Rect> contain_fit(
        const SourceDescriptor& source,
        Rect viewport) noexcept
    {
        const auto display = oriented_display_size(source);
        if (!display || !viewport.valid())
            return std::nullopt;

        const double scale = (std::min)(
            viewport.width / display->width,
            viewport.height / display->height);
        Rect fitted{
            viewport.x + (viewport.width - display->width * scale) * 0.5,
            viewport.y + (viewport.height - display->height * scale) * 0.5,
            display->width * scale,
            display->height * scale};
        return fitted.valid()
            ? std::optional<Rect>{fitted}
            : std::nullopt;
    }

    enum class PresentationAvailability : std::uint8_t
    {
        no_source = 0u,
        metadata_only
    };

    enum class TransportMode : std::uint8_t
    {
        disabled = 0u,
        stopped,
        paused,
        playing
    };

    struct PreviewState final
    {
        std::optional<SourceDescriptor> source{};
        std::optional<RevisionStamp> latest_revision{};
        double playhead_seconds{};
        TransportMode transport = TransportMode::disabled;
        bool loop{};
    };

    [[nodiscard]] inline PresentationAvailability presentation_availability(
        const PreviewState& state) noexcept
    {
        // This contract owns admitted metadata, fit, and timing only. A decoder
        // must provide a separately admitted frame before any pixels are shown.
        return state.source
            ? PresentationAvailability::metadata_only
            : PresentationAvailability::no_source;
    }

    [[nodiscard]] inline bool transport_enabled(
        const PreviewState& state) noexcept
    {
        return state.source.has_value()
            && state.transport != TransportMode::disabled;
    }

    [[nodiscard]] inline AdmissionResult admit(
        PreviewState& state,
        SourceDescriptor source,
        RevisionStamp revision) noexcept
    {
        if (!revision.valid())
            return {AdmissionError::invalid_revision};
        if (state.latest_revision
            && !newer_than(revision, *state.latest_revision))
        {
            return {AdmissionError::stale_revision};
        }
        const AdmissionResult source_result = validate(source);
        if (!source_result)
            return source_result;

        state.source = std::move(source);
        state.latest_revision = revision;
        state.playhead_seconds = 0.0;
        state.transport = TransportMode::stopped;
        state.loop = false;
        return {};
    }

    [[nodiscard]] inline AdmissionResult clear(
        PreviewState& state,
        RevisionStamp revision) noexcept
    {
        if (!revision.valid())
            return {AdmissionError::invalid_revision};
        if (state.latest_revision
            && !newer_than(revision, *state.latest_revision))
        {
            return {AdmissionError::stale_revision};
        }
        state.source.reset();
        state.latest_revision = revision;
        state.playhead_seconds = 0.0;
        state.transport = TransportMode::disabled;
        state.loop = false;
        return {};
    }

    [[nodiscard]] inline bool set_loop(PreviewState& state, bool enabled) noexcept
    {
        if (!transport_enabled(state))
            return false;
        state.loop = enabled;
        return true;
    }

    [[nodiscard]] inline bool seek(
        PreviewState& state,
        double seconds) noexcept
    {
        if (!transport_enabled(state) || !std::isfinite(seconds))
            return false;
        state.playhead_seconds = (std::clamp)(
            seconds,
            0.0,
            state.source->duration_seconds);
        if (state.playhead_seconds >= state.source->duration_seconds
            && state.transport == TransportMode::playing
            && !state.loop)
        {
            state.transport = TransportMode::stopped;
        }
        return true;
    }

    [[nodiscard]] inline bool play(PreviewState& state) noexcept
    {
        if (!transport_enabled(state))
            return false;
        if (state.playhead_seconds >= state.source->duration_seconds)
            state.playhead_seconds = 0.0;
        state.transport = TransportMode::playing;
        return true;
    }

    [[nodiscard]] inline bool pause(PreviewState& state) noexcept
    {
        if (!transport_enabled(state)
            || state.transport != TransportMode::playing)
        {
            return false;
        }
        state.transport = TransportMode::paused;
        return true;
    }

    [[nodiscard]] inline bool stop(PreviewState& state) noexcept
    {
        if (!transport_enabled(state))
            return false;
        state.playhead_seconds = 0.0;
        state.transport = TransportMode::stopped;
        return true;
    }

    [[nodiscard]] inline bool advance(
        PreviewState& state,
        double elapsed_seconds) noexcept
    {
        if (!transport_enabled(state)
            || state.transport != TransportMode::playing
            || !std::isfinite(elapsed_seconds)
            || elapsed_seconds < 0.0)
        {
            return false;
        }

        const double duration = state.source->duration_seconds;
        const double advanced = state.playhead_seconds + elapsed_seconds;
        if (state.loop)
        {
            state.playhead_seconds = std::fmod(advanced, duration);
            if (state.playhead_seconds < 0.0)
                state.playhead_seconds += duration;
        }
        else if (advanced >= duration)
        {
            state.playhead_seconds = duration;
            state.transport = TransportMode::stopped;
        }
        else
        {
            state.playhead_seconds = advanced;
        }
        return true;
    }

    [[nodiscard]] inline std::optional<std::uint64_t> current_frame_index(
        const PreviewState& state) noexcept
    {
        if (!state.source)
            return std::nullopt;
        const double frame = state.playhead_seconds * state.source->frame_rate.value();
        const std::uint64_t index = frame > 0.0
            ? static_cast<std::uint64_t>(frame)
            : 0u;
        return (std::min)(index, state.source->frame_count - 1u);
    }

    struct ContractReport final
    {
        bool admitted_metadata{};
        bool invalid_source_rejected{};
        bool stale_revision_rejected{};
        bool no_source_disables_transport{};
        bool contain_16_by_9{};
        bool contain_rotated_90{};
        bool contain_pixel_aspect{};
        bool clamped_seek_and_end{};
        bool loop_wrap{};
        bool metadata_only_presentation{};

        [[nodiscard]] constexpr bool passed() const noexcept
        {
            return admitted_metadata
                && invalid_source_rejected
                && stale_revision_rejected
                && no_source_disables_transport
                && contain_16_by_9
                && contain_rotated_90
                && contain_pixel_aspect
                && clamped_seek_and_end
                && loop_wrap
                && metadata_only_presentation;
        }
    };

    [[nodiscard]] inline bool nearly_equal(
        double left,
        double right,
        double tolerance = 1.0e-7) noexcept
    {
        return std::abs(left - right) <= tolerance;
    }

    [[nodiscard]] inline SourceDescriptor contract_source()
    {
        return SourceDescriptor{
            .logical_path = "Assets/Video/intro.mp4",
            .content_sha256 =
                "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
            .provider_revision = "probe.v1+contract",
            .source_width = 1920u,
            .source_height = 1080u,
            .pixel_aspect = {1u, 1u},
            .rotation = Rotation::degrees_0,
            .frame_count = 300u,
            .frame_rate = {30u, 1u},
            .duration_seconds = 10.0};
    }

    [[nodiscard]] inline ContractReport run_contract()
    {
        ContractReport report{};
        PreviewState state{};
        report.no_source_disables_transport =
            !transport_enabled(state)
            && !play(state)
            && !seek(state, 1.0)
            && !advance(state, 1.0)
            && presentation_availability(state)
                == PresentationAvailability::no_source;

        SourceDescriptor source = contract_source();
        const AdmissionResult admitted = admit(state, source, {1u, 1u});
        report.admitted_metadata = admitted
            && state.source
            && state.source->logical_path == source.logical_path
            && state.source->content_sha256 == source.content_sha256
            && state.source->provider_revision == source.provider_revision
            && state.source->source_width == 1920u
            && state.source->source_height == 1080u
            && state.source->pixel_aspect == Rational{1u, 1u}
            && state.source->rotation == Rotation::degrees_0
            && state.source->frame_count == 300u
            && state.source->frame_rate == Rational{30u, 1u}
            && nearly_equal(state.source->duration_seconds, 10.0);

        SourceDescriptor invalid = source;
        invalid.logical_path = "../outside.mp4";
        SourceDescriptor invalid_hash = source;
        invalid_hash.content_sha256.front() = 'A';
        report.invalid_source_rejected =
            validate(invalid).error == AdmissionError::invalid_logical_path
            && validate(invalid_hash).error == AdmissionError::invalid_content_hash;

        SourceDescriptor stale_source = source;
        stale_source.logical_path = "Assets/Video/stale.mp4";
        const AdmissionResult repeated_revision = admit(
            state,
            stale_source,
            {1u, 1u});
        SourceDescriptor newer_source = source;
        newer_source.logical_path = "Assets/Video/newer.mp4";
        const AdmissionResult newer_generation = admit(
            state,
            newer_source,
            {2u, 1u});
        const AdmissionResult stale_generation = admit(
            state,
            stale_source,
            {1u, 99u});
        report.stale_revision_rejected =
            repeated_revision.error == AdmissionError::stale_revision
            && newer_generation
            && stale_generation.error == AdmissionError::stale_revision
            && state.source
            && state.source->logical_path == newer_source.logical_path
            && state.latest_revision == RevisionStamp{2u, 1u};

        const Rect viewport{20.0, 30.0, 1000.0, 1000.0};
        const auto landscape = contain_fit(source, viewport);
        report.contain_16_by_9 = landscape
            && nearly_equal(landscape->x, 20.0)
            && nearly_equal(landscape->y, 248.75)
            && nearly_equal(landscape->width, 1000.0)
            && nearly_equal(landscape->height, 562.5);

        SourceDescriptor rotated = source;
        rotated.rotation = Rotation::degrees_90;
        const auto portrait = contain_fit(rotated, viewport);
        report.contain_rotated_90 = portrait
            && nearly_equal(portrait->x, 238.75)
            && nearly_equal(portrait->y, 30.0)
            && nearly_equal(portrait->width, 562.5)
            && nearly_equal(portrait->height, 1000.0);

        SourceDescriptor anamorphic = source;
        anamorphic.source_width = 720u;
        anamorphic.source_height = 480u;
        anamorphic.pixel_aspect = {8u, 9u};
        const auto pixel_aspect_fit = contain_fit(
            anamorphic,
            {0.0, 0.0, 1600.0, 900.0});
        report.contain_pixel_aspect = pixel_aspect_fit
            && nearly_equal(pixel_aspect_fit->x, 200.0)
            && nearly_equal(pixel_aspect_fit->y, 0.0)
            && nearly_equal(pixel_aspect_fit->width, 1200.0)
            && nearly_equal(pixel_aspect_fit->height, 900.0);

        const bool seek_low = seek(state, -5.0)
            && nearly_equal(state.playhead_seconds, 0.0);
        const bool seek_high = seek(state, 50.0)
            && nearly_equal(state.playhead_seconds, 10.0);
        const bool started_at_end = play(state)
            && nearly_equal(state.playhead_seconds, 0.0);
        const bool sought_near_end = seek(state, 9.75);
        const bool reached_end = advance(state, 1.0)
            && nearly_equal(state.playhead_seconds, 10.0)
            && state.transport == TransportMode::stopped
            && current_frame_index(state) == 299u;
        report.clamped_seek_and_end = seek_low
            && seek_high
            && started_at_end
            && sought_near_end
            && reached_end;

        const bool loop_enabled = set_loop(state, true);
        const bool loop_started = play(state);
        const bool loop_positioned = seek(state, 9.75);
        report.loop_wrap = loop_enabled
            && loop_started
            && loop_positioned
            && advance(state, 0.5)
            && nearly_equal(state.playhead_seconds, 0.25)
            && state.transport == TransportMode::playing;
        report.metadata_only_presentation = presentation_availability(state)
            == PresentationAvailability::metadata_only;
        return report;
    }
}
