// SPDX-License-Identifier: MPL-2.0

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

import media.timeline_preview;

#if defined(EPOCH_MEDIA_TIMELINE_PREVIEW_CONTRACT_MAIN)
int main()
{
    namespace media = epochengine::media::timeline_preview;
    if (!media::run_contract().passed())
        return 1;

    media::PreviewState preview;
    if (media::current_atlas_frame(preview, {4, 2}))
        return 2;
    auto source = media::contract_source();
    source.frame_count = 7;
    source.frame_rate = {2, 1};
    source.duration_seconds = 3.5;
    if (!media::admit(preview, source, {1, 1}) || !media::seek(preview, 2.5))
        return 3;
    const auto frame = media::current_atlas_frame(preview, {4, 2});
    if (!frame || frame->index != 5 || frame->x != 1920 || frame->y != 1080
        || frame->width != 1920 || frame->height != 1080
        || media::current_atlas_frame(preview, {4, 1})
        || media::current_atlas_frame(preview, {0, 2}))
        return 4;
    if (!media::seek(preview, 3.5))
        return 5;
    const auto final_frame = media::current_atlas_frame(preview, {4, 2});
    if (!final_frame || final_frame->index != 6 || final_frame->x != 3840)
        return 6;

    source = media::contract_source();
    if (!media::admit(preview, source, {1, 2}) || !media::set_loop(preview, true)
        || !media::play(preview) || !media::seek(preview, 0.25)
        || !media::advance(preview, 1.0e20) || preview.playhead_seconds != 0.25)
        return 7;

    source.frame_count = (std::numeric_limits<std::uint64_t>::max)();
    source.frame_rate = {1, 1};
    source.duration_seconds = static_cast<double>(source.frame_count);
    if (!media::admit(preview, source, {1, 3})
        || !media::seek(preview, source.duration_seconds)
        || media::current_frame_index(preview) != source.frame_count - 1)
        return 8;

    source = media::contract_source();
    source.frame_rate = {30'000, 1'001};
    source.duration_seconds = static_cast<double>(source.frame_count) * 1'001 / 30'000;
    if (!media::admit(preview, source, {1, 4}))
        return 9;
    for (std::uint64_t index = 1; index < source.frame_count; ++index)
    {
        const double boundary = static_cast<double>(index) * 1'001 / 30'000;
        if (!media::seek(preview, std::nextafter(boundary, 0.0))
            || media::current_frame_index(preview) != index - 1u
            || !media::seek(preview, boundary)
            || media::current_frame_index(preview) != index)
            return 9;
    }
    return 0;
}
#endif
