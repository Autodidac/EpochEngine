/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

module ai.curated_context_bundle;
import core.sha256;

namespace epochengine::ai::curated_context_bundle
{
    namespace
    {
        using Bytes = std::vector<std::uint8_t>;
        constexpr std::array<std::uint8_t, 8> magic{
            'E','P','C','B','N','D','L','1'};
        constexpr std::size_t seal_size{32};

        [[nodiscard]] std::string sha(std::span<const std::uint8_t> v)
        { return core::sha256::hex(core::sha256::hash(v)); }
        [[nodiscard]] std::string sha(std::string_view v)
        { return core::sha256::hex(core::sha256::hash(v)); }
        [[nodiscard]] bool hex(std::string_view v) noexcept
        {
            return v.size() == 64 && std::all_of(v.begin(), v.end(),
                [](unsigned char c) { return std::isdigit(c)
                    || (c >= 'a' && c <= 'f'); });
        }
        [[nodiscard]] bool id(std::string_view v, std::size_t max = 128) noexcept
        {
            return !v.empty() && v.size() <= max
                && std::all_of(v.begin(), v.end(), [](unsigned char c) {
                    return std::isalnum(c) || c == '-' || c == '_' || c == ':';
                });
        }
        [[nodiscard]] std::string root_text(const std::filesystem::path& p)
        {
            std::string v = p.lexically_normal().generic_string();
            while (v.size() > 3 && v.back() == '/') v.pop_back();
            return v;
        }
        [[nodiscard]] bool relative_path(std::string_view v) noexcept
        {
            if (v.empty() || v.size() > 4096 || v.front() == '/'
                || v.back() == '/' || v.find('\\') != v.npos
                || v.find(':') != v.npos || v.find("//") != v.npos)
                return false;
            std::size_t at = 0; bool first = true;
            while (at < v.size())
            {
                const auto slash = v.find('/', at);
                const auto end = slash == v.npos ? v.size() : slash;
                const auto part = v.substr(at, end - at);
                if (part.empty() || part == "." || part == ".."
                    || (first && part == "Projects")) return false;
                first = false;
                if (slash == v.npos) break;
                at = slash + 1;
            }
            return true;
        }
        [[nodiscard]] bool utf8(std::string_view v) noexcept
        {
            for (std::size_t i = 0; i < v.size();)
            {
                const auto c = static_cast<std::uint8_t>(v[i]);
                if (c == 0x7f
                    || c == 0
                    || (c < 0x20 && c != '\n' && c != '\r' && c != '\t'))
                    return false;
                if (c < 0x80) { ++i; continue; }
                std::size_t n = (c & 0xe0) == 0xc0 ? 2
                    : (c & 0xf0) == 0xe0 ? 3 : (c & 0xf8) == 0xf0 ? 4 : 0;
                if (!n || i + n > v.size()) return false;
                std::uint32_t cp = c & (n == 2 ? 0x1f : n == 3 ? 0x0f : 0x07);
                for (std::size_t j = 1; j < n; ++j)
                {
                    const auto d = static_cast<std::uint8_t>(v[i + j]);
                    if ((d & 0xc0) != 0x80) return false;
                    cp = (cp << 6) | (d & 0x3f);
                }
                if ((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800)
                    || (n == 4 && cp < 0x10000) || cp > 0x10ffff
                    || (cp >= 0xd800 && cp <= 0xdfff)) return false;
                i += n;
            }
            return true;
        }
        [[nodiscard]] std::uint32_t lines(std::string_view v) noexcept
        {
            if (v.empty()) return 0;
            std::uint32_t n = 1;
            for (std::size_t i = 0; i + 1 < v.size(); ++i)
                if (v[i] == '\n') ++n;
            return n;
        }

        void put8(Bytes& b, std::uint8_t v) { b.push_back(v); }
        void put32(Bytes& b, std::uint32_t v)
        { for (int i = 0; i != 4; ++i) put8(b, std::uint8_t(v >> (i * 8))); }
        void put64(Bytes& b, std::uint64_t v)
        { for (int i = 0; i != 8; ++i) put8(b, std::uint8_t(v >> (i * 8))); }
        void puts(Bytes& b, std::string_view v)
        {
            put32(b, static_cast<std::uint32_t>(v.size()));
            if (!v.empty())
                b.insert(b.end(),
                    reinterpret_cast<const std::uint8_t*>(v.data()),
                    reinterpret_cast<const std::uint8_t*>(v.data()) + v.size());
        }
        void put_binding(Bytes& b, const Binding& v, bool canonical)
        {
            puts(b, v.project_id);
            puts(b, canonical ? root_text(v.reviewed_root)
                : v.reviewed_root.generic_string());
            puts(b, v.project_manifest_sha256); puts(b, v.project_profile_sha256);
            put64(b, v.reviewed_revision); put8(b, std::uint8_t(v.audience));
            put8(b, std::uint8_t(v.provider)); puts(b, v.model_binding);
            puts(b, v.endpoint_binding); put64(b, v.session_id);
            put64(b, v.request_id); puts(b, v.campaign_id);
            put64(b, v.campaign_generation); put8(b, v.operator_shared);
        }
        void put_limits(Bytes& b, const Limits& v)
        {
            put32(b, v.maximum_entries); put64(b, v.maximum_entry_bytes);
            put64(b, v.maximum_total_bytes); put64(b, v.maximum_chunk_bytes);
            put32(b, v.maximum_chunks);
        }
        void put_entry(Bytes& b, const ReviewedEntry& v)
        {
            puts(b, v.project_relative_path); puts(b, v.declared_symbol);
            put32(b, v.first_line); put32(b, v.last_line);
            put64(b, v.source_revision); puts(b, v.exact_bytes);
            puts(b, v.provenance.review_id); puts(b, v.provenance.reviewer_binding);
            puts(b, v.provenance.selection_sha256);
            put64(b, v.provenance.reviewed_at_unix_seconds);
        }
        [[nodiscard]] Bytes encode(const Binding& binding, const Limits& limits,
            std::span<const ReviewedEntry> entries, std::uint64_t generation,
            bool canonical = true)
        {
            Bytes b(magic.begin(), magic.end()); put32(b, format_version);
            put64(b, generation); put_binding(b, binding, canonical);
            put_limits(b, limits); put32(b, static_cast<std::uint32_t>(entries.size()));
            for (const auto& e : entries) put_entry(b, e);
            return b;
        }

        struct Cursor { std::span<const std::uint8_t> b; std::size_t at{}; };
        [[nodiscard]] bool get8(Cursor& c, std::uint8_t& v) noexcept
        { if (c.at >= c.b.size()) return false; v = c.b[c.at++]; return true; }
        [[nodiscard]] bool get32(Cursor& c, std::uint32_t& v) noexcept
        {
            v = 0; for (int i = 0; i != 4; ++i) { std::uint8_t x{};
                if (!get8(c, x)) return false; v |= std::uint32_t(x) << (i * 8); }
            return true;
        }
        [[nodiscard]] bool get64(Cursor& c, std::uint64_t& v) noexcept
        {
            v = 0; for (int i = 0; i != 8; ++i) { std::uint8_t x{};
                if (!get8(c, x)) return false; v |= std::uint64_t(x) << (i * 8); }
            return true;
        }
        [[nodiscard]] bool gets(Cursor& c, std::string& v, std::size_t max) noexcept
        {
            std::uint32_t n{}; if (!get32(c, n) || n > max || n > c.b.size() - c.at)
                return false;
            v.assign(reinterpret_cast<const char*>(c.b.data() + c.at), n);
            c.at += n; return true;
        }
        [[nodiscard]] bool get_binding(Cursor& c, Binding& v) noexcept
        {
            std::string root; std::uint8_t audience{}, provider{}, shared{};
            if (!gets(c, v.project_id, 128) || !gets(c, root, 4096)
                || !gets(c, v.project_manifest_sha256, 64)
                || !gets(c, v.project_profile_sha256, 64)
                || !get64(c, v.reviewed_revision) || !get8(c, audience)
                || audience > static_cast<std::uint8_t>(Audience::outbound_mcp)
                || !get8(c, provider)
                || provider > static_cast<std::uint8_t>(
                    project_profile::Provider::engine_selected)
                || !gets(c, v.model_binding, 256)
                || !gets(c, v.endpoint_binding, 1024)
                || !get64(c, v.session_id) || !get64(c, v.request_id)
                || !gets(c, v.campaign_id, 128)
                || !get64(c, v.campaign_generation) || !get8(c, shared)
                || shared > 1) return false;
            v.reviewed_root = root; v.audience = Audience(audience);
            v.provider = project_profile::Provider(provider);
            v.operator_shared = shared == 1; return true;
        }
        [[nodiscard]] bool get_limits(Cursor& c, Limits& v) noexcept
        {
            return get32(c, v.maximum_entries) && get64(c, v.maximum_entry_bytes)
                && get64(c, v.maximum_total_bytes)
                && get64(c, v.maximum_chunk_bytes) && get32(c, v.maximum_chunks);
        }
        [[nodiscard]] bool get_entry(Cursor& c, ReviewedEntry& v) noexcept
        {
            return gets(c, v.project_relative_path, 4096)
                && gets(c, v.declared_symbol, 256) && get32(c, v.first_line)
                && get32(c, v.last_line) && get64(c, v.source_revision)
                && gets(c, v.exact_bytes, 1024 * 1024)
                && gets(c, v.provenance.review_id, 128)
                && gets(c, v.provenance.reviewer_binding, 256)
                && gets(c, v.provenance.selection_sha256, 64)
                && get64(c, v.provenance.reviewed_at_unix_seconds);
        }
        [[nodiscard]] bool decode(std::span<const std::uint8_t> bytes,
            Request& request, std::uint64_t& generation) noexcept
        {
            Cursor c{bytes}; for (auto expected : magic) { std::uint8_t actual{};
                if (!get8(c, actual) || actual != expected) return false; }
            std::uint32_t version{}, count{};
            if (!get32(c, version) || version != format_version
                || !get64(c, generation) || !get_binding(c, request.binding)
                || !get_limits(c, request.limits) || !get32(c, count)
                || count > 256) return false;
            request.reviewed_entries.resize(count);
            for (auto& entry : request.reviewed_entries)
                if (!get_entry(c, entry)) return false;
            return c.at == c.b.size();
        }

        [[nodiscard]] std::string content_sha(const ReviewedEntry& e)
        { return sha(e.exact_bytes); }
        [[nodiscard]] std::string selection_sha(const ReviewedEntry& e)
        {
            return sha(e.project_relative_path + "\n" + e.declared_symbol + "\n"
                + std::to_string(e.first_line) + "\n" + std::to_string(e.last_line)
                + "\n" + std::to_string(e.source_revision) + "\n" + content_sha(e));
        }
        [[nodiscard]] std::string entry_sha(const ReviewedEntry& e)
        { Bytes b; puts(b, "epoch-curated-context-entry/v1"); put_entry(b, e); return sha(b); }
        [[nodiscard]] bool less_entry(const ReviewedEntry& a,
            const ReviewedEntry& b) noexcept
        {
            return std::tie(a.project_relative_path, a.first_line, a.last_line,
                a.declared_symbol, a.provenance.review_id)
                < std::tie(b.project_relative_path, b.first_line, b.last_line,
                b.declared_symbol, b.provenance.review_id);
        }
        [[nodiscard]] bool same_range(const ReviewedEntry& a,
            const ReviewedEntry& b) noexcept
        { return a.project_relative_path == b.project_relative_path
            && a.first_line == b.first_line && a.last_line == b.last_line
            && a.declared_symbol == b.declared_symbol; }
        [[nodiscard]] bool overlap(const ReviewedEntry& a,
            const ReviewedEntry& b) noexcept
        { return a.project_relative_path == b.project_relative_path
            && a.first_line <= b.last_line && b.first_line <= a.last_line; }

        [[nodiscard]] Result refuse(const Request& q, Code code, std::string status)
        {
            Result r{}; r.code = code; r.status = status; r.refusal = {
                .code = code, .project_id = q.binding.project_id,
                .campaign_id = q.binding.campaign_id,
                .session_id = q.binding.session_id, .request_id = q.binding.request_id,
                .expected_generation = q.expected_generation, .status = status};
            Bytes raw; puts(raw, "epoch-curated-context-attempt/v1");
            put_binding(raw, q.binding, false); put_limits(raw, q.limits);
            put64(raw, q.expected_generation);
            const auto count = static_cast<std::uint32_t>(
                (std::min<std::size_t>)(q.reviewed_entries.size(), 256));
            put32(raw, count);
            for (std::uint32_t i = 0; i < count; ++i) put_entry(raw, q.reviewed_entries[i]);
            r.refusal.request_sha256 = sha(raw);
            r.refusal.receipt_sha256 = sha(std::string{code_name(code)} + "\n"
                + r.refusal.project_id + "\n" + r.refusal.campaign_id + "\n"
                + std::to_string(r.refusal.session_id) + "\n"
                + std::to_string(r.refusal.request_id) + "\n"
                + r.refusal.request_sha256);
            return r;
        }

        [[nodiscard]] Result assemble(const Request& q, std::uint64_t generation,
            bool canonicalRoot)
        {
            const auto& b = q.binding; const std::string root = root_text(b.reviewed_root);
            if (!id(b.project_id) || !b.reviewed_root.is_absolute()
                || (canonicalRoot && root != b.reviewed_root.generic_string())
                || !hex(b.project_manifest_sha256) || !hex(b.project_profile_sha256)
                || !b.reviewed_revision || !b.session_id || !b.request_id
                || !id(b.campaign_id) || !b.campaign_generation || !b.operator_shared
                || b.model_binding.empty() || b.model_binding.size() > 256
                || b.endpoint_binding.empty() || b.endpoint_binding.size() > 1024)
                return refuse(q, Code::invalid_binding, "Invalid reviewed project/session binding.");
            if ((b.audience == Audience::local_model
                    && b.provider != project_profile::Provider::epoch_local_qwen38)
                || (b.audience == Audience::outbound_mcp
                    && b.provider != project_profile::Provider::external_mcp))
                return refuse(q, Code::provider_unresolved, "Audience/provider mismatch.");
            if (!q.limits.valid() || q.reviewed_entries.empty()
                || q.reviewed_entries.size() > q.limits.maximum_entries)
                return refuse(q, Code::budget_exceeded, "Entry count or limits exceed policy.");

            auto entries = q.reviewed_entries; std::sort(entries.begin(), entries.end(), less_entry);
            std::vector<ReviewedEntry> kept; std::uint64_t total{};
            for (const auto& e : entries)
            {
                if (!relative_path(e.project_relative_path))
                    return refuse(q, Code::path_rejected, "Path is noncanonical or crosses project scope.");
                if (e.declared_symbol.empty() || e.declared_symbol.size() > 256
                    || !utf8(e.declared_symbol) || !e.first_line
                    || e.last_line < e.first_line)
                    return refuse(q, Code::invalid_binding, "Symbol/range is invalid.");
                if (e.source_revision != b.reviewed_revision)
                    return refuse(q, Code::stale_revision, "Entry revision is stale.");
                if (e.exact_bytes.empty() || e.exact_bytes.size() > q.limits.maximum_entry_bytes)
                    return refuse(q, Code::budget_exceeded, "Entry byte budget exceeded.");
                if (!utf8(e.exact_bytes)
                    || lines(e.exact_bytes) != e.last_line - e.first_line + 1)
                    return refuse(q, Code::binary_rejected, "Only exact UTF-8 source ranges are admitted.");
                if (!id(e.provenance.review_id)
                    || !id(e.provenance.reviewer_binding, 256)
                    || !hex(e.provenance.selection_sha256)
                    || !e.provenance.reviewed_at_unix_seconds
                    || selection_sha(e) != e.provenance.selection_sha256)
                    return refuse(q, Code::invalid_binding, "Provenance does not bind the selection.");
                if (!kept.empty() && same_range(kept.back(), e))
                {
                    if (kept.back() == e) continue;
                    return refuse(q, Code::duplicate_conflict, "Duplicate selection conflicts.");
                }
                if (!kept.empty() && overlap(kept.back(), e))
                    return refuse(q, Code::overlapping_range, "Reviewed ranges overlap.");
                if (e.exact_bytes.size() > q.limits.maximum_total_bytes - total)
                    return refuse(q, Code::budget_exceeded, "Total byte budget exceeded.");
                total += e.exact_bytes.size(); kept.push_back(e);
            }

            Result r{}; r.code = Code::ready; r.bundle.binding = b;
            r.bundle.binding.reviewed_root = root; r.bundle.limits = q.limits;
            r.bundle.entries = std::move(kept);
            r.bundle.canonical_bytes = encode(r.bundle.binding, r.bundle.limits,
                r.bundle.entries, generation);
            r.bundle.bundle_sha256 = sha(r.bundle.canonical_bytes);
            core::sha256::Hasher h; h.update("epoch-curated-context-request/v1");
            h.update(r.bundle.canonical_bytes); r.bundle.request_sha256 =
                core::sha256::hex(h.finish());
            for (const auto& e : r.bundle.entries) r.bundle.evidence.push_back({
                .project_relative_path=e.project_relative_path,
                .declared_symbol=e.declared_symbol, .first_line=e.first_line,
                .last_line=e.last_line, .source_revision=e.source_revision,
                .byte_count=static_cast<std::uint64_t>(e.exact_bytes.size()),
                .content_sha256=content_sha(e), .entry_sha256=entry_sha(e),
                .provenance=e.provenance});
            for (std::uint64_t at = 0; at < r.bundle.canonical_bytes.size();
                at += q.limits.maximum_chunk_bytes)
            {
                if (r.bundle.chunks.size() >= q.limits.maximum_chunks)
                    return refuse(q, Code::budget_exceeded, "Chunk budget exceeded.");
                const auto n = static_cast<std::size_t>((std::min<std::uint64_t>)(
                    q.limits.maximum_chunk_bytes, r.bundle.canonical_bytes.size() - at));
                Chunk c{.index=static_cast<std::uint32_t>(r.bundle.chunks.size()),
                    .offset=at};
                c.bytes.assign(r.bundle.canonical_bytes.begin() + at,
                    r.bundle.canonical_bytes.begin() + at + n); c.sha256 = sha(c.bytes);
                r.bundle.chunks.push_back(std::move(c));
            }
            r.bundle.evidence_summary = "Reviewed curated context: project="
                + b.project_id + ", revision=" + std::to_string(b.reviewed_revision)
                + ", entries=" + std::to_string(r.bundle.entries.size())
                + ", source_bytes=" + std::to_string(total) + ", chunks="
                + std::to_string(r.bundle.chunks.size()) + ", audience="
                + std::string{b.audience == Audience::local_model
                    ? "local_model" : "outbound_mcp"}
                + ", provider=" + std::to_string(
                    static_cast<unsigned>(b.provider))
                + ", bundle_sha256="
                + r.bundle.bundle_sha256
                + ". No path was read and no transport was started.";
            r.bundle.resume = {.project_id=b.project_id, .reviewed_root=root,
                .campaign_id=b.campaign_id, .session_id=b.session_id,
                .request_id=b.request_id, .generation=generation,
                .request_sha256=r.bundle.request_sha256,
                .bundle_sha256=r.bundle.bundle_sha256};
            r.status = "Curated context bundle ready for its bound audience.";
            return r;
        }

        [[nodiscard]] Bytes sealed(Bytes payload)
        {
            const auto d = core::sha256::hash(payload);
            payload.insert(payload.end(), d.bytes.begin(), d.bytes.end());
            return payload;
        }
    }

    Result build(const Request& q, const ResumeState* prior) noexcept
    {
        try
        {
            std::uint64_t generation = 1;
            if (prior)
            {
                if (prior->project_id != q.binding.project_id
                    || prior->reviewed_root != root_text(q.binding.reviewed_root)
                    || prior->campaign_id != q.binding.campaign_id
                    || prior->session_id != q.binding.session_id)
                    return refuse(q, Code::cross_project_checkpoint, "Prior state belongs elsewhere.");
                if (q.expected_generation != prior->generation)
                    return refuse(q, Code::stale_generation, "Expected generation is stale.");
                if (q.binding.request_id <= prior->request_id)
                    return refuse(q, Code::replay_rejected, "Request identity was consumed.");
                generation = prior->generation + 1;
            }
            else if (q.expected_generation)
                return refuse(q, Code::stale_generation, "Initial generation must be zero.");
            Result r = assemble(q, generation, true);
            if (r && prior && r.bundle.request_sha256 == prior->request_sha256)
                return refuse(q, Code::replay_rejected, "Request digest was consumed.");
            return r;
        }
        catch (...) { return refuse(q, Code::invalid_binding, "Bundle construction failed."); }
    }

    CheckpointRecord checkpoint(const Bundle& b) noexcept
    {
        try
        {
            Request q{.binding=b.binding, .limits=b.limits,
                .reviewed_entries=b.entries};
            const Result rebuilt = assemble(q, b.resume.generation, true);
            if (!rebuilt || rebuilt.bundle.canonical_bytes != b.canonical_bytes
                || rebuilt.bundle.bundle_sha256 != b.bundle_sha256
                || rebuilt.bundle.request_sha256 != b.request_sha256
                || rebuilt.bundle.evidence != b.evidence
                || rebuilt.bundle.chunks != b.chunks
                || rebuilt.bundle.evidence_summary != b.evidence_summary
                || rebuilt.bundle.resume != b.resume)
                return {.code=Code::checkpoint_noncanonical,
                    .status="Bundle is not canonical."};
            CheckpointRecord r{}; r.code=Code::ready;
            r.canonical_bytes=sealed(b.canonical_bytes); r.sha256=sha(r.canonical_bytes);
            r.status="Canonical curated-context checkpoint sealed."; return r;
        }
        catch (...) { return {.code=Code::checkpoint_malformed,
            .status="Checkpoint serialization failed."}; }
    }

    RestoreResult restore(std::span<const std::uint8_t> bytes,
        const RestoreExpectation& x) noexcept
    {
        try
        {
            RestoreResult r{};
            if (!id(x.project_id) || !x.reviewed_root.is_absolute()
                || !id(x.campaign_id) || !x.session_id || !x.current_generation
                || !hex(x.checkpoint_sha256))
                return {.code=Code::invalid_binding, .status="Invalid restore expectation."};
            if (bytes.size() <= magic.size()+4+seal_size || bytes.size()>10*1024*1024)
                return {.code=Code::checkpoint_malformed, .status="Invalid checkpoint length."};
            const std::string recordSha=sha(bytes);
            if (recordSha != x.checkpoint_sha256)
                return {.code=Code::checkpoint_integrity, .status="Record digest mismatch."};
            const auto payload=bytes.first(bytes.size()-seal_size);
            const auto tail=bytes.last(seal_size); const auto seal=core::sha256::hash(payload);
            std::uint8_t diff{}; for (std::size_t i=0;i<seal_size;++i)
                diff |= tail[i]^seal.bytes[i];
            if (diff) return {.code=Code::checkpoint_integrity, .status="Payload seal mismatch."};
            Request q{}; std::uint64_t generation{};
            if (!decode(payload,q,generation))
                return {.code=Code::checkpoint_malformed, .status="Malformed checkpoint payload."};
            if (q.binding.project_id!=x.project_id
                || root_text(q.binding.reviewed_root)!=root_text(x.reviewed_root)
                || q.binding.campaign_id!=x.campaign_id
                || q.binding.session_id!=x.session_id)
                return {.code=Code::cross_project_checkpoint, .status="Checkpoint belongs elsewhere."};
            if (generation!=x.current_generation)
                return {.code=Code::stale_generation, .status="Checkpoint generation is stale."};
            const Result rebuilt=assemble(q,generation,false);
            if (!rebuilt) return {.code=rebuilt.code,.status=rebuilt.status};
            if (rebuilt.bundle.canonical_bytes.size()!=payload.size()
                || !std::equal(rebuilt.bundle.canonical_bytes.begin(),
                    rebuilt.bundle.canonical_bytes.end(),payload.begin()))
                return {.code=Code::checkpoint_noncanonical,
                    .status="Checkpoint encoding is noncanonical."};
            r.code=Code::ready; r.bundle=rebuilt.bundle;
            r.canonical_sha256=recordSha;
            r.status="Checkpoint restored without source reads or transport."; return r;
        }
        catch (...) { return {.code=Code::checkpoint_malformed,
            .status="Checkpoint restore failed closed."}; }
    }
}
