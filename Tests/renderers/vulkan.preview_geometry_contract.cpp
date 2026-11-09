// Production functions are inserted by the CPU harness. Only the device/context
// boundary and preview input storage are fixtures; no graphics driver is loaded.
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>
namespace epochengine::core { struct Context {}; }
namespace epochengine::render_arcade { ARCADE_PLANE ARCADE_FACING }
namespace epochengine::previewgrid {
struct Vec3 { float x{}, y{}, z{}; };
struct Vertex { Vec3 position{}, color{}; };
struct Camera { Vec3 eye{}, target{}; };
enum class ObjectPreviewPrimitive { Cube, EngineArcadeScreen };
struct ObjectMarker {
    Vec3 position{}, scale{1,1,1};
    ObjectPreviewPrimitive primitive{ObjectPreviewPrimitive::Cube};
    bool sampledRenderSurface{};
};
struct ObjectPreviewGeometryRoute { bool sampled_surface{}; };
ObjectPreviewGeometryRoute object_preview_geometry_route(ObjectPreviewPrimitive p) {
    return {p == ObjectPreviewPrimitive::EngineArcadeScreen};
}
struct Vertices : std::vector<Vertex> {
    using std::vector<Vertex>::vector;
    std::size_t reportedSize{};
    std::size_t size() const { return reportedSize ? reportedSize : std::vector<Vertex>::size(); }
};
struct SolidDrawFrame { std::shared_ptr<const Vertices> geometry{}; };
struct MeshWireDrawFrame { std::shared_ptr<const Vertices> depth_triangles{}, helper_lines{}; };
struct PublishedSceneFrame {
    Camera camera{};
    std::shared_ptr<const SolidDrawFrame> solids{};
    std::shared_ptr<const MeshWireDrawFrame> wire{};
    std::shared_ptr<const Vertices> lines{};
    std::vector<ObjectMarker> markers{};
    std::uint64_t revision{};
};
struct GridGeometry { Vertices vertices{}; std::vector<std::uint32_t> indices{}; };
inline auto legacySolids = std::make_shared<Vertices>(100000);
inline auto legacyLines = std::make_shared<Vertices>(6);
inline auto legacyWire = std::make_shared<MeshWireDrawFrame>();
inline auto legacyGrid = std::make_shared<GridGeometry>(GridGeometry{Vertices(3), {0,1,2}});
inline std::size_t legacyMarkers = 4;
inline bool legacyScreen = true;
inline std::uint64_t legacyRevision = 1;
auto object_solid_geometry_for(const void*) { return legacySolids; }
auto object_marker_geometry_for(const void*) { return legacyLines; }
auto object_mesh_wire_frame_for(const void*) { return legacyWire; }
std::shared_ptr<const GridGeometry> grid_geometry_for(const void*) { return legacyGrid; }
std::array<Vertex,8> look_marker_vertices_for(const void*) { return {}; }
std::size_t look_marker_vertex_count_for(const void*) { return legacyMarkers; }
Camera camera_for(const void*) { return {{0,0,8}}; }
std::uint64_t preview_geometry_revision_for(const void*) { return legacyRevision; }
std::vector<ObjectMarker> sampled_render_surface_markers_for(const void*) {
    return legacyScreen ? std::vector<ObjectMarker>{{}} : std::vector<ObjectMarker>{};
}
TRIANGLE_FACING
WIRE_LINES
SAMPLED_SURFACES
}
namespace epochengine::vulkancontext {
namespace previewgrid = epochengine::previewgrid;
namespace render_arcade = epochengine::render_arcade;
struct Vertex { std::array<float,3> position, color; std::array<float,2> uv; };
MESH_OWNER
inline std::shared_ptr<const previewgrid::PublishedSceneFrame> previewSceneFrame, uploadedPreviewSceneFrame;
inline std::uint64_t previewGeometryRevision{};
inline std::uint32_t indexCount{}, solidIndexCount{}, lineIndexCount{}, arcadeScreenIndexOffset{}, arcadeScreenIndexCount{};
inline unsigned uploads{};
inline unsigned indexUploads{};
inline bool indexBuffer{}, previewIndicesSequential{};
inline bool failIndices{};
const epochengine::core::Context* bound_context() { return nullptr; }
void createVertexBuffer() {
    VERTEX_BUFFER_ADMISSION
    ++uploads;
}
void createIndexBuffer() {
    INDEX_BUFFER_ADMISSION
    if (failIndices) throw std::runtime_error("fixture GPU upload failure");
    ++indexUploads;
    indexBuffer=true;
    INDEX_BUFFER_COMMIT
}
void upload_frame() {
    const epochengine::core::Context* ctx = nullptr;
    UPLOAD_DECISION
}
}
#define CHECK(value) do { if (!(value)) return __LINE__; } while (false)
int main() {
using namespace epochengine::vulkancontext;
using namespace epochengine::previewgrid;
auto indices = build_preview_indices(nullptr, {});
CHECK(indices.size() == 100019 && indices[65535] == 65535 && indices[65536] == 65536 && indices[99999] == 99999);
const std::array<std::uint32_t,6> screenIndices{100000,100001,100002,100000,100002,100003};
for (std::size_t i=0;i<6;++i) CHECK(indices[100000+i] == screenIndices[i]);
CHECK(indices[100006] == 100004 && indices[100008] == 100006 && indices.back() == 100016);
legacyScreen = false;
CHECK(build_preview_indices(nullptr, {}).size() == 100013);
legacySolids->reportedSize = (std::numeric_limits<std::uint32_t>::max)();
try { (void)build_preview_indices(nullptr, {}); return __LINE__; } catch (const std::length_error&) {}
legacySolids->reportedSize=0;
legacyGrid->indices = {3};
try { (void)build_preview_indices(nullptr, {}); return __LINE__; } catch (const std::out_of_range&) {}
legacyGrid->indices = {0,1,2};
auto empty = std::make_shared<const Vertices>();
auto solids = std::make_shared<SolidDrawFrame>();
solids->geometry = std::make_shared<const Vertices>(Vertices{{{0,0,0},{.5f,.25f,1}},{{1,0,0},{.5f,.25f,1}},{{0,1,0},{.5f,.25f,1}}});
auto wire = std::make_shared<MeshWireDrawFrame>(MeshWireDrawFrame{empty,empty});
auto frame = std::make_shared<PublishedSceneFrame>();
frame->solids=solids; frame->wire=wire; frame->lines=empty; frame->camera.eye={0,0,8}; frame->revision=1;
previewSceneFrame=frame; upload_frame(); CHECK(uploads == 1 && indexCount == 3);
CHECK(indexUploads==1 && previewIndicesSequential);
createIndexBuffer(); CHECK(indexUploads==1);
indexBuffer=false; createIndexBuffer(); CHECK(indexUploads==2);
const std::array<std::uint32_t,3> sequential{0,1,2},sameSizeDifferent{0,2,1};
CHECK(preview_indices_are_sequential(sequential));
CHECK(!preview_indices_are_sequential(sameSizeDifferent));
// A same-length nonsequential previous upload cannot borrow canonical identity.
previewIndicesSequential=false; createIndexBuffer(); CHECK(indexUploads==3 && previewIndicesSequential);
auto resizedFrame=std::make_shared<PublishedSceneFrame>(*frame);
resizedFrame->lines=std::make_shared<const Vertices>(2);
previewSceneFrame=resizedFrame; createIndexBuffer(); CHECK(indexUploads==4 && indexCount==5 && lineIndexCount==2);
previewSceneFrame=frame; createIndexBuffer(); CHECK(indexUploads==5 && indexCount==3);
auto packet = std::make_shared<PublishedSceneFrame>(*frame);
packet->revision=900; packet->camera.eye={12,3,-8}; packet->camera.target={4,2,7};
previewSceneFrame=packet; upload_frame(); CHECK(uploads == 1);
for (unsigned i=0;i<256;++i) {
    auto cameraPacket=std::make_shared<PublishedSceneFrame>(*frame);
    cameraPacket->revision=1000+i; cameraPacket->camera.eye={float(i),3,-8};
    previewSceneFrame=cameraPacket; upload_frame();
}
CHECK(uploads==1);
// Equal geometry wrappers do not change the baked vertex/index data.
packet->solids=std::make_shared<SolidDrawFrame>(*solids); packet->wire=std::make_shared<MeshWireDrawFrame>(*wire);
CHECK(preview_geometry_matches(*packet,*frame));
auto vertices = build_preview_vertices(nullptr,frame);
CHECK(vertices.size()==3 && vertices[0].color[0]==std::pow(.5f,2.2f));
// Repeated colors, clamped input and more distinct values than memo slots must
// remain bit-identical to the original conversion; collisions cannot reuse a
// different RGB value or a previous scene's colors.
auto colorGeometry=std::make_shared<Vertices>();
for(unsigned i=0;i<260;++i) {
    const Vec3 color{float(i%91)/83.0f-.1f,float(i%73)/69.0f,float(i%31)/23.0f};
    for(unsigned repeat=0;repeat<3;++repeat)colorGeometry->push_back({{float(i),0,0},color});
}
auto colorFrame=std::make_shared<PublishedSceneFrame>(*frame);
colorFrame->solids=std::make_shared<SolidDrawFrame>(SolidDrawFrame{colorGeometry});
const auto colored=build_preview_vertices(nullptr,colorFrame);
CHECK(colored.size()==colorGeometry->size());
for(std::size_t i=0;i<colored.size();++i) {
    const auto expected=preview_color_to_vulkan((*colorGeometry)[i].color);
    for(unsigned channel=0;channel<3;++channel)
        CHECK(std::bit_cast<std::uint32_t>(colored[i].color[channel])==std::bit_cast<std::uint32_t>(expected[channel]));
}
// A reference-lighting/color change creates a new immutable solid geometry.
auto relit=std::make_shared<SolidDrawFrame>(*solids);
auto relitVertices=std::make_shared<Vertices>(*solids->geometry); (*relitVertices)[0].color.x=.8f; relit->geometry=relitVertices;
packet->solids=relit; CHECK(!preview_geometry_matches(*packet,*frame));
previewSceneFrame=packet; upload_frame(); CHECK(uploads==2);
CHECK(indexUploads==5); // Color/reference-lighting changes preserve equal indices.
CHECK(build_preview_vertices(nullptr,packet)[0].color[0]==std::pow(.8f,2.2f));
// Same count with different index contents must upload. The legacy grid supplies
// nonsequential data; count equality cannot hide it.
const auto retainedFrame=previewSceneFrame;
legacySolids=std::make_shared<Vertices>(); legacyLines=std::make_shared<Vertices>();
legacyMarkers=0; legacyScreen=false; legacyGrid->vertices=Vertices(3);legacyGrid->indices={0,2,1};
previewSceneFrame.reset();const auto sameCountUploads=indexUploads;
createIndexBuffer(); CHECK(indexUploads==sameCountUploads+1 && indexCount==3 && !previewIndicesSequential);
previewSceneFrame=retainedFrame;createIndexBuffer();CHECK(previewIndicesSequential);
// A failed GPU upload must not commit the new geometry key.
auto changed=std::make_shared<PublishedSceneFrame>(*packet); changed->lines=std::make_shared<const Vertices>(2);
previewSceneFrame=changed; failIndices=true;
try { upload_frame(); return __LINE__; } catch(const std::runtime_error&) {}
CHECK(uploadedPreviewSceneFrame==packet); failIndices=false; upload_frame(); CHECK(uploadedPreviewSceneFrame==changed);
// Wire-only packets retain their triangles even with no legacy line buffer.
auto wireOnly=std::make_shared<PublishedSceneFrame>(*frame);
wireOnly->solids=std::make_shared<SolidDrawFrame>(SolidDrawFrame{empty});
auto triangleWire=std::make_shared<MeshWireDrawFrame>(); triangleWire->depth_triangles=solids->geometry; triangleWire->helper_lines=empty;
wireOnly->wire=triangleWire; wireOnly->camera.eye={0,0,-8};
CHECK(build_preview_vertices(nullptr,wireOnly).size()==6 && build_preview_indices(nullptr,wireOnly).size()==6);
previewSceneFrame=wireOnly; upload_frame(); CHECK(indexCount==6);
auto turned=std::make_shared<PublishedSceneFrame>(*wireOnly); turned->camera.target={9,8,7};
CHECK(preview_geometry_matches(*turned,*wireOnly));
unsigned wireUploads=uploads; previewSceneFrame=turned; upload_frame(); CHECK(uploads==wireUploads);
turned->camera.eye={0,0,8}; CHECK(!preview_geometry_matches(*turned,*wireOnly));
CHECK(build_preview_vertices(nullptr,turned).empty() && build_preview_indices(nullptr,turned).empty());
solidIndexCount=lineIndexCount=arcadeScreenIndexOffset=arcadeScreenIndexCount=99;
previewSceneFrame=turned; upload_frame(); CHECK(indexCount==0 && solidIndexCount==0 && lineIndexCount==0 && arcadeScreenIndexOffset==0 && arcadeScreenIndexCount==0);
CHECK(!previewIndicesSequential);
turned=std::make_shared<PublishedSceneFrame>(*wireOnly);
auto wireReplacement=std::make_shared<MeshWireDrawFrame>(*triangleWire); wireReplacement->depth_triangles=std::make_shared<const Vertices>(*solids->geometry);
turned->wire=wireReplacement; CHECK(!preview_geometry_matches(*turned,*wireOnly));
wireReplacement->depth_triangles=triangleWire->depth_triangles; wireReplacement->helper_lines=std::make_shared<const Vertices>(2);
CHECK(!preview_geometry_matches(*turned,*wireOnly));
// Sampled-screen camera visibility and baked transform both invalidate uploads.
auto sampled=std::make_shared<PublishedSceneFrame>(*frame);
sampled->markers.push_back({{0,0,0},{1,1,1},ObjectPreviewPrimitive::EngineArcadeScreen,true});
CHECK(!preview_geometry_matches(*sampled,*frame));
CHECK(build_preview_vertices(nullptr,sampled).size()==7 && build_preview_indices(nullptr,sampled).size()==9);
auto sampledMove=std::make_shared<PublishedSceneFrame>(*sampled); sampledMove->markers[0].position.x=2;
CHECK(!preview_geometry_matches(*sampledMove,*sampled));
sampledMove->markers=sampled->markers; sampledMove->markers[0].scale.x=2;
CHECK(!preview_geometry_matches(*sampledMove,*sampled));
sampledMove->markers=sampled->markers; sampledMove->camera.eye.z=-8;
CHECK(!preview_geometry_matches(*sampledMove,*sampled));
CHECK(build_preview_vertices(nullptr,sampledMove).size()==3 && build_preview_indices(nullptr,sampledMove).size()==3);
// Returning to the legacy path invalidates a retained packet, then uses revision.
previewSceneFrame.reset(); unsigned before=uploads; upload_frame(); CHECK(uploads==before+1);
upload_frame(); CHECK(uploads==before+1); ++legacyRevision; upload_frame(); CHECK(uploads==before+2);
return 0;
}
