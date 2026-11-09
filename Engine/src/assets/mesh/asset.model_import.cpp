// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>
module asset.model_import;
import asset.mesh_artifact;
import asset.texture_artifact;
import asset.texture_import;

namespace epochengine::asset::model_import
{
    namespace
    {
        using Json = nlohmann::json;
        using Matrix = std::array<double, 16>;
        constexpr auto missing = (std::numeric_limits<std::uint32_t>::max)();
        struct Failure { Code code; const char* reason; };
        [[noreturn]] void refuse(Code code, const char* reason) { throw Failure{code, reason}; }
        void require(bool good, const char* reason)
        { if (!good) refuse(Code::invalid_format, reason); }
        void supported(bool good, const char* reason)
        { if (!good) refuse(Code::unsupported_feature, reason); }
        void bounded(bool good)
        { if (!good) refuse(Code::limit_exceeded, "The model exceeds the configured import limits."); }
        [[nodiscard]] bool valid_limits(const Limits& l) noexcept
        {
            return l.maximum_file_bytes >= 28u && l.maximum_file_bytes <= 256ull * 1024ull * 1024ull
                && l.maximum_json_bytes > 0u && l.maximum_json_bytes <= 8u * 1024u * 1024u
                && l.maximum_json_values > 0u && l.maximum_json_values <= 262'144u
                && l.maximum_nodes > 0u && l.maximum_nodes <= 8192u
                && l.maximum_primitives > 0u && l.maximum_primitives <= 4096u
                && l.maximum_images <= 128u && l.maximum_image_bytes <= l.maximum_file_bytes
                && l.geometry.maximum_vertices >= 3u && l.geometry.maximum_vertices <= 1'048'576u
                && l.geometry.maximum_indices >= 3u && l.geometry.maximum_indices <= 6'291'456u
                && l.geometry.maximum_bytes >= 192u && l.geometry.maximum_bytes <= 64ull * 1024ull * 1024ull;
        }
        [[nodiscard]] std::uint32_t word(std::span<const std::byte> b, std::size_t at)
        {
            require(at <= b.size() && b.size() - at >= 4u, "The GLB word is truncated.");
            std::uint32_t value{};
            for (unsigned i = 0u; i != 4u; ++i) value |= std::to_integer<std::uint32_t>(b[at+i]) << (i*8u);
            return value;
        }
        [[nodiscard]] std::uint64_t integer(const Json& j)
        {
            require(j.is_number_unsigned() || (j.is_number_integer() && j.get<std::int64_t>() >= 0),
                "Expected a nonnegative integer.");
            return j.get<std::uint64_t>();
        }
        [[nodiscard]] std::uint64_t number(const Json& j, const char* key, std::uint64_t fallback)
        { return j.contains(key) ? integer(j.at(key)) : fallback; }
        [[nodiscard]] double real(const Json& j)
        {
            require(j.is_number(), "Expected a finite number.");
            const double value = j.get<double>();
            require(std::isfinite(value), "Nonfinite model metadata is not allowed.");
            return value;
        }
        [[nodiscard]] const Json& array(const Json& j, const char* key)
        {
            static const Json empty = Json::array();
            if (!j.contains(key)) return empty;
            const auto& value = j.at(key);
            require(value.is_array(), "Expected a model array.");
            return value;
        }
        void no_extensions(const Json& j)
        {
            supported(!j.contains("extensions") || (j.at("extensions").is_object() && j.at("extensions").empty()),
                "This reader does not support glTF extensions or compressed data.");
        }
        template<std::size_t N>
        [[nodiscard]] std::array<double, N> real_array(const Json& j)
        {
            require(j.is_array() && j.size() == N, "Model vector has the wrong length.");
            std::array<double, N> result{};
            for (std::size_t i = 0u; i != N; ++i) result[i] = real(j.at(i));
            return result;
        }
        [[nodiscard]] Matrix identity() noexcept { return {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}; }
        [[nodiscard]] Matrix multiply(const Matrix& a, const Matrix& b)
        {
            Matrix result{};
            for (std::size_t c=0;c!=4;++c) for(std::size_t r=0;r!=4;++r)
                for(std::size_t k=0;k!=4;++k) result[c*4+r] += a[k*4+r]*b[c*4+k];
            for (double value : result) require(std::isfinite(value), "Node transform overflows.");
            return result;
        }
        [[nodiscard]] Matrix local_matrix(const Json& node)
        {
            no_extensions(node);
            supported(!node.contains("skin") && !node.contains("weights") && !node.contains("camera"),
                "Skinned, morphing or camera nodes are not supported by the static reader.");
            Matrix result = identity();
            if (node.contains("matrix"))
            {
                require(!node.contains("translation") && !node.contains("rotation") && !node.contains("scale"),
                    "A node cannot contain both a matrix and TRS.");
                result = real_array<16>(node.at("matrix"));
                require(result[3]==0 && result[7]==0 && result[11]==0 && result[15]==1,
                    "Only affine node matrices are supported.");
                return result;
            }
            const auto t = node.contains("translation") ? real_array<3>(node.at("translation")) : std::array<double,3>{};
            const auto s = node.contains("scale") ? real_array<3>(node.at("scale")) : std::array<double,3>{1,1,1};
            const auto q = node.contains("rotation") ? real_array<4>(node.at("rotation")) : std::array<double,4>{0,0,0,1};
            const auto [x,y,z,w] = q;
            require(std::abs(x*x+y*y+z*z+w*w-1.0) <= 1.0e-5, "Node quaternion must be normalized.");
            result = {(1-2*y*y-2*z*z)*s[0], (2*x*y+2*z*w)*s[0], (2*x*z-2*y*w)*s[0],0,
                (2*x*y-2*z*w)*s[1],(1-2*x*x-2*z*z)*s[1],(2*y*z+2*x*w)*s[1],0,
                (2*x*z+2*y*w)*s[2],(2*y*z-2*x*w)*s[2],(1-2*x*x-2*y*y)*s[2],0,t[0],t[1],t[2],1};
            for(double value : result) require(std::isfinite(value), "Node TRS overflows.");
            return result;
        }
        struct View { std::size_t offset{}, length{}, stride{}; };
        struct Accessor { View view{}; std::size_t offset{}, count{}, stride{}, components{}, component_bytes{}; std::uint32_t type{}; };
        struct Reader final
        {
            const Limits& limits;
            Json document;
            std::span<const std::byte> binary;
            std::vector<View> views;
            std::vector<Accessor> accessors;
            Result output;
            std::uint64_t vertices{}, indices{}, image_bytes{};
            [[nodiscard]] std::uint32_t index(const Json& j, std::size_t size)
            {
                const auto value=integer(j);
                require(value<size, "Model index is out of range.");
                return static_cast<std::uint32_t>(value);
            }
            void read_buffers()
            {
                const auto& buffers=array(document,"buffers");
                require(buffers.size()==1u && buffers[0].is_object(), "GLB requires one embedded buffer.");
                if(buffers[0].contains("uri")) refuse(Code::external_reference,"External/data buffer URIs are not admitted.");
                no_extensions(buffers[0]);
                const auto declared=number(buffers[0],"byteLength",0u);
                require(declared>0u && declared<=binary.size() && binary.size()-declared<=3u,"Embedded buffer length is invalid.");
                binary=binary.first(static_cast<std::size_t>(declared));
                const auto& source=array(document,"bufferViews"); bounded(source.size()<=16'384u);
                for(const auto& item:source)
                {
                    require(item.is_object(),"Invalid buffer view."); no_extensions(item);
                    require(number(item,"buffer",missing)==0u,"Buffer view references a missing buffer.");
                    const auto at=number(item,"byteOffset",0u), length=number(item,"byteLength",0u), stride=number(item,"byteStride",0u);
                    require(length>0u && at<=binary.size() && length<=binary.size()-at,"Buffer view escapes embedded data.");
                    require(stride==0u || (stride>=4u && stride<=252u && stride%4u==0u),"Invalid buffer-view stride.");
                    views.push_back({static_cast<std::size_t>(at),static_cast<std::size_t>(length),static_cast<std::size_t>(stride)});
                }
                const auto& sourceAccessors=array(document,"accessors"); bounded(sourceAccessors.size()<=16'384u);
                for(const auto& item:sourceAccessors)
                {
                    require(item.is_object(),"Invalid accessor."); no_extensions(item);
                    supported(!item.contains("sparse"),"Sparse accessors are not supported.");
                    supported(!item.value("normalized",false),"Normalized accessors are not supported by this static reader.");
                    require(item.contains("bufferView"),"Accessor has no embedded buffer view.");
                    const auto view=views.at(index(item.at("bufferView"),views.size()));
                    require(item.contains("type") && item.at("type").is_string(),"Accessor type is missing.");
                    const auto type=item.at("type").get<std::string>();
                    std::size_t components=type=="SCALAR"?1u:type=="VEC2"?2u:type=="VEC3"?3u:0u;
                    supported(components!=0u,"Only scalar, VEC2 and VEC3 accessors are supported.");
                    const auto component=number(item,"componentType",0u);
                    const std::size_t width=component==5121u?1u:component==5123u?2u:(component==5125u||component==5126u)?4u:0u;
                    supported(width!=0u,"Unsupported accessor component type.");
                    const auto count=number(item,"count",0u), at=number(item,"byteOffset",0u);
                    bounded(count>0u && count<=limits.geometry.maximum_indices);
                    const auto element=width*components, stride=view.stride?view.stride:element;
                    require(stride>=element && at%width==0u && (view.offset+at)%width==0u && at<=view.length
                        && element<=view.length-at && count-1u<=(view.length-at-element)/stride,"Accessor escapes its buffer view.");
                    accessors.push_back({view,static_cast<std::size_t>(at),static_cast<std::size_t>(count),stride,components,width,static_cast<std::uint32_t>(component)});
                }
            }
            [[nodiscard]] std::uint32_t uint_at(const Accessor& a,std::size_t n,std::size_t component=0u) const
            {
                const auto at=a.view.offset+a.offset+n*a.stride+component*a.component_bytes;
                std::uint32_t value{};
                for(std::size_t i=0;i<a.component_bytes;++i)value|=std::to_integer<std::uint32_t>(binary[at+i])<<(i*8u);
                return value;
            }
            [[nodiscard]] float float_at(const Accessor& a,std::size_t n,std::size_t c) const
            {
                const auto value=std::bit_cast<float>(uint_at(a,n,c));
                if(!std::isfinite(value))refuse(Code::invalid_geometry,"Nonfinite mesh attribute.");
                return value;
            }
            [[nodiscard]] const Accessor& attribute(const Json& value,std::size_t components,std::size_t count=0u)
            {
                const auto& a=accessors.at(index(value,accessors.size()));
                require(a.type==5126u && a.components==components && (count==0u||a.count==count),"Attribute type or count is invalid.");
                return a;
            }
            void read_images()
            {
                const auto& images=array(document,"images");bounded(images.size()<=limits.maximum_images);
                for(const auto& image:images)
                {
                    require(image.is_object(),"Invalid image metadata."); no_extensions(image);
                    if(image.contains("uri"))refuse(Code::external_reference,"External/data image URIs are not admitted.");
                    require(image.contains("bufferView") && image.contains("mimeType") && image.at("mimeType").is_string(),"Embedded image needs bufferView and MIME type.");
                    const auto& view=views.at(index(image.at("bufferView"),views.size()));
                    require(view.stride==0u,"Embedded image buffer view cannot be interleaved.");
                    image_bytes+=view.length;bounded(image_bytes<=limits.maximum_image_bytes);
                    const auto bytes=binary.subspan(view.offset,view.length);
                    const auto mime=image.at("mimeType").get<std::string>();
                    supported(mime=="image/png"||mime=="image/jpeg","Only embedded PNG/JPEG image references are supported.");
                    constexpr std::array<std::byte,8> png{std::byte{137},std::byte{80},std::byte{78},std::byte{71},std::byte{13},std::byte{10},std::byte{26},std::byte{10}};
                    require(mime=="image/png"?(bytes.size()>=png.size()&&std::equal(png.begin(),png.end(),bytes.begin()))
                        :(bytes.size()>=4u&&bytes[0]==std::byte{255}&&bytes[1]==std::byte{216}&&bytes[bytes.size()-2u]==std::byte{255}&&bytes.back()==std::byte{217}),"Embedded image signature does not match its MIME type.");
                    output.images.push_back({mime,{bytes.begin(),bytes.end()},texture::deterministic_content_hash("epoch.model.embedded_image.v1",bytes)});
                }
            }
            [[nodiscard]] std::optional<TextureBinding> binding(const Json& owner,const char* key)
            {
                if(!owner.contains(key))return {};
                const auto& info=owner.at(key);require(info.is_object()&&info.contains("index"),"Invalid material texture reference.");no_extensions(info);
                supported(number(info,"texCoord",0u)==0u,"Only TEXCOORD_0 materials are supported.");
                const auto& textures=array(document,"textures"); const auto& source=textures.at(index(info.at("index"),textures.size()));
                require(source.is_object()&&source.contains("source"),"Texture has no embedded image.");no_extensions(source);
                TextureBinding result{.image=index(source.at("source"),output.images.size())};
                if(source.contains("sampler"))
                {
                    const auto& samplers=array(document,"samplers"); const auto& s=samplers.at(index(source.at("sampler"),samplers.size()));
                    require(s.is_object(),"Invalid sampler.");no_extensions(s);
                    const auto wrap=[&](const char* name){auto v=number(s,name,10497u);require(v==33071u||v==33648u||v==10497u,"Invalid sampler wrap mode.");return static_cast<std::uint32_t>(v);};
                    result.sampler.wrap_s=wrap("wrapS");result.sampler.wrap_t=wrap("wrapT");
                    if(s.contains("magFilter")){auto v=integer(s.at("magFilter"));require(v==9728u||v==9729u,"Invalid sampler magnification filter.");result.sampler.mag_filter=static_cast<std::uint32_t>(v);}
                    if(s.contains("minFilter")){auto v=integer(s.at("minFilter"));require(v==9728u||v==9729u||(v>=9984u&&v<=9987u),"Invalid sampler minification filter.");result.sampler.min_filter=static_cast<std::uint32_t>(v);}
                }
                return result;
            }
            void read_materials()
            {
                const auto& materials=array(document,"materials");bounded(materials.size()<=1024u);
                for(const auto& item:materials)
                {
                    require(item.is_object(),"Invalid material.");no_extensions(item);Material material;
                    if(item.contains("name")){require(item.at("name").is_string(),"Invalid material name.");material.name=item.at("name").get<std::string>();bounded(material.name.size()<=4096u);}
                    const auto factor=[&](const Json& j,const char* key,double fallback){const auto v=j.contains(key)?real(j.at(key)):fallback;require(v>=0&&v<=1,"Material factor is outside [0,1].");return static_cast<float>(v);};
                    if(item.contains("pbrMetallicRoughness"))
                    {
                        const auto& p=item.at("pbrMetallicRoughness");require(p.is_object(),"Invalid PBR metadata.");no_extensions(p);
                        if(p.contains("baseColorFactor")){auto v=real_array<4>(p.at("baseColorFactor"));for(std::size_t i=0;i!=4;++i){require(v[i]>=0&&v[i]<=1,"Invalid base color factor.");material.base_color[i]=static_cast<float>(v[i]);}}
                        material.metallic=factor(p,"metallicFactor",1);material.roughness=factor(p,"roughnessFactor",1);
                        material.base_color_texture=binding(p,"baseColorTexture");material.metallic_roughness_texture=binding(p,"metallicRoughnessTexture");
                    }
                    material.normal_texture=binding(item,"normalTexture");material.occlusion_texture=binding(item,"occlusionTexture");material.emissive_texture=binding(item,"emissiveTexture");
                    if(item.contains("normalTexture")&&item.at("normalTexture").contains("scale")){const auto value=real(item.at("normalTexture").at("scale"));require(std::abs(value)<=1e9,"Normal scale is unrepresentable.");material.normal_scale=static_cast<float>(value);}
                    if(item.contains("occlusionTexture"))material.occlusion_strength=factor(item.at("occlusionTexture"),"strength",1);
                    if(item.contains("emissiveFactor")){auto v=real_array<3>(item.at("emissiveFactor"));for(std::size_t i=0;i!=3;++i){require(v[i]>=0&&v[i]<=1,"Invalid emissive factor.");material.emissive[i]=static_cast<float>(v[i]);}}
                    if(item.contains("alphaMode")){require(item.at("alphaMode").is_string(),"Invalid alpha mode.");auto mode=item.at("alphaMode").get<std::string>();require(mode=="OPAQUE"||mode=="MASK"||mode=="BLEND","Invalid alpha mode.");material.alpha_mode=mode=="MASK"?AlphaMode::mask:mode=="BLEND"?AlphaMode::blend:AlphaMode::opaque;}
                    if(item.contains("alphaCutoff")){const auto value=real(item.at("alphaCutoff"));require(value>=0&&value<=1e9,"Invalid alpha cutoff.");material.alpha_cutoff=static_cast<float>(value);}
                    if(item.contains("doubleSided")){require(item.at("doubleSided").is_boolean(),"Invalid double-sided flag.");material.double_sided=item.at("doubleSided").get<bool>();}
                    output.materials.push_back(std::move(material));
                }
            }
            void primitive(const Json& p,const Matrix& world,std::uint32_t node,std::uint32_t meshIndex,std::uint32_t primitiveIndex)
            {
                bounded(output.primitives.size()<limits.maximum_primitives);require(p.is_object(),"Invalid primitive.");no_extensions(p);
                supported(number(p,"mode",4u)==4u,"Only triangle primitives are supported.");
                supported(!p.contains("targets"),"Morph targets are not supported.");
                require(p.contains("attributes")&&p.at("attributes").is_object(),"Primitive attributes are missing.");
                const auto& attributes=p.at("attributes");require(attributes.contains("POSITION"),"Primitive positions are missing.");
                for(const auto& [name,value]:attributes.items()){(void)value;supported(name=="POSITION"||name=="NORMAL"||name=="TEXCOORD_0","Unsupported vertex attribute would be lost.");}
                const auto& positions=attribute(attributes.at("POSITION"),3u);bounded(positions.count<=limits.geometry.maximum_vertices);
                const Accessor* normals=attributes.contains("NORMAL")?&attribute(attributes.at("NORMAL"),3u,positions.count):nullptr;
                const Accessor* uv=attributes.contains("TEXCOORD_0")?&attribute(attributes.at("TEXCOORD_0"),2u,positions.count):nullptr;
                std::vector<std::uint32_t> triangles;
                if(p.contains("indices"))
                {
                    const auto& a=accessors.at(index(p.at("indices"),accessors.size()));
                    require(a.components==1u && (a.type==5121u||a.type==5123u||a.type==5125u)&&a.view.stride==0u,"Invalid triangle index accessor.");
                    triangles.reserve(a.count);for(std::size_t i=0;i<a.count;++i)triangles.push_back(uint_at(a,i));
                }
                else {triangles.reserve(positions.count);for(std::size_t i=0;i<positions.count;++i)triangles.push_back(static_cast<std::uint32_t>(i));}
                require(triangles.size()>=3u&&triangles.size()%3u==0u,"Incomplete triangles.");
                vertices+=positions.count;indices+=triangles.size();
                bounded(vertices<=limits.geometry.maximum_vertices&&indices<=limits.geometry.maximum_indices
                    && 48u+vertices*44u+indices*4u<=limits.geometry.maximum_bytes);
                const double a=world[0],b=world[4],c=world[8],d=world[1],e=world[5],f=world[9],g=world[2],h=world[6],i=world[10];
                const double det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
                require(std::isfinite(det)&&std::abs(det)>1e-18,"Singular node transform.");
                const std::array<double,9> inverseTranspose{(e*i-f*h)/det,(f*g-d*i)/det,(d*h-e*g)/det,
                    (c*h-b*i)/det,(a*i-c*g)/det,(b*g-a*h)/det,(b*f-c*e)/det,(c*d-a*f)/det,(a*e-b*d)/det};
                std::vector<mesh::Vertex> points(positions.count);
                const auto normalized=[](std::array<double,3> v){const double size=std::hypot(v[0],v[1],v[2]);if(!std::isfinite(size)||size<=1e-15)refuse(Code::invalid_geometry,"Degenerate transformed normal.");return std::array<float,3>{static_cast<float>(v[0]/size),static_cast<float>(v[1]/size),static_cast<float>(v[2]/size)};};
                for(std::size_t n=0;n<positions.count;++n)
                {
                    std::array<double,3> position{float_at(positions,n,0),float_at(positions,n,1),float_at(positions,n,2)};
                    for(std::size_t r=0;r<3u;++r){const auto value=world[r]*position[0]+world[4+r]*position[1]+world[8+r]*position[2]+world[12+r];if(!std::isfinite(value)||std::abs(value)>1e9)refuse(Code::invalid_geometry,"Transformed position exceeds mesh coordinates.");points[n].position[r]=static_cast<float>(value);}
                    points[n].color={1,1,1};points[n].normal={0,0,0};
                    if(normals){std::array<double,3> normal{float_at(*normals,n,0),float_at(*normals,n,1),float_at(*normals,n,2)},out{};for(std::size_t r=0;r<3u;++r)out[r]=inverseTranspose[r*3]*normal[0]+inverseTranspose[r*3+1]*normal[1]+inverseTranspose[r*3+2]*normal[2];points[n].normal=normalized(out);}
                    if(uv)points[n].uv={float_at(*uv,n,0),float_at(*uv,n,1)};
                }
                for(auto value:triangles)if(value>=points.size())refuse(Code::invalid_geometry,"Triangle index exceeds vertex count.");
                if(det<0)for(std::size_t n=0;n<triangles.size();n+=3)std::swap(triangles[n+1],triangles[n+2]);
                if(!normals)
                {
                    std::vector<std::array<double,3>> sums(points.size());
                    for(std::size_t n=0;n<triangles.size();n+=3){const auto& x=points[triangles[n]].position;const auto& y=points[triangles[n+1]].position;const auto& z=points[triangles[n+2]].position;
                        const std::array<double,3> cross{(double(y[1])-x[1])*(double(z[2])-x[2])-(double(y[2])-x[2])*(double(z[1])-x[1]),(double(y[2])-x[2])*(double(z[0])-x[0])-(double(y[0])-x[0])*(double(z[2])-x[2]),(double(y[0])-x[0])*(double(z[1])-x[1])-(double(y[1])-x[1])*(double(z[0])-x[0])};
                        for(std::size_t v=0;v<3;++v)for(std::size_t r=0;r<3;++r)sums[triangles[n+v]][r]+=cross[r];}
                    for(std::size_t n=0;n<points.size();++n)points[n].normal=normalized(sums[n]);
                }
                std::optional<std::uint32_t> material;
                if(p.contains("material")){material=index(p.at("material"),output.materials.size());const auto& m=output.materials[*material];require(uv||!(m.base_color_texture||m.metallic_roughness_texture||m.normal_texture||m.occlusion_texture||m.emissive_texture),"Textured primitive needs TEXCOORD_0.");}
                const auto artifact=mesh::build_artifact(points,triangles,limits.geometry);
                if(!artifact)refuse(artifact.code==mesh::Code::limit_exceeded?Code::limit_exceeded:Code::invalid_geometry,"Static mesh artifact validation failed.");
                output.primitives.push_back({artifact.artifact,material,node,meshIndex,primitiveIndex,!normals,uv!=nullptr,det<0});
            }
            void read_scene()
            {
                const auto& nodes=array(document,"nodes");const auto& meshes=array(document,"meshes");bounded(nodes.size()<=limits.maximum_nodes&&meshes.size()<=limits.maximum_primitives);
                require(!nodes.empty()&&!meshes.empty(),"No static scene geometry.");
                std::vector<std::uint32_t> parents(nodes.size(),missing);std::vector<Matrix> local;
                for(std::size_t n=0;n<nodes.size();++n)
                {
                    require(nodes[n].is_object(),"Invalid node.");local.push_back(local_matrix(nodes[n]));
                    for(const auto& child:array(nodes[n],"children")){const auto c=index(child,nodes.size());require(c!=n&&parents[c]==missing,"Node graph has duplicate parents or a cycle.");parents[c]=static_cast<std::uint32_t>(n);}
                }
                for(std::size_t n=0;n<nodes.size();++n){auto p=static_cast<std::uint32_t>(n);unsigned depth{};while(p!=missing){require(++depth<=64u,"Cyclic or excessively deep node graph.");p=parents[p];}}
                std::vector<std::uint32_t> roots;const auto& scenes=array(document,"scenes");
                if(!scenes.empty())
                {
                    const auto selected=number(document,"scene",0u);require(selected<scenes.size(),"Default scene is invalid.");output.selected_scene=static_cast<std::uint32_t>(selected);
                    const auto& scene=scenes[static_cast<std::size_t>(selected)];require(scene.is_object(),"Invalid scene.");no_extensions(scene);
                    for(const auto& root:array(scene,"nodes")){const auto n=index(root,nodes.size());require(parents[n]==missing&&std::find(roots.begin(),roots.end(),n)==roots.end(),"Scene roots overlap.");roots.push_back(n);}
                }
                else {require(!document.contains("scene"),"Scene index without scenes.");for(std::size_t n=0;n<nodes.size();++n)if(parents[n]==missing)roots.push_back(static_cast<std::uint32_t>(n));}
                std::function<void(std::uint32_t,const Matrix&)> visit=[&](std::uint32_t n,const Matrix& parent)
                {
                    const auto world=multiply(parent,local[n]);
                    if(nodes[n].contains("mesh"))
                    {
                        const auto m=index(nodes[n].at("mesh"),meshes.size());const auto& source=meshes[m];require(source.is_object(),"Invalid mesh.");no_extensions(source);supported(!source.contains("weights"),"Mesh morph weights are unsupported.");
                        const auto& primitives=array(source,"primitives");require(!primitives.empty(),"Mesh has no primitives.");bounded(primitives.size()<=limits.maximum_primitives);
                        for(std::size_t p=0;p<primitives.size();++p)primitive(primitives[p],world,n,m,static_cast<std::uint32_t>(p));
                    }
                    for(const auto& child:array(nodes[n],"children"))visit(index(child,nodes.size()),world);
                };
                for(const auto n:roots)visit(n,identity());require(!output.primitives.empty(),"Selected scene has no triangle primitives.");
            }
        };
    }

    Result read_glb(std::span<const std::byte> bytes,Limits limits) noexcept
    {
        try
        {
            bounded(valid_limits(limits));bounded(bytes.size()<=limits.maximum_file_bytes);
            require(bytes.size()>=28u && word(bytes,0)==0x46546c67u && word(bytes,4)==2u && word(bytes,8)==bytes.size(),"Invalid GLB2 header or file length.");
            std::span<const std::byte> json,binary;std::size_t offset=12u;unsigned chunks{};
            while(offset<bytes.size())
            {
                require(bytes.size()-offset>=8u,"Truncated GLB chunk header.");const auto length=word(bytes,offset),type=word(bytes,offset+4u);offset+=8u;
                require(length%4u==0u && length<=bytes.size()-offset,"Invalid GLB chunk length.");
                if(type==0x4e4f534au){require(chunks==0u&&json.empty(),"GLB JSON chunk must be first and unique.");bounded(length<=limits.maximum_json_bytes);json=bytes.subspan(offset,length);}
                else if(type==0x004e4942u){require(chunks==1u&&binary.empty(),"GLB binary chunk must follow JSON and be unique.");binary=bytes.subspan(offset,length);}
                else refuse(Code::unsupported_feature,"Unknown GLB chunks are not admitted.");
                offset+=length;++chunks;
            }
            require(!json.empty()&&!binary.empty()&&chunks==2u,"GLB needs JSON and embedded binary chunks.");
            std::uint32_t values{};std::array<std::set<std::string>,66> objectKeys;
            auto callback=[&](int depth,Json::parse_event_t event,Json& parsed)
            {
                bounded(depth>=0&&depth<=64 && ++values<=limits.maximum_json_values);
                if(event==Json::parse_event_t::object_start)objectKeys[static_cast<std::size_t>(depth)+1u].clear();
                if(event==Json::parse_event_t::key){require(parsed.is_string(),"Invalid JSON key.");require(objectKeys[static_cast<std::size_t>(depth)].insert(parsed.get<std::string>()).second,"Duplicate JSON keys are not admitted.");}
                return true;
            };
            const auto* begin=reinterpret_cast<const char*>(json.data());
            Reader reader{limits,Json::parse(begin,begin+json.size(),callback),binary};
            auto& document=reader.document;require(document.is_object()&&document.contains("asset")&&document.at("asset").is_object(),"glTF asset metadata is missing.");
            const auto& asset=document.at("asset");require(asset.value("version",std::string{})=="2.0"&&(!asset.contains("minVersion")||asset.at("minVersion")=="2.0"),"Only glTF2 assets are supported.");
            no_extensions(document);no_extensions(asset);
            supported(array(document,"extensionsRequired").empty()&&array(document,"extensionsUsed").empty(),"glTF extensions need a separately supported importer.");
            supported(array(document,"animations").empty()&&array(document,"skins").empty()&&array(document,"cameras").empty(),"Animation, skinning and cameras are unsupported.");
            reader.read_buffers();reader.read_images();reader.read_materials();reader.read_scene();
            reader.output.source_content=texture::deterministic_content_hash("epoch.model.glb.v1",bytes);reader.output.code=Code::ready;
            reader.output.diagnostic="Static GLB validated; encoded images/material metadata retained. Project import and texture compilation are separate.";
            return std::move(reader.output);
        }
        catch(const Failure& failure){return {.code=failure.code,.diagnostic=failure.reason};}
        catch(const std::bad_alloc&){return {.code=Code::allocation_failure,.diagnostic="Model import allocation failed."};}
        catch(...){return {.code=Code::invalid_format,.diagnostic="Malformed or unsupported GLB metadata."};}
    }
    BaseColorCompilation compile_base_color_glb(std::span<const std::byte> bytes,
        Limits limits, std::uint32_t maximum_texture_dimension,
        std::uint64_t maximum_texture_bytes) noexcept
    {
        try
        {
            if (maximum_texture_dimension == 0u || maximum_texture_dimension > 16'384u
                || maximum_texture_bytes == 0u || maximum_texture_bytes > 256ull * 1024ull * 1024ull)
                return {.code=Code::limit_exceeded,.diagnostic="Invalid base-color texture limits."};
            auto model = read_glb(bytes, limits);
            if (!model) return {.code=model.code,.diagnostic=model.diagnostic};
            BaseColorCompilation result{};
            std::uint64_t texture_bytes{};
            for (const auto& primitive : model.primitives)
            {
                if (!primitive.material || !primitive.texcoords_present)
                    return {.code=Code::unsupported_feature,.diagnostic="Project base-color compilation needs a material and TEXCOORD_0 on every primitive."};
                const auto& material = model.materials.at(*primitive.material);
                if (!material.base_color_texture || material.alpha_mode != AlphaMode::opaque
                    || material.double_sided || material.base_color != std::array<float,4>{1,1,1,1}
                    || material.metallic != 0.0f || material.roughness != 1.0f
                    || material.emissive != std::array<float,3>{}
                    || material.metallic_roughness_texture || material.normal_texture
                    || material.occlusion_texture || material.emissive_texture)
                    return {.code=Code::unsupported_feature,.diagnostic="Project base-color compilation supports opaque, single-sided, unit-factor base color only. PBR, tint, emissive and alpha behavior were not discarded."};
                const auto image_index = material.base_color_texture->image;
                const auto existing = std::ranges::find(result.textures, image_index,
                    &CompiledBaseColorTexture::source_image);
                if (existing != result.textures.end())
                {
                    result.primitive_textures.push_back(static_cast<std::uint32_t>(existing - result.textures.begin()));
                    continue;
                }
                const auto& image = model.images.at(image_index);
                if (image.mime_type != "image/png")
                    return {.code=Code::unsupported_feature,.diagnostic="Project base-color compilation currently requires a fully decoded embedded PNG."};
                texture::TextureCompileProfile profile{};
                profile.format = texture::ArtifactFormat::rgba8_srgb;
                profile.color_space = texture::ColorSpace::srgb;
                auto compiled = texture::import_texture_memory({.source=image.encoded,
                    .extension=".png",.profile=profile,
                    .limits={.maximum_source_bytes=limits.maximum_image_bytes,
                        .maximum_decoded_bytes=maximum_texture_bytes,
                        .maximum_dimension=maximum_texture_dimension}});
                if (!compiled)
                    return {.code=Code::invalid_format,.diagnostic="An embedded base-color PNG failed decoding or texture compilation."};
                const auto retained_bytes = compiled.artifact.identity.estimated_artifact_bytes;
                if (retained_bytes > maximum_texture_bytes - texture_bytes)
                    return {.code=Code::limit_exceeded,.diagnostic="The compiled base-color texture set exceeds its byte budget."};
                texture_bytes += retained_bytes;
                result.primitive_textures.push_back(static_cast<std::uint32_t>(result.textures.size()));
                result.textures.push_back({image_index, std::move(compiled.artifact), compiled.decoded_bytes});
            }
            result.model = std::move(model);
            result.code = Code::ready;
            result.diagnostic = "Opaque base-color mesh/texture artifacts compiled. Project approval, publication and rendering are separate.";
            return result;
        }
        catch (const std::bad_alloc&)
        { return {.code=Code::allocation_failure,.diagnostic="Base-color model compilation allocation failed."}; }
        catch (...)
        { return {.code=Code::invalid_format,.diagnostic="Base-color model compilation failed without publishing assets."}; }
    }

    Result read_glb_file(const std::filesystem::path& path,Limits limits) noexcept
    {
        try
        {
            if(!valid_limits(limits))return {.code=Code::limit_exceeded,.diagnostic="Invalid import limits."};
            const auto status=std::filesystem::symlink_status(path);
            if(!std::filesystem::is_regular_file(status)||std::filesystem::is_symlink(status))return {.code=Code::read_failed,.diagnostic="Choose a regular GLB file."};
            std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)return {.code=Code::read_failed};
            const auto length=file.tellg();if(length<0||std::uint64_t(length)>limits.maximum_file_bytes)return {.code=Code::limit_exceeded};
            std::vector<std::byte> bytes(static_cast<std::size_t>(length));file.seekg(0);
            if(!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))||file.peek()!=std::char_traits<char>::eof())return {.code=Code::read_failed};
            return read_glb(bytes,limits);
        }
        catch(const std::bad_alloc&){return {.code=Code::allocation_failure};}
        catch(...){return {.code=Code::read_failed};}
    }
}
