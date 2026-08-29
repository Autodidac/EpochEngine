// SPDX-License-Identifier: LicenseRef-MIT-NoSell

import scene.surface_alignment;

#if defined(EPOCH_SCENE_SURFACE_ALIGNMENT_CONTRACT_MAIN)
int main()
{
    return epochengine::scene::surface_alignment::run_contract().passed()
        ? 0
        : 1;
}
#endif
