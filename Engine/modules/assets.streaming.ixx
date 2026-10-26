/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
// Epoch system notes (auto-checked):
// Epoch STL: honor core.stl_types.hpp types/operators; fix callers, not core overloads.
module;

#include <deque>
#include <mutex>
#include <memory>
#include <optional>
#include <stop_token>
#include <cstdint>

#include "../src/epoch/epoch.common.hpp"
#include "../include/core.stl_types.hpp"

export module assets.streaming;

import asset.texture_import;
import asset.texture_artifact;
import project.asset_registry;

//import <optional>;

export namespace epochengine
{
    enum class AssetKind : u8 { texture, mesh, shader_blob };

    struct StreamingRequest
    {
        AssetKind kind{};
        u32 asset_id = 0;
        u32 a = 0, b = 0, c = 0;
    };

    namespace streaming_detail { struct State; struct Operation; }

    enum class TextureStreamCode : std::uint8_t
    {
        queued, reused, ready, invalid_request, capacity_exceeded,
        conflicting_request, import_failed, cancelled, closed, resident_budget_exceeded
    };
    struct TextureStreamLimits
    {
        std::uint32_t maximum_entries{64};
        std::uint32_t maximum_pending{32};
        // Bounds references held by this cache; caller-held snapshots/tickets have separate lifetimes.
        std::uint64_t maximum_cached_bytes{64ull * 1024ull * 1024ull};
    };
    struct TextureStreamRequest
    {
        project_assets::AssetIdentity identity{};
        asset::texture::TextureImportRequest source{};
    };
    struct TextureStreamSnapshot
    {
        project_assets::AssetIdentity identity{};
        std::uint64_t content_revision{};
        std::shared_ptr<const asset::texture::CompiledTextureArtifact> artifact{};
        [[nodiscard]] explicit operator bool() const noexcept { return artifact && static_cast<bool>(*artifact); }
    };
    struct TextureStreamResult
    {
        TextureStreamCode code{TextureStreamCode::invalid_request};
        asset::texture::TextureImportStatus import_status{asset::texture::TextureImportStatus::invalid_request};
        TextureStreamSnapshot snapshot{};
    };
    struct TextureStreamMetrics
    {
        std::uint64_t requests{}, coalesced{}, physical_imports{}, publications{}, failed_imports{};
        std::uint64_t cached_bytes{}, active_imports{};
        std::uint32_t pending{};
    };
    class TextureStreamTicket
    {
        friend class Streamer;
        std::shared_ptr<streaming_detail::State> state_{};
        std::shared_ptr<streaming_detail::Operation> operation_{};
    public:
        [[nodiscard]] bool valid() const noexcept { return static_cast<bool>(operation_); }
        [[nodiscard]] bool same_operation(const TextureStreamTicket&) const noexcept;
        [[nodiscard]] std::optional<TextureStreamResult> poll() const;
        // Cancels the shared operation for every coalesced waiter. A running decoder
        // retires at its next bounded checkpoint; metrics retain active_imports until then.
        void cancel() const;
    };
    struct TextureStreamAdmission
    {
        TextureStreamCode code{TextureStreamCode::invalid_request};
        TextureStreamTicket ticket{};
        bool started_load{};
    };
    class TextureStreamWork
    {
        friend class Streamer;
        std::shared_ptr<streaming_detail::State> state_{};
        std::shared_ptr<streaming_detail::Operation> operation_{};
    public:
        [[nodiscard]] bool valid() const noexcept { return static_cast<bool>(operation_); }
        // Run on the existing taskgraph/scheduler. This object creates no thread/pool.
        // Repeated execution of a copied work item is harmless; import starts once.
        void execute(std::stop_token cancellation = {}) const;
    };
    class Streamer
    {
    public:
        explicit Streamer(TextureStreamLimits limits = {});
        ~Streamer();
        void push(StreamingRequest r)
        {
            std::scoped_lock lk(m_mtx);
            m_q.push_back(r);
        }
        [[nodiscard]] std::optional<StreamingRequest> pop()
        {
            std::scoped_lock lk(m_mtx);
            if (m_q.empty()) return std::nullopt;
            auto r = m_q.front(); m_q.pop_front(); return r;
        }
        [[nodiscard]] TextureStreamAdmission request_texture(TextureStreamRequest, bool reload = false);
        [[nodiscard]] std::optional<TextureStreamWork> pop_texture_work();
        [[nodiscard]] std::optional<TextureStreamSnapshot> texture_snapshot(project_assets::AssetIdentity) const;
        [[nodiscard]] TextureStreamMetrics texture_metrics() const;
        // Releases the cache's references, not clients' immutable snapshots or GPU objects.
        [[nodiscard]] bool evict_texture(project_assets::AssetIdentity);
        void close_textures();
    private:
        mutable std::mutex m_mtx{};
        std::deque<StreamingRequest> m_q{};
        std::shared_ptr<streaming_detail::State> textures_{};
    };
} // namespace epochengine
