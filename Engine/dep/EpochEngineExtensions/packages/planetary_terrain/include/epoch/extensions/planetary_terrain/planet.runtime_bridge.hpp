// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once
// Optional linked-project startup bridge. Call only after normal project Bind.
// This performs no model/network work and does not start the application loop.
extern "C" bool epoch_install_planetary_runtime(const char* project_root, const char* project_id) noexcept;
