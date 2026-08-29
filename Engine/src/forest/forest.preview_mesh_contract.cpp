// SPDX-License-Identifier: LicenseRef-MIT-NoSell

import forest.preview_mesh;

#if defined(EPOCH_FOREST_PREVIEW_MESH_CONTRACT_MAIN)
int main()
{
    return epochengine::forest::preview_mesh::run_contract().passed() ? 0 : 1;
}
#endif
