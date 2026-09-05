// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <cstdint>
#include <string>

import editor.candidate_artifact;
import platform.child_process;

namespace
{
    namespace artifact = epochengine::editor::candidate_artifact;
    using artifact::Lane;
    using artifact::Binding;
    using artifact::Ledger;
    using epochengine::platform::child_process::ExecutableIdentity;
#if defined(_WIN32)
    const std::string root = "C:/epoch-contract/session-one";
    const std::string sibling = "C:/epoch-contract/session-two";
#else
    const std::string root = "/epoch-contract/session-one";
    const std::string sibling = "/epoch-contract/session-two";
#endif

    Binding build(Ledger& ledger, Lane lane, std::string workspace = root,
        std::uint32_t generation = 17)
    {
        const auto ticket = ledger.begin_build(lane);
        return {workspace, generation, lane,
            workspace + "/" + std::string{artifact::relative_executable(lane)},
            ExecutableIdentity{4096u, std::string(64u, 'a')}, ticket};
    }

    bool accept_lane(Ledger& ledger, Lane lane)
    {
        const auto binding = build(ledger, lane);
        if (!ledger.accept_build(binding)) return false;
        const auto test = ledger.for_test(root, 17, lane);
        return test && ledger.accept_test(*test, test->identity, test->identity, true);
    }

    bool complete(Ledger& ledger)
    {
        if (!ledger.reset(root, 17)
            || !accept_lane(ledger, Lane::DebugEditor)
            || !accept_lane(ledger, Lane::ReleaseEditor)
            || !accept_lane(ledger, Lane::HeadlessCi)) return false;
        if (ledger.for_preview(root, 17)) return false;
        const auto full = ledger.for_test(root, 17, Lane::FullValidation);
        return full && ledger.accept_test(*full, full->identity, full->identity, true)
            && ledger.for_preview(root, 17).has_value();
    }

    int contract()
    {
        Ledger ledger{};
        if (ledger.begin_build(Lane::ReleaseEditor) != 0
            || ledger.for_preview(root, 17)
            || ledger.reset("relative/path", 17)
            || ledger.reset(root, 0)
            || ledger.reset(root + "/../escape", 17)
            || ledger.reset(root + "/", 17)
            || ledger.reset(std::string{"/"}, 17)) return 1;
        if (!ledger.reset(root, 17)) return 2;
        auto debug = build(ledger, Lane::DebugEditor);
        auto forged = debug;
        forged.workspace_root = sibling;
        forged.executable_path = sibling + "/x64/Debug/EpochEditor.exe";
        if (ledger.accept_build(forged)) return 3;
        forged = debug;
        ++forged.generation;
        if (ledger.accept_build(forged)) return 4;
        forged = debug;
        forged.executable_path = root + "/../outside/EpochEditor.exe";
        if (ledger.accept_build(forged)) return 5;
        forged = debug;
        ++forged.build_ticket;
        if (ledger.accept_build(forged)) return 6;
        forged = debug;
        forged.identity.sha256 = std::string(64, '0');
        if (ledger.accept_build(forged)) return 7;
        if (!ledger.accept_build(debug) || ledger.accept_build(debug)
            || ledger.for_test(sibling, 17, Lane::DebugEditor)
            || ledger.for_test(root, 18, Lane::DebugEditor)
            || ledger.for_test(root, 17, Lane::FullValidation)
            || ledger.for_preview(root, 17)) return 8;

        auto changed = debug.identity;
        changed.sha256.front() = 'b';
        if (ledger.accept_test(debug, debug.identity, changed, true)
            || ledger.for_test(root, 17, Lane::DebugEditor)) return 9;
        debug = build(ledger, Lane::DebugEditor);
        if (!ledger.accept_build(debug)
            || ledger.accept_test(debug, changed, debug.identity, true)) return 10;
        debug = build(ledger, Lane::DebugEditor);
        if (!ledger.accept_build(debug)
            || ledger.accept_test(debug, debug.identity, debug.identity, false)) return 11;

        if (!complete(ledger)) return 12;
        const auto accepted = *ledger.for_preview(root, 17);
        if (accepted.lane != Lane::ReleaseEditor
            || accepted.executable_path != root + "/x64/Release/EpochEditor.exe"
            || ledger.for_preview(sibling, 17) || ledger.for_preview(root, 18)) return 13;
        auto full = *ledger.for_test(root, 17, Lane::FullValidation);
        changed = full.identity;
        --changed.size_bytes;
        if (ledger.accept_test(full, full.identity, changed, true)
            || ledger.for_preview(root, 17)) return 14;

        if (!complete(ledger)) return 15;
        const auto old_release = *ledger.for_preview(root, 17);
        ledger.invalidate();
        if (ledger.for_preview(root, 17) || ledger.accept_build(old_release)
            || ledger.accept_test(old_release, old_release.identity, old_release.identity, true)) return 16;
        if (!ledger.reset(root, 17)) return 17;
        auto new_release = build(ledger, Lane::ReleaseEditor);
        if (new_release.build_ticket <= old_release.build_ticket
            || ledger.accept_build(old_release) || !ledger.accept_build(new_release)
            || ledger.accept_test(old_release, old_release.identity, old_release.identity, true)) return 18;
        if (!ledger.for_test(root, 17, Lane::ReleaseEditor)
            || ledger.for_test(root, 17, Lane::FullValidation)) return 19;

        if (!complete(ledger)) return 20;
        (void)ledger.begin_build(Lane::ReleaseEditor);
        if (!ledger.for_test(root, 17, Lane::DebugEditor)
            || ledger.for_test(root, 17, Lane::HeadlessCi)
            || ledger.for_preview(root, 17)) return 21;
        if (!complete(ledger)) return 22;
        (void)ledger.begin_build(Lane::HeadlessCi);
        if (!ledger.for_test(root, 17, Lane::ReleaseEditor)
            || ledger.for_preview(root, 17)) return 23;
        if (!complete(ledger)) return 24;
        (void)ledger.begin_build(Lane::DebugEditor);
        if (ledger.for_test(root, 17, Lane::ReleaseEditor)
            || ledger.for_test(root, 17, Lane::HeadlessCi)
            || ledger.for_preview(root, 17)) return 25;

        if (!complete(ledger)) return 26;
        const auto before_invalid_lane = ledger.for_preview(root, 17);
        if (ledger.begin_build(static_cast<Lane>(255)) != 0
            || ledger.for_test(root, 17, static_cast<Lane>(255))
            || !ledger.for_preview(root, 17) || !before_invalid_lane) return 27;
        const auto evidence = artifact::canonical_evidence(*before_invalid_lane);
        if (evidence.find("epoch.candidate-executable/v1") == std::string::npos
            || evidence.find("\"size_bytes\":4096") == std::string::npos
            || evidence.find(before_invalid_lane->identity.sha256) == std::string::npos
            || evidence.find("\"lane\":\"release_editor\"") == std::string::npos) return 28;
        return 0;
    }
}

int main()
{
    try { return contract(); }
    catch (...) { return 99; }
}
