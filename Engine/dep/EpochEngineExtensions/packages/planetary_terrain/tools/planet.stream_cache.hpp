// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Private producer persistence; no Engine or renderer dependency.
#pragma once
#include <array>
#include <algorithm>
#include <bit>
#include <charconv>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

namespace planet_stream_cache
{
    // The producer already validated and encoded this exact artifact. Verify
    // every written byte without decoding/copying/hashing its geometry again.
    // Fixed scratch also avoids another artifact-sized allocation at publication.
    inline bool matches_bytes(std::istream& input,std::span<const std::byte> expected)
    {
        if(!input.good())return false;
        std::array<std::byte,64u*1024u> buffer;
        while(!expected.empty())
        {
            const auto count=(std::min)(expected.size(),buffer.size());
            if(!input.read(reinterpret_cast<char*>(buffer.data()),static_cast<std::streamsize>(count))
                ||!std::equal(buffer.begin(),buffer.begin()+count,expected.begin()))return false;
            expected=expected.subspan(count);
        }
        return input.peek()==std::char_traits<char>::eof()&&!input.bad();
    }
    struct Options final
    {
        std::uint32_t seed{}, cells{};
        std::size_t body{};
        bool ocean_surface{};
        std::uint32_t size_reference_body{};
        double reference_radius_meters{6'371'000.0};
    };
    inline std::string encode_options(const Options& options)
    {
        const bool sized=options.size_reference_body!=0u||options.reference_radius_meters!=6'371'000.0;
        auto result=std::string{sized?"epoch.planet.stream-cache.v3\n":options.ocean_surface?"epoch.planet.stream-cache.v2\n":"epoch.planet.stream-cache.v1\n"}
            +"seed="+std::to_string(options.seed)+"\ncells="+std::to_string(options.cells)
            +"\nbody="+std::to_string(options.body)+"\n";
        if(options.ocean_surface||sized)result+="ocean_surface="+std::to_string(options.ocean_surface?1:0)+"\n";
        if(sized){std::ostringstream size;size<<std::setprecision(17)<<options.reference_radius_meters;
            result+="size_reference_body="+std::to_string(options.size_reference_body)+"\nreference_radius_meters="+size.str()+"\n";}
        return result;
    }
    template<class T> bool write_scalar(std::ostream& output,T value)
    {
        if constexpr(std::is_floating_point_v<T>)
        {
            if constexpr(sizeof(T)==4u) return write_scalar(output,std::bit_cast<std::uint32_t>(value));
            else return write_scalar(output,std::bit_cast<std::uint64_t>(value));
        }
        else
        {
            static_assert(std::is_unsigned_v<T>);
            std::array<char,sizeof(T)> bytes{};
            for(std::size_t i=0;i<sizeof(T);++i) bytes[i]=static_cast<char>((value>>(i*8u))&0xffu);
            output.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
            return output.good();
        }
    }
    template<class T> bool read_scalar(std::istream& input,T& value)
    {
        if constexpr(std::is_floating_point_v<T>)
        {
            if constexpr(sizeof(T)==4u) {std::uint32_t bits{};if(!read_scalar(input,bits))return false;value=std::bit_cast<T>(bits);}
            else {std::uint64_t bits{};if(!read_scalar(input,bits))return false;value=std::bit_cast<T>(bits);}
        }
        else
        {
            static_assert(std::is_unsigned_v<T>);value=0;
            // Read one bounded scalar through the existing buffered stream.
            // Per-byte istream::get constructs a sentry for every mesh byte;
            // direct streambuf reads retain byte order and exact short-read refusal.
            std::array<unsigned char,sizeof(T)> bytes{};
            if(input.rdbuf()->sgetn(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))
                !=static_cast<std::streamsize>(bytes.size()))return false;
            for(std::size_t i=0;i<sizeof(T);++i)
                value|=static_cast<T>(bytes[i])<<(i*8u);
        }
        return static_cast<bool>(input);
    }
    template<class V> bool write_vec(std::ostream& output,const V& value)
    {return write_scalar(output,value.x)&&write_scalar(output,value.y)&&write_scalar(output,value.z);}
    template<class T> bool read_scalar(std::span<const std::byte>& input,T& value)
    {
        if(input.size()<sizeof(T))return false;
        if constexpr(std::is_floating_point_v<T>)
        {
            if constexpr(sizeof(T)==4u) {std::uint32_t bits{};if(!read_scalar(input,bits))return false;value=std::bit_cast<T>(bits);}
            else {std::uint64_t bits{};if(!read_scalar(input,bits))return false;value=std::bit_cast<T>(bits);}
        }
        else
        {
            static_assert(std::is_unsigned_v<T>);value=0;
            for(std::size_t i=0;i<sizeof(T);++i)
                value|=static_cast<T>(std::to_integer<unsigned char>(input[i]))<<(i*8u);
            input=input.subspan(sizeof(T));
        }
        return true;
    }
    template<class Input,class V> bool read_vec(Input& input,V& value)
    {return read_scalar(input,value.x)&&read_scalar(input,value.y)&&read_scalar(input,value.z);}
    template<class Key> std::string filename(Key key)
    {
        return "tile-"+std::to_string(key.body)+"-"+std::to_string(key.face)+"-"+std::to_string(key.level)
            +"-"+std::to_string(key.x)+"-"+std::to_string(key.y)+".epoch_patch";
    }
    template<class Key> bool parse_filename(std::string_view text,Key& key)
    {
        const auto original=text;
        constexpr std::string_view prefix="tile-",suffix=".epoch_patch";
        if(!text.starts_with(prefix)||!text.ends_with(suffix))return false;
        text.remove_prefix(prefix.size());text.remove_suffix(suffix.size());
        std::array<std::uint32_t,5> values{};
        for(std::size_t i=0;i<values.size();++i)
        {
            const auto end=i+1u==values.size()?text.size():text.find('-');
            if(end==std::string_view::npos||end==0u)return false;
            const auto result=std::from_chars(text.data(),text.data()+end,values[i]);
            if(result.ec!=std::errc{}||result.ptr!=text.data()+end)return false;
            text.remove_prefix(end);if(i+1u!=values.size())text.remove_prefix(1u);
        }
        if(!text.empty()||values[0]>255u||values[1]>=6u||values[2]>20u
            ||values[3]>=(1u<<values[2])||values[4]>=(1u<<values[2]))return false;
        key={values[3],values[4],static_cast<std::uint8_t>(values[0]),static_cast<std::uint8_t>(values[1]),static_cast<std::uint8_t>(values[2])};
        return filename(key)==original;
    }
    inline bool plain_path(const std::filesystem::path& path,bool require_directory)
    {
        const auto absolute=std::filesystem::absolute(path).lexically_normal();
        auto current=absolute.root_path();
        for(const auto& component:absolute.relative_path())
        {
            current/=component;
            const auto status=std::filesystem::symlink_status(current);
            std::error_code ec;
            (void)std::filesystem::read_symlink(current,ec);
            if(!ec||std::filesystem::is_symlink(status))return false;
            if(current!=absolute&&!std::filesystem::is_directory(status))return false;
        }
        const auto status=std::filesystem::symlink_status(absolute);
        return require_directory?std::filesystem::is_directory(status):std::filesystem::is_regular_file(status);
    }
    inline bool new_output(const std::filesystem::path& destination)
    {
        return !destination.empty()&&!destination.filename().empty()
            &&plain_path(std::filesystem::absolute(destination).parent_path(),true)
            &&std::filesystem::create_directory(destination);
    }
    template<class Key,class Mesh> bool write_patch(const std::filesystem::path& path,Key key,const Mesh& mesh)
    {
        std::ofstream output{path,std::ios::binary|std::ios::noreplace};
        if(!output)return false;
        output.write("EPLOD01",7);
        write_scalar(output,static_cast<std::uint32_t>(key.body));write_scalar(output,static_cast<std::uint32_t>(key.face));
        write_scalar(output,static_cast<std::uint32_t>(key.level));write_scalar(output,key.x);write_scalar(output,key.y);
        write_vec(output,mesh.anchor);write_scalar(output,mesh.bound_radius);
        for(const auto& corner:mesh.corners)write_vec(output,corner);
        write_scalar(output,static_cast<std::uint32_t>(mesh.vertices.size()));write_scalar(output,static_cast<std::uint32_t>(mesh.indices.size()));
        for(const auto& vertex:mesh.vertices)
        {
            write_vec(output,vertex.position);write_vec(output,vertex.normal);
            for(const auto value:vertex.color)write_scalar(output,value);
            write_vec(output,vertex.parent_delta);
        }
        for(const auto index:mesh.indices)write_scalar(output,index);
        output.close();return static_cast<bool>(output);
    }
    template<class Key,class Mesh> bool read_patch(const std::filesystem::path& path,Key expected,std::uint32_t cells,Mesh& mesh,
        bool ocean_surface=false)
    {
        if(cells<2u||cells>64u||!plain_path(path,false))return false;
        const std::uint64_t side=cells+1u,vertices=side*side+4u*cells,indices=6u*cells*cells+24u*cells;
        const std::uint64_t header_bytes=7u+5u*4u+4u*8u+4u*3u*4u+2u*4u;
        const auto bytes=std::filesystem::file_size(path);
        const std::uint64_t layers=bytes==header_bytes+vertices*52u+indices*4u?1u
            :ocean_surface&&bytes==header_bytes+2u*(vertices*52u+indices*4u)?2u:0u;
        if(!layers)return false;
        std::ifstream input{path,std::ios::binary};
        // Exact cell/layer sizes bound this temporary to at most675'035 bytes.
        // It is released before the next patch; mesh+encoded bytes fit the
        // existing conservative per-patch retained bound. Path admission above
        // is repeated for every file, including every ancestor/reparse check.
        std::vector<std::byte> encoded(static_cast<std::size_t>(bytes));
        if(!input.read(reinterpret_cast<char*>(encoded.data()),static_cast<std::streamsize>(encoded.size()))
            ||input.peek()!=std::char_traits<char>::eof())return false;
        std::span<const std::byte> payload{encoded};
        if(std::string_view{reinterpret_cast<const char*>(payload.data()),7u}!="EPLOD01")return false;
        payload=payload.subspan(7u);
        std::array<std::uint32_t,5> key{};for(auto& value:key)if(!read_scalar(payload,value))return false;
        if(key!=std::array<std::uint32_t,5>{expected.body,expected.face,expected.level,expected.x,expected.y})return false;
        if(!read_vec(payload,mesh.anchor)||!read_scalar(payload,mesh.bound_radius))return false;
        for(auto& corner:mesh.corners)if(!read_vec(payload,corner))return false;
        std::uint32_t nv{},ni{};if(!read_scalar(payload,nv)||!read_scalar(payload,ni)||nv!=vertices*layers||ni!=indices*layers)return false;
        mesh.vertices.resize(nv);mesh.indices.resize(ni);
        for(auto& vertex:mesh.vertices)
        {
            if(!read_vec(payload,vertex.position)||!read_vec(payload,vertex.normal))return false;
            for(auto& value:vertex.color)if(!read_scalar(payload,value))return false;
            if(!read_vec(payload,vertex.parent_delta))return false;
        }
        for(auto& index:mesh.indices)if(!read_scalar(payload,index))return false;
        return payload.empty();
    }
}
