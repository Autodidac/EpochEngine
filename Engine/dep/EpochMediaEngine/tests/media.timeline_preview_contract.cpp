// SPDX-License-Identifier: MPL-2.0

import media.timeline_preview;

#if defined(EPOCH_MEDIA_TIMELINE_PREVIEW_CONTRACT_MAIN)
int main()
{
    return epochengine::media::timeline_preview::run_contract().passed()
        ? 0
        : 1;
}
#endif
