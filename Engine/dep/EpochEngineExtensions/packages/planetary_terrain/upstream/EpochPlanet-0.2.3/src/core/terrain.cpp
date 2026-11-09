#include "epoch/planet/terrain.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <optional>

namespace epoch::planet {
std::size_t TileHash::operator()(TileKey k) const noexcept {
    std::uint64_t h=(static_cast<std::uint64_t>(k.x)<<32)|k.y;
    h^=static_cast<std::uint64_t>(k.body)*0x9e3779b97f4a7c15ull;
    h^=static_cast<std::uint64_t>(k.face)*0xbf58476d1ce4e5b9ull;
    h^=static_cast<std::uint64_t>(k.level)*0x94d049bb133111ebull;
    h=(h^(h>>30))*0xbf58476d1ce4e5b9ull;h=(h^(h>>27))*0x94d049bb133111ebull;
    return static_cast<std::size_t>(h^(h>>31));
}
std::array<TileKey,4> children(TileKey k) {
    if(k.level>=20||k.face>=6||k.x>=(1u<<k.level)||k.y>=(1u<<k.level)) throw std::invalid_argument("Tile level exceeds supported precision/budget");
    const auto l=static_cast<std::uint8_t>(k.level+1);
    return {{{k.x*2,k.y*2,k.body,k.face,l},{k.x*2+1,k.y*2,k.body,k.face,l},
             {k.x*2,k.y*2+1,k.body,k.face,l},{k.x*2+1,k.y*2+1,k.body,k.face,l}}};
}
TileKey parent(TileKey key) {
    if(key.level==0) return key;
    key.x/=2;key.y/=2;--key.level;return key;
}
bool contains(TileKey a,TileKey b) noexcept {
    return a.body==b.body&&a.face==b.face&&a.level<=b.level&&b.level<=20&&
        (b.x>>(b.level-a.level))==a.x&&(b.y>>(b.level-a.level))==a.y;
}
namespace {
struct FaceUv {std::uint8_t face;double u,v;};
FaceUv face_uv(DVec3 direction) {
    const auto d=normalized(direction);
    const auto x=std::abs(d.x),y=std::abs(d.y),z=std::abs(d.z);
    if(x>=y&&x>=z) return d.x>=0?FaceUv{0,-d.z/x,d.y/x}:FaceUv{1,d.z/x,d.y/x};
    if(y>=z) return d.y>=0?FaceUv{2,d.x/y,-d.z/y}:FaceUv{3,d.x/y,d.z/y};
    return d.z>=0?FaceUv{4,d.x/z,d.y/z}:FaceUv{5,-d.x/z,d.y/z};
}
bool tile_less(TileKey a,TileKey b) {
    return std::tie(a.body,a.face,a.level,a.y,a.x)<std::tie(b.body,b.face,b.level,b.y,b.x);
}
using TileSet=std::unordered_set<TileKey,TileHash>;
std::optional<TileKey> covering(const TileSet& leaves,TileKey key) {
    for(;;) {
        if(leaves.contains(key)) return key;
        if(key.level==0) return std::nullopt;
        key=parent(key);
    }
}
}
TileKey locate_tile(std::uint8_t body,DVec3 direction,std::uint8_t level) {
    if(level>20||!finite(direction)||length(direction)<1e-12) throw std::invalid_argument("Invalid cube lookup");
    const auto uv=face_uv(direction);const auto n=1u<<level;
    const auto coord=[&](double value) {return static_cast<std::uint32_t>(std::clamp(std::floor((value+1.0)*0.5*n),0.0,static_cast<double>(n-1)));};
    return {coord(uv.u),coord(uv.v),body,uv.face,level};
}
std::array<TileKey,4> neighbors(TileKey key) {
    // Probe immediately across the edge, not a whole projected cell beyond the face.
    // This preserves cube-face orientation, including the reversed axes of polar faces.
    constexpr double epsilon=1e-5;
    return {locate_tile(key.body,tile_direction(key,-epsilon,0.5),key.level),
            locate_tile(key.body,tile_direction(key,1.0+epsilon,0.5),key.level),
            locate_tile(key.body,tile_direction(key,0.5,-epsilon),key.level),
            locate_tile(key.body,tile_direction(key,0.5,1.0+epsilon),key.level)};
}
std::vector<TileKey> balance_cut(std::span<const TileKey> input) {
    TileSet leaves(input.begin(),input.end());
    bool changed=true;
    while(changed) {
        changed=false;
        std::vector<TileKey> ordered(leaves.begin(),leaves.end());
        std::sort(ordered.begin(),ordered.end(),tile_less);
        for(const auto key:ordered) {
            if(!leaves.contains(key)) continue;
            for(const auto edge:neighbors(key)) {
                const auto adjacent=covering(leaves,edge);
                if(!adjacent||key.level<=adjacent->level+1) continue;
                auto replacement=key;
                while(replacement.level>adjacent->level+1) replacement=parent(replacement);
                std::erase_if(leaves,[&](TileKey item){return contains(replacement,item);});
                leaves.insert(replacement);changed=true;break;
            }
        }
    }
    std::vector<TileKey> result(leaves.begin(),leaves.end());
    std::sort(result.begin(),result.end(),tile_less);return result;
}
DVec3 cube_direction(std::uint8_t face,double u,double v) {
    switch(face) {
    case 0:return normalized(DVec3{1,v,-u});case 1:return normalized(DVec3{-1,v,u});
    case 2:return normalized(DVec3{u,1,-v});case 3:return normalized(DVec3{u,-1,v});
    case 4:return normalized(DVec3{u,v,1});case 5:return normalized(DVec3{-u,v,-1});
    default:throw std::invalid_argument("Cube face must be in [0,5]");
    }
}
DVec3 tile_direction(TileKey k,double u,double v) {
    if(!std::isfinite(u)||!std::isfinite(v)||k.level>20||k.x>=(1u<<k.level)||k.y>=(1u<<k.level)) throw std::invalid_argument("Invalid tile coordinates");
    const double n=static_cast<double>(1u<<k.level);
    return cube_direction(k.face,(static_cast<double>(k.x)+u)*2.0/n-1.0,
                                 (static_cast<double>(k.y)+v)*2.0/n-1.0);
}
static void add_vertex(Mesh& m,DVec3 p,DVec3 n,FVec3 color,float material) {
    m.vertices.push_back({FVec3{p-m.anchor},FVec3{n},{color.x,color.y,color.z,material}});
}
static void triangle(Mesh& m,std::uint32_t a,std::uint32_t b,std::uint32_t c) {
    m.indices.insert(m.indices.end(),{a,b,c});
}
static void cone(Mesh& m,DVec3 base,DVec3 up,double height,double radius,FVec3 color) {
    const auto t=tangent(up),b=cross(up,t);
    constexpr int sides=5;
    for(int j=0;j<sides;++j) {
        const double a0=2*pi*j/sides,a1=2*pi*(j+1)/sides;
        const auto p=base+(t*std::cos(a0)+b*std::sin(a0))*radius;
        const auto q=base+(t*std::cos(a1)+b*std::sin(a1))*radius;
        const auto top=base+up*height,n=normalized(cross(q-p,top-p));
        const auto start=static_cast<std::uint32_t>(m.vertices.size());
        add_vertex(m,p,n,color,2);add_vertex(m,q,n,color,2);add_vertex(m,top,n,color,2);
        triangle(m,start,start+1,start+2);
    }
}
Mesh build_tile(const Body& body,TileKey key,MeshConfig config,std::stop_token stop) {
    if(config.cells<2||config.cells>64||key.body!=body.id||!std::isfinite(body.radius)||body.radius<=0||body.radius>1e12||!std::isfinite(body.height_scale)||body.height_scale<0||body.height_scale>body.radius*0.5) throw std::invalid_argument("Invalid terrain mesh request");
    const int n=config.cells,s=n+1;
    Mesh mesh;const auto center=tile_direction(key,0.5,0.5);
    mesh.anchor=center*(body.radius+sample_height(body,center));
    const double span=body.radius*2.0/static_cast<double>(1u<<key.level);
    mesh.bound_radius=0;
    const std::size_t side=static_cast<std::size_t>(s);
    mesh.vertices.reserve(side*side*(body.ocean?2u:1u)+512);
    mesh.indices.reserve(static_cast<std::size_t>(n*n)*6*(body.ocean?2u:1u)+1500);
    // Preserve the existing skirt strategy and add a spherical chord-error allowance.
    // A 2:1 cut limits the coarser edge length to twice this tile's grid spacing.
    const double spacing=span/n;
    const double skirt=std::max(4.0*body.geography_scale,spacing*0.28+2.0*spacing*spacing/body.radius+
        body.height_scale*0.002/std::pow(2.0,key.level*0.25));
    std::vector<DVec3> directions(side*side);
    std::vector<SurfaceSample> samples(side*side);
    bool has_water=false;
    for(int y=0;y<s;++y) {
        if(stop.stop_requested()) return {};
        for(int x=0;x<s;++x) {
            const auto i=static_cast<std::size_t>(y*s+x);
            directions[i]=tile_direction(key,static_cast<double>(x)/n,static_cast<double>(y)/n);
            samples[i]=sample_surface(body,directions[i]);
            has_water|=samples[i].height<=2.0;
        }
    }
    const auto layer=[&](bool water) {
        const auto start=static_cast<std::uint32_t>(mesh.vertices.size());
        std::vector<DVec3> coarse;
        if(key.level>0) {
            coarse.resize(side*side);
            for(int y=0;y<s;++y) {
                if(stop.stop_requested()) return;
                for(int x=0;x<s;++x) {
                    const auto d=tile_direction(parent(key),static_cast<double>(x)/n,static_cast<double>(y)/n);
                    coarse[static_cast<std::size_t>(y*s+x)]=d*(body.radius+(water?0.0:sample_height(body,d)));
                }
            }
        }
        for(int y=0;y<s;++y) {
            if(stop.stop_requested()) return;
            for(int x=0;x<s;++x) {
                const auto i=static_cast<std::size_t>(y*s+x);
                const auto d=directions[i];const auto& sample=samples[i];
                const auto p=d*(body.radius+(water?0.0:sample.height));
                // A fixed metric normal stencil makes lighting agree at shared LOD vertices.
                const auto normal=water||body.kind==BodyKind::gas?d:surface_normal(body,d,3.0*body.geography_scale);
                add_vertex(mesh,p,normal,water?FVec3{0.015f,0.085f,0.12f}:sample.color,water?1.0f:0.0f);
                if(!coarse.empty()) {
                    const double px=(static_cast<double>(key.x&1u)*n+x)*0.5;
                    const double py=(static_cast<double>(key.y&1u)*n+y)*0.5;
                    const auto ix=std::min(static_cast<int>(px),n-1),iy=std::min(static_cast<int>(py),n-1);
                    const auto fx=px-ix,fy=py-iy;
                    const auto a=coarse[static_cast<std::size_t>(iy*s+ix)];
                    const auto b=coarse[static_cast<std::size_t>(iy*s+ix+1)];
                    const auto c=coarse[static_cast<std::size_t>((iy+1)*s+ix)];
                    const auto e=coarse[static_cast<std::size_t>((iy+1)*s+ix+1)];
                    // Match the actual parent indices (a,b,e) and (a,e,c), not bilinear interpolation.
                    const auto parent_position=fx>=fy?a+(b-a)*fx+(e-b)*fy:a+(e-c)*fx+(c-a)*fy;
                    mesh.vertices.back().parent_delta=FVec3{parent_position-p};
                }
            }
        }
        for(int y=0;y<n;++y) for(int x=0;x<n;++x) {
            const auto a=start+static_cast<std::uint32_t>(y*s+x),b=a+1,c=a+static_cast<std::uint32_t>(s),d=c+1;
            triangle(mesh,a,b,d);triangle(mesh,a,d,c);
        }
        if(config.skirts) {
            std::vector<std::uint32_t> edge;edge.reserve(static_cast<std::size_t>(n)*4);
            for(int x=0;x<n;++x) edge.push_back(start+static_cast<std::uint32_t>(x));
            for(int y=0;y<n;++y) edge.push_back(start+static_cast<std::uint32_t>(y*s+n));
            for(int x=n;x>0;--x) edge.push_back(start+static_cast<std::uint32_t>(n*s+x));
            for(int y=n;y>0;--y) edge.push_back(start+static_cast<std::uint32_t>(y*s));
            const auto lower=static_cast<std::uint32_t>(mesh.vertices.size());
            for(auto idx:edge) {
                auto v=mesh.vertices[idx];
                const auto p=mesh.anchor+DVec3{v.position};
                v.position=FVec3{p-normalized(p)*skirt-mesh.anchor};mesh.vertices.push_back(v);
            }
            for(std::size_t i=0;i<edge.size();++i) {
                const auto j=(i+1)%edge.size();
                const auto a=lower+static_cast<std::uint32_t>(i),b=lower+static_cast<std::uint32_t>(j);
                triangle(mesh,edge[i],a,b);triangle(mesh,edge[i],b,edge[j]);
            }
        }
    };
    layer(false);if(stop.stop_requested()) return {};
    if(body.ocean&&has_water) layer(true);
    mesh.corners={mesh.vertices[0].position,mesh.vertices[static_cast<std::size_t>(n)].position,
        mesh.vertices[static_cast<std::size_t>(n*s+n)].position,mesh.vertices[static_cast<std::size_t>(n*s)].position};
    if(stop.stop_requested()) return {};
    // Optional baked vegetation for offline mesh exports. The viewer uses independent instanced forest patches.
    // Deterministic near-surface vegetation, generated on a fixed L14 lattice.
    // Descendant tiles select disjoint subsets of the same points: no reshuffling at LOD changes.
    if(config.vegetation&&body.kind==BodyKind::temperate&&key.level>=14) {
        const unsigned shift=key.level-14;const auto bx=key.x>>shift,by=key.y>>shift;
        const auto rootSeed=hash32(body.seed^hash32(bx)^hash32(by*733u)^hash32(key.face*29u));
        const double scale=static_cast<double>(1u<<shift);
        for(std::uint32_t i=0;i<512;++i) {
            const auto h=hash32(rootSeed+i*1013u);
            const double u=static_cast<double>(h&0xffffu)/65536.0,v=static_cast<double>(h>>16)/65536.0;
            const double lu=u*scale-static_cast<double>(key.x-(bx<<shift));
            const double lv=v*scale-static_cast<double>(key.y-(by<<shift));
            if(lu<0||lu>=1||lv<0||lv>=1) continue;
            const auto d=tile_direction(key,lu,lv);const auto sample=sample_surface(body,d);
            if(sample.height<120*body.geography_scale||sample.height>3000*body.geography_scale||sample.temperature<0.23||sample.moisture<0.4) continue;
            const auto base=d*(body.radius+sample.height);const double height=28.0+(h%3300u)*0.01;
            cone(mesh,base,d,height*0.5,0.7,{0.13f,0.08f,0.045f});
            cone(mesh,base+d*(height*0.22),d,height*0.78,height*0.24,{0.035f,0.11f,0.045f});
            cone(mesh,base+d*(height*0.48),d,height*0.59,height*0.18,{0.05f,0.16f,0.055f});
        }
    }
    for(const auto& vertex:mesh.vertices) {
        mesh.bound_radius=std::max({mesh.bound_radius,length(DVec3{vertex.position}),
            length(DVec3{vertex.position}+DVec3{vertex.parent_delta})});
    }
    mesh.bound_radius+=1.0; // float conversion and cancellation allowance
    return mesh;
}
// Refinement target only, not collision authority. March the actual procedural
// surface with a conservative Lipschitz step so a narrow ridge cannot be skipped
// in favour of a farther sphere intersection. A miss or exhausted work budget
// retains the ordinary viewport/screen-error policy.
struct RefinementHit {DVec3 direction;double distance;};
static std::optional<RefinementHit> refinement_hit(const Body& body,DVec3 eye,DVec3 forward,bool ocean) {
    const double envelope=1.18*std::abs(body.height_scale)+9.0*std::abs(body.geography_scale);
    const double inner=body.radius-envelope;
    if(inner<=0.0)return {};
    double begin{},end{};
    if(!ray_sphere(eye,forward,body.radius+envelope,begin,end))return {};
    begin=std::max(0.0,begin);
    // Quintic lattice interpolation: each partial derivative is at most 3.75.
    // Sum the normalized fBm octave derivatives used by height_field, including
    // domain warp, ridge power, smoothstep, regional and fine terms.
    const double noiseSlope=3.75*std::sqrt(3.0);
    const auto fbmSlope=[&](unsigned octaves) {
        double amplitude=.5,frequency=1.0,total{},norm{};
        for(unsigned i=0;i<octaves;++i){total+=amplitude*frequency;norm+=amplitude;amplitude*=.5;frequency*=2.03;}
        return noiseSlope*total/norm;
    };
    const double warpedSlope=1.0+.14*std::sqrt(3.0)*3.0*noiseSlope;
    const double continentSlope=3.6*warpedSlope*fbmSlope(5);
    const double ridgeSlope=29.0*warpedSlope*fbmSlope(5);
    const double heightSlope=std::abs(body.height_scale)*(.63*continentSlope
        +.32*(1.5/.48*continentSlope+5.0*ridgeSlope)+.025*120.0*warpedSlope*fbmSlope(4))
        +9.0*std::abs(body.geography_scale)*4700.0*fbmSlope(5);
    const double lipschitz=body.kind==BodyKind::gas?1.0:1.0+heightSlope/inner;
    double t=begin;
    for(unsigned step=0;step<16384u && t<=end;++step) {
        const auto point=eye+forward*t;const double radius=length(point);
        if(!std::isfinite(radius)||radius<=inner)return {};
        const auto direction=point/radius;
        const double terrain=sample_height(body,direction);
        const double gap=radius-body.radius-(ocean&&body.ocean?std::max(0.0,terrain):terrain);
        if(!std::isfinite(gap)||gap<-.05)return {}; // Camera below the surface: no invented target.
        if(gap<=.05)return RefinementHit{direction,t};
        const double next=t+.9*gap/lipschitz;
        if(next<=t)return {};
        t=next;
    }
    return {};
}
static TerrainPlan plan_terrain_impl(const Universe& world,DVec3 camera,double seconds,int height,double fov,
                                    LodSettings settings,const TerrainPlan* previous,
                                    std::optional<std::size_t> selected_body,bool ocean_surface=false,
                                    std::optional<TerrainView> refinement_view=std::nullopt) {
    if(selected_body && *selected_body>=world.bodies().size())
        throw std::invalid_argument("Invalid selected terrain body");
    if(!finite(camera)||!std::isfinite(seconds)||height<=0||!std::isfinite(fov)||fov<=0.1||fov>=3.0||
       !std::isfinite(settings.patch_pixels)||settings.patch_pixels<32||settings.max_level>20||
       !std::isfinite(settings.hysteresis)||settings.hysteresis<0||settings.hysteresis>0.45)
        throw std::invalid_argument("Invalid LOD planning input");
    double cone_angle{},cone_cosine{};
    if(refinement_view) {
        if(!finite(refinement_view->forward)||!std::isfinite(length(refinement_view->forward))||length(refinement_view->forward)<1e-12
            ||!std::isfinite(refinement_view->aspect_ratio)||refinement_view->aspect_ratio<0.1||refinement_view->aspect_ratio>10.0)
            throw std::invalid_argument("Invalid terrain refinement view");
        refinement_view->forward=normalized(refinement_view->forward);
        if(refinement_view->up) {
            const auto supplied=*refinement_view->up;
            const double magnitude=length(supplied);
            if(!finite(supplied)||!std::isfinite(magnitude)||magnitude<1e-12)
                throw std::invalid_argument("Invalid terrain refinement up");
            const auto up=supplied/magnitude;
            const auto perpendicular=up-refinement_view->forward*dot(up,refinement_view->forward);
            if(length(perpendicular)<1e-6)throw std::invalid_argument("Parallel terrain refinement up");
            refinement_view->up=normalized(perpendicular);
        }
        // The circumscribed cone contains all four rectangular viewport corners.
        cone_angle=std::atan(std::tan(fov*0.5)*std::sqrt(1.0+refinement_view->aspect_ratio*refinement_view->aspect_ratio));
        cone_cosine=std::cos(cone_angle);
    }
    struct Candidate { TileKey key;double priority;bool in_view{};bool at_focus{};double attention{}; };
    const auto compare=[](const Candidate& a,const Candidate& b) {
        if(a.attention!=b.attention)return a.attention<b.attention;
        if(a.at_focus!=b.at_focus)return !a.at_focus;
        if(a.in_view!=b.in_view)return !a.in_view;
        return a.priority==b.priority?tile_less(b.key,a.key):a.priority<b.priority;
    };
    std::priority_queue<Candidate,std::vector<Candidate>,decltype(compare)> queue(compare);
    TerrainPlan plan;TileSet leaves,split_before;
    if(previous) {
        const TileSet old_leaves(previous->leaves.begin(),previous->leaves.end());
        for(auto key:previous->needed) if(!old_leaves.contains(key)) split_before.insert(key);
    }
    struct Focus {TileKey tile;double distance;};
    struct BodyView { DVec3 local,up;double radius,near_weight;std::array<std::optional<Focus>,3> foci{}; };
    std::vector<BodyView> views;views.reserve(world.bodies().size());
    for(const auto& b:world.bodies()) {
        const auto local=camera-world.center(b.id,seconds);
        double near_weight=1.0;
        if(refinement_view&&refinement_view->up) {
            const double ground=sample_height(b,normalized(local));
            const double altitude=std::max(0.0,length(local)-b.radius-(ocean_surface&&b.ocean?std::max(0.0,ground):ground));
            const double near=std::clamp(b.radius*.002,25.0,5000.0);
            const double far=std::max(near*4.0,std::clamp(b.radius*.02,250.0,100000.0));
            const double blend=std::clamp((altitude-near)/(far-near),0.0,1.0);
            near_weight=1.0-blend*blend*(3.0-2.0*blend);
        }
        std::array<std::optional<Focus>,3> foci{};
        if(refinement_view&&(!selected_body||b.id==world.body(*selected_body).id)) {
            const auto focus=[&](DVec3 ray) -> std::optional<Focus> {
                if(const auto hit=refinement_hit(b,local,ray,ocean_surface))
                    return Focus{locate_tile(static_cast<std::uint8_t>(b.id),hit->direction,settings.max_level),hit->distance};
                return {};
            };
            if(!refinement_view->up||near_weight>0.0)foci[0]=focus(refinement_view->forward);
            // Forward, 45 degrees camera-down, and radial nadir. The camera
            // basis is orthonormal above, so the second ray is exactly 45
            // degrees even when rolled. Each march retains its 16384 bound:
            // at most 49152 surface evaluations per selected body.
            if(refinement_view->up&&near_weight>0.0) {
                foci[1]=focus(normalized(refinement_view->forward-*refinement_view->up));
                foci[2]=focus(-normalized(local));
            }
        }
        views.push_back({local,normalized(local),length(local),near_weight,foci});
    }
    const double focal=static_cast<double>(height)/(2.0*std::tan(fov*0.5));
    const auto score=[&](TileKey k)->Candidate {
        const auto& body=world.body(k.body);const auto& view=views[k.body];
        const double n=static_cast<double>(1u<<k.level),span=2.0*body.radius/n;
        const auto middle=tile_direction(k,0.5,0.5);
        bool at_focus{};double focus_distance=std::numeric_limits<double>::max();
        for(const auto& focus:view.foci)if(focus&&contains(k,focus->tile)) {
            at_focus=true;focus_distance=std::min(focus_distance,focus->distance);
        }
        // Only actual footprint-hit ancestry may refine outside the view cone.
        // The rest of the horizon retains the existing exclusion and budgets.
        const bool footprint_focus=refinement_view&&refinement_view->up&&at_focus;
        double central_weight=1.0;
        if(refinement_view) {
            const auto to_center=middle*body.radius-view.local;
            const double center_distance=length(to_center);
            // Normalizing a cube-face point is 1-Lipschitz (its length is at
            // least one). Half a tile diagonal therefore bounds every surface
            // direction. The height envelope includes all terms in height_field.
            const double extent=body.radius*std::min(2.0,std::sqrt(2.0)/n)
                +std::abs(body.height_scale)*1.18+9.0*std::abs(body.geography_scale);
            if(center_distance>extent) {
                const double separation=std::acos(std::clamp(dot(to_center/center_distance,refinement_view->forward),-1.0,1.0));
                const double angular_extent=std::asin(std::min(1.0,extent/center_distance));
                if(separation>cone_angle+angular_extent&&!footprint_focus)return {k,0.0,false};
                const double offset=std::clamp((separation-angular_extent-cone_angle*.75)/(cone_angle*.25),0.0,1.0);
                central_weight=1.0-offset*offset*(3.0-2.0*offset);
            }
        }
        const double angular=std::min(1.7,span/body.radius);
        if(view.radius>body.radius+body.height_scale*2&&dot(view.up,middle)+angular<body.radius/view.radius-0.12) return {k,0.0,false};
        // Measure the closest point on the tile's projected cube domain, rather than its center.
        // A camera beside a tile edge must not lose detail simply because the center is far away.
        double u=0,v=0,denominator=0;
        const auto d=view.local;
        switch(k.face) {
        case 0:denominator=d.x;u=-d.z;v=d.y;break;
        case 1:denominator=-d.x;u=d.z;v=d.y;break;
        case 2:denominator=d.y;u=d.x;v=-d.z;break;
        case 3:denominator=-d.y;u=d.x;v=d.z;break;
        case 4:denominator=d.z;u=d.x;v=d.y;break;
        default:denominator=-d.z;u=-d.x;v=d.y;break;
        }
        DVec3 direction=middle;
        if(denominator>1e-6) {
            const double low_u=2.0*k.x/n-1.0,low_v=2.0*k.y/n-1.0;
            direction=cube_direction(k.face,std::clamp(u/denominator,low_u,low_u+2.0/n),
                                            std::clamp(v/denominator,low_v,low_v+2.0/n));
        }
        const auto surface=direction*(body.radius+(k.level>=8?sample_height(body,direction):0.0));
        // A separate queue tier spends the finite detail budget on the part of
        // an intersecting tile actually in view first. The conservative envelope
        // above alone excludes refinement; this tier does not cull geometry.
        const auto to_surface=surface-view.local;
        const double surface_distance=length(to_surface);
        const bool in_view=footprint_focus||(refinement_view&&surface_distance>1e-9
            &&dot(to_surface/surface_distance,refinement_view->forward)>=cone_cosine);
        double distance=length(view.local-surface);
        // A camera over deep ocean is close to the visible water, not to the
        // seabed. Both surfaces share this tile's screen-error budget; opt-in
        // water must never change dry terrain or collision/landing queries.
        if(ocean_surface&&body.ocean)
            distance=std::min(distance,length(view.local-direction*body.radius));
        distance=std::max(2.0,distance);
        const double threshold=settings.patch_pixels*(split_before.contains(k)?1.0-settings.hysteresis:1.0+settings.hysteresis);
        // A footprint shares the same screen-error ordering between its hits,
        // measured at their actual range. The legacy single-ray score is exact.
        if(footprint_focus)distance=std::max(2.0,focus_distance)*view.near_weight+distance*(1.0-view.near_weight);
        // Spend up to two additional levels on ray-hit tiles. The authored
        // maximum level, complete balanced cut and fixed leaf budget still win.
        if(refinement_view&&refinement_view->up) {
            // From orbit, spread detail across the circumscribed viewport cone.
            // Smoothly trade that region for the three ray hits near the ground;
            // The queue's attention weight is continuous across that trade;
            // the orbital plateau covers a region rather than one ray ancestry.
            const double weight=view.near_weight*(at_focus?1.0:0.0)+(1.0-view.near_weight)*central_weight;
            const double priority=span*focal/distance/threshold*(1.0+3.0*weight);
            // A bounded monotonic screen-error term prevents an infinitesimal
            // altitude-weight change from replacing the entire queue tier.
            return {k,priority,false,false,2.0*weight+priority/(1.0+priority)};
        }
        return {k,span*focal/distance/threshold*(at_focus?4.0:1.0),in_view,at_focus};
    };
    for(const auto& body:world.bodies()) {
        if(selected_body && body.id!=world.body(*selected_body).id) continue;
        for(std::uint8_t face=0;face<6;++face) {
            const TileKey key{0,0,static_cast<std::uint8_t>(body.id),face,0};
            leaves.insert(key);queue.push(score(key));plan.needed.push_back(key);
        }
    }
    const auto budget=std::clamp(settings.max_leaves,leaves.size(),std::size_t{2048});
    while(!queue.empty()) {
        const auto candidate=queue.top();queue.pop();
        if(candidate.priority<=1||candidate.key.level>=settings.max_level||!leaves.contains(candidate.key)) continue;
        // Split the necessary coarse neighbors first. Budget includes the whole balancing closure.
        TileSet pending;std::vector<TileKey> ordered;
        const auto request=[&](auto&& self,TileKey key)->void {
            if(!pending.insert(key).second) return;
            for(const auto edge:neighbors(key)) {
                const auto adjacent=covering(leaves,edge);
                if(adjacent&&adjacent->level<key.level) self(self,*adjacent);
            }
            ordered.push_back(key);
        };
        request(request,candidate.key);
        if(leaves.size()+3*ordered.size()>budget) continue;
        for(const auto key:ordered) {
            if(!leaves.erase(key)) continue;
            for(const auto child:children(key)) {
                leaves.insert(child);plan.needed.push_back(child);queue.push(score(child));
            }
        }
    }
    plan.leaves.assign(leaves.begin(),leaves.end());
    std::sort(plan.leaves.begin(),plan.leaves.end(),tile_less);
    // Selected-body streaming keeps the existing screen-error priority and
    // balancing-closure insertion order. Legacy all-world callers retain their
    // stable ancestor inventory; the leaf set and signature are unchanged.
    if(!selected_body)
        std::stable_sort(plan.needed.begin(),plan.needed.end(),[](TileKey a,TileKey b) {
            return a.level==b.level?tile_less(a,b):a.level<b.level;
        });
    plan.signature=14695981039346656037ull;
    for(const auto key:plan.leaves) {
        const std::array<std::uint32_t,5> words{key.body,key.face,key.level,key.x,key.y};
        for(auto word:words) {plan.signature^=word;plan.signature*=1099511628211ull;}
    }
    return plan;
}
TerrainPlan plan_terrain(const Universe& world,DVec3 camera,double seconds,int height,double fov,
                         LodSettings settings,const TerrainPlan* previous) {
    return plan_terrain_impl(world,camera,seconds,height,fov,settings,previous,std::nullopt);
}
TerrainPlan plan_body_terrain(const Universe& world,std::size_t body_index,DVec3 camera,double seconds,
                              int height,double fov,LodSettings settings,const TerrainPlan* previous) {
    return plan_terrain_impl(world,camera,seconds,height,fov,settings,previous,body_index);
}
TerrainPlan plan_body_terrain(const Universe& world,std::size_t body_index,DVec3 camera,double seconds,
                              int height,double fov,LodSettings settings,const TerrainPlan* previous,bool ocean_surface) {
    return plan_terrain_impl(world,camera,seconds,height,fov,settings,previous,body_index,ocean_surface);
}
TerrainPlan plan_body_terrain(const Universe& world,std::size_t body_index,DVec3 camera,double seconds,
                              int height,double fov,LodSettings settings,const TerrainPlan* previous,bool ocean_surface,
                              const TerrainView& view) {
    return plan_terrain_impl(world,camera,seconds,height,fov,settings,previous,body_index,ocean_surface,view);
}
} // namespace epoch::planet
