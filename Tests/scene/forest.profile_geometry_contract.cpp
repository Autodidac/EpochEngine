// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
import forest.factory;
import forest.preview_mesh;
import authoring.morphology;
import project.forest_library;

namespace f = epochengine::forest;
namespace p = f::preview_mesh;
namespace m = epochengine::authoring::morphology;
namespace l = epochengine::project_forests;
namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    const fs::path root = fs::absolute(argv[1]);
    fs::create_directories(root);
    std::ofstream observations(root / "observations.txt");
    std::ofstream failures(root / "failures.txt");
    unsigned failed{};
    const auto check = [&](bool condition, const char* description) {
        if (!condition) { ++failed; failures << description << '\n'; }
    };
    const auto near = [](float a, float b) { return std::abs(a-b) < 0.0001f; };
    const auto distance = [](auto a, auto b) {
        const auto x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;
        return std::sqrt(x*x+y*y+z*z);
    };
    const auto branch_length = [&](const auto& graph) {
        return graph.segments.size() > 1u
            ? distance(graph.segments[1].start, graph.segments[1].end) : 0.0f;
    };
    auto document = f::make_default_plant_lab_document();
    const f::ForestTextureReference bark{"Assets/Textures/bark.epoch_texture",{1,2,3,4}};
    const f::ForestTextureReference foliage{"Assets/Textures/leaf.epoch_texture",{5,6,7,8}};
    check(f::apply_forest_material_edit(document,f::ForestTextureSlot::bark,bark),"set bark");
    check(f::apply_forest_material_edit(document,f::ForestTextureSlot::foliage,foliage),"set foliage");
    const auto initial = f::compile_forest_asset(document);
    const auto initialMesh = p::build(initial.graph,initial.sample);
    check(initial.valid && initialMesh.valid,"initial actual compiler/mesh");

    // Every admitted Length value must reach the real segment geometry. The
    // original trunk scale is retained throughout edits and exact Undo/Redo.
    for (const float length : {0.25f,1.0f,1.25f,4.0f,8.0f,128.0f})
    {
        auto edited=document;
        check(f::apply_forest_profile_edit(edited,f::ForestProfileProperty::BranchLengthMeters,length),"length edit");
        const auto changed=f::compile_forest_asset(edited);
        const float expected=length*initial.graph.recipe.growth.segment_length_meters/.92f;
        observations << "length " << length << " actual " << branch_length(changed.graph)
            << " expected " << expected << '\n';
        check(changed.valid && p::build(changed.graph,changed.sample).valid,"length actual mesh");
        check(near(branch_length(changed.graph),expected),"length reaches segment beyond node step");
        check(near(changed.graph.recipe.growth.root_length_meters,initial.graph.recipe.growth.root_length_meters),"length retains trunk");
        check(f::undo_forest_profile_edit(edited),"length undo");
        check(f::build_morphology_graph(edited.profile).content_hash==initial.graph.content_hash,"length undo exact graph");
        check(f::redo_forest_profile_edit(edited),"length redo");
        check(f::build_morphology_graph(edited.profile).content_hash==changed.graph.content_hash,"length redo exact graph");
        check(edited.profile.barkTexture==bark&&edited.profile.foliageTexture==foliage,"length retains supplied textures");
    }

    // Exercise both dimensions and real mesh UVs/attachments, without a
    // recording substitute for the morphology or indexed mesh owner.
    for(const auto mode:{f::ForestPreviewMode::Mode2D,f::ForestPreviewMode::Mode3D})
    {
        auto plant=document;plant.profile.previewMode=mode;
        plant.contentHash=f::forest_profile_content_hash(plant.profile);
        const auto baseline=f::compile_forest_asset(plant);
        const auto baseMesh=p::build(baseline.graph,baseline.sample);
        for(const float size:{.03f,.12f,.56f,2.f})
        {
            auto edited=plant;
            check(f::apply_forest_profile_edit(edited,f::ForestProfileProperty::LeafSizeMeters,size),"leaf size edit");
            const auto changed=f::compile_forest_asset(edited);
            const auto mesh=p::build(changed.graph,changed.sample);
            observations << "leaf-size " << int(mode) << ' ' << size << " terminals "
                << changed.graph.terminals.size() << " first-size " << changed.graph.terminals.front().scale_meters << '\n';
            check(changed.valid&&mesh.valid&&mesh.contentSignature!=baseMesh.contentSignature,"leaf size reaches actual mesh");
            check(changed.graph.terminals.size()==baseline.graph.terminals.size(),"leaf size retains sites");
            check(mesh.vertices.size()==baseMesh.vertices.size(),"leaf size retains topology");
            for(std::size_t i=0;i<changed.graph.terminals.size();++i)
            {
                const auto& a=changed.graph.terminals[i];const auto& b=baseline.graph.terminals[i];
                check(near(a.scale_meters/b.scale_meters,size/.28f),"leaf size scales terminals");
                check(distance(a.position,b.position)==0,"leaf size retains attachment");
            }
            for(std::size_t i=0;i<mesh.vertices.size()&&i<baseMesh.vertices.size();++i)
                check(mesh.vertices[i].uv.x==baseMesh.vertices[i].uv.x&&mesh.vertices[i].uv.y==baseMesh.vertices[i].uv.y
                    &&mesh.vertices[i].material==baseMesh.vertices[i].material,"leaf size retains bark/foliage UVs");
            check(f::undo_forest_profile_edit(edited)&&f::build_morphology_graph(edited.profile).content_hash==baseline.graph.content_hash,"leaf size undo");
            check(f::redo_forest_profile_edit(edited)&&f::build_morphology_graph(edited.profile).content_hash==changed.graph.content_hash,"leaf size redo");
        }
        for(const float groups:{1.f,3.f,7.f,16.f})
        {
            auto edited=plant;
            check(f::apply_forest_profile_edit(edited,f::ForestProfileProperty::LeafSitesPerBranch,groups),"leaf groups edit");
            const auto changed=f::compile_forest_asset(edited);
            const auto mesh=p::build(changed.graph,changed.sample);
            observations << "leaf-groups " << int(mode) << ' ' << groups << " terminals " << changed.graph.terminals.size() << '\n';
            check(changed.valid&&mesh.valid&&!changed.graph.truncated,"leaf groups actual mesh");
            check(changed.graph.terminals.size()==24u*static_cast<unsigned>(groups),"leaf groups reach real site count");
            check(f::undo_forest_profile_edit(edited)&&f::build_morphology_graph(edited.profile).content_hash==baseline.graph.content_hash,"leaf groups undo");
            check(f::redo_forest_profile_edit(edited)&&f::build_morphology_graph(edited.profile).content_hash==changed.graph.content_hash,"leaf groups redo");
        }
    }

    auto trunk=document;
    check(f::apply_forest_profile_edit(trunk,f::ForestProfileProperty::BranchLevels,0),"zero levels edit");
    observations << "zero-levels stored " << trunk.profile.branch.levels << '\n';
    check(trunk.profile.branch.levels==0u,"zero levels remains zero");
    const auto compiled=f::compile_forest_asset(trunk);
    const auto mesh=p::build(compiled.graph,compiled.sample);
    check(compiled.valid&&mesh.valid&&m::validate(compiled.graph),"zero levels valid compiler/mesh");
    check(compiled.graph.recipe.growth.generations==0u,"zero generations truthful recipe");
    check(compiled.graph.segments.size()==1u&&compiled.graph.nodes.size()==2u,"zero levels only trunk");
    check(compiled.graph.terminals.empty()&&mesh.leafCardCount==0u,"zero levels no leaves");
    check(near(compiled.graph.recipe.growth.root_length_meters,trunk.profile.config.targetHeightMeters),"zero levels trunk target height");
    check(f::undo_forest_profile_edit(trunk)&&f::build_morphology_graph(trunk.profile).content_hash==initial.graph.content_hash,"zero levels undo");
    check(f::redo_forest_profile_edit(trunk)&&f::build_morphology_graph(trunk.profile).content_hash==compiled.graph.content_hash,"zero levels redo");

    // Force zero independently of the setter so source admission is tested
    // even when the baseline setter clamps it back to one.
    trunk.profile.branch.levels=0u;trunk.contentHash=f::forest_profile_content_hash(trunk.profile);
    l::ForestAssetLibrary owner{"forest-geometry-contract",root/"project"};
    auto published=owner.publish_materialized("Assets/Forest/trunk.epoch_forest",trunk);
    observations << "zero-publish " << l::forest_library_code_name(published.code) << '\n';
    check(bool(published),"zero levels source/artifact publication admitted");
    if(published)
    {
        l::ForestAssetLibrary reopened{"forest-geometry-contract",root/"project"};
        const auto loaded=reopened.load_source("Assets/Forest/trunk.epoch_forest");
        check(bool(loaded)&&loaded.document.profile.branch.levels==0u,"zero levels source roundtrip");
        check(loaded.document.profile.barkTexture==bark&&loaded.document.profile.foliageTexture==foliage,"source retains supplied materials");
        const auto artifact=reopened.load_materialized_exact("Assets/Forest/trunk.epoch_forest",published.artifact);
        check(bool(artifact)&&artifact.artifact.graph.segments.size()==1u&&artifact.artifact.graph.terminals.empty(),"zero levels materialized roundtrip");
        auto undo=loaded.document;
        check(f::undo_forest_profile_edit(undo)&&undo.profile.branch.levels==4u,"zero levels reopened undo");
        check(f::redo_forest_profile_edit(undo)&&undo.profile.branch.levels==0u,"zero levels reopened redo");
    }
    observations << "failed " << failed << '\n';
    return failed ? 1 : 0;
}
