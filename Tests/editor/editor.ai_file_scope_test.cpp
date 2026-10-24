#include "editor.ai_file_scope.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 2) return 1;
    namespace fs = std::filesystem;
    using epochengine::editor_ai_file_scope::allows_text;
    const auto checkout = fs::weakly_canonical(fs::path{argv[1]});
    const auto fixture = checkout / "build/edition-split/file-scope" /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    struct Cleanup {
        fs::path root;
        ~Cleanup() { std::error_code error; fs::remove_all(root, error); }
    } cleanup{fixture};
    const auto owner = fixture / "owner";
    const auto clone = fixture / "clone";
    const auto source = owner / "Engine/src/test.cpp";
    const auto game = owner / "Projects/game";
    const auto write = [](const fs::path& path, const std::string& bytes = "fixture") {
        fs::create_directories(path.parent_path());
        std::ofstream out{path, std::ios::binary}; out << bytes;
        return static_cast<bool>(out);
    };
    if (!write(owner / "Engine.sln") || !write(source)
        || !write(game / "project.epoch.json") || !write(game / "scripts/game.cpp")
        || !write(clone / "Engine/modules/epoch.version.ixx")
        || !write(clone / "Engine/include/epoch.edition.hpp")
        || !write(clone / "Engine/src/test.cpp")) return 2;
    if (allows_text(source, owner) || allows_text(owner, owner)
        || allows_text(source.parent_path() / "../src/test.cpp", owner)
        || allows_text(clone / "Engine/src/test.cpp", owner)
        || allows_text(clone / "Engine/src/test.cpp")) return 3;
    if (!allows_text(game, owner) || !allows_text(game / "scripts/game.cpp", owner)
        || !allows_text(game / "scripts/new.cpp", owner)) return 4;
    // A manifest inside an implementation folder cannot grant source access.
    if (!write(owner / "Engine/src/project.epoch.json") || allows_text(source, owner)) return 5;
    // Explicit generic files outside the recognized checkout need no project.
    // A staged Public tree may itself be nested in a Development checkout.
    // This no-write path must sit outside every recognized ancestor engine.
    auto external = checkout.parent_path();
    for (auto cursor = checkout; !cursor.empty(); cursor = cursor.parent_path())
    {
        std::error_code boundaryError;
        if (epochengine::editor_ai_file_scope::engine_tree(cursor, boundaryError))
            external = cursor.parent_path();
        if (boundaryError) return 19;
        if (cursor == cursor.parent_path()) break;
    }
    if (!allows_text(external / "external-reference.txt", owner)) return 6;
    if (!write(game / "engine-copy/Engine.sln")
        || !write(game / "engine-copy/Engine/src/test.cpp")
        || allows_text(game / "engine-copy/Engine/src/test.cpp", owner)) return 7;
    std::error_code error;
    fs::create_directory_symlink(owner / "Engine/src", game / "linked-source", error);
    if (!error && allows_text(game / "linked-source/test.cpp", owner)) return 8;
    error.clear();
    const auto reference = game / "reference.txt";
    const auto linkedSource = game / "source-alias.txt";
    const auto linkedReference = game / "reference-alias.txt";
    if (!write(reference)) return 9;
    fs::create_hard_link(source, linkedSource, error);
    if (error) return 101;
    if (allows_text(linkedSource, owner)) return 10;
    fs::create_hard_link(reference, linkedReference, error);
    if (error) return 102;
    if (allows_text(reference, owner) || allows_text(linkedReference, owner)) return 11;
    // Engine copies selected as a project remain protected even with a root
    // manifest, while a separate normal project still owns its script files.
    if (!write(clone / "project.epoch.json") || allows_text(clone, owner)
        || !allows_text(game / "scripts/game.cpp", owner)) return 12;
#if defined(EPOCH_TEST_PUBLIC_GUI_PATHS)
    set_test_engine_root(owner);
    if (project_ai_scope_allowed(owner.string())
        || project_ai_scope_allowed((owner / "Engine/src").string())
        || project_ai_scope_allowed(clone.string())
        || project_ai_scope_allowed((game / "engine-copy").string())
        || !project_ai_scope_allowed(game.string())
        || !project_ai_scope_allowed({})) return 13;
    const auto denied = [&](const fs::path& path) {
        AiChat chat;
        return !append_ai_chat_attachment(chat, path)
            && chat.draftReferenceFiles.empty() && chat.draftReferenceImages.empty();
    };
    if (!denied(source) || !denied(source.parent_path() / "../src/test.cpp")
        || !denied(linkedSource) || !denied(linkedReference)
        || !denied(clone / "Engine/src/test.cpp")) return 14;
    if (fs::exists(game / "linked-source") && !denied(game / "linked-source/test.cpp")) return 15;
    AiChat ordinary;
    if (!append_ai_chat_attachment(ordinary, game / "scripts/game.cpp")
        || ordinary.draftReferenceFiles.size() != 1u
        || ordinary.draftReferenceFiles.front().content != "fixture") return 16;
    // A complete 1x1 PNG remains an ordinary reference attachment. Thumbnail
    // drawing is mocked by the CPU harness; no image-understanding claim.
    const unsigned char png[]{
        137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,4,0,0,0,181,28,12,2,
        0,0,0,11,73,68,65,84,120,218,99,100,248,15,0,1,5,1,1,39,24,227,102,0,0,0,0,73,69,78,68,174,66,96,130};
    const auto image = owner / "cache/reference.png";
    if (!write(image, {reinterpret_cast<const char*>(png), sizeof(png)})) return 17;
    AiChat visual;
    if (!append_ai_chat_attachment(visual, image)
        || visual.draftReferenceImages.size() != 1u
        || !visual.draftReferenceFiles.empty()
        || visual.draftReferenceImages.front().mime != "image/png") return 18;
#endif
    return 0;
}
