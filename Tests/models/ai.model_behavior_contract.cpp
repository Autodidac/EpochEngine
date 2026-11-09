// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include "../../Engine/src/ai/native/ai.backend_model_runtime_model.cpp"
#include "../../Engine/src/ai/native/ai.backend_model_profile_import.hpp"
#include "../../Engine/src/ai/ai.native_resident_selection.hpp"
#include <cassert>
#include <fstream>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <cstdlib>
#endif
#undef assert
#define assert(value) do { if (!(value)) return __LINE__; } while (false)

int main()
{
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#ifdef _DEBUG
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
#endif
    try {
    namespace behavior = epochengine::ai::model_behavior;
    using namespace epoch_ai::model;
    static_assert(behavior::classify_text("Publisher/MODEL-UnCeNsOrEd-Q4").labels == behavior::uncensored);
    static_assert(behavior::classify_text("MODEL-ab_lit_erated").labels == behavior::abliterated);
    static_assert(behavior::classify_text("MODEL-de-restricted").labels == behavior::derestricted);
    static_assert(!behavior::classify_text("unsloth/Qwen3.8-Flash-Next-UD-Q2_K_XL").flagged());
    static_assert(!behavior::classify_text("Qwen/Qwen3.8-27B-Instruct").flagged());
    static_assert(!behavior::classify_text("").flagged());
    static_assert(!epochengine::ai::native_runtime::detail::should_establish_foundation(false, false, true));
    static_assert(epochengine::ai::native_runtime::detail::should_establish_foundation(false, true, true));
    static_assert(epochengine::ai::native_runtime::detail::should_establish_foundation(false, false));
    static_assert(!epochengine::ai::native_runtime::detail::should_establish_foundation(true, false));

    RuntimeModelProfile ordinary;
    ordinary.id = "model-contract";
    ordinary.repo = "publisher/model-contract";
    const auto original = to_json(ordinary);
    assert(!original.contains("behavior_labels") && !original.contains("tags"));
    assert(!behavior_for(ordinary).flagged());
    assert(to_json(runtime_model_from_json(original)) == original);

    auto modified = original;
    modified["behavior_labels"] = nlohmann::json::array({"abliterated"});
    modified["tags"] = nlohmann::json::array({"de-restricted", "custom"});
    const auto decoded = runtime_model_from_json(modified);
    assert(behavior_for(decoded).labels == (behavior::abliterated | behavior::derestricted));
    assert(behavior_for(decoded) == behavior_for(modified));
    assert(to_json(decoded) == modified);
    assert(behavior::choice_label("My model", behavior_for(decoded)) == "My model [Abliterated / Derestricted]");
    assert(behavior::choice_label("Other model", {}) == "Other model");

    auto boolean = original;
    boolean["uncensored"] = true;
    assert(behavior_for(boolean).labels == behavior::uncensored);
    assert(behavior_for(runtime_model_from_json(boolean)).labels == behavior::uncensored);
    boolean["uncensored"] = "false";
    boolean["id"] = "my-abliterated-model";
    boolean["behavior_labels"] = false;
    boolean["tags"] = nlohmann::json::object();
    assert(behavior_for(boolean).labels == behavior::abliterated);
    assert(behavior_for(runtime_model_from_json(boolean)).labels == behavior::abliterated);
    assert(!behavior_for(nlohmann::json::array()).flagged());

    auto imported = decoded;
    imported.source = ModelSourceKind::local_gguf;
    imported.local_path = std::filesystem::current_path() / "existing-model.gguf";
    {std::ofstream file{imported.local_path}; file << "Synthetic metadata fixture; not model weights.";}
    const auto manifest = std::filesystem::current_path() / "manifest.json";
    {std::ofstream file{manifest}; file << to_json(imported).dump();}
    const auto admitted = read_profile_import(manifest, {});
    if (!admitted) {std::ofstream file{"import-error.txt"}; file << admitted.error();}
    assert(admitted && admitted->registrations.size() == 1u);
    assert(behavior_for(admitted->registrations.front()) == behavior_for(imported));
    auto unrelated = to_json(imported);
    unrelated["tool_approval"] = "allow-everything";
    {std::ofstream file{manifest}; file << unrelated.dump();}
    assert(!read_profile_import(manifest, {}));
    } catch (const std::exception& error) {
        std::ofstream file{"contract-error.txt"}; file << error.what(); return 90;
    }
}
