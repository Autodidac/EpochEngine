// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

#include <string>

#include "ai.backend_model_model_provider.hpp"

namespace epoch_ai::agent {

// Provider text is untrusted: never forward reasoning, truncated completions,
// tool requests, prose answers, or raw source code as preparatory instructions.
struct HandoffValidation final {
    std::string notes;
    std::string rejection_reason;
    [[nodiscard]] explicit operator bool() const noexcept { return !notes.empty(); }
};

[[nodiscard]] HandoffValidation validate_handoff(const model::ModelResponse& response);

} // namespace epoch_ai::agent
