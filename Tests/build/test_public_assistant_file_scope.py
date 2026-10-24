"""Production Public project/attachment admission with filesystem-only inputs."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]


class PublicAssistantFileScope(unittest.TestCase):
    def test_actual_project_and_attachment_paths_deny_engine_aliases(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC CPU lane')
        tools = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        sdk = Path('C:/Program Files (x86)/Windows Kits/10')
        kits = sorted((sdk/'Include').glob('*'))
        if not tools or not kits:
            self.skipTest('MSVC/SDK unavailable')
        project = definition((ROOT/'Engine/src/editor/application/editor.application_assistant_authoring.inl').read_text(encoding='utf-8'),
                             '        [[nodiscard]] bool project_ai_scope_allowed(')
        attachment = definition((ROOT/'Engine/src/editor/application/editor.application_models_attachments.inl').read_text(encoding='utf-8'),
                                '        [[nodiscard]] bool append_ai_chat_attachment(')
        prefix = r'''
#include "editor.ai_file_scope.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
namespace editor_ai_file_scope=epochengine::editor_ai_file_scope;
inline std::filesystem::path testEngineRoot;
inline void set_test_engine_root(std::filesystem::path root) { testEngineRoot=std::move(root); }
inline std::filesystem::path resolve_editor_path(std::filesystem::path path) { return path; }
namespace core::path {
 inline std::filesystem::path executable_dir() { return testEngineRoot/"output"; }
 inline std::filesystem::path find_epoch_repo_root(const std::filesystem::path&) { return testEngineRoot; }
}
namespace ai {
 struct EvidencePaths { std::string workspace_root; };
 inline EvidencePaths default_evidence_paths() { return {testEngineRoot.generic_string()}; }
}
// Only preview rendering and digest services are mocked. Real filesystem
// scope, selected-project policy, attachment bytes and admission code execute.
namespace core::sha256 {
 inline std::string hash(std::string_view) { return "image-test-identity"; }
 inline std::string hex(std::string identity) { return identity; }
}
struct SpriteHandle { bool is_valid() const { return false; } };
namespace gui {
 struct Vec2 { float x{},y{}; };
 struct ChatImageAttachment { std::string id,name,status; SpriteHandle preview; Vec2 source_size; };
 inline SpriteHandle register_runtime_surface(std::string_view,const std::vector<std::uint8_t>&,unsigned,unsigned) { return {}; }
}
namespace platform::filesystem {
 struct Thumbnail { std::vector<std::uint8_t> rgba; unsigned width{},height{}; };
 inline Thumbnail image_thumbnail(std::string_view) { return {}; }
}
struct AiChat {
 struct Image { std::string name,mime,bytes,identity; };
 struct File { std::string name,content; };
 std::string input;
 std::vector<Image> draftReferenceImages;
 std::vector<File> draftReferenceFiles;
 std::unordered_map<std::string,gui::ChatImageAttachment> referencePreviews;
 std::vector<std::string> statuses;
 void append_status(std::string message) { statuses.push_back(std::move(message)); }
};
'''
        fixture = (ROOT/'Tests/editor/editor.ai_file_scope_test.cpp').read_text(encoding='utf-8')
        parent = ROOT/'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='public-assistant-scope-', dir=parent))
        try:
            tool, kit = tools[-1], kits[-1]
            cpp, binary = scratch/'contract.cpp', scratch/'contract.exe'
            cpp.write_text(prefix+project+'\n'+attachment+'\n'+fixture, encoding='utf-8')
            args = [str(tool/'bin/Hostx64/x64/cl.exe'), '/nologo', '/utf-8', '/std:c++latest', '/EHsc',
                    '/DEPOCH_EDITION_PUBLIC=1', '/DEPOCH_TEST_PUBLIC_GUI_PATHS=1',
                    '/I'+str(ROOT/'Engine/src/editor/ai'), '/I'+str(tool/'include'), '/I'+str(kit/'ucrt'),
                    '/Fo'+str(scratch/'contract.obj'), '/Fe'+str(binary), str(cpp), '/link',
                    '/LIBPATH:'+str(tool/'lib/x64'), '/LIBPATH:'+str(sdk/'Lib'/kit.name/'um/x64'),
                    '/LIBPATH:'+str(sdk/'Lib'/kit.name/'ucrt/x64')]
            compiled = subprocess.run(args, cwd=scratch, capture_output=True, text=True, timeout=45)
            self.assertEqual(compiled.returncode, 0, compiled.stdout[-6000:]+compiled.stderr)
            result = subprocess.run([str(binary), str(ROOT)], cwd=scratch, capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, f'Actual scope contract exit {result.returncode}: '+result.stdout+result.stderr)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)
