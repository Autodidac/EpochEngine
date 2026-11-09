// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

import asset.mesh_artifact;
import scene.snapshot;
import scene.serializer;
import scene.document;
import scene.runtime;
import render.preview_grid;
import editor.application;

#define MESH_CHECK(expression) do { if (!(expression)) return __LINE__; } while (false)

int main(int argc, char** argv)
{
    namespace mesh = epochengine::asset::mesh;
    namespace scene = epochengine::scene;
    namespace preview = epochengine::previewgrid;
    std::array<mesh::Vertex, 4> vertices{};
    vertices[0].position = {-1.0f, 0.0f, -1.0f};
    vertices[1].position = {1.0f, 0.0f, -1.0f};
    vertices[2].position = {1.0f, 0.0f, 1.0f};
    vertices[3].position = {-1.0f, 0.0f, 1.0f};
    const std::array<std::uint32_t, 6> indices{0u, 2u, 1u, 0u, 3u, 2u};
    const auto first = mesh::build_artifact(vertices, indices);
    MESH_CHECK(first);
    const auto encoded = mesh::encode_artifact(*first.artifact);
    const auto decoded = mesh::decode_artifact(encoded);
    MESH_CHECK(decoded && decoded.artifact->key() == first.artifact->key());
    MESH_CHECK(mesh::encode_artifact(*decoded.artifact) == encoded);
    vertices[0].position[1] = 0.2f;
    MESH_CHECK(first.artifact->vertices()[0].position[1] == 0.0f);
    const auto changed = mesh::build_artifact(vertices, indices);
    MESH_CHECK(changed && changed.artifact->key() != first.artifact->key());
    auto invalid_indices = indices;
    invalid_indices[0] = 4u;
    MESH_CHECK(!mesh::build_artifact(vertices, invalid_indices));
    invalid_indices = indices;
    invalid_indices[0] = invalid_indices[1];
    MESH_CHECK(!mesh::build_artifact(vertices, invalid_indices));
    const auto original = vertices[0].position[0];
    vertices[0].position[0] = (std::numeric_limits<float>::infinity)();
    MESH_CHECK(!mesh::build_artifact(vertices, indices));
    vertices[0].position[0] = original;
    MESH_CHECK(!mesh::build_artifact(vertices, indices, {3u, 6u, 1024u}));
    auto corrupted = encoded;
    corrupted.back() ^= std::byte{1};
    MESH_CHECK(!mesh::decode_artifact(corrupted));
    corrupted = encoded;
    corrupted.push_back(std::byte{});
    MESH_CHECK(!mesh::decode_artifact(corrupted));
    MESH_CHECK(!mesh::decode_artifact(std::span<const std::byte>{encoded}.first(20u)));

    scene::SceneSnapshot snapshot{};
    snapshot.scene_id = "mesh_contract";
    snapshot.project_id = "mesh_project";
    snapshot.objects.push_back({.id = 17u, .name = "Terrain", .type = "IndexedMesh",
        .category = "World", .mesh_asset = scene::SceneMeshSnapshot{
            "Assets/Mesh/test.epoch_mesh", first.artifact->key().words, 4u, 6u}});
    snapshot.objects.push_back({.id = 20u, .name = "Camera", .type = "Camera"});
    snapshot.objects.push_back({.id = 21u, .name = "Spawn", .type = "Spawn"});
    snapshot.primary_camera = 20u;
    snapshot.primary_spawn = 21u;
    snapshot.objects[0].texture_material = scene::SceneTextureMaterialSnapshot{
        .logical_path = "Assets/Textures/terrain.epoch_texture", .artifact_key = {2u, 3u, 5u, 7u}, .stable_key = 11u};
    const auto text = scene::serialize_snapshot_text(snapshot);
    MESH_CHECK(text.starts_with("epoch_snapshot 4\n"));
    const auto parsed = scene::parse_snapshot_text(text);
    if (!parsed.ok && argc == 2)
    {
        std::filesystem::create_directories(argv[1]);
        std::ofstream diagnostic{std::filesystem::path{argv[1]} / "parse-failure.txt"};
        diagnostic << parsed.error << "\n" << text;
    }
    MESH_CHECK(parsed.ok && parsed.snapshot.objects[0].mesh_asset == snapshot.objects[0].mesh_asset);
    MESH_CHECK(parsed.snapshot.objects[0].texture_material == snapshot.objects[0].texture_material);
    // Large reference metadata remains bounded and persists without allocating
    // vertex buffers; it does not relax the default artifact decoder.
    auto large_reference = snapshot;
    large_reference.objects[0].mesh_asset->vertex_count = 267'126u;
    large_reference.objects[0].mesh_asset->index_count = 1'587'600u;
    const auto large_reference_text = scene::serialize_snapshot_text(large_reference);
    const auto large_reference_parsed = scene::parse_snapshot_text(large_reference_text);
    MESH_CHECK(large_reference_parsed.ok);
    MESH_CHECK(large_reference_parsed.snapshot.objects[0].mesh_asset == large_reference.objects[0].mesh_asset);
    large_reference.objects[0].mesh_asset->vertex_count = 1'048'577u;
    MESH_CHECK(scene::serialize_snapshot_text(large_reference).empty());
    large_reference.objects[0].mesh_asset->vertex_count = 267'126u;
    large_reference.objects[0].mesh_asset->index_count = 6'291'459u;
    MESH_CHECK(scene::serialize_snapshot_text(large_reference).empty());
    auto document = epochengine::authoring::scene::SceneDocument::from_snapshot(parsed.snapshot);
    MESH_CHECK(document);
    const auto projected = document.value.project_snapshot(0u, 0.0);
    MESH_CHECK(projected && projected.snapshot.objects[0].mesh_asset == snapshot.objects[0].mesh_asset);
    // A regenerated provider mesh replaces its one authored object atomically;
    // undo restores the earlier artifact, rather than keeping duplicate terrain.
    {
        namespace authored = epochengine::authoring::scene;
        const auto original = document.value.objects().front();
        auto replacement = original.descriptor;
        replacement.mesh_asset->artifact_key = changed.artifact->key().words;
        const std::array<authored::SceneOperation, 2> replace{
            authored::ObjectDestroyedOperation{.object=original.handle},
            authored::ObjectCreatedOperation{.object=replacement}};
        MESH_CHECK(document.value.apply_transaction(replace, "Regenerate terrain"));
        auto regenerated = document.value.project_snapshot(0u, 0.0);
        MESH_CHECK(regenerated && regenerated.snapshot.objects.size() == snapshot.objects.size());
        MESH_CHECK(regenerated.snapshot.objects[0].id == snapshot.objects[0].id);
        MESH_CHECK(regenerated.snapshot.objects[0].mesh_asset->artifact_key == changed.artifact->key().words);
        MESH_CHECK(document.value.undo());
        MESH_CHECK(document.value.project_snapshot(0u, 0.0).snapshot.objects[0].mesh_asset == snapshot.objects[0].mesh_asset);
        MESH_CHECK(document.value.redo());
        MESH_CHECK(document.value.project_snapshot(0u, 0.0).snapshot.objects[0].mesh_asset->artifact_key == changed.artifact->key().words);
    }
    epochengine::scene_runtime::SceneRuntime runtime{};
    MESH_CHECK(runtime.replace(parsed.snapshot).committed());
    MESH_CHECK(runtime.projection()->entities()[0].mesh_asset == snapshot.objects[0].mesh_asset);
    auto invalid_snapshot = snapshot;
    invalid_snapshot.objects[0].mesh_asset->logical_path = "../escape.epoch_mesh";
    MESH_CHECK(scene::serialize_snapshot_text(invalid_snapshot).empty());

    // Versions1/2/3 remain readable; older object lines contain no mesh suffix.
    auto old = text;
    old.replace(0u, std::string("epoch_snapshot 4").size(), "epoch_snapshot 3");
    const auto suffix = old.find(" mesh 1 path ");
    MESH_CHECK(suffix != std::string::npos);
    old.erase(suffix, old.find('\n', suffix) - suffix);
    for (auto mesh_suffix = old.find(" mesh 0"); mesh_suffix != std::string::npos; mesh_suffix = old.find(" mesh 0"))
        old.erase(mesh_suffix, 7u);
    const auto old_parsed = scene::parse_snapshot_text(old);
    MESH_CHECK(old_parsed.ok && !old_parsed.snapshot.objects[0].mesh_asset);
    old.replace(0u, std::string("epoch_snapshot 3").size(), "epoch_snapshot 2");
    for (auto material = old.find(" material "); material != std::string::npos; material = old.find(" material "))
        old.erase(material, old.find('\n', material) - material);
    MESH_CHECK(scene::parse_snapshot_text(old).ok);
    MESH_CHECK(scene::parse_snapshot_text("epoch_snapshot 1\nscene \"old\"\nworld \"legacy\"\nframe 0\ntime 0\nobjects 0\ntimeline_keys 0\n").ok);

    // A radius-ten planet is outside the default camera eye, yet cannot fit
    // that camera's frustum. Frame actual geometry once without moving another
    // context or replacing a restored/custom/authored camera.
    {
        std::array<mesh::Vertex,6> body{};
        body[0].position={10,0,0};body[1].position={-10,0,0};
        body[2].position={0,10,0};body[3].position={0,-10,0};
        body[4].position={0,0,10};body[5].position={0,0,-10};
        const std::array<std::uint32_t,24> bodyIndices{0,2,4,4,2,1,1,2,5,5,2,0,0,4,3,4,1,3,1,5,3,5,0,3};
        const auto planet=mesh::build_artifact(body,bodyIndices);MESH_CHECK(planet);
        preview::ObjectMarker planetMarker{.primitive=preview::ObjectPreviewPrimitive::IndexedMesh,.mesh=planet.artifact};
        int initialContext{},otherContext{},restoredContext{};
        const auto initialView=preview::capture_camera_rig_snapshot(&initialContext);
        const auto framing=preview::initial_orbit_mesh_framing(planetMarker,initialView,16.0f/9.0f);
        MESH_CHECK(framing&&framing->geometryRadius==10.0f&&initialView.distance>framing->geometryRadius);
        MESH_CHECK(framing->distance>=32.5f&&framing->target.x==0.0f&&framing->target.y==0.0f&&framing->target.z==0.0f);
        const auto portrait=preview::initial_orbit_mesh_framing(planetMarker,initialView,0.25f);
        MESH_CHECK(portrait&&portrait->distance>framing->distance);
        auto transformed=planetMarker;transformed.position={4,3,-2};transformed.rotationDegrees={0,45,0};transformed.scale={2,1,0.5f};
        const auto moved=preview::initial_orbit_mesh_framing(transformed,initialView,16.0f/9.0f);
        MESH_CHECK(moved&&moved->target.x==4&&moved->target.y==3&&moved->target.z==-2&&std::abs(moved->geometryRadius-20.0f)<0.0001f);
        auto small=planetMarker;small.scale={0.05f,0.05f,0.05f};
        MESH_CHECK(!preview::initial_orbit_mesh_framing(small,initialView,16.0f/9.0f));
        MESH_CHECK(!preview::initial_orbit_mesh_framing(planetMarker,initialView,16.0f/9.0f,false));
        auto explicitView=initialView;explicitView.distance=25.0f;
        MESH_CHECK(!preview::initial_orbit_mesh_framing(planetMarker,explicitView,16.0f/9.0f));
        explicitView=initialView;explicitView.mode=preview::CameraMode::FPS;
        MESH_CHECK(!preview::initial_orbit_mesh_framing(planetMarker,explicitView,16.0f/9.0f));
        explicitView.mode=preview::CameraMode::Canvas2D;
        MESH_CHECK(!preview::initial_orbit_mesh_framing(planetMarker,explicitView,16.0f/9.0f));
        auto invalidMarker=planetMarker;invalidMarker.scale.x=0.0f;
        MESH_CHECK(!preview::initial_orbit_mesh_framing(invalidMarker,initialView,16.0f/9.0f));
        MESH_CHECK(!preview::initial_orbit_mesh_framing(planetMarker,initialView,0.0f));
        const auto otherBefore=preview::capture_camera_rig_snapshot(&otherContext);
        MESH_CHECK(preview::frame_initial_orbit_mesh(&initialContext,planetMarker,16.0f/9.0f));
        MESH_CHECK(preview::camera_distance_for(&initialContext)==framing->distance);
        MESH_CHECK(preview::camera_distance_for(&otherContext)==otherBefore.distance);
        MESH_CHECK(!preview::frame_initial_orbit_mesh(&initialContext,planetMarker,16.0f/9.0f));
        MESH_CHECK(preview::restore_camera_rig_snapshot(&restoredContext,initialView));
        MESH_CHECK(!preview::frame_initial_orbit_mesh(&restoredContext,planetMarker,16.0f/9.0f));
        preview::reset_camera(&restoredContext);
        MESH_CHECK(!preview::frame_initial_orbit_mesh(&restoredContext,planetMarker,16.0f/9.0f));
        MESH_CHECK(planetMarker.mesh->key()==planet.artifact->key()&&planetMarker.scale.x==1.0f&&planetMarker.position.x==0.0f);
        std::vector<epochengine::EditorSceneSeedEntity> seeds{{"EditorCamera","Camera","Editor",{0,4.8f,8.5f},{-28,0,0}}};
        auto authored=seeds;
        MESH_CHECK(epochengine::editor_seed_cameras_unchanged(authored,seeds));
        authored[0].position[0]=1.0f;
        MESH_CHECK(!epochengine::editor_seed_cameras_unchanged(authored,seeds));
        authored=seeds;authored.push_back({"AuthoredCamera","Camera","Gameplay"});
        MESH_CHECK(!epochengine::editor_seed_cameras_unchanged(authored,seeds));
        authored=seeds;authored[0].rotation[1]=45.0f;
        MESH_CHECK(!epochengine::editor_seed_cameras_unchanged(authored,seeds));
        authored.clear();
        MESH_CHECK(!epochengine::editor_seed_cameras_unchanged(authored,seeds));
        preview::cleanup_context(&initialContext);preview::cleanup_context(&otherContext);preview::cleanup_context(&restoredContext);
    }

    int context{};
    preview::ObjectMarker marker{.position = {3.0f, 2.0f, 1.0f},
        .scale = {2.0f, 3.0f, 4.0f}, .primitive = preview::ObjectPreviewPrimitive::IndexedMesh,
        .mesh = first.artifact};
    MESH_CHECK(preview::set_object_markers(&context, {&marker, 1u}));
    const auto revision = preview::preview_content_revision_for(&context);
    const auto solid = preview::object_solid_vertices_for(&context);
    MESH_CHECK(solid.size() == 6u && solid[0].position.x == 1.0f
        && solid[0].position.y == 2.0f && solid[0].position.z == -3.0f);
    marker.mesh = decoded.artifact;
    MESH_CHECK(preview::set_object_markers(&context, {&marker, 1u}));
    MESH_CHECK(preview::preview_content_revision_for(&context) == revision);
    marker.mesh = changed.artifact;
    MESH_CHECK(preview::set_object_markers(&context, {&marker, 1u}));
    MESH_CHECK(preview::preview_content_revision_for(&context) != revision);
    std::vector<std::uint32_t> maximum_indices(196'608u);
    for (std::size_t i = 0u; i < maximum_indices.size(); ++i) maximum_indices[i] = indices[i % indices.size()];
    const auto maximum = mesh::build_artifact(vertices, maximum_indices);
    MESH_CHECK(maximum);
    marker.mesh = maximum.artifact;
    std::array<preview::ObjectMarker, 2> excessive{marker, marker};
    const auto before_rejection = preview::preview_content_revision_for(&context);
    MESH_CHECK(!preview::set_object_markers(&context, excessive));
    MESH_CHECK(preview::preview_content_revision_for(&context) == before_rejection);
    // Larger scenes require a caller-supplied budget; the default admission
    // stays unchanged. A failed replacement must retain the complete old frame.
    const preview::MeshFrameLimits largerFrame{393'216u, 16ull * 1024ull * 1024ull};
    MESH_CHECK(preview::set_object_markers(&context, excessive, largerFrame));
    const auto admitted_revision = preview::preview_content_revision_for(&context);
    MESH_CHECK(admitted_revision != before_rejection);
    MESH_CHECK(preview::set_object_markers(&context, excessive, largerFrame));
    MESH_CHECK(preview::preview_content_revision_for(&context) == admitted_revision);
    MESH_CHECK(!preview::set_object_markers(&context, excessive, {393'215u, largerFrame.maximum_bytes}));
    MESH_CHECK(!preview::set_object_markers(&context, excessive, {largerFrame.maximum_indices, 1u}));
    MESH_CHECK(!preview::set_object_markers(&context, {}, {6'291'457u, largerFrame.maximum_bytes}));
    MESH_CHECK(!preview::set_object_markers(&context, {}, {largerFrame.maximum_indices, 64ull * 1024ull * 1024ull + 1u}));
    MESH_CHECK(preview::preview_content_revision_for(&context) == admitted_revision);
    preview::clear_object_markers(&context);

    MESH_CHECK(argc == 2);
    const auto project = std::filesystem::path{argv[1]};
    std::filesystem::create_directories(project / "Assets/Mesh");
    const auto file = project / "Assets/Mesh/test.epoch_mesh";
    { std::ofstream output{file, std::ios::binary};
      output.write(reinterpret_cast<const char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
      MESH_CHECK(output); }
    MESH_CHECK(mesh::read_project_artifact(project, "Assets/Mesh/test.epoch_mesh", first.artifact->key()));
    MESH_CHECK(!mesh::read_project_artifact(project, "../test.epoch_mesh", first.artifact->key()));
    const std::string embedded_null{"Assets/Mesh/test.epoch_mesh\0suffix", 33u};
    MESH_CHECK(!mesh::read_project_artifact(project, embedded_null, first.artifact->key()));
    MESH_CHECK(!mesh::read_project_artifact(project, "Assets//Mesh/test.epoch_mesh", first.artifact->key()));
    MESH_CHECK(!mesh::read_project_artifact(project, "Assets/Mesh/test.epoch_mesh/", first.artifact->key()));
    MESH_CHECK(!mesh::read_project_artifact(project, "Assets/Mesh/test.epoch_mesh", changed.artifact->key()));
    auto trailing_reference = *snapshot.objects[0].mesh_asset;
    trailing_reference.logical_path += '/';
    MESH_CHECK(!trailing_reference.valid());
    const auto escaped_project = project / "escaped-project";
    const auto outside = project / "outside-project";
    std::filesystem::create_directories(escaped_project);
    std::filesystem::create_directories(outside);
    std::error_code symlink_error;
    if (!std::filesystem::exists(escaped_project / "Assets"))
        std::filesystem::create_directory_symlink(outside, escaped_project / "Assets", symlink_error);
    if (std::filesystem::exists(escaped_project / "Assets"))
    {
        MESH_CHECK(!std::filesystem::exists(outside / "Mesh"));
        MESH_CHECK(!mesh::prepare_project_mesh_directory(escaped_project));
        MESH_CHECK(!std::filesystem::exists(outside / "Mesh"));
        std::ofstream proof{project / "junction-refusal.txt"};
        proof << "Existing Assets link rejected before creating outside-project/Mesh.\n";
    }
    MESH_CHECK(mesh::prepare_project_mesh_directory(project));

    return 0;
}
