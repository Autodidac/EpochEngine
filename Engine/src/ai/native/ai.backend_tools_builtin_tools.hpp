// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once
#include "ai.backend_core_config.hpp"
#include "ai.backend_model_lm_studio_provider.hpp"
#include "ai.backend_addons_addon_registry.hpp"
#include "ai.backend_persistence_session_store.hpp"
#include "ai.backend_tools_tool_registry.hpp"
#include "ai.backend_workspace_checkpoint_store.hpp"
#include "ai.backend_workspace_workspace.hpp"

namespace epoch_ai::tools {
void register_builtin_tools(ToolRegistry&, workspace::Workspace&, workspace::CheckpointStore&, persistence::SessionStore&, model::LmStudioProvider&, addons::AddonRegistry&, const core::Config&);
}
