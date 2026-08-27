/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
#include <string_view>

import scripting.compiler;

namespace epochengine::compiler
{
    static_assert(compile_status_name(CompileStatus::succeeded) == "succeeded");
    static_assert(compile_status_name(CompileStatus::verification_failed)
        == "verification_failed");
    static_assert(compile_refusal_name(CompileRefusal::path_traversal)
        == "path_traversal");
    static_assert(compile_refusal_name(CompileRefusal::source_changed)
        == "source_changed");
    static_assert(compile_refusal_name(CompileRefusal::output_changed)
        == "output_changed");
}