"""Production tree attachment, bounded leaf rotation and cap winding, Debug/Release CPU proof."""
from pathlib import Path
import hashlib,json,os,re,subprocess,unittest
ROOT=Path(__file__).resolve().parents[2]
CODE=r'''

import std;
import forest.preview_mesh;
import forest.factory;
import authoring.morphology;
import asset.texture_artifact;
import project.lightning;
import scene.snapshot;
import scene.serializer;
import scene.runtime;
#define CHECK(x) do { if(!(x)) { std::ofstream("forest-junction-failure.txt") << __LINE__ << ": " << #x; std::exit(1); } } while(false)
namespace f=epochengine::forest;
namespace p=f::preview_mesh;
auto sub(p::Vector3 a,p::Vector3 b){return p::Vector3{a.x-b.x,a.y-b.y,a.z-b.z};}
auto cross(p::Vector3 a,p::Vector3 b){return p::Vector3{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float dot(p::Vector3 a,p::Vector3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float triangle_distance_sq(p::Vector3 point,p::Vector3 a,p::Vector3 b,p::Vector3 c){
 const auto distance=[&](p::Vector3 q){const auto delta=sub(point,q);return dot(delta,delta);};
 const auto along=[](p::Vector3 a,p::Vector3 delta,float t){return p::Vector3{a.x+delta.x*t,a.y+delta.y*t,a.z+delta.z*t};};
 const auto ab=sub(b,a),ac=sub(c,a),ap=sub(point,a);
 const float d1=dot(ab,ap),d2=dot(ac,ap);if(d1<=0&&d2<=0)return dot(ap,ap);
 const auto bp=sub(point,b);const float d3=dot(ab,bp),d4=dot(ac,bp);if(d3>=0&&d4<=d3)return dot(bp,bp);
 const float vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return distance(along(a,ab,d1/(d1-d3)));
 const auto cp=sub(point,c);const float d5=dot(ab,cp),d6=dot(ac,cp);if(d6>=0&&d5<=d6)return dot(cp,cp);
 const float vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return distance(along(a,ac,d2/(d2-d6)));
 const float va=d3*d6-d5*d4;if(va<=0&&d4-d3>=0&&d5-d6>=0)return distance(along(b,sub(c,b),(d4-d3)/((d4-d3)+(d5-d6))));
 const float inverse=1/(va+vb+vc);return distance(along(along(a,ab,vb*inverse),ac,vc*inverse));
}
namespace a=epochengine::asset::texture;
double sampled_alpha(const a::CompiledTextureArtifact& image, p::Vector2 uv)
{
    const auto& mip = image.mips.front();
    const double x = double(uv.x) * mip.width - 0.5;
    const double y = double(uv.y) * mip.height - 0.5;
    const auto left = static_cast<unsigned>(std::floor(x));
    const auto top = static_cast<unsigned>(std::floor(y));
    const auto alpha = [&](unsigned px, unsigned py) {
        return double(std::to_integer<unsigned>(mip.texels[
            std::size_t{py} * mip.row_pitch_bytes + px * 4u + 3u]));
    };
    const auto fractionX = x - left, fractionY = y - top;
    return std::lerp(std::lerp(alpha(left, top), alpha(left + 1u, top), fractionX),
        std::lerp(alpha(left, top + 1u), alpha(left + 1u, top + 1u), fractionX), fractionY);
}

a::CompiledTextureArtifact offcentre_leaf(bool separatedStems = false)
{
    a::CompiledTextureMip mip{.width=64u,.height=96u,.row_pitch_bytes=256u};
    mip.texels.resize(64u * 96u * 4u);
    for (unsigned y=0u; y<96u; ++y) for (unsigned x=0u; x<64u; ++x)
    {
        const double ellipse=(double(x)-31)*(double(x)-31)/400.0
            +(double(y)-35)*(double(y)-35)/625.0;
        const bool stem = (y>=56u && y<=86u
            && std::abs(double(x) - (8.0 + (86.0-y)*0.5)) <= 1.0)
            || (separatedStems && y>=80u && y<=86u && x>=50u && x<=52u);
        const auto offset=(std::size_t{y}*64u+x)*4u;
        mip.texels[offset]=std::byte{35};mip.texels[offset+1]=std::byte{160};mip.texels[offset+2]=std::byte{45};
        mip.texels[offset+3]=ellipse<=1.0 || stem ? std::byte{255}
            : ellipse<=1.15 ? std::byte{128} : std::byte{0};
    }
    mip.content=a::compiled_texture_mip_content(mip);
    a::CompiledTextureArtifact result;
    result.identity=a::build_compiled_texture_artifact_identity(
        {a::deterministic_content_hash("offcentre-stem",mip.texels),1u},
        {.format=a::ArtifactFormat::rgba8_srgb,.color_space=a::ColorSpace::srgb},64u,96u,1u);
    result.identity.compilation_required=false;
    result.mips.push_back(std::move(mip));
    result.payload_content=a::compiled_texture_payload_content(result.identity,result.mips);
    result.status=a::ArtifactCompilationStatus::ready;
    return result;
}

constexpr unsigned bark_source_width = 128u, bark_source_height = 192u;
std::array<unsigned, 3> bark_color(unsigned x, unsigned y)
{
    // The edges both contain real grain, so margin detection keeps the full
    // width. The x and y ramps expose both seam discontinuities.
    return {24u + 112u*x/(bark_source_width-1u) + 84u*y/(bark_source_height-1u),
        30u + 90u*x/(bark_source_width-1u) + 74u*y/(bark_source_height-1u),
        18u + 48u*x/(bark_source_width-1u) + 58u*y/(bark_source_height-1u)};
}

a::CompiledTextureArtifact full_frame_bark()
{
    a::CompiledTextureMip mip{.width=bark_source_width,.height=bark_source_height,
        .row_pitch_bytes=bark_source_width*4u};
    mip.texels.resize(bark_source_width*bark_source_height*4u);
    for (unsigned y=0;y<mip.height;++y)
        for (unsigned x=0;x<mip.width;++x)
        {
            const auto color=bark_color(x,y);
            const auto offset=std::size_t{y}*mip.row_pitch_bytes+x*4u;
            for (unsigned c=0;c<3;++c) mip.texels[offset+c]=static_cast<std::byte>(color[c]);
            mip.texels[offset+3u]=std::byte{255};
        }
    mip.content=a::compiled_texture_mip_content(mip);
    a::CompiledTextureArtifact result;
    result.identity=a::build_compiled_texture_artifact_identity(
        {a::deterministic_content_hash("full-frame-bark",mip.texels),1u},
        {.format=a::ArtifactFormat::rgba8_srgb,.color_space=a::ColorSpace::srgb},
        mip.width,mip.height,1u);
    result.identity.compilation_required=false;
    result.mips.push_back(std::move(mip));
    result.payload_content=a::compiled_texture_payload_content(result.identity,result.mips);
    result.status=a::ArtifactCompilationStatus::ready;
    return result;
}

int periodic_bark_contract()
{
    const auto bark=full_frame_bark();
    CHECK(a::validate_compiled_artifact(bark));
    const auto leaf=offcentre_leaf();
    const auto leafOnly=p::compose_material_atlas(nullptr,&leaf,0,{1,1,1},{1,1,1});
    const auto combined=p::compose_material_atlas(&bark,&leaf,0,{1,1,1},{1,1,1});
    CHECK(leafOnly&&combined&&a::validate_compiled_artifact(combined.artifact));
    const auto& atlas=combined.artifact.mips.front();
    const auto& unmodifiedLeaves=leafOnly.artifact.mips.front();
    CHECK(atlas.width==1024u&&atlas.height==512u);
    const auto channel=[&](unsigned x,unsigned y,unsigned c) {
        return std::to_integer<unsigned>(atlas.texels[
            std::size_t{y}*atlas.row_pitch_bytes+x*4u+c]);
    };
    for (unsigned c=0;c<3;++c)
    {
        // Left/right bark close along a cylinder; top/bottom repeat through
        // the same texture sampler used by a placed tree.
        for (unsigned y=0;y<atlas.height;++y)
            CHECK(channel(18u,y,c)==channel(494u,y,c));
        for (unsigned x=0;x<512u;++x)
            CHECK(channel(x,0,c)==channel(x,511u,c));
        // Away from the feather, the authored grain, aspect mapping and
        // exact source channel bytes survive on both sides of the tile.
        for (const auto& xy : {std::array{90u,60u},std::array{300u,280u},std::array{400u,440u}})
        {
            const auto x=(xy[0]-18u)*(bark_source_width-1u)/476u;
            const auto y=(xy[1]*bark_source_width/476u)%bark_source_height;
            CHECK(channel(xy[0],xy[1],c)==bark_color(x,y)[c]);
        }
    }
    for (unsigned y=0;y<atlas.height;++y)
    {
        const auto start=std::size_t{y}*atlas.row_pitch_bytes+512u*4u;
        CHECK(std::equal(atlas.texels.begin()+start,atlas.texels.begin()+start+512u*4u,
            unmodifiedLeaves.texels.begin()+start));
    }
    const auto repeated=p::compose_material_atlas(&bark,&leaf,0,{1,1,1},{1,1,1});
    CHECK(repeated&&repeated.artifact.identity.key==combined.artifact.identity.key);
    return 0;
}

int visible_stem_contract()
{
    const auto& oak=p::material_atlas();
    CHECK(a::validate_compiled_artifact(oak));
    const auto source=offcentre_leaf();
    CHECK(a::validate_compiled_artifact(source));
    const auto custom=p::compose_material_atlas(nullptr,&source,0,{1,1,1},{1,1,1});
    CHECK(custom);
    CHECK(custom.artifact.identity.key==p::compose_material_atlas(nullptr,&source,0,{1,1,1},{1,1,1}).artifact.identity.key);
    const auto separatedSource=offcentre_leaf(true);
    const auto separated=p::compose_material_atlas(nullptr,&separatedSource,0,{1,1,1},{1,1,1});
    CHECK(separated);

    for (const auto* texture : {&oak,&custom.artifact,&separated.artifact})
    {
        CHECK(sampled_alpha(*texture,p::material_uv(p::MaterialSlot::foliage,{.5f,0}))>200.0);
        if (texture != &separated.artifact)
        {
            CHECK(sampled_alpha(*texture,p::material_uv(p::MaterialSlot::foliage,{0,0}))==0.0);
            CHECK(sampled_alpha(*texture,p::material_uv(p::MaterialSlot::foliage,{1,0}))==0.0);
        }
        for (float size : {.02f,.04f,.28f,.59f,2.f})
        {
            p::Geometry geometry;geometry.segmentCount=1u;geometry.leafCount=1u;
            geometry.segments[0]={{0,0,0},{0,1,0},.1f,.1f,0,17};
            geometry.leaves[0]={.position={.1f,.55f,0},.outward={0,0,1},.up={1,0,0},.size=size,.sourceSegment=17};
            const auto mesh=p::build(geometry);CHECK(mesh.valid);
            const auto& petiole=mesh.vertices[mesh.vertices.size()-12u];
            // Visible material at the exact attached vertex, at every leaf size.
            CHECK(std::abs(petiole.position.x-.098f)<.001f);
            CHECK(std::abs(petiole.position.y-.55f)<.001f);
            CHECK(std::abs(petiole.position.z)<.001f);
            CHECK(sampled_alpha(*texture,p::material_uv(petiole.material,petiole.uv))>200.0);
        }
    }
    bool semitransparent{}, opaque{}, transparent{};
    const auto& mip=custom.artifact.mips.front();
    for (unsigned y=18u;y<=494u;++y) for(unsigned x=530u;x<=1006u;++x)
    {
        const auto alpha=std::to_integer<unsigned>(mip.texels[std::size_t{y}*mip.row_pitch_bytes+x*4u+3u]);
        semitransparent|=alpha==128u;opaque|=alpha==255u;transparent|=alpha==0u;
    }
    CHECK(semitransparent&&opaque&&transparent);
    return 0;
}

int lightning_scene_contract()
{
    namespace lightning = epochengine::project_lightning;
    namespace scene = epochengine::scene;
    scene::SceneSnapshot authored{.scene_id = "project:storm", .project_id = "storm", .world_name = "Storm"};
    const auto id = scene::stable_scene_object_id(authored.scene_id, "Lightning_01");
    const auto camera = scene::stable_scene_object_id(authored.scene_id, "Camera");
    const auto spawn = scene::stable_scene_object_id(authored.scene_id, "Spawn");
    authored.primary_camera = camera;
    authored.primary_spawn = spawn;
    authored.objects.push_back({.id = id, .name = "Lightning_01", .type = std::string{lightning::scene_type},
        .category = "World", .position = {2.0f, 6.0f, -3.0f}, .rotation = {10.0f, 15.0f, 20.0f},
        .scale = {1.1f, 1.2f, 1.3f}, .position_anchor = std::array{5200.0, 100.0, -250.0}});
    authored.objects.push_back({.id = camera, .name = "Camera", .type = "Camera", .category = "World"});
    authored.objects.push_back({.id = spawn, .name = "Spawn", .type = "Spawn", .category = "World"});
    const auto reopened = scene::parse_snapshot_text(scene::serialize_snapshot_text(authored));
    CHECK(reopened && reopened.snapshot.objects.size() == authored.objects.size());
    epochengine::scene_runtime::SceneRuntime playing;
    CHECK(playing.replace(reopened.snapshot).committed());
    const auto* live = playing.projection()->find(id);
    CHECK(live && live->source_id == id && live->type == lightning::scene_type && live->visible());
    CHECK(live->transform.position == authored.objects.front().position
        && live->transform.rotation == authored.objects.front().rotation
        && live->transform.scale == authored.objects.front().scale
        && live->position_anchor == authored.objects.front().position_anchor && !live->mesh_asset);

    lightning::PreviewCache editor, play;
    const auto peak = editor.sample(id, 0.0);
    CHECK(peak && peak.intensity == 1.0f && peak.mesh->vertices().size() <= lightning::mesh_limits.maximum_vertices);
    CHECK(peak.mesh->indices().size() <= lightning::mesh_limits.maximum_indices);
    CHECK(editor.sample(id, 0.0).mesh == peak.mesh);
    const auto fading = editor.sample(id, 0.13);
    CHECK(fading && fading.intensity > 0.0f && fading.intensity < peak.intensity && fading.mesh == peak.mesh);
    CHECK(fading.color()[2] < peak.color()[2]);
    CHECK(!editor.sample(id, 0.4) && !editor.sample(id, -1.0) && !editor.sample(0u, 0.0));
    CHECK(!editor.sample(id, std::numeric_limits<double>::infinity()));
    CHECK(editor.sample(id, lightning::repeat_seconds).mesh == peak.mesh);
    const double propagating_time = lightning::repeat_seconds - 0.05;
    const auto advancing = editor.sample(id, propagating_time);
    CHECK(advancing && advancing.mesh->indices().size() < peak.mesh->indices().size());
    CHECK(editor.sample(id, 0.0).mesh->key() == peak.mesh->key());
    const auto rewind = editor.sample(id, propagating_time);
    CHECK(rewind && rewind.intensity == advancing.intensity && rewind.mesh->key() == advancing.mesh->key());
    const auto replay = play.sample(live->source_id, propagating_time);
    CHECK(replay && replay.mesh->key() == advancing.mesh->key());
    const auto other = play.sample(live->source_id + 1u, propagating_time);
    CHECK(other && other.mesh->key() != replay.mesh->key());

    namespace morphology = epochengine::authoring::morphology;
    auto recipe = morphology::default_recipe(morphology::Domain::electrical);
    recipe.growth.seed = id;
    const auto graph = morphology::build_graph(recipe);
    bool has_sharp_spatial_turn = false;
    for (const auto& segment : graph.segments) {
        if (segment.depth < 2u) continue;
        const auto parent = std::find_if(graph.segments.begin(), graph.segments.end(),
            [&](const auto& candidate) { return candidate.child_node == segment.parent_node; });
        CHECK(parent != graph.segments.end());
        const auto a = parent->tangent_end;
        const auto b = segment.tangent_end;
        const float alignment = a.x*b.x + a.y*b.y + a.z*b.z;
        has_sharp_spatial_turn |= alignment < 0.75f && a.z*b.z < -0.12f;
    }
    CHECK(has_sharp_spatial_turn);
    return 0;
}

int main(){
 CHECK(periodic_bark_contract()==0);
 CHECK(visible_stem_contract()==0);
 CHECK(lightning_scene_contract()==0);
 CHECK(p::run_contract().passed());
 namespace morph=epochengine::authoring::morphology;
 auto recipe=morph::default_recipe(morph::Domain::electrical);
 auto graph=morph::build_graph(recipe);
 auto strike=morph::sample_discharge(graph,.10);
 CHECK(strike.valid&&strike.intensity==1&&strike.geometry.terminals.empty());
 CHECK(strike.geometry.segments.size()==graph.segments.size());
 CHECK(p::extract(graph,morph::sample(graph,recipe.duration_seconds)).leafCount==0);
 auto leaflessMesh=p::build(graph,strike.geometry);
 CHECK(leaflessMesh.valid&&leaflessMesh.leafCardCount==0);
 CHECK(morph::sample_discharge(graph,-1).valid&&morph::sample_discharge(graph,-1).geometry.segments.empty());
 CHECK(morph::sample_discharge(graph,1).geometry.segments.empty());
 CHECK(morph::sample_discharge(graph,.2).intensity<strike.intensity);
 auto early=morph::sample_discharge(graph,.025);auto late=morph::sample_discharge(graph,.2);
 auto rewind=morph::sample_discharge(graph,.025);
 CHECK(early.geometry.segments.size()==rewind.geometry.segments.size()&&early.intensity==rewind.intensity);
 for(std::size_t i=0;i<early.geometry.segments.size();++i){auto a=early.geometry.segments[i],b=rewind.geometry.segments[i];
  CHECK(a.end.x==b.end.x&&a.end.y==b.end.y&&a.end.z==b.end.z&&a.progress==b.progress);}
 CHECK(graph.content_hash==morph::build_graph(recipe).content_hash);
 ++recipe.growth.seed;CHECK(graph.content_hash!=morph::build_graph(recipe).content_hash);
 CHECK(!morph::sample_discharge(graph,std::numeric_limits<double>::quiet_NaN()).valid);
 CHECK(!morph::sample_discharge(graph,.1,{.propagation_seconds=0}).valid);
 CHECK(!morph::sample_discharge(graph,.1,{.hold_seconds=-1}).valid);
 CHECK(!morph::sample_discharge(graph,.1,{.decay_seconds=std::numeric_limits<double>::infinity()}).valid);
 CHECK(!morph::sample_discharge(morph::build_graph(morph::default_recipe(morph::Domain::plant)),.1).valid);
 auto shifted=morph::sample_discharge(graph,100.1,{.start_seconds=100});
 CHECK(shifted.geometry.segments.size()==strike.geometry.segments.size());
 p::Geometry g;g.segmentCount=3;
 g.segments[0]={{0,0,0},{0,1,0},.3f,.2f};
 g.segments[1]={{0,1,0},{.7f,1.8f,0},.14f,.05f};
 g.segments[2]={{0,1,0},{-.7f,1.8f,0},.14f,.05f};
 auto m=p::build(g);CHECK(m.valid&&m.vertices.size()==117&&m.indices.size()==408);
 CHECK(m.rootCaps==1&&m.terminalCaps==2&&m.overlappingJunctions==2);
 CHECK(m.contentSignature==p::build(g).contentSignature);
 for(unsigned s=0;s<3;++s){
  const unsigned base=s?27+(s-1)*45:0;
  const unsigned indexBase=s?72+(s-1)*168:0, indexCount=s?168:72;
  auto center=[&](unsigned ring){p::Vector3 c{};for(unsigned j=0;j<8;++j){auto v=m.vertices[base+ring+j].position;c.x+=v.x/8;c.y+=v.y/8;c.z+=v.z/8;}return c;};
  auto begin=center(0),end=center(9);
  auto rootDelta=sub(m.vertices[base].position,begin), tipDelta=sub(m.vertices[base+9].position,end);
  const float rootRadius=std::sqrt(dot(rootDelta,rootDelta)), tipRadius=std::sqrt(dot(tipDelta,tipDelta));
  CHECK(tipRadius<rootRadius);
  CHECK(std::abs(tipRadius/rootRadius-g.segments[s].radiusEnd/g.segments[s].radiusStart)<1e-5f);
  CHECK(std::abs(end.x-g.segments[s].end.x)<1e-6f&&std::abs(end.y-g.segments[s].end.y)<1e-6f);
  if(s){CHECK(begin.y<1&&begin.y>0);for(unsigned i=0;i<9;++i){auto v=m.vertices[base+i].position;CHECK(v.x*v.x+v.z*v.z<.2f*.2f&&v.y<1&&v.y>0);}}
  for(unsigned t=0;t<indexCount;t+=3){auto i=indexBase+t;auto a=m.vertices[m.indices[i]].position,b=m.vertices[m.indices[i+1]].position,c=m.vertices[m.indices[i+2]].position;
   auto n=cross(sub(b,a),sub(c,a));CHECK(dot(n,n)>1e-12f);
   if(t>=indexCount-24)CHECK(dot(n,sub(end,begin))*(s?1:-1)>0);
  }
  // Exactly one external cap per segment here; no internal disk triangles.
  for(unsigned i=indexBase;i<indexBase+indexCount;++i)CHECK(m.indices[i]>=base&&m.indices[i]<base+(s?45:27));
  const unsigned cap=base+(s?36:18);
  auto capCenter=m.vertices[cap].position;
  if(s){
   auto direction=sub(end,begin);float length=std::sqrt(dot(direction,direction));
   auto delta=sub(capCenter,end);
   CHECK(std::abs(dot(delta,direction)/length-tipRadius*1.33f)<1e-6f);
   auto radius=sub(m.vertices[cap+1].position,capCenter);
   CHECK(std::abs(std::sqrt(dot(radius,radius))-tipRadius*.12f)<1e-6f);
  }
  for(unsigned j=1;j<=8;++j){auto v=m.vertices[cap+j];
   CHECK(std::abs(dot(v.normal,m.vertices[cap].normal)-1)<1e-6f);
   float u=v.uv.x-.5f,w=v.uv.y-.5f;CHECK(std::abs(u*u+w*w-.25f)<1e-6f);
  }
 }
 // Regression: .72 authored taper must survive .60 junction fitting.
 auto widening=g;widening.segments[1].radiusStart=.2f;widening.segments[1].radiusEnd=.144f;
 auto fitted=p::build(widening);CHECK(fitted.valid&&fitted.overlappingJunctions==2);
 auto ringRadius=[&](unsigned base){p::Vector3 c{};for(unsigned j=0;j<8;++j){auto v=fitted.vertices[base+j].position;c.x+=v.x/8;c.y+=v.y/8;c.z+=v.z/8;}auto d=sub(fitted.vertices[base].position,c);return std::sqrt(dot(d,d));};
 CHECK(std::abs(ringRadius(27)-.12f)<1e-6f);
 CHECK(std::abs(ringRadius(36)-.0864f)<1e-6f);
 CHECK(ringRadius(36)<ringRadius(27));
 // A child is fitted narrower and inset into its parent. Leaf stems must
 // intersect those rendered faces rather than float at the authored radius.
 auto attachment=g;
 for(unsigned i=0;i<3;++i)attachment.segments[i].sourceSegment=i;
 const auto wood=p::build(attachment);CHECK(wood.valid);
 auto point=[](epochengine::voxel::Float3 v){return p::Vector3{v.x,v.y,v.z};};
 const auto rawAxis=sub(point(attachment.segments[1].end),point(attachment.segments[1].start));
 const float rawLength=std::sqrt(dot(rawAxis,rawAxis));
 const p::Vector3 radial{rawAxis.y/rawLength,-rawAxis.x/rawLength,0};
 const float at=.55f, rawRadius=.14f*(1-at)+.05f*at;
 const auto authored=p::Vector3{rawAxis.x*at+radial.x*rawRadius,
  1+rawAxis.y*at+radial.y*rawRadius,0};
 attachment.leafCount=1;
 attachment.leaves[0]={.position={authored.x,authored.y,authored.z},.outward={0,0,1},
  .up={radial.x,radial.y,0},.size=.02f,.sourceSegment=1};
 p::Vector3 previous{};
 for(float size:{.02f,.04f,.28f,.59f,2.f}) {
  attachment.leaves[0].size=size;
  auto visible=p::build(attachment);CHECK(visible.valid&&visible.leafCardCount==1);
  const auto base=visible.vertices[wood.vertices.size()].position;
  if(size!=.02f)CHECK(std::sqrt(dot(sub(base,previous),sub(base,previous)))<1e-6f);
  previous=base;CHECK(std::sqrt(dot(sub(base,authored),sub(base,authored)))>.002f);
  p::Vector3 first{},last{};
  for(unsigned side=0;side<8;++side){auto a=wood.vertices[27+side].position,b=wood.vertices[36+side].position;
   first.x+=a.x/8;first.y+=a.y/8;first.z+=a.z/8;last.x+=b.x/8;last.y+=b.y/8;last.z+=b.z/8;}
  const auto axis=sub(last,first);const auto length=std::sqrt(dot(axis,axis));
  const p::Vector3 tangent{axis.x/length,axis.y/length,axis.z/length};
  const float t=dot(sub(base,first),axis)/dot(axis,axis);
  float nearest=-std::numeric_limits<float>::infinity();
  for(unsigned side=0;side<8;++side){
   auto interpolate=[&](unsigned side){auto a=wood.vertices[27+side].position,b=wood.vertices[36+side].position;
    return p::Vector3{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};};
   const auto a=interpolate(side),b=interpolate(side+1);
   auto n=cross(sub(b,a),tangent);const auto normalLength=std::sqrt(dot(n,n));
   const auto distance=dot(n,sub(base,a))/normalLength;
   CHECK(distance<=1e-6f);nearest=std::max(nearest,distance);
  }
  CHECK(nearest>-.005f); // Petiole starts just inside an actual branch face.
 }
 auto bad=g;bad.segments[0].radiusEnd=std::numeric_limits<float>::quiet_NaN();CHECK(!p::build(bad).valid);
 auto document=f::make_default_plant_lab_document();
 CHECK(document.journal.size()==4&&document.profile.branch.lengthReferenceMeters==0&&document.profile.branch.branchLengthMeters==.92f);
 auto compiled=f::compile_forest_asset(document);CHECK(compiled.valid);
 CHECK(p::build(compiled.graph,compiled.sample).valid);

 // Either legacy-zero control activates canopy with one reversible edit.
 for(auto property:{f::ForestProfileProperty::LeafSizeMeters,f::ForestProfileProperty::LeafSitesPerBranch}) {
  auto legacy=f::make_forest_asset_document("Legacy");
  const auto before=f::build_morphology_graph(legacy.profile).content_hash;
  auto resolved=f::forest_canopy_settings(legacy.profile);
  CHECK(!resolved.enabled&&resolved.leafSizeMeters==.28f&&resolved.leafSitesPerBranch==6u);
  CHECK(f::apply_forest_profile_edit(legacy,property,property==f::ForestProfileProperty::LeafSizeMeters?.305f:7.f));
  resolved=f::forest_canopy_settings(legacy.profile);CHECK(resolved.enabled);
  auto active=f::build_morphology_graph(legacy.profile);CHECK(active.content_hash!=before&&active.terminals.size()>16);
  CHECK(f::undo_forest_profile_edit(legacy));CHECK(!f::forest_canopy_settings(legacy.profile).enabled);
  CHECK(f::build_morphology_graph(legacy.profile).content_hash==before);
  CHECK(f::redo_forest_profile_edit(legacy));CHECK(f::build_morphology_graph(legacy.profile).content_hash==active.content_hash);
 }
 // Leaves begin at their branch attachment at every growth size.
 CHECK(p::material_uv(p::MaterialSlot::foliage,{.5f,0}).y > p::material_uv(p::MaterialSlot::foliage,{.5f,1}).y);
 for(float size : {.04f,.28f}) {
  p::Geometry leaves;leaves.leafCount=1;
  leaves.leaves[0]={.position={2,3,4},.outward={0,0,1},.up={0,1,0},.size=size};
  auto blade=p::build(leaves);CHECK(blade.valid);
  CHECK(blade.vertices[0].position.x==2&&blade.vertices[0].position.y==3&&blade.vertices[0].position.z==4);
  for(const auto& v:blade.vertices)CHECK(v.position.y>=3&&v.position.z==4);
 }
 auto canopy=compiled.graph;
 CHECK(!canopy.terminals.empty());
 // Leaf roll is stable for the saved recipe, changes both ways, and cannot
 // rotate the petiole or grow/shrink the blade. Strict 2D cards stay planar.
 const auto repeated=f::build_morphology_graph(document.profile);
 CHECK(canopy.content_hash==repeated.content_hash&&morph::validate(canopy));
 unsigned rolledPositive=0,rolledNegative=0;
 const auto same=[](auto a,auto b){return a.x==b.x&&a.y==b.y&&a.z==b.z;};
 for(std::size_t i=0;i<canopy.terminals.size();++i){
  const auto& leaf=canopy.terminals[i];const auto& again=repeated.terminals[i];
  CHECK(same(leaf.position,again.position)&&same(leaf.up,again.up)&&same(leaf.outward,again.outward));
  CHECK(leaf.scale_meters==again.scale_meters);
  p::Vector3 up{leaf.up.x,leaf.up.y,leaf.up.z},normal{leaf.outward.x,leaf.outward.y,leaf.outward.z};
  CHECK(std::isfinite(dot(up,normal))&&std::abs(dot(up,normal))<1e-5f);
  CHECK(std::abs(dot(up,up)-1)<1e-5f&&std::abs(dot(normal,normal)-1)<1e-5f);
  auto branch=std::find_if(canopy.segments.begin(),canopy.segments.end(),[&](const auto& s){return s.child_node==leaf.node;});
  CHECK(branch!=canopy.segments.end());
  auto axis=sub({branch->end.x,branch->end.y,branch->end.z},{branch->start.x,branch->start.y,branch->start.z});
  float length=std::sqrt(dot(axis,axis));axis={axis.x/length,axis.y/length,axis.z/length};
  float projection=dot(axis,up);auto original=sub(axis,{up.x*projection,up.y*projection,up.z*projection});
  length=std::sqrt(dot(original,original));original={original.x/length,original.y/length,original.z/length};
  CHECK(dot(original,normal)>=std::cos(35.f*.0174532925199433f)-1e-5f);
  const float turn=dot(cross(up,original),normal);
  rolledPositive+=turn>.1f;rolledNegative+=turn<-.1f;
 }
 CHECK(rolledPositive>canopy.terminals.size()/5&&rolledNegative>canopy.terminals.size()/5);
 auto differentSeed=document.profile;++differentSeed.seed.value;
 CHECK(f::build_morphology_graph(differentSeed).content_hash!=canopy.content_hash);
 for(const auto& terminal:canopy.terminals){
  auto it=std::find_if(canopy.segments.begin(),canopy.segments.end(),[&](const auto& segment){return segment.child_node==terminal.node;});
  CHECK(it!=canopy.segments.end());
  p::Vector3 axis{it->end.x-it->start.x,it->end.y-it->start.y,it->end.z-it->start.z};
  p::Vector3 relative{terminal.position.x-it->start.x,terminal.position.y-it->start.y,terminal.position.z-it->start.z};
  float t=dot(relative,axis)/dot(axis,axis);
  auto radial=sub(relative,{axis.x*t,axis.y*t,axis.z*t});
  float radius=it->radius_start_meters*(1-t)+it->radius_end_meters*t;
  CHECK(std::abs(std::sqrt(dot(radial,radial))-radius)<1e-5f);
  CHECK(std::abs(terminal.up.x*terminal.outward.x+terminal.up.y*terminal.outward.y+terminal.up.z*terminal.outward.z)<1e-5f);
  CHECK(radial.x*terminal.up.x+radial.y*terminal.up.y+radial.z*terminal.up.z>0);
 }
 // Real sampled crowns stay connected throughout growth in 2D and 3D.
 for(auto mode:{f::ForestPreviewMode::Mode2D,f::ForestPreviewMode::Mode3D})for(float size:{.02f,.59f}){
  auto plant=f::make_default_plant_lab_document();plant.profile.previewMode=mode;plant.profile.leafSizeMeters=size;
  const auto grown=f::build_morphology_graph(plant.profile);
  if(mode==f::ForestPreviewMode::Mode2D)for(const auto& leaf:grown.terminals)
   CHECK(leaf.position.z==0&&leaf.up.z==0&&leaf.outward.x==0&&leaf.outward.y==0&&leaf.outward.z==1);
  for(float fraction:{.35f,.65f,1.f}){
   const auto sample=morph::sample(grown,grown.recipe.duration_seconds*fraction);
   auto geometry=p::extract(grown,sample);if(geometry.leafCount==0)continue;
   auto branches=geometry;branches.leafCount=0;
   const auto woodMesh=p::build(branches),plantMesh=p::build(geometry);CHECK(woodMesh.valid&&plantMesh.valid);
   for(std::size_t leaf=0;leaf<geometry.leafCount;++leaf){
    const auto base=plantMesh.vertices[woodMesh.vertices.size()+leaf*12].position;
    float closest=std::numeric_limits<float>::infinity();
    for(std::size_t index=0;index<woodMesh.indices.size();index+=3)
     closest=std::min(closest,triangle_distance_sq(base,woodMesh.vertices[woodMesh.indices[index]].position,
      woodMesh.vertices[woodMesh.indices[index+1]].position,woodMesh.vertices[woodMesh.indices[index+2]].position));
    CHECK(closest<.0001f);
   }
  }
 }

}

'''
class ForestJunctions(unittest.TestCase):
 def test_overlapping_junction_geometry(self):
  if os.name != "nt": self.skipTest("Installed MSVC CPU module lane")
  material_owner=ROOT/"Engine/src/forest/forest.preview_mesh.cpp"
  lightning_owner=ROOT/"Engine/modules/project/project.lightning.ixx"
  identity=hashlib.sha256(CODE.encode()+material_owner.read_bytes()+lightning_owner.read_bytes()).hexdigest()[:16]
  proof=ROOT/"Artifacts/Reports/forest-bark-repeat-20261116"/identity;proof.mkdir(parents=True,exist_ok=True)
  receipt=proof/"result.json";receipt.write_text(json.dumps({"pass":False,"status":"running"})+"\n")
  build=ROOT/"Artifacts/Intermediate/tests/forest-bark-repeat"/identity
  source=proof/"forest.junction_contract.cpp";source.write_text(CODE,encoding="utf-8")
  roots=[ROOT/"Engine/modules",ROOT/"Engine/dep/EpochPlatformEngine/modules"]
  found={}
  for folder in roots:
   for path in folder.rglob("*.ixx"):
    match=re.search(r"^export module\s+([\w.:]+);",path.read_text(encoding="utf-8-sig"),re.M)
    if match: found[match.group(1)]=path
  implementations=[ROOT/p for p in (
   'Engine/src/forest/forest.preview_mesh.cpp', 'Engine/src/assets/texture/asset.texture_artifact.cpp',
   'Engine/src/assets/texture/asset.texture_import.cpp','Engine/src/assets/image.stb.cpp',
   'Engine/src/platform/core.path.cpp','Engine/dep/EpochPlatformEngine/src/core/core.log.cpp',
   'Engine/dep/EpochPlatformEngine/src/core/core.time.cpp')]
  selected={};pending=["forest.preview_mesh","forest.factory","project.lightning","scene.serializer","scene.runtime"]
  for path in implementations:
   pending+=re.findall(r"^(?:export )?(?:import|module)\s+([\w.]+);",path.read_text(encoding="utf-8-sig"),re.M)
  while pending:
   name=pending.pop()
   if name in selected or name in ("std","std.compat"):continue
   self.assertIn(name,found);selected[name]=found[name]
   pending+=re.findall(r"^(?:export )?import\s+([\w.]+);",selected[name].read_text(encoding="utf-8-sig"),re.M)
  paths=lambda ps:" ".join('"'+p.as_posix()+'"' for p in ps)
  (proof/"CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(ForestJunctions LANGUAGES CXX)
add_executable(anchor "{source.as_posix()}" {paths(implementations)})
target_compile_features(anchor PRIVATE cxx_std_23)
target_link_libraries(anchor PRIVATE shell32)
target_compile_options(anchor PRIVATE /W3 /WX /utf-8 /Z7)
target_include_directories(anchor PRIVATE "{(ROOT/'Engine/include').as_posix()}" "{(ROOT/'Engine/dep/EpochPlatformEngine/include').as_posix()}")
target_sources(anchor PRIVATE FILE_SET CXX_MODULES BASE_DIRS {paths(roots)} FILES {paths(selected.values())})
enable_testing()
add_test(NAME actual_scene_anchor COMMAND anchor "{(proof/'fixture').as_posix()}/$<CONFIG>")
''',encoding="utf-8")
  commands=[["cmake","-S",str(proof),"-B",str(build),"-G","Visual Studio 17 2022","-A","x64"]]
  for config in ("Debug","Release"):
   commands += [["cmake","--build",str(build),"--config",config,"--parallel","1"],["ctest","--test-dir",str(build),"-C",config,"--output-on-failure"]]
  scratch=proof/"tmp";scratch.mkdir(exist_ok=True)
  env=dict(os.environ,EPOCH_ASSET_ROOT=str(ROOT/"Engine/resource"),TEMP=str(scratch),TMP=str(scratch),TMPDIR=str(scratch))
  results=[]
  for index,command in enumerate(commands):
   completed=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,timeout=180)
   (proof/f"lane-{index}.log").write_text(completed.stdout+completed.stderr,encoding="utf-8")
   results.append({"command":command,"exit_code":completed.returncode})
   receipt.write_text(json.dumps({"pass":False,"status":"failed" if completed.returncode else "running","runs":results},indent=2)+"\n")
   self.assertEqual(completed.returncode,0,completed.stdout[-8500:]+completed.stderr)
   if command[0]=="ctest":
    empty_assets=proof/"procedural-only"
    empty_assets.mkdir(exist_ok=True)
    fallback=subprocess.run(command,cwd=ROOT,env=dict(env,EPOCH_ASSET_ROOT=str(empty_assets)),
                            capture_output=True,text=True,timeout=180)
    (proof/f"lane-{index}-procedural.log").write_text(fallback.stdout+fallback.stderr,encoding="utf-8")
    results.append({"command":command,"asset_root":str(empty_assets),"exit_code":fallback.returncode})
    self.assertEqual(fallback.returncode,0,fallback.stdout[-8500:]+fallback.stderr)
  receipt.write_text(json.dumps({"pass":True,"scope":"Production forest geometry and material checks, and the normal lightning scene save/reopen and runtime projection with bounded pulse geometry, seed replay, rewind, pause and decay. Native renderer and Editor acceptance remain open.","runs":results,"sources":{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [material_owner,*selected.values()]}},indent=2)+"\n")

if __name__ == "__main__":unittest.main()

