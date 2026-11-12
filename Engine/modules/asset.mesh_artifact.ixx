// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>
#include <vector>

export module asset.mesh_artifact;
import asset.texture_artifact;

export namespace epochengine::asset::mesh
{
    using ContentHash = texture::ContentHash;
    struct Vertex final
    {
        std::array<float, 3> position{};
        std::array<float, 3> normal{0.0f, 1.0f, 0.0f};
        std::array<float, 2> uv{};
        std::array<float, 3> color{0.55f, 0.65f, 0.40f};
    };
    struct Limits final
    {
        std::uint32_t maximum_vertices{65'536u};
        std::uint32_t maximum_indices{196'608u};
        std::uint64_t maximum_bytes{4ull * 1024ull * 1024ull};
    };
    enum class Code : std::uint8_t
    {
        ready, invalid_geometry, limit_exceeded, invalid_format,
        content_mismatch, invalid_path, read_failed, allocation_failed
    };
    class Artifact;
    struct Result final
    {
        Code code{Code::invalid_geometry};
        std::shared_ptr<const Artifact> artifact{};
        [[nodiscard]] explicit operator bool() const noexcept
        { return code == Code::ready && static_cast<bool>(artifact); }
    };
    [[nodiscard]] Result build_artifact(std::span<const Vertex>,
        std::span<const std::uint32_t>, Limits = {}) noexcept;
    [[nodiscard]] Result decode_artifact(std::span<const std::byte>, Limits) noexcept;
    class Artifact final
    {
        friend Result build_artifact(std::span<const Vertex>,
            std::span<const std::uint32_t>, Limits) noexcept;
        friend Result decode_artifact(std::span<const std::byte>, Limits) noexcept;
        std::vector<Vertex> vertices_{};
        std::vector<std::uint32_t> indices_{};
        ContentHash key_{};
        std::array<float, 3> minimum_{}, maximum_{};
        Artifact() = default;
        void derive_bounds() noexcept
        {
            minimum_ = maximum_ = vertices_.front().position;
            for (const auto& vertex : vertices_)
                for (std::size_t axis = 0u; axis < 3u; ++axis)
                {
                    minimum_[axis] = (std::min)(minimum_[axis], vertex.position[axis]);
                    maximum_[axis] = (std::max)(maximum_[axis], vertex.position[axis]);
                }
        }
    public:
        [[nodiscard]] std::span<const Vertex> vertices() const noexcept { return vertices_; }
        [[nodiscard]] std::span<const std::uint32_t> indices() const noexcept { return indices_; }
        [[nodiscard]] ContentHash key() const noexcept { return key_; }
        [[nodiscard]] auto minimum() const noexcept { return minimum_; }
        [[nodiscard]] auto maximum() const noexcept { return maximum_; }
    };
}
namespace epochengine::asset::mesh::detail
{
    inline constexpr std::uint32_t magic = 0x48534d45u; // EMSH, little endian.
    inline constexpr std::uint64_t header_bytes = 48u;
    [[nodiscard]] inline bool valid_limits(Limits limits) noexcept
    {
        return limits.maximum_vertices >= 3u && limits.maximum_vertices <= 1'048'576u
            && limits.maximum_indices >= 3u && limits.maximum_indices <= 6'291'456u
            && limits.maximum_bytes >= header_bytes + 3u * 44u + 3u * 4u
            && limits.maximum_bytes <= 64ull * 1024ull * 1024ull
            && limits.maximum_bytes <= (std::numeric_limits<std::size_t>::max)()
            && limits.maximum_bytes <= std::uint64_t((std::numeric_limits<std::streamsize>::max)());
    }
    [[nodiscard]] inline bool bounded(std::uint64_t vertices,
        std::uint64_t indices, Limits limits) noexcept
    {
        // Validate the policy first, so count arithmetic is bounded even when
        // the caller or encoded header supplies its largest integer values.
        return valid_limits(limits)
            && vertices >= 3u && vertices <= limits.maximum_vertices
            && indices >= 3u && indices <= limits.maximum_indices
            && indices % 3u == 0u
            && header_bytes + vertices * 44u + indices * 4u <= limits.maximum_bytes;
    }
    inline void append32(std::vector<std::byte>& bytes, std::uint32_t value)
    {
        for (unsigned shift = 0u; shift < 32u; shift += 8u)
            bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffu));
    }
    [[nodiscard]] inline std::uint32_t read32(std::span<const std::byte> bytes,
        std::size_t& offset) noexcept
    {
        std::uint32_t value{};
        if constexpr (std::endian::native == std::endian::little)
        {
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            offset += sizeof(value);
        }
        else
            for (unsigned shift = 0u; shift < 32u; shift += 8u)
                value |= std::to_integer<std::uint32_t>(bytes[offset++]) << shift;
        return value;
    }
    [[nodiscard]] inline Vertex read_vertex(std::span<const std::byte> bytes,
        std::size_t offset) noexcept
    {
        Vertex vertex{};
        const auto real = [&]() { return std::bit_cast<float>(read32(bytes, offset)); };
        for (auto& value : vertex.position) value = real();
        for (auto& value : vertex.normal) value = real();
        for (auto& value : vertex.uv) value = real();
        for (auto& value : vertex.color) value = real();
        return vertex;
    }
    template<class VertexAt, class IndexAt, class PositionAt>
    [[nodiscard]] inline bool valid_geometry(std::size_t vertex_count,
        std::size_t index_count, VertexAt vertex_at, IndexAt index_at, PositionAt position_at) noexcept
    {
        for (std::size_t index = 0u; index < vertex_count; ++index)
        {
            const auto& vertex = vertex_at(index);
            for (float value : vertex.position)
                if (!std::isfinite(value) || std::abs(value) > 1.0e9f) return false;
            float normal_squared{};
            for (float value : vertex.normal)
            {
                if (!std::isfinite(value)) return false;
                normal_squared += value * value;
            }
            if (!std::isfinite(normal_squared) || normal_squared < 1.0e-12f) return false;
            for (float value : vertex.uv) if (!std::isfinite(value)) return false;
            for (float value : vertex.color)
                if (!std::isfinite(value) || value < 0.0f || value > 1.0f) return false;
        }
        for (std::size_t index = 0u; index < index_count; index += 3u)
        {
            const auto a = index_at(index), b = index_at(index + 1u), c = index_at(index + 2u);
            if (a >= vertex_count || b >= vertex_count || c >= vertex_count
                || a == b || b == c || a == c) return false;
            const auto p = position_at(a), q = position_at(b), r = position_at(c);
            const double x = (double(q[1])-p[1])*(double(r[2])-p[2])-(double(q[2])-p[2])*(double(r[1])-p[1]);
            const double y = (double(q[2])-p[2])*(double(r[0])-p[0])-(double(q[0])-p[0])*(double(r[2])-p[2]);
            const double z = (double(q[0])-p[0])*(double(r[1])-p[1])-(double(q[1])-p[1])*(double(r[0])-p[0]);
            if (x*x + y*y + z*z <= 1.0e-24) return false;
        }
        return true;
    }
    inline void append_payload(std::vector<std::byte>& bytes,
        std::span<const Vertex> vertices, std::span<const std::uint32_t> indices)
    {
        const auto start = bytes.size();
        bytes.resize(start + 8u + vertices.size() * 44u + indices.size() * 4u);
        auto* cursor = bytes.data() + start;
        const auto write32 = [&cursor](std::uint32_t value) {
            if constexpr (std::endian::native == std::endian::little)
            {
                std::memcpy(cursor, &value, sizeof(value));
                cursor += sizeof(value);
            }
            else
                for (unsigned shift = 0u; shift < 32u; shift += 8u)
                    *cursor++ = static_cast<std::byte>((value >> shift) & 0xffu);
        };
        write32(static_cast<std::uint32_t>(vertices.size()));
        write32(static_cast<std::uint32_t>(indices.size()));
        for (const auto& vertex : vertices)
        {
            const auto real = [&](float value) { write32(std::bit_cast<std::uint32_t>(value == 0.0f ? 0.0f : value)); };
            for (float value : vertex.position) real(value);
            for (float value : vertex.normal) real(value);
            for (float value : vertex.uv) real(value);
            for (float value : vertex.color) real(value);
        }
        for (const auto index : indices) write32(index);
    }
    [[nodiscard]] inline ContentHash content_key(std::span<const Vertex> vertices,
        std::span<const std::uint32_t> indices)
    {
        std::vector<std::byte> bytes{};
        bytes.reserve(8u + vertices.size() * 44u + indices.size() * 4u);
        append_payload(bytes, vertices, indices);
        return texture::deterministic_content_hash("epoch.mesh.artifact.v1", bytes);
    }
}
export namespace epochengine::asset::mesh
{
    [[nodiscard]] inline Result build_artifact(std::span<const Vertex> vertices,
        std::span<const std::uint32_t> indices, Limits limits) noexcept
    {
        if (!detail::bounded(vertices.size(), indices.size(), limits))
            return {Code::limit_exceeded};
        if (!detail::valid_geometry(vertices.size(), indices.size(),
                [&](std::size_t index) -> const Vertex& { return vertices[index]; },
                [&](std::size_t index) { return indices[index]; },
                [&](std::size_t index) { return vertices[index].position; })) return {};
        try
        {
            auto artifact = std::shared_ptr<Artifact>{new Artifact{}};
            artifact->vertices_.assign(vertices.begin(), vertices.end());
            artifact->indices_.assign(indices.begin(), indices.end());
            artifact->key_ = detail::content_key(vertices, indices);
            artifact->derive_bounds();
            return {Code::ready, std::move(artifact)};
        }
        catch (...) { return {Code::allocation_failed}; }
    }
    [[nodiscard]] inline std::vector<std::byte> encode_artifact(const Artifact& artifact) noexcept
    {
        try
        {
            std::vector<std::byte> bytes{};
            bytes.reserve(detail::header_bytes + artifact.vertices().size() * 44u + artifact.indices().size() * 4u);
            detail::append32(bytes, detail::magic);
            detail::append32(bytes, 1u);
            for (const auto word : artifact.key().words)
            {
                detail::append32(bytes, static_cast<std::uint32_t>(word));
                detail::append32(bytes, static_cast<std::uint32_t>(word >> 32u));
            }
            detail::append_payload(bytes, artifact.vertices(), artifact.indices());
            return bytes;
        }
        catch (...) { return {}; }
    }
    [[nodiscard]] inline Result decode_artifact(std::span<const std::byte> bytes,
        Limits limits = {}) noexcept
    {
        if (!detail::valid_limits(limits)) return {Code::limit_exceeded};
        if (bytes.size() < detail::header_bytes || bytes.size() > limits.maximum_bytes)
            return {Code::invalid_format};
        std::size_t offset{};
        if (detail::read32(bytes, offset) != detail::magic || detail::read32(bytes, offset) != 1u)
            return {Code::invalid_format};
        ContentHash expected{};
        for (auto& word : expected.words)
        {
            word = detail::read32(bytes, offset);
            word |= static_cast<std::uint64_t>(detail::read32(bytes, offset)) << 32u;
        }
        const auto vertex_count = detail::read32(bytes, offset), index_count = detail::read32(bytes, offset);
        if (!detail::bounded(vertex_count, index_count, limits)
            || bytes.size() != detail::header_bytes + std::uint64_t(vertex_count) * 44u + std::uint64_t(index_count) * 4u)
            return {Code::limit_exceeded};
        const auto vertex_at = [&](std::size_t index)
        { return detail::read_vertex(bytes, detail::header_bytes + index * 44u); };
        const auto index_at = [&](std::size_t index)
        {
            auto at = static_cast<std::size_t>(detail::header_bytes + std::uint64_t(vertex_count) * 44u + index * 4u);
            return detail::read32(bytes, at);
        };
        const auto position_at = [&](std::size_t index)
        {
            auto at = static_cast<std::size_t>(detail::header_bytes + index * 44u);
            std::array<float, 3> position{};
            for (auto& value : position) value = std::bit_cast<float>(detail::read32(bytes, at));
            return position;
        };
        // Keep malformed geometry allocation-free. Triangle validation
        // needs positions only; the full vertex channels are checked once.
        if (!detail::valid_geometry(vertex_count, index_count, vertex_at, index_at, position_at)) return {};
        try
        {
            auto artifact = std::shared_ptr<Artifact>{new Artifact{}};
            artifact->vertices_.resize(vertex_count);
            artifact->indices_.resize(index_count);
            bool noncanonical_zero{};
            const auto needs_zero_normalization = [](const auto& values) {
                return std::any_of(values.begin(), values.end(), [](float value) {
                    return value == 0.0f && std::bit_cast<std::uint32_t>(value) != 0u;
                });
            };
            for (auto& vertex : artifact->vertices_)
            {
                vertex = detail::read_vertex(bytes, offset); offset += 44u;
                noncanonical_zero = noncanonical_zero || needs_zero_normalization(vertex.position)
                    || needs_zero_normalization(vertex.normal) || needs_zero_normalization(vertex.uv) || needs_zero_normalization(vertex.color);
            }
            for (auto& index : artifact->indices_) index = detail::read32(bytes, offset);
            // Version1 hashes counts and payload with signed zero normalized.
            // Normal producer bytes are already canonical; rare legacy -0 input
            // retains the original canonicalizing path and identical key policy.
            artifact->key_ = noncanonical_zero
                ? detail::content_key(artifact->vertices_, artifact->indices_)
                : texture::deterministic_content_hash("epoch.mesh.artifact.v1", bytes.subspan(40u));
            if (artifact->key_ != expected) return {Code::content_mismatch};
            artifact->derive_bounds();
            return {Code::ready, std::move(artifact)};
        }
        catch (...) { return {Code::allocation_failed}; }
    }
    // Explicit import of an encoded data artifact; this never executes a producer.
    [[nodiscard]] inline Result read_artifact_file(const std::filesystem::path& path,
        Limits limits = {}) noexcept
    {
        if (!detail::valid_limits(limits)) return {Code::limit_exceeded};
        try
        {
            if (!std::filesystem::is_regular_file(path)) return {Code::read_failed};
            std::ifstream source(path, std::ios::binary | std::ios::ate);
            if (!source) return {Code::read_failed};
            const auto length = source.tellg();
            if (length < 0 || std::uint64_t(length) > limits.maximum_bytes) return {Code::limit_exceeded};
            if (std::uint64_t(length) < detail::header_bytes) return {Code::invalid_format};
            std::array<std::byte, detail::header_bytes> header{};
            source.seekg(0);
            if (!source.read(reinterpret_cast<char*>(header.data()), std::streamsize(header.size())))
                return {Code::read_failed};
            std::size_t at{};
            if (detail::read32(header, at) != detail::magic || detail::read32(header, at) != 1u)
                return {Code::invalid_format};
            at = 40u;
            const auto vertices = detail::read32(header, at), indices = detail::read32(header, at);
            if (!detail::bounded(vertices, indices, limits)
                || std::uint64_t(length) != detail::header_bytes + std::uint64_t(vertices) * 44u + std::uint64_t(indices) * 4u)
                return {Code::limit_exceeded};
            std::vector<std::byte> bytes(static_cast<std::size_t>(length));
            std::copy(header.begin(), header.end(), bytes.begin());
            if (!source.read(reinterpret_cast<char*>(bytes.data() + header.size()),
                    static_cast<std::streamsize>(bytes.size() - header.size())))
                return {Code::read_failed};
            // Refuse growth instead of silently admitting a prefix of a changing file.
            if (source.peek() != std::char_traits<char>::eof()) return {Code::read_failed};
            return decode_artifact(bytes, limits);
        }
        catch (...) { return {Code::read_failed}; }
    }
    // Check each existing ancestor before creating its child. A project Assets
    // junction must not make an import create directories outside the project.
    [[nodiscard]] inline std::optional<std::filesystem::path> prepare_project_mesh_directory(
        const std::filesystem::path& project_root) noexcept
    {
        try
        {
            const auto root = std::filesystem::canonical(project_root);
            auto directory = root;
            for (const auto* name : {"Assets", "Mesh"})
            {
                const auto next = directory / name;
                if (!std::filesystem::exists(next))
                {
                    if (std::filesystem::is_symlink(std::filesystem::symlink_status(next))) return {};
                    if (!std::filesystem::create_directory(next)) return {};
                }
                const auto canonical = std::filesystem::canonical(next);
                const auto relative = canonical.lexically_relative(root);
                if (relative.empty() || relative.is_absolute()) return {};
                for (const auto& part : relative) if (part == "..") return {};
                if (!std::filesystem::is_directory(canonical)) return {};
                directory = canonical;
            }
            return directory;
        }
        catch (...) { return {}; }
    }
    // This is project-relative file admission, not OS confinement or package trust.
    [[nodiscard]] inline Result read_project_artifact(const std::filesystem::path& project_root,
        std::string_view logical_path, ContentHash expected, Limits limits = {}) noexcept
    {
        if (!detail::valid_limits(limits)) return {Code::limit_exceeded};
        try
        {
            const std::filesystem::path relative{logical_path};
            if (project_root.empty() || logical_path.empty() || relative.is_absolute()
                || logical_path.find('\\') != std::string_view::npos
                || logical_path.find(':') != std::string_view::npos
                || logical_path.back() == '/' || logical_path.find("//") != std::string_view::npos
                || expected.empty()) return {Code::invalid_path};
            for (const unsigned char character : logical_path)
                if (character < 0x20u || character == 0x7fu) return {Code::invalid_path};
            for (const auto& part : relative)
                if (part == "." || part == ".." || part.empty()) return {Code::invalid_path};
            if (relative.begin() == relative.end()
                || (*relative.begin() != "Assets" && *relative.begin() != "Library")) return {Code::invalid_path};
            const auto root = std::filesystem::canonical(project_root);
            const auto target = std::filesystem::canonical(root / relative);
            const auto admitted = target.lexically_relative(root);
            if (admitted.empty() || admitted.is_absolute()) return {Code::invalid_path};
            for (const auto& part : admitted) if (part == "..") return {Code::invalid_path};
            auto result = read_artifact_file(target, limits);
            if (result && result.artifact->key() != expected) return {Code::content_mismatch};
            return result;
        }
        catch (...) { return {Code::read_failed}; }
    }
}
