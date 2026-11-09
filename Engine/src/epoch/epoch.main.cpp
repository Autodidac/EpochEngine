// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include "epoch.main.hpp"

// Keep the default entry in its own archive member. A generated application
// supplies its own main while sharing the identical compiled runtime library.
#if !defined(EPOCH_MAIN_IN_MAIN_CPP)
int main(int argc, char** argv)
{
    return epochengine::core::run_application_main(argc, argv);
}
#endif
