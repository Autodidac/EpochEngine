"""Forest pointer painting: actual body-space hit math and owner wiring."""
from pathlib import Path
import unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT=Path(__file__).resolve().parents[2]
OWNER=ROOT/'Engine/src/editor/application'

class ForestPointerPaint(unittest.TestCase):
    def test_real_body_space_hits(self):
        source=(OWNER/'editor.application_document_editing.inl').read_text()
        function=definition(source,'        [[nodiscard]] std::optional<std::array<double,3>> forest_brush_surface_direction(')
        report=ROOT/'Artifacts/Reports/forest-pointer-paint-nov6'
        report.mkdir(parents=True,exist_ok=True)
        cpp=report/'consumer.cpp'
        cpp.write_text('#include <array>\n#include <optional>\n#include <cmath>\n#include <cassert>\n#include <limits>\n'+function+r"""
int main(){
 auto front=forest_brush_surface_direction({0,0,20},{0,0,-1},10);
 assert(front && std::abs((*front)[2]-1)<1e-12);
 auto miss=forest_brush_surface_direction({0,0,20},{1,0,0},10);assert(!miss);
 assert(!forest_brush_surface_direction({0,0,20},{0,0,1},10));
 auto inside=forest_brush_surface_direction({0,0,5},{1,0,0},10);
 assert(inside && std::abs((*inside)[2]-.5)<1e-12);
 auto far=forest_brush_surface_direction({0,0,6371000.0+1000},{0,0,-1},6371000);
 assert(far && std::abs((*far)[2]-1)<1e-12);
 // Non-uniform planet scaling changes ray magnitude, never radial direction.
 auto scaled=forest_brush_surface_direction({0,0,20},{0,0,-.25},10);
 assert(scaled && std::abs((*scaled)[2]-1)<1e-12);
 assert(!forest_brush_surface_direction({0,0,20},{0,0,0},10));
 assert(!forest_brush_surface_direction({0,0,20},{0,0,-1},0));
 assert(!forest_brush_surface_direction({0,0,20},{0,0,-1},std::numeric_limits<double>::infinity()));
 assert(!forest_brush_surface_direction({0,0,20},{NAN,0,-1},10));
}
""",encoding='utf-8')
        compile_and_run(self,cpp,report)

    def test_stamps_use_owned_region_worker_and_do_not_retarget_camera(self):
        source=(OWNER/'editor.application_document_editing.inl').read_text()
        brush=definition(source,'        void update_forest_brush(')
        self.assertIn('held&&!state.forestBrushHeld',brush)
        self.assertIn('state.forestSurfacePending',brush)
        self.assertIn('frame.scene_input_captured||modalVisible||!ctx->has_input_focus_safe()',brush)
        self.assertIn('state.projectBuildPending||state.projectRuntimeProcess.valid()',brush)
        self.assertIn('state.forestBrushLastDirection',brush)
        self.assertIn('spacing*spacing',brush)
        self.assertIn('begin_forest_surface_placement(state,planet,ordinal,state.forestArea,center)',brush)
        self.assertNotIn('create_editor_scene_entities',brush)
        self.assertNotIn('pick_editor_scene_entity',brush)
        placement=(OWNER/'editor.application_scene_preview.inl').read_text()
        self.assertIn('if(brushDirection)direction=*brushDirection;',placement)
        self.assertIn('state.forestPlacementFocusPending=!intent->area;',placement)
        self.assertIn('region.terrain_object_id=intent->entity;',placement)
        self.assertIn('"Paint forest area"',placement)
        viewport=(OWNER/'editor.frame_viewport.inl').read_text()
        self.assertIn('update_forest_brush(ctx,editor,result,modal_visible_now());',viewport)
        self.assertIn('Hold the left mouse button and drag over terrain',viewport)
        self.assertLess(brush.index('if(!held)'),brush.index('pointer.x<viewport.position.x'))
        undo=definition((OWNER/'editor.application_scene_transactions.inl').read_text(),'        [[nodiscard]] bool undo_editor_scene(')
        self.assertLess(undo.index('state.forestSurfacePlacement.reset()'),undo.index('state.sceneDocument.undo()'))
        reset=definition((OWNER/'editor.application_project_documents.inl').read_text(),'        void reset_project_bound_editor_state(')
        self.assertIn('state.forestSurfacePlacement.reset()',reset)
        self.assertIn('state.forestBrushLastDirection.reset()',reset)
        poll=definition(placement,'        void poll_forest_surface_placement(')
        self.assertIn('if(!intent)return;',poll)
        self.assertIn('state.projectId!=intent->projectId',poll)

    def test_assistant_paints_visible_terrain_and_respects_user_view_change(self):
        runtime=(OWNER/'editor.frame_project_runtime.inl').read_text()
        dispatch=runtime.split('else if (call.tool == "forest.paint")',1)[1].split('else if (call.tool == "project.save")',1)[0]
        self.assertLess(dispatch.index('editor.forestTerrainPainting = true'),dispatch.index('editor.requestedMainSurface = EditorMainSurface::ForestFactory'))
        self.assertIn('editor.forestPlacementCamera=previewgrid::camera_for(ctx.get())',dispatch)
        self.assertIn('editor.requestedMainSurface.value_or(editor.mainSurface)==EditorMainSurface::ForestFactory',runtime)
        self.assertIn('current&&editor.aiToolExecution!="forest.paint"',runtime)
        workspace=(OWNER/'editor.frame_workspace_docking.inl').read_text()
        forest=workspace.rsplit('case EditorMainSurface::ForestFactory:',1)[1].split('case EditorMainSurface::PlantLab:',1)[0]
        self.assertLess(forest.index('if (editor.forestTerrainPainting)'),forest.index('else if (editor.forestDesignCamera)'))
        self.assertIn('*editor.forestProjectCamera',forest)

if __name__=='__main__':unittest.main()

