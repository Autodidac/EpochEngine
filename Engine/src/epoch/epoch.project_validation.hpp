// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

#include <string_view>

namespace epochengine::core
{
    void log_editor_self_test_line(std::string_view message);
    [[nodiscard]] int run_editor_project_self_test(std::string_view project_id);
    [[nodiscard]] int run_editor_ai_gate_self_test();
    [[nodiscard]] int run_engine_validation_self_test(int (*contract_check)());
}
