"""Compile production Vulkan preview preparation/upload decisions on CPU only."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def section(source, first, last):
    start = source.index(first)
    return source[start:source.index(last, start)].replace('export ', '')


class VulkanPreviewIndices(unittest.TestCase):
    def test_actual_mesh_and_immutable_upload_identity_debug_release(self):
        owner = (ROOT / 'Engine/modules/vulkan.context_meshcube.ixx').read_text(encoding='utf-8')
        grid = (ROOT / 'Engine/modules/render.preview_grid.ixx').read_text(encoding='utf-8')
        arcade = (ROOT / 'Engine/modules/render.arcade.ixx').read_text(encoding='utf-8')
        commands = (ROOT / 'Engine/modules/vulkan.context_commands.ixx').read_text(encoding='utf-8')
        buffers = (ROOT / 'Engine/modules/vulkan.context_buffers.ixx').read_text(encoding='utf-8')
        code = (ROOT / 'Tests/renderers/vulkan.preview_geometry_contract.cpp').read_text(encoding='utf-8')
        code = code.replace('ARCADE_PLANE', section(arcade,
            '    [[nodiscard]] constexpr float screen_sample_plane_z(', '    static_assert('))
        code = code.replace('ARCADE_FACING', section(arcade,
            '    [[nodiscard]] constexpr bool screen_sample_plane_faces_viewer(', '    static_assert('))
        code = code.replace('TRIANGLE_FACING', section(grid,
            '    export [[nodiscard]] constexpr bool clockwise_solid_triangle_faces_camera(',
            '    static_assert(clockwise_solid_triangle_faces_camera('))
        code = code.replace('WIRE_LINES', section(grid, '    export template<class Emit>',
            '    export [[nodiscard]] inline Vec3 normalize('))
        code = code.replace('SAMPLED_SURFACES', section(grid,
            '    export [[nodiscard]] inline std::vector<ObjectMarker> sampled_render_surface_markers_for(const PublishedSceneFrame&',
            '    export [[nodiscard]] inline std::shared_ptr<const GridGeometry> grid_geometry_for('))
        code = code.replace('MESH_OWNER', owner[owner.index(
            '    inline constexpr std::uint32_t kArcadeScreenVertexCount'):owner.rindex('\n}')].replace('export ', ''))
        code = code.replace('VERTEX_BUFFER_ADMISSION', section(buffers,
            '        const auto vertices =', '        const vk::DeviceSize bufferSize ='))
        code = code.replace('INDEX_BUFFER_ADMISSION', section(buffers,
            '        const auto indices =', '        const vk::DeviceSize bufferSize ='))
        code = code.replace('INDEX_BUFFER_COMMIT', section(buffers,
            '        previewIndicesSequential = sequentialIndices;', '\n    }\n}'))
        decision = section(commands, '            const std::uint64_t previewRevision =',
            '\n        }\n\n        const bool renderScenePreview')
        code = code.replace('UPLOAD_DECISION', decision)
        parent = ROOT / 'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='vulkan-preview-', dir=parent) as temp:
            scratch = Path(temp)
            source = scratch / 'contract.cpp'
            source.write_text(code, encoding='utf-8')
            for configuration in ('Debug', 'Release'):
                with self.subTest(configuration=configuration):
                    binary = scratch / ('contract.exe' if os.name == 'nt' else 'contract')
                    if os.name == 'nt':
                        toolsets = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
                        kitroot = Path('C:/Program Files (x86)/Windows Kits/10')
                        kits = sorted((kitroot / 'Include').glob('*'))
                        if not toolsets or not kits:
                            self.skipTest('MSVC/SDK unavailable')
                        toolset, kit = toolsets[-1], kits[-1]
                        args = [str(toolset / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc', '/WX',
                                '/Od' if configuration == 'Debug' else '/O2',
                                '/I' + str(toolset / 'include'), '/I' + str(kit / 'ucrt'), '/I' + str(kit / 'shared'), '/I' + str(kit / 'um'),
                                '/Fo' + str(scratch / 'contract.obj'), '/Fe' + str(binary), str(source), '/link',
                                '/LIBPATH:' + str(toolset / 'lib/x64'), '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'um/x64'),
                                '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'ucrt/x64')]
                    else:
                        compiler = shutil.which('clang++') or shutil.which('g++')
                        if not compiler:
                            self.skipTest('C++23 compiler unavailable')
                        args = [compiler, '-std=c++23', '-O0' if configuration == 'Debug' else '-O2', str(source), '-o', str(binary)]
                    def compile_run(text):
                        source.write_text(text, encoding='utf-8')
                        built = subprocess.run(args, cwd=scratch, capture_output=True, text=True, timeout=90)
                        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                        return subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True, timeout=15)
                    tested = compile_run(code)
                    self.assertEqual(tested.returncode, 0, f'production preview contract failed at check {tested.returncode}')
                    # Reintroducing the previous packet/revision key must fail
                    # this same CPU test, proving sensitivity to needless uploads.
                    old_key = 'previewRevision != previewGeometryRevision || previewSceneFrame != uploadedPreviewSceneFrame'
                    regressed = code.replace('if (geometryChanged)', f'if ({old_key})', 1)
                    self.assertNotEqual(regressed, code)
                    self.assertNotEqual(compile_run(regressed).returncode, 0,
                                        'Former per-packet upload bug escaped the contract')


if __name__ == '__main__':
    unittest.main()
