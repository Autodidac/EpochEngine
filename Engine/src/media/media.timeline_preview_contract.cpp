// SPDX-License-Identifier: LicenseRef-MIT-NoSell

import media.timeline_preview;

#if defined(EPOCH_MEDIA_TIMELINE_PREVIEW_CONTRACT_MAIN)
int main()
{
    return epochengine::media::timeline_preview::run_contract().passed()
        ? 0
        : 1;
}
#endif
