// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <array>
#include <deque>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>
module assets.streaming;

namespace epochengine::streaming_detail
{
    struct Operation
    {
        TextureStreamRequest request{};
        std::optional<TextureStreamResult> result{};
        std::stop_source cancellation{};
        bool started{};
    };
    struct Slot
    {
        project_assets::AssetIdentity identity{};
        TextureStreamSnapshot current{};
        TextureStreamRequest accepted{};
        std::shared_ptr<Operation> active{};
    };
    struct State
    {
        std::mutex mutex{};
        std::array<Slot, 64> slots{};
        std::deque<std::shared_ptr<Operation>> pending{};
        TextureStreamLimits limits{};
        TextureStreamMetrics metrics{};
        bool closed{};
    };
    bool same(const TextureStreamRequest& a, const TextureStreamRequest& b)
    {
        return a.identity == b.identity && a.source.source_path == b.source.source_path
            && a.source.source_sequence == b.source.source_sequence && a.source.profile == b.source.profile
            && a.source.limits.maximum_source_bytes == b.source.limits.maximum_source_bytes
            && a.source.limits.maximum_decoded_bytes == b.source.limits.maximum_decoded_bytes
            && a.source.limits.maximum_dimension == b.source.limits.maximum_dimension;
    }
    Slot* find(State& state, project_assets::AssetIdentity identity)
    {
        for (std::uint32_t i = 0; i < state.limits.maximum_entries && i < state.slots.size(); ++i)
            if (state.slots[i].identity == identity) return &state.slots[i];
        return nullptr;
    }
    void cancel(const std::shared_ptr<Operation>& operation)
    {
        if (!operation || operation->result) return;
        operation->cancellation.request_stop();
        operation->result = TextureStreamResult{TextureStreamCode::cancelled};
    }
}
namespace epochengine
{
    Streamer::Streamer(TextureStreamLimits limits) : textures_(std::make_shared<streaming_detail::State>())
    {
        textures_->limits = limits;
        if (!limits.maximum_entries || limits.maximum_entries > 64 || !limits.maximum_pending
            || limits.maximum_pending > 64 || !limits.maximum_cached_bytes) textures_->closed = true;
    }
    Streamer::~Streamer() { close_textures(); }
    bool TextureStreamTicket::same_operation(const TextureStreamTicket& other) const noexcept
    { return operation_ && operation_ == other.operation_; }
    std::optional<TextureStreamResult> TextureStreamTicket::poll() const
    {
        if (!state_ || !operation_) return std::nullopt;
        std::scoped_lock lock(state_->mutex);
        return operation_->result;
    }
    void TextureStreamTicket::cancel() const
    {
        if (!state_ || !operation_) return;
        std::scoped_lock lock(state_->mutex);
        streaming_detail::cancel(operation_);
        if (auto* slot = streaming_detail::find(*state_, operation_->request.identity); slot && slot->active == operation_)
            slot->active.reset();
    }
    TextureStreamAdmission Streamer::request_texture(TextureStreamRequest request, bool reload)
    {
        TextureStreamAdmission admission;
        if (!request.identity || request.identity.kind != project_assets::AssetKind::texture
            || request.source.source_path.empty() || !request.source.source_sequence || !request.source.limits.valid()) return admission;
        request.source.source_path = request.source.source_path.lexically_normal();
        auto& state = *textures_;
        std::scoped_lock lock(state.mutex);
        ++state.metrics.requests;
        if (state.closed) { admission.code = TextureStreamCode::closed; return admission; }
        auto* slot = streaming_detail::find(state, request.identity);
        if (slot && slot->active && !slot->active->result)
        {
            if (streaming_detail::same(slot->active->request, request))
            {
                ++state.metrics.coalesced;
                admission.code = TextureStreamCode::queued;
                admission.ticket.state_ = textures_; admission.ticket.operation_ = slot->active;
                return admission;
            }
            if (!reload) { admission.code = TextureStreamCode::conflicting_request; return admission; }
        }
        if (slot && slot->current && !reload && streaming_detail::same(slot->accepted, request))
        {
            auto operation = std::make_shared<streaming_detail::Operation>();
            operation->request = request;
            operation->result = TextureStreamResult{TextureStreamCode::ready,
                asset::texture::TextureImportStatus::ready, slot->current};
            admission.code = TextureStreamCode::reused;
            admission.ticket.state_ = textures_; admission.ticket.operation_ = std::move(operation);
            return admission;
        }
        // Retire cancelled queued entries before admitting more work.
        std::erase_if(state.pending, [](const auto& operation) { return operation->result.has_value(); });
        if (state.pending.size() >= state.limits.maximum_pending)
        { admission.code = TextureStreamCode::capacity_exceeded; return admission; }
        if (!slot)
        {
            for (std::uint32_t i = 0; i < state.limits.maximum_entries; ++i)
                if (!state.slots[i].identity) { slot = &state.slots[i]; break; }
            if (!slot) { admission.code = TextureStreamCode::capacity_exceeded; return admission; }
        }
        auto operation = std::make_shared<streaming_detail::Operation>();
        operation->request = std::move(request);
        state.pending.push_back(operation);
        streaming_detail::cancel(slot->active);
        slot->identity = operation->request.identity;
        slot->active = operation;
        admission.code = TextureStreamCode::queued; admission.started_load = true;
        admission.ticket.state_ = textures_; admission.ticket.operation_ = std::move(operation);
        return admission;
    }
    std::optional<TextureStreamWork> Streamer::pop_texture_work()
    {
        auto& state = *textures_;
        std::scoped_lock lock(state.mutex);
        while (!state.pending.empty())
        {
            auto operation = std::move(state.pending.front()); state.pending.pop_front();
            if (operation->result) continue;
            TextureStreamWork work; work.state_ = textures_; work.operation_ = std::move(operation); return work;
        }
        return std::nullopt;
    }
    void TextureStreamWork::execute(std::stop_token cancellation) const
    {
        if (!state_ || !operation_) return;
        {
            std::scoped_lock lock(state_->mutex);
            if (operation_->started || operation_->result) return;
            if (state_->closed || cancellation.stop_requested() || operation_->cancellation.stop_requested())
            { streaming_detail::cancel(operation_); return; }
            operation_->started = true;
            ++state_->metrics.physical_imports; ++state_->metrics.active_imports;
        }
        // The existing importer owns all format/size/CRC/allocation validation.
        auto imported = asset::texture::import_texture_file(operation_->request.source);
        std::shared_ptr<const asset::texture::CompiledTextureArtifact> artifact;
        if (imported)
        {
            try { artifact = std::make_shared<asset::texture::CompiledTextureArtifact>(std::move(imported.artifact)); }
            catch (...) { imported.status = asset::texture::TextureImportStatus::allocation_failure; }
        }
        std::scoped_lock lock(state_->mutex);
        --state_->metrics.active_imports;
        auto* slot = streaming_detail::find(*state_, operation_->request.identity);
        if (operation_->result || !slot || slot->active != operation_) return;
        if (state_->closed || cancellation.stop_requested() || operation_->cancellation.stop_requested())
        { streaming_detail::cancel(operation_); slot->active.reset(); return; }
        TextureStreamResult result{TextureStreamCode::import_failed, imported.status};
        if (artifact)
        {
            const auto previous_bytes = slot->current ? slot->current.artifact->identity.estimated_artifact_bytes : 0;
            const auto bytes = artifact->identity.estimated_artifact_bytes;
            if (bytes <= state_->limits.maximum_cached_bytes - (state_->metrics.cached_bytes - previous_bytes)
                && slot->current.content_revision != (std::numeric_limits<std::uint64_t>::max)())
            {
                slot->current = {slot->identity, slot->current.content_revision + 1, std::move(artifact)};
                slot->accepted = std::move(operation_->request);
                state_->metrics.cached_bytes = state_->metrics.cached_bytes - previous_bytes + bytes;
                ++state_->metrics.publications;
                result = {TextureStreamCode::ready, imported.status, slot->current};
            }
            else result.code = TextureStreamCode::resident_budget_exceeded;
        }
        else ++state_->metrics.failed_imports;
        operation_->result = std::move(result); slot->active.reset();
    }
    std::optional<TextureStreamSnapshot> Streamer::texture_snapshot(project_assets::AssetIdentity identity) const
    {
        std::scoped_lock lock(textures_->mutex);
        const auto* slot = streaming_detail::find(*textures_, identity);
        return slot && slot->current ? std::optional{slot->current} : std::nullopt;
    }
    TextureStreamMetrics Streamer::texture_metrics() const
    {
        std::scoped_lock lock(textures_->mutex);
        auto metrics = textures_->metrics;
        for (const auto& operation : textures_->pending) if (!operation->result) ++metrics.pending;
        return metrics;
    }
    bool Streamer::evict_texture(project_assets::AssetIdentity identity)
    {
        std::scoped_lock lock(textures_->mutex);
        auto* slot = streaming_detail::find(*textures_, identity);
        if (!slot) return false;
        streaming_detail::cancel(slot->active); slot->active.reset();
        if (slot->current) textures_->metrics.cached_bytes -= slot->current.artifact->identity.estimated_artifact_bytes;
        *slot = {}; return true;
    }
    void Streamer::close_textures()
    {
        std::scoped_lock lock(textures_->mutex);
        textures_->closed = true;
        for (auto& slot : textures_->slots) { streaming_detail::cancel(slot.active); slot = {}; }
        textures_->pending.clear(); textures_->metrics.cached_bytes = 0;
    }
}
