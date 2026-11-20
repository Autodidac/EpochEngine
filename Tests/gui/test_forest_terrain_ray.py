"""Exact forest mesh picking against real immutable artifacts and ray primitives."""
from pathlib import Path
import json
import re
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


class ForestTerrainRay(unittest.TestCase):
    def test_actual_indexed_mesh_picker_debug_release(self):
        owner = ROOT / 'Engine/src/editor/application'
        state = (owner / 'editor.application_state.inl').read_text(encoding='utf-8')
        source = (owner / 'editor.application_document_editing.inl').read_text(encoding='utf-8')
        cache = definition(state, '        struct ForestTerrainRayCache', True)
        picker = definition(source, '        [[nodiscard]] std::optional<std::array<double,3>> forest_terrain_mesh_hit(')
        lasso_picker = definition(source, '        [[nodiscard]] std::optional<std::array<double,3>> forest_lasso_surface_direction(')
        preview_owner = (ROOT / 'Engine/modules/render/render.preview_grid.ixx').read_text(encoding='utf-8')
        transform = definition(preview_owner, '    export struct PreciseMeshTransform final', True).replace('export ', '')
        planetary_owner = (ROOT / 'Engine/modules/project/project.planetary.ixx').read_text(encoding='utf-8')
        planetary_types = '\n'.join(definition(planetary_owner, f'    struct {name} final', True)
                                    for name in ('GeneratedMeshFrame', 'GeneratedBody'))
        texture = (ROOT / 'Engine/src/assets/texture/asset.texture_artifact.cpp').read_text(encoding='utf-8')
        hashing = ('namespace epochengine::asset::texture {\n'
                   + re.search(r'constexpr std::uint64_t kFnvPrime[^;]+;', texture).group(0)
                   + definition(texture, '        class StableHash final', True)
                   + definition(texture, '    ContentHash deterministic_content_hash(') + '\n}')
        modules = []
        for name in ('asset.texture_artifact.ixx', 'asset.mesh_artifact.ixx',
                     'render/render.math.ixx', 'render/render.ray.ixx'):
            text = (ROOT / 'Engine/modules' / name).read_text(encoding='utf-8')
            text = re.sub(r'^(?:module;|export module [^;]+;|import [^;]+;)\s*$', '', text, flags=re.M)
            modules.append(text.replace('export namespace', 'namespace'))
        report = ROOT / 'Artifacts/Reports/forest-terrain-ray' / str(time.time_ns())
        report.mkdir(parents=True)
        cpp = report / 'consumer.cpp'
        cpp.write_text('\n'.join(modules) + hashing + '\nnamespace asset = epochengine::asset;\n'
                       + 'namespace project_planetary {\n' + planetary_types + '\n}\n'
                       + 'namespace previewgrid {\n' + transform + r'''
struct ObjectMarker {
    std::shared_ptr<const asset::mesh::Artifact> mesh;
    std::optional<PreciseMeshTransform> preciseTransform;
};
}
''' + cache + picker + lasso_picker + r'''
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)
namespace m = epochengine::asset::mesh;
namespace r = epochengine::ray;
std::shared_ptr<const m::Artifact> mesh_at(float nearZ, float farZ, unsigned repeats=1) {
    std::vector<m::Vertex> vertices;
    std::vector<std::uint32_t> indices;
    // Far triangles come first, forcing nearest-hit replacement later.
    for(float z : {farZ,nearZ}) for(unsigned n=0;n<repeats;++n) {
        const auto i=static_cast<std::uint32_t>(vertices.size());
        vertices.push_back({{-2,-2,z}}); vertices.push_back({{2,-2,z}}); vertices.push_back({{0,2,z}});
        indices.insert(indices.end(),{i,i+1,i+2});
    }
    return m::build_artifact(vertices,indices).artifact;
}
int lasso_direction() {
    previewgrid::ObjectMarker marker{mesh_at(3,8), previewgrid::PreciseMeshTransform{{10,2,-6},{2,3,4}}};
    project_planetary::GeneratedBody body{};
    body.center_meters={100,200,317};
    body.first_index=3;
    body.index_count=3;
    const project_planetary::GeneratedMeshFrame frame{3,{100,200,300}};
    ForestTerrainRayCache cache;
    r::RayDesc ray{{10,2,-6},{0,0,1},0,20};
    const auto hit=forest_lasso_surface_direction(marker,ray,1,frame,body,false,cache);
    CHECK(hit && (*hit)[2]==-1);
    // Camera-space distances must account for nonuniform scale and perspective angle.
    ray.maximumDistance=7;
    CHECK(!forest_lasso_surface_direction(marker,ray,1,frame,body,false,cache));
    CHECK(forest_lasso_surface_direction(marker,ray,.5,frame,body,false,cache)==hit);
    ray.maximumDistance=20;
    body.first_index=0;
    CHECK(!forest_lasso_surface_direction(marker,ray,1,frame,body,false,cache));
    CHECK(forest_lasso_surface_direction(marker,ray,1,frame,body,true,cache)==hit);
    ray={{10,2,40},{0,0,-1},0,60};
    CHECK(!forest_lasso_surface_direction(marker,ray,1,frame,body,true,cache));
    return 0;
}
int main() {
    if(const auto code=lasso_direction())return code;
    auto mesh=mesh_at(3,8,300); CHECK(mesh);
    ForestTerrainRayCache cache;
    r::RayDesc ray{{0,0,0},{0,0,4},0,20};
    auto hit=forest_terrain_mesh_hit(mesh,ray,&cache);
    CHECK(hit && std::abs((*hit)[2]-3)<1e-6 && cache.chunks.size()==3);
    const auto* chunks=cache.chunks.data();
    CHECK(forest_terrain_mesh_hit(mesh,ray,&cache)==hit && cache.chunks.data()==chunks);
    CHECK(forest_terrain_mesh_hit(mesh,ray)==hit);
    ray.minimumDistance=4;
    hit=forest_terrain_mesh_hit(mesh,ray,&cache); CHECK(hit && (*hit)[2]==8);
    ray.maximumDistance=7; CHECK(!forest_terrain_mesh_hit(mesh,ray,&cache));
    ray={{0,0,6},{0,0,-1},0,4};
    hit=forest_terrain_mesh_hit(mesh,ray,&cache); CHECK(hit && (*hit)[2]==3);
    ray={{0,0,0},{0,0,-1},0,20}; CHECK(!forest_terrain_mesh_hit(mesh,ray,&cache));
    ray={{3,0,0},{0,0,1},0,20}; CHECK(!forest_terrain_mesh_hit(mesh,ray,&cache));
    // Bounds span both planes. The finite ray ends inside the box but hits near plane.
    mesh=mesh_at(3,8); ray={{0,0,4},{0,0,-1},0,2};
    hit=forest_terrain_mesh_hit(mesh,ray,&cache); CHECK(hit && (*hit)[2]==3);
    CHECK(cache.chunks.size()==1 && cache.artifact.lock()==mesh);
    // A new immutable artifact with the same topology must replace cached bounds.
    mesh=mesh_at(13,18); ray={{0,0,0},{0,0,1},0,20};
    hit=forest_terrain_mesh_hit(mesh,ray,&cache); CHECK(hit && (*hit)[2]==13);
    ray.maximumDistance=12; CHECK(!forest_terrain_mesh_hit(mesh,ray,&cache));
    ray.maximumDistance=13; CHECK(forest_terrain_mesh_hit(mesh,ray,&cache));
    ray.direction={0,0,0}; CHECK(!forest_terrain_mesh_hit(mesh,ray,&cache));
    ray.direction={0,0,1}; ray.origin.x=std::numeric_limits<float>::quiet_NaN();
    CHECK(!forest_terrain_mesh_hit(mesh,ray,&cache));
    CHECK(!forest_terrain_mesh_hit({},r::RayDesc{},&cache));
    std::weak_ptr<const m::Artifact> old=mesh;
    mesh.reset(); CHECK(old.expired() && cache.artifact.expired());
    return 0;
}
''', encoding='utf-8')
        records = compile_and_run(self, cpp, report)
        (report / 'result.json').write_text(json.dumps(records, indent=2), encoding='utf-8')

    def test_gesture_uses_exact_visible_body_and_commits_only_valid_release(self):
        owner = ROOT / 'Engine/src/editor/application/editor.application_document_editing.inl'
        source = owner.read_text(encoding='utf-8')
        gesture = definition(source, '        void update_forest_lasso(')
        for admission in ('ContextType::OpenGL', 'input::Key::Escape', 'frame.scene_input_captured',
                          'gesture->projectGeneration != generation', 'gesture->sceneRevision != state.sceneDocumentRevision',
                          'gesture->context != ctx.get()', 'gesture->selection != state.selectedEntityId',
                          'gesture->source != policy.source', 'gesture->mesh != *policy.generated_mesh',
                          'gesture->bodyIndex != bodyId', 'marker->mesh != terrain.meshSnapshot',
                          'visible->solids->markers.size() == 1u', '!marker->paintView',
                          '&visible->camera', 'forest_lasso_surface_direction', 'gesture.reset()'):
            self.assertIn(admission, gesture)
        self.assertNotIn('forest_brush_surface_direction', gesture)
        self.assertLess(gesture.index('if (held) return;'), gesture.index('ForestLassoFootprint::from_directions'))
        self.assertLess(gesture.index('if (!footprint)'), gesture.index('begin_forest_surface_placement'))
        dispatcher = definition(source, '        void update_forest_brush(')
        self.assertIn('modalVisible||!ctx->has_input_focus_safe()', dispatcher)
        self.assertLess(dispatcher.index('state.forestLassoGesture.reset()'), dispatcher.index('update_forest_lasso('))


if __name__ == '__main__':
    unittest.main()
