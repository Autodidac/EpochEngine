// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/local_ai_llama_cpp_runtime/local_ai_llama_cpp_runtime.hpp"

#include <stdexcept>

using namespace epoch::extensions::local_ai_llama_cpp_runtime;

int main()
{
    const auto layout = make_runtime_layout("cache");
    const auto blocked = make_build_plan(layout, false);
    if (blocked.valid || blocked.startsServer)
        throw std::runtime_error("local AI package bypassed its approval or server gate");

    const auto unpinned = make_build_plan(layout, true);
    if (unpinned.valid || unpinned.immutableSourcePinned)
        throw std::runtime_error("local AI package accepted an unpinned upstream source");

    const auto approved = make_build_plan(
        layout,
        true,
        "0123456789abcdef0123456789abcdef01234567");
    if (!approved.valid || approved.startsServer || approved.configureArguments.empty())
        throw std::runtime_error("approved pinned local AI build plan is incomplete");
    if (approved.sourceLicense != "MIT" || !approved.immutableSourcePinned)
        throw std::runtime_error("llama.cpp license or immutable provenance is missing");
    return 0;
}