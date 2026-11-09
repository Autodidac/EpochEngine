// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

namespace epochengine::core
{
    // The existing native/CLI startup dispatcher remains runtime-owned.
    int run_application_main(int argc, char** argv);
}
