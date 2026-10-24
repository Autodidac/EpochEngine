// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include "updater.distribution.hpp"

using namespace epochengine::updater::distribution;

static_assert(EPOCH_UPDATE_DISTRIBUTION_SITE == !public_edition);
static_assert(marker_matches(source_marker));
static_assert(!marker_matches(""));
static_assert(!marker_matches("#define EPOCH_SOURCE_EDITION_PUBLIC 2\n"));
static_assert(!marker_matches("// #define EPOCH_SOURCE_EDITION_PUBLIC 0\n"));
static_assert(!marker_matches("#define EPOCH_SOURCE_EDITION_PUBLIC 0\n#define EPOCH_SOURCE_EDITION_PUBLIC 1\n"));
static_assert(!release_download_allowed("https://example.com/runtime.zip"));
static_assert(!release_download_allowed("https://github.com/Other/Repo/releases/download/v1/runtime.zip"));
static_assert(!release_download_allowed("https://epoch.adamrushford.chatgpt.site.evil.test/downloads/releases/runtime.zip"));
static_assert(!release_download_allowed("https://epoch.adamrushford.chatgpt.site/downloads/releases/../private/runtime.zip"));
static_assert(!release_download_allowed("https://github.com/Autodidac/EpochEngine/releases/download/%2e%2e/runtime.zip"));

int main()
{
    const auto public_url = "https://github.com/Autodidac/EpochEngine/releases/download/v0.90.183/runtime.zip";
    const auto development_url = "https://epoch.adamrushford.chatgpt.site/downloads/releases/runtime.zip";
    if (release_download_allowed(public_url) != public_edition
        || release_download_allowed(development_url) == public_edition) return 1;
    if (marker_matches("#define EPOCH_SOURCE_EDITION_PUBLIC 1\n") != public_edition
        || marker_matches("#define EPOCH_SOURCE_EDITION_PUBLIC 0\n") == public_edition) return 2;
    return 0;
}
