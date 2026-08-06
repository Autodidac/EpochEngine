/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module render.texture.residency;

import render.canvas2d;
import render.device;

export namespace epochengine::texture_residency
{
    struct ArtifactDigest final
    {
        std::array<std::uint64_t, 4> words{};

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return words == std::array<std::uint64_t, 4>{};
        }

        friend constexpr bool operator==(ArtifactDigest, ArtifactDigest) noexcept = default;
    };

    struct ArtifactKey final
    {
        canvas2d::LogicalTextureReference logical{};
        ArtifactDigest digest{};
        std::uint32_t compiler_schema_version{1};
        std::uint32_t width{};
        std::uint32_t height{};
        std::uint32_t mip_levels{1};
        TextureFormat format{TextureFormat::rgba8_unorm};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return static_cast<bool>(logical)
                && !digest.empty()
                && compiler_schema_version != 0
                && format != TextureFormat::depth24_stencil8
                && format != TextureFormat::depth32_float
                && width != 0
                && height != 0
                && mip_levels != 0
                && texture_format_bytes_per_texel(format) != 0;
        }

        friend constexpr bool operator==(const ArtifactKey&, const ArtifactKey&) noexcept = default;
    };

    struct TexturePayloadView final
    {
        const void* data{};
        std::uint64_t size_bytes{};
        std::uint32_t row_pitch_bytes{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return data != nullptr && size_bytes != 0;
        }
    };

    struct ResidencyHandle final
    {
        static constexpr std::uint32_t invalid_index =
            (std::numeric_limits<std::uint32_t>::max)();

        std::uint32_t index{invalid_index};
        std::uint32_t generation{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return index != invalid_index && generation != 0;
        }

        friend constexpr bool operator==(ResidencyHandle, ResidencyHandle) noexcept = default;
    };

    enum class ResidencyCode : std::uint8_t
    {
        resident,
        reused,
        recreated,
        invalid_request,
        invalid_limits,
        unsupported_format,
        entry_limit_exceeded,
        host_allocation_failed,
        resident_budget_exceeded,
        upload_budget_exceeded,
        backend_allocation_failed,
        backend_upload_failed,
        backend_not_ready,
        stale_handle,
        not_found
    };

    [[nodiscard]] constexpr const char* residency_code_name(
        ResidencyCode code) noexcept
    {
        switch (code)
        {
        case ResidencyCode::resident: return "resident";
        case ResidencyCode::reused: return "reused";
        case ResidencyCode::recreated: return "recreated";
        case ResidencyCode::invalid_request: return "invalid_request";
        case ResidencyCode::invalid_limits: return "invalid_limits";
        case ResidencyCode::unsupported_format: return "unsupported_format";
        case ResidencyCode::entry_limit_exceeded: return "entry_limit_exceeded";
        case ResidencyCode::host_allocation_failed: return "host_allocation_failed";
        case ResidencyCode::resident_budget_exceeded: return "resident_budget_exceeded";
        case ResidencyCode::upload_budget_exceeded: return "upload_budget_exceeded";
        case ResidencyCode::backend_allocation_failed: return "backend_allocation_failed";
        case ResidencyCode::backend_upload_failed: return "backend_upload_failed";
        case ResidencyCode::backend_not_ready: return "backend_not_ready";
        case ResidencyCode::stale_handle: return "stale_handle";
        case ResidencyCode::not_found: return "not_found";
        }
        return "unknown";
    }

    struct ResidencyLimits final
    {
        std::uint32_t maximum_entries{4'096};
        std::uint64_t maximum_resident_bytes{512ull * 1024ull * 1024ull};
        std::uint64_t maximum_single_texture_bytes{256ull * 1024ull * 1024ull};
        std::uint64_t maximum_transient_replacement_bytes{
            256ull * 1024ull * 1024ull};
        std::uint64_t maximum_upload_bytes_per_frame{64ull * 1024ull * 1024ull};
        std::uint32_t maximum_evictions_per_acquire{64};
    };

    [[nodiscard]] constexpr bool valid(const ResidencyLimits& limits) noexcept
    {
        return limits.maximum_entries != 0
            && limits.maximum_resident_bytes != 0
            && limits.maximum_single_texture_bytes != 0
            && limits.maximum_transient_replacement_bytes != 0
            && limits.maximum_upload_bytes_per_frame != 0
            && limits.maximum_evictions_per_acquire != 0
            && limits.maximum_single_texture_bytes <= limits.maximum_resident_bytes;
    }

    struct ResidencyPolicy final
    {
        bool allow_eviction{true};
        bool require_backend_ready{true};
        bool pin{};
        bool evictable{true};
        std::uint32_t priority{};
    };

    struct ResidencyRequest final
    {
        ArtifactKey artifact{};
        TexturePayloadView payload{};
        ResidencyPolicy policy{};
        std::uint64_t frame_sequence{};
        std::string_view debug_name{"TextureResidency"};
    };

    struct ResidencySnapshot final
    {
        ResidencyHandle handle{};
        ArtifactKey artifact{};
        TextureHandle physical{};
        std::uint64_t backend_epoch{};
        std::uint64_t resident_bytes{};
        std::uint64_t last_used_frame{};
        std::uint32_t priority{};
        bool pinned{};
        bool evictable{};
        bool backend_ready{};
    };

    struct ResidencyMetrics final
    {
        std::uint64_t acquire_requests{};
        std::uint64_t allocations{};
        std::uint64_t recreations{};
        std::uint64_t cache_hits{};
        std::uint64_t uploads{};
        std::uint64_t upload_failures{};
        std::uint64_t evictions{};
        std::uint64_t explicit_releases{};
        std::uint64_t backend_retirements{};
        std::uint64_t rejected_requests{};
        std::uint64_t uploaded_bytes{};
        std::uint64_t active_entries{};
        std::uint64_t resident_bytes{};
        std::uint64_t peak_entries{};
        std::uint64_t peak_resident_bytes{};
        std::uint64_t peak_transient_replacement_bytes{};
        std::uint64_t current_frame{};
        std::uint64_t current_frame_upload_bytes{};
        std::uint64_t backend_epoch{};
    };

    struct AcquireResult final
    {
        ResidencyCode code{ResidencyCode::invalid_request};
        ResidencyHandle handle{};
        TextureHandle physical{};
        std::uint64_t resident_bytes{};
        std::uint32_t evicted_entries{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResidencyCode::resident
                || code == ResidencyCode::reused
                || code == ResidencyCode::recreated;
        }
    };

    class TextureResidencyCache final
    {
    public:
        TextureResidencyCache(
            IRenderDevice& device,
            std::uint64_t backend_epoch,
            ResidencyLimits limits = {}) noexcept
            : device_(&device), backend_epoch_(backend_epoch), limits_(limits)
        {
            metrics_.backend_epoch = backend_epoch_;
        }

        ~TextureResidencyCache()
        {
            retire_all();
        }

        TextureResidencyCache(const TextureResidencyCache&) = delete;
        TextureResidencyCache& operator=(const TextureResidencyCache&) = delete;
        TextureResidencyCache(TextureResidencyCache&&) = delete;
        TextureResidencyCache& operator=(TextureResidencyCache&&) = delete;

        [[nodiscard]] bool begin_frame(std::uint64_t frame_sequence) noexcept
        {
            if (frame_sequence == 0 || frame_sequence < metrics_.current_frame)
                return false;
            if (frame_sequence != metrics_.current_frame)
            {
                metrics_.current_frame = frame_sequence;
                metrics_.current_frame_upload_bytes = 0;
            }
            return true;
        }

        [[nodiscard]] AcquireResult acquire(const ResidencyRequest& request)
        {
            ++metrics_.acquire_requests;
            if (!valid(limits_) || device_ == nullptr || backend_epoch_ == 0)
                return reject(ResidencyCode::invalid_limits);
            if (!request.artifact)
                return reject(ResidencyCode::invalid_request);
            if (texture_format_bytes_per_texel(request.artifact.format) == 0)
                return reject(ResidencyCode::unsupported_format);
            if (request.frame_sequence != 0 && !begin_frame(request.frame_sequence))
                return reject(ResidencyCode::invalid_request);

            const std::optional<std::uint32_t> exact = find_artifact(request.artifact);
            std::optional<std::uint32_t> recreationIndex{};
            if (exact)
            {
                Slot& slot = slots_[*exact];
                if (slot.backend_epoch == backend_epoch_
                    && slot.physical
                    && (!request.policy.require_backend_ready
                        || device_->texture_ready(slot.physical)))
                {
                    slot.last_used_frame = effective_frame(request.frame_sequence);
                    slot.priority = (std::max)(slot.priority, request.policy.priority);
                    slot.pinned = slot.pinned || request.policy.pin;
                    slot.evictable = slot.evictable && request.policy.evictable;
                    ++metrics_.cache_hits;
                    return AcquireResult{
                        ResidencyCode::reused,
                        make_handle(*exact),
                        slot.physical,
                        slot.resident_bytes,
                        0 };
                }

                recreationIndex = *exact;
            }

            if (!valid_upload_request(request))
                return reject(ResidencyCode::invalid_request);
            const std::uint64_t residentBytes = payload_resident_bytes(request);
            if (residentBytes == 0)
                return reject(ResidencyCode::invalid_request);
            if (residentBytes > limits_.maximum_single_texture_bytes
                || residentBytes > limits_.maximum_resident_bytes)
            {
                return reject(ResidencyCode::resident_budget_exceeded);
            }
            if (residentBytes > limits_.maximum_upload_bytes_per_frame
                || metrics_.current_frame_upload_bytes
                    > limits_.maximum_upload_bytes_per_frame - residentBytes)
            {
                return reject(ResidencyCode::upload_budget_exceeded);
            }

            if (recreationIndex)
            {
                const Slot& existing = slots_[*recreationIndex];
                if (residentBytes != existing.resident_bytes)
                    return reject(ResidencyCode::invalid_request);
                if (residentBytes
                    > limits_.maximum_transient_replacement_bytes)
                {
                    return reject(ResidencyCode::resident_budget_exceeded);
                }
            }

            std::vector<std::uint32_t> plannedEvictions{};
            if (!recreationIndex)
            {
                ResidencyCode roomCode{ResidencyCode::host_allocation_failed};
                try
                {
                    roomCode = plan_room(
                        residentBytes,
                        request.policy.allow_eviction,
                        plannedEvictions);
                    if (roomCode == ResidencyCode::resident
                        && plannedEvictions.empty()
                        && !has_inactive_slot())
                    {
                        slots_.reserve(slots_.size() + 1u);
                    }
                }
                catch (...)
                {
                    return reject(ResidencyCode::host_allocation_failed);
                }
                if (roomCode != ResidencyCode::resident)
                    return reject(roomCode);
                if (!plannedEvictions.empty()
                    && residentBytes
                        > limits_.maximum_transient_replacement_bytes)
                {
                    return reject(ResidencyCode::resident_budget_exceeded);
                }
            }

            TextureDesc desc{};
            desc.width = request.artifact.width;
            desc.height = request.artifact.height;
            desc.mip_levels = request.artifact.mip_levels;
            desc.format = request.artifact.format;
            desc.sampled = true;
            desc.storage = false;
            desc.render_target = false;
            desc.depth_stencil = false;
            desc.sparse = false;
            desc.debug_name = "TextureResidency";

            TextureHandle physical{};
            try
            {
                physical = device_->create_texture(desc);
            }
            catch (...)
            {
                return reject(ResidencyCode::backend_allocation_failed);
            }
            if (!physical)
                return reject(ResidencyCode::backend_allocation_failed);
            PendingTexture pending{*device_, physical};
            if (recreationIndex || !plannedEvictions.empty())
            {
                metrics_.peak_transient_replacement_bytes = (std::max)(
                    metrics_.peak_transient_replacement_bytes,
                    residentBytes);
            }

            TextureUploadDesc upload{};
            upload.mip_level = 0;
            upload.x = 0;
            upload.y = 0;
            upload.width = request.artifact.width;
            upload.height = request.artifact.height;
            upload.row_pitch_bytes = request.payload.row_pitch_bytes;
            upload.format = request.artifact.format;
            upload.data = request.payload.data;
            upload.size_bytes = request.payload.size_bytes;

            try
            {
                if (!device_->upload_texture(physical, upload))
                {
                    ++metrics_.upload_failures;
                    return reject(ResidencyCode::backend_upload_failed);
                }
            }
            catch (...)
            {
                ++metrics_.upload_failures;
                return reject(ResidencyCode::backend_upload_failed);
            }
            ++metrics_.uploads;
            metrics_.uploaded_bytes += residentBytes;
            metrics_.current_frame_upload_bytes += residentBytes;
            try
            {
                if (request.policy.require_backend_ready
                    && !device_->texture_ready(physical))
                {
                    ++metrics_.upload_failures;
                    return reject(ResidencyCode::backend_not_ready);
                }
            }
            catch (...)
            {
                ++metrics_.upload_failures;
                return reject(ResidencyCode::backend_not_ready);
            }

            if (recreationIndex)
            {
                const std::uint32_t index = *recreationIndex;
                Slot& slot = slots_[index];
                const TextureHandle retiredPhysical = slot.physical;
                const std::uint64_t retiredBytes = slot.resident_bytes;
                const std::uint32_t nextGeneration = slot.generation + 1u;
                if (retiredPhysical)
                    device_->destroy(retiredPhysical);
                const std::uint32_t retainedPriority = (std::max)(
                    slot.priority, request.policy.priority);
                const bool retainedPinned = slot.pinned || request.policy.pin;
                const bool retainedEvictable = slot.evictable && request.policy.evictable;
                if (metrics_.resident_bytes >= retiredBytes)
                    metrics_.resident_bytes -= retiredBytes;
                else
                    metrics_.resident_bytes = 0;

                slot = {};
                slot.generation = nextGeneration == 0 ? 1u : nextGeneration;
                slot.active = true;
                slot.artifact = request.artifact;
                slot.physical = physical;
                pending.release();
                slot.backend_epoch = backend_epoch_;
                slot.resident_bytes = residentBytes;
                slot.last_used_frame = effective_frame(request.frame_sequence);
                slot.priority = retainedPriority;
                slot.pinned = retainedPinned;
                slot.evictable = retainedEvictable;

                ++metrics_.allocations;
                ++metrics_.recreations;
                metrics_.resident_bytes += residentBytes;
                metrics_.peak_resident_bytes = (std::max)(
                    metrics_.peak_resident_bytes,
                    metrics_.resident_bytes);
                return AcquireResult{
                    ResidencyCode::recreated,
                    make_handle(index),
                    physical,
                    residentBytes,
                    0};
            }

            for (const std::uint32_t victim : plannedEvictions)
                deactivate_slot(victim, true);
            const std::uint32_t evictedEntries =
                static_cast<std::uint32_t>(plannedEvictions.size());

            const std::uint32_t index = allocate_slot();
            Slot& slot = slots_[index];
            slot.active = true;
            slot.artifact = request.artifact;
            slot.physical = physical;
            pending.release();
            slot.backend_epoch = backend_epoch_;
            slot.resident_bytes = residentBytes;
            slot.last_used_frame = effective_frame(request.frame_sequence);
            slot.priority = request.policy.priority;
            slot.pinned = request.policy.pin;
            slot.evictable = request.policy.evictable;

            ++metrics_.allocations;
            ++metrics_.active_entries;
            metrics_.resident_bytes += residentBytes;
            metrics_.peak_entries = (std::max)(metrics_.peak_entries, metrics_.active_entries);
            metrics_.peak_resident_bytes = (std::max)(metrics_.peak_resident_bytes, metrics_.resident_bytes);

            return AcquireResult{
                ResidencyCode::resident,
                make_handle(index),
                physical,
                residentBytes,
                evictedEntries };
        }

        [[nodiscard]] std::optional<ResidencySnapshot> resolve(
            ResidencyHandle handle) const noexcept
        {
            const Slot* const slot = resolve_slot(handle);
            if (!slot)
                return std::nullopt;
            return snapshot(handle.index, *slot);
        }

        [[nodiscard]] std::optional<ResidencySnapshot> resolve(
            const ArtifactKey& artifact) const noexcept
        {
            const std::optional<std::uint32_t> index = find_artifact(artifact);
            if (!index)
                return std::nullopt;
            return snapshot(*index, slots_[*index]);
        }

        [[nodiscard]] ResidencyCode touch(
            ResidencyHandle handle,
            std::uint64_t frame_sequence) noexcept
        {
            Slot* const slot = resolve_slot(handle);
            if (!slot)
                return handle ? ResidencyCode::stale_handle : ResidencyCode::not_found;
            if (!begin_frame(frame_sequence))
                return ResidencyCode::invalid_request;
            slot->last_used_frame = frame_sequence;
            return ResidencyCode::reused;
        }

        [[nodiscard]] ResidencyCode set_pinned(
            ResidencyHandle handle,
            bool pinned) noexcept
        {
            Slot* const slot = resolve_slot(handle);
            if (!slot)
                return handle ? ResidencyCode::stale_handle : ResidencyCode::not_found;
            slot->pinned = pinned;
            return ResidencyCode::reused;
        }

        [[nodiscard]] ResidencyCode release(ResidencyHandle handle) noexcept
        {
            Slot* const slot = resolve_slot(handle);
            if (!slot)
                return handle ? ResidencyCode::stale_handle : ResidencyCode::not_found;
            deactivate_slot(handle.index, false);
            ++metrics_.explicit_releases;
            return ResidencyCode::resident;
        }

        [[nodiscard]] std::uint32_t trim_to_budget() noexcept
        {
            std::uint32_t evicted{};
            while ((metrics_.active_entries > limits_.maximum_entries
                    || metrics_.resident_bytes > limits_.maximum_resident_bytes)
                && evicted < limits_.maximum_evictions_per_acquire)
            {
                const std::optional<std::uint32_t> candidate = eviction_candidate();
                if (!candidate)
                    break;
                deactivate_slot(*candidate, true);
                ++evicted;
            }
            return evicted;
        }

        void retire_all() noexcept
        {
            if (!device_)
                return;
            bool retiredAny = false;
            for (std::uint32_t index = 0; index < slots_.size(); ++index)
            {
                if (!slots_[index].active)
                    continue;
                deactivate_slot(index, false);
                retiredAny = true;
            }
            if (retiredAny)
                ++metrics_.backend_retirements;
        }

        [[nodiscard]] bool reset_backend_epoch(std::uint64_t backend_epoch) noexcept
        {
            if (backend_epoch == 0 || backend_epoch <= backend_epoch_)
                return false;
            retire_all();
            backend_epoch_ = backend_epoch;
            metrics_.backend_epoch = backend_epoch_;
            metrics_.current_frame = 0;
            metrics_.current_frame_upload_bytes = 0;
            return true;
        }

        [[nodiscard]] std::vector<ResidencySnapshot> snapshots() const
        {
            std::vector<ResidencySnapshot> result{};
            result.reserve(static_cast<std::size_t>(metrics_.active_entries));
            for (std::uint32_t index = 0; index < slots_.size(); ++index)
            {
                if (slots_[index].active)
                    result.push_back(snapshot(index, slots_[index]));
            }
            return result;
        }

        [[nodiscard]] const ResidencyLimits& limits() const noexcept { return limits_; }
        [[nodiscard]] const ResidencyMetrics& metrics() const noexcept { return metrics_; }
        [[nodiscard]] std::uint64_t backend_epoch() const noexcept { return backend_epoch_; }

    private:
        class PendingTexture final
        {
        public:
            PendingTexture(
                IRenderDevice& device,
                TextureHandle physical) noexcept
                : device_(&device), physical_(physical)
            {
            }

            ~PendingTexture()
            {
                if (device_ && physical_)
                    device_->destroy(physical_);
            }

            PendingTexture(const PendingTexture&) = delete;
            PendingTexture& operator=(const PendingTexture&) = delete;
            PendingTexture(PendingTexture&&) = delete;
            PendingTexture& operator=(PendingTexture&&) = delete;

            void release() noexcept
            {
                physical_ = {};
            }

        private:
            IRenderDevice* device_{};
            TextureHandle physical_{};
        };

        struct Slot final
        {
            ArtifactKey artifact{};
            TextureHandle physical{};
            std::uint64_t backend_epoch{};
            std::uint64_t resident_bytes{};
            std::uint64_t last_used_frame{};
            std::uint32_t priority{};
            std::uint32_t generation{1};
            bool pinned{};
            bool evictable{true};
            bool active{};
        };

        [[nodiscard]] bool valid_upload_request(const ResidencyRequest& request) const noexcept
        {
            if (!request.payload)
                return false;
            TextureUploadDesc upload{};
            upload.width = request.artifact.width;
            upload.height = request.artifact.height;
            upload.row_pitch_bytes = request.payload.row_pitch_bytes;
            upload.format = request.artifact.format;
            upload.data = request.payload.data;
            upload.size_bytes = request.payload.size_bytes;
            return epochengine::valid(upload)
                && request.artifact.mip_levels == 1;
        }

        [[nodiscard]] static std::uint64_t payload_resident_bytes(
            const ResidencyRequest& request) noexcept
        {
            TextureUploadDesc upload{};
            upload.width = request.artifact.width;
            upload.height = request.artifact.height;
            upload.row_pitch_bytes = request.payload.row_pitch_bytes;
            upload.format = request.artifact.format;
            upload.data = request.payload.data;
            upload.size_bytes = request.payload.size_bytes;
            return minimum_texture_upload_bytes(upload);
        }

        [[nodiscard]] std::uint64_t effective_frame(
            std::uint64_t requested) const noexcept
        {
            return requested == 0 ? metrics_.current_frame : requested;
        }

        [[nodiscard]] std::optional<std::uint32_t> find_artifact(
            const ArtifactKey& artifact) const noexcept
        {
            for (std::uint32_t index = 0; index < slots_.size(); ++index)
            {
                const Slot& slot = slots_[index];
                if (slot.active && slot.artifact == artifact)
                    return index;
            }
            return std::nullopt;
        }

        [[nodiscard]] std::optional<std::uint32_t> eviction_candidate(
            std::span<const std::uint32_t> excluded = {}) const noexcept
        {
            std::optional<std::uint32_t> selected{};
            for (std::uint32_t index = 0; index < slots_.size(); ++index)
            {
                const Slot& candidate = slots_[index];
                if (!candidate.active || candidate.pinned || !candidate.evictable)
                    continue;
                if (std::find(excluded.begin(), excluded.end(), index)
                    != excluded.end())
                {
                    continue;
                }
                if (!selected)
                {
                    selected = index;
                    continue;
                }

                const Slot& current = slots_[*selected];
                if (candidate.priority < current.priority
                    || (candidate.priority == current.priority
                        && candidate.last_used_frame < current.last_used_frame)
                    || (candidate.priority == current.priority
                        && candidate.last_used_frame == current.last_used_frame
                        && index < *selected))
                {
                    selected = index;
                }
            }
            return selected;
        }

        [[nodiscard]] ResidencyCode plan_room(
            std::uint64_t incoming_bytes,
            bool allow_eviction,
            std::vector<std::uint32_t>& planned_evictions) const
        {
            std::uint64_t prospectiveEntries = metrics_.active_entries;
            std::uint64_t prospectiveBytes = metrics_.resident_bytes;
            const auto hasRoom = [&]() noexcept
            {
                return prospectiveEntries < limits_.maximum_entries
                    && prospectiveBytes
                        <= limits_.maximum_resident_bytes - incoming_bytes;
            };
            if (hasRoom())
                return ResidencyCode::resident;
            if (!allow_eviction)
            {
                return prospectiveEntries >= limits_.maximum_entries
                    ? ResidencyCode::entry_limit_exceeded
                    : ResidencyCode::resident_budget_exceeded;
            }

            while (!hasRoom()
                && planned_evictions.size()
                    < limits_.maximum_evictions_per_acquire)
            {
                const std::optional<std::uint32_t> candidate =
                    eviction_candidate(std::span<const std::uint32_t>{
                        planned_evictions});
                if (!candidate)
                    break;
                const Slot& victim = slots_[*candidate];
                planned_evictions.push_back(*candidate);
                if (prospectiveEntries != 0)
                    --prospectiveEntries;
                prospectiveBytes = prospectiveBytes >= victim.resident_bytes
                    ? prospectiveBytes - victim.resident_bytes
                    : 0;
            }
            if (hasRoom())
                return ResidencyCode::resident;
            return prospectiveEntries >= limits_.maximum_entries
                ? ResidencyCode::entry_limit_exceeded
                : ResidencyCode::resident_budget_exceeded;
        }

        [[nodiscard]] bool has_inactive_slot() const noexcept
        {
            return std::any_of(
                slots_.begin(),
                slots_.end(),
                [](const Slot& slot) noexcept { return !slot.active; });
        }

        [[nodiscard]] std::uint32_t allocate_slot()
        {
            for (std::uint32_t index = 0; index < slots_.size(); ++index)
            {
                if (!slots_[index].active)
                    return index;
            }
            slots_.push_back({});
            return static_cast<std::uint32_t>(slots_.size() - 1u);
        }

        void deactivate_slot(std::uint32_t index, bool eviction) noexcept
        {
            if (index >= slots_.size())
                return;
            Slot& slot = slots_[index];
            if (!slot.active)
                return;
            if (device_ && slot.physical)
                device_->destroy(slot.physical);
            if (metrics_.active_entries != 0)
                --metrics_.active_entries;
            if (metrics_.resident_bytes >= slot.resident_bytes)
                metrics_.resident_bytes -= slot.resident_bytes;
            else
                metrics_.resident_bytes = 0;
            const std::uint32_t nextGeneration = slot.generation + 1u;
            slot = {};
            slot.generation = nextGeneration == 0 ? 1u : nextGeneration;
            if (eviction)
                ++metrics_.evictions;
        }

        [[nodiscard]] ResidencyHandle make_handle(std::uint32_t index) const noexcept
        {
            return ResidencyHandle{index, slots_[index].generation};
        }

        [[nodiscard]] Slot* resolve_slot(ResidencyHandle handle) noexcept
        {
            if (!handle || handle.index >= slots_.size())
                return nullptr;
            Slot& slot = slots_[handle.index];
            return slot.active && slot.generation == handle.generation ? &slot : nullptr;
        }

        [[nodiscard]] const Slot* resolve_slot(ResidencyHandle handle) const noexcept
        {
            if (!handle || handle.index >= slots_.size())
                return nullptr;
            const Slot& slot = slots_[handle.index];
            return slot.active && slot.generation == handle.generation ? &slot : nullptr;
        }

        [[nodiscard]] ResidencySnapshot snapshot(
            std::uint32_t index,
            const Slot& slot) const noexcept
        {
            return ResidencySnapshot{
                make_handle(index),
                slot.artifact,
                slot.physical,
                slot.backend_epoch,
                slot.resident_bytes,
                slot.last_used_frame,
                slot.priority,
                slot.pinned,
                slot.evictable,
                device_ && slot.physical && device_->texture_ready(slot.physical) };
        }

        [[nodiscard]] AcquireResult reject(
            ResidencyCode code,
            std::uint32_t evicted_entries = 0) noexcept
        {
            ++metrics_.rejected_requests;
            return AcquireResult{code, {}, {}, 0, evicted_entries};
        }

        IRenderDevice* device_{};
        std::uint64_t backend_epoch_{};
        ResidencyLimits limits_{};
        ResidencyMetrics metrics_{};
        std::vector<Slot> slots_{};
    };

    enum class ResidencyContractFailure : std::uint8_t
    {
        none,
        upload_descriptor,
        first_acquire,
        cache_reuse,
        pinning,
        eviction,
        stale_handle,
        backend_reset,
        recreation,
        upload_failure,
        upload_budget,
        metrics
    };

    [[nodiscard]] constexpr const char* residency_contract_failure_name(
        ResidencyContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ResidencyContractFailure::none: return "pass";
        case ResidencyContractFailure::upload_descriptor: return "upload_descriptor";
        case ResidencyContractFailure::first_acquire: return "first_acquire";
        case ResidencyContractFailure::cache_reuse: return "cache_reuse";
        case ResidencyContractFailure::pinning: return "pinning";
        case ResidencyContractFailure::eviction: return "eviction";
        case ResidencyContractFailure::stale_handle: return "stale_handle";
        case ResidencyContractFailure::backend_reset: return "backend_reset";
        case ResidencyContractFailure::recreation: return "recreation";
        case ResidencyContractFailure::upload_failure: return "upload_failure";
        case ResidencyContractFailure::upload_budget: return "upload_budget";
        case ResidencyContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    namespace detail
    {
        class ResidencyContractCommandContext final : public ICommandContext
        {
        public:
            void begin(const char*) override {}
            void end() override {}
            void debug_marker(const char*) override {}
            void barrier() override {}
        };

        class ResidencyContractDevice final : public IRenderDevice
        {
        public:
            struct TextureRecord final
            {
                TextureDesc desc{};
                bool active{};
                bool ready{};
            };

            std::string backend_name() const override { return "residency-contract"; }
            BufferHandle create_buffer(const BufferDesc&) override { return BufferHandle{++buffer_}; }
            SamplerHandle create_sampler(const SamplerDesc&) override { return SamplerHandle{++sampler_}; }
            ShaderHandle create_shader(const ShaderDesc&) override { return ShaderHandle{++shader_}; }
            PipelineHandle create_pipeline(const PipelineDesc&) override { return PipelineHandle{++pipeline_}; }
            MaterialHandle create_material(const MaterialDesc&) override { return MaterialHandle{++material_}; }
            RenderTargetHandle create_render_target(const RenderTargetDesc&) override { return RenderTargetHandle{++target_}; }
            BindingSetHandle create_binding_set(const CommandResourceBindings&) override { return BindingSetHandle{++binding_}; }
            MeshHandle create_mesh(const MeshDesc&) override { return MeshHandle{++mesh_}; }
            ModelHandle create_model(const ModelDesc&) override { return ModelHandle{++model_}; }

            TextureHandle create_texture(const TextureDesc& desc) override
            {
                for (std::uint32_t index = 0; index < textures_.size(); ++index)
                {
                    if (!textures_[index].active)
                    {
                        textures_[index] = TextureRecord{desc, true, false};
                        ++create_count;
                        return TextureHandle{index + 1u};
                    }
                }
                textures_.push_back(TextureRecord{desc, true, false});
                ++create_count;
                return TextureHandle{static_cast<std::uint32_t>(textures_.size())};
            }

            bool upload_texture(TextureHandle handle, const TextureUploadDesc& upload) override
            {
                if (throw_next_upload)
                {
                    throw_next_upload = false;
                    throw 1;
                }
                TextureRecord* const record = resolve(handle);
                if (!record || !epochengine::valid(upload) || fail_next_upload)
                {
                    fail_next_upload = false;
                    return false;
                }
                if (upload.x != 0 || upload.y != 0 || upload.mip_level != 0
                    || upload.width != record->desc.width
                    || upload.height != record->desc.height
                    || upload.format != record->desc.format)
                {
                    return false;
                }
                record->ready = true;
                ++upload_count;
                return true;
            }

            bool texture_ready(TextureHandle handle) const noexcept override
            {
                if (fail_next_ready_check)
                {
                    fail_next_ready_check = false;
                    return false;
                }
                const TextureRecord* const record = resolve(handle);
                return record && record->ready;
            }

            [[nodiscard]] bool mark_not_ready(TextureHandle handle) noexcept
            {
                TextureRecord* const record = resolve(handle);
                if (!record)
                    return false;
                record->ready = false;
                return true;
            }

            void destroy(BufferHandle) noexcept override {}
            void destroy(SamplerHandle) noexcept override {}
            void destroy(ShaderHandle) noexcept override {}
            void destroy(PipelineHandle) noexcept override {}
            void destroy(MaterialHandle) noexcept override {}
            void destroy(RenderTargetHandle) noexcept override {}
            void destroy(BindingSetHandle) noexcept override {}
            void destroy(MeshHandle) noexcept override {}
            void destroy(ModelHandle) noexcept override {}

            void destroy(TextureHandle handle) noexcept override
            {
                TextureRecord* const record = resolve(handle);
                if (!record)
                    return;
                *record = {};
                ++destroy_count;
            }

            ICommandContext& acquire_graphics_context() override { return context_; }
            void present(ISwapchain&) override {}

            bool fail_next_upload{};
            bool throw_next_upload{};
            mutable bool fail_next_ready_check{};
            std::uint64_t create_count{};
            std::uint64_t upload_count{};
            std::uint64_t destroy_count{};

        private:
            [[nodiscard]] TextureRecord* resolve(TextureHandle handle) noexcept
            {
                if (!handle || handle.value > textures_.size())
                    return nullptr;
                TextureRecord& record = textures_[handle.value - 1u];
                return record.active ? &record : nullptr;
            }

            [[nodiscard]] const TextureRecord* resolve(TextureHandle handle) const noexcept
            {
                if (!handle || handle.value > textures_.size())
                    return nullptr;
                const TextureRecord& record = textures_[handle.value - 1u];
                return record.active ? &record : nullptr;
            }

            ResidencyContractCommandContext context_{};
            std::vector<TextureRecord> textures_{};
            std::uint32_t buffer_{};
            std::uint32_t sampler_{};
            std::uint32_t shader_{};
            std::uint32_t pipeline_{};
            std::uint32_t material_{};
            std::uint32_t target_{};
            std::uint32_t binding_{};
            std::uint32_t mesh_{};
            std::uint32_t model_{};
        };

        [[nodiscard]] constexpr ArtifactKey contract_artifact(
            std::uint64_t asset,
            std::uint64_t revision,
            std::uint64_t digest) noexcept
        {
            return ArtifactKey{
                canvas2d::LogicalTextureReference{asset, revision},
                ArtifactDigest{{digest, digest + 1u, digest + 2u, digest + 3u}},
                1,
                4,
                4,
                1,
                TextureFormat::rgba8_unorm };
        }
    }

    [[nodiscard]] ResidencyContractFailure texture_residency_runtime_contract_failure()
    {
        std::array<std::byte, 64> pixels{};
        TextureUploadDesc upload{};
        upload.width = 4;
        upload.height = 4;
        upload.row_pitch_bytes = 16;
        upload.format = TextureFormat::rgba8_unorm;
        upload.data = pixels.data();
        upload.size_bytes = pixels.size();
        if (!epochengine::valid(upload) || minimum_texture_upload_bytes(upload) != pixels.size())
            return ResidencyContractFailure::upload_descriptor;

        detail::ResidencyContractDevice device{};
        ResidencyLimits limits{};
        limits.maximum_entries = 2;
        limits.maximum_resident_bytes = 128;
        limits.maximum_single_texture_bytes = 64;
        limits.maximum_upload_bytes_per_frame = 128;
        limits.maximum_evictions_per_acquire = 2;
        TextureResidencyCache cache{device, 1, limits};
        if (!cache.begin_frame(1))
            return ResidencyContractFailure::first_acquire;

        const ResidencyRequest firstRequest{
            detail::contract_artifact(1, 1, 10),
            TexturePayloadView{pixels.data(), pixels.size(), 16},
            ResidencyPolicy{.priority = 7},
            1,
            "contract.first" };
        const AcquireResult first = cache.acquire(firstRequest);
        if (!first || first.code != ResidencyCode::resident
            || !first.handle || !first.physical || device.create_count != 1
            || device.upload_count != 1)
        {
            return ResidencyContractFailure::first_acquire;
        }

        const AcquireResult reused = cache.acquire(firstRequest);
        if (!reused || reused.code != ResidencyCode::reused
            || reused.handle != first.handle || reused.physical != first.physical
            || device.create_count != 1 || device.upload_count != 1)
        {
            return ResidencyContractFailure::cache_reuse;
        }

        if (!device.mark_not_ready(first.physical))
            return ResidencyContractFailure::recreation;
        device.fail_next_upload = true;
        ResidencyRequest weakerRecreationRequest = firstRequest;
        weakerRecreationRequest.policy.priority = 1;
        const AcquireResult failedRecreation = cache.acquire(weakerRecreationRequest);
        if (failedRecreation.code != ResidencyCode::backend_upload_failed
            || !cache.resolve(first.handle)
            || device.destroy_count != 1)
        {
            return ResidencyContractFailure::recreation;
        }
        const AcquireResult transactionalRecreation = cache.acquire(weakerRecreationRequest);
        if (!transactionalRecreation
            || transactionalRecreation.code != ResidencyCode::recreated
            || transactionalRecreation.handle == first.handle
            || cache.resolve(first.handle)
            || !cache.resolve(transactionalRecreation.handle)
            || cache.resolve(transactionalRecreation.handle)->priority != 7
            || cache.resolve(transactionalRecreation.handle)->pinned
            || !cache.resolve(transactionalRecreation.handle)->evictable
            || device.destroy_count != 2)
        {
            return ResidencyContractFailure::recreation;
        }

        ResidencyRequest secondRequest = firstRequest;
        secondRequest.artifact = detail::contract_artifact(2, 1, 20);
        secondRequest.frame_sequence = 2;
        secondRequest.policy.pin = true;
        secondRequest.policy.priority = 10;
        const AcquireResult second = cache.acquire(secondRequest);
        if (!second || cache.set_pinned(second.handle, true) != ResidencyCode::reused)
            return ResidencyContractFailure::pinning;

        if (!cache.begin_frame(2))
            return ResidencyContractFailure::eviction;
        const std::uint64_t evictionsBeforeFailedPlan =
            cache.metrics().evictions;
        const std::uint64_t residentBeforeFailedPlan =
            cache.metrics().resident_bytes;
        const std::uint64_t destroysBeforeFailedPlan = device.destroy_count;
        device.throw_next_upload = true;
        ResidencyRequest failedPlanRequest = firstRequest;
        failedPlanRequest.artifact = detail::contract_artifact(3, 1, 30);
        failedPlanRequest.frame_sequence = 2;
        const AcquireResult failedPlan = cache.acquire(failedPlanRequest);
        if (failedPlan.code != ResidencyCode::backend_upload_failed
            || failedPlan.evicted_entries != 0
            || !cache.resolve(transactionalRecreation.handle)
            || !cache.resolve(second.handle)
            || cache.resolve(failedPlanRequest.artifact)
            || cache.metrics().evictions != evictionsBeforeFailedPlan
            || cache.metrics().active_entries != 2
            || cache.metrics().resident_bytes != residentBeforeFailedPlan
            || device.destroy_count != destroysBeforeFailedPlan + 1u)
        {
            return ResidencyContractFailure::upload_failure;
        }

        ResidencyRequest thirdRequest = firstRequest;
        thirdRequest.artifact = detail::contract_artifact(3, 1, 30);
        thirdRequest.frame_sequence = 2;
        const AcquireResult third = cache.acquire(thirdRequest);
        if (!third || third.evicted_entries != 1 || cache.resolve(transactionalRecreation.handle)
            || !cache.resolve(second.handle) || !cache.resolve(third.handle))
        {
            return ResidencyContractFailure::eviction;
        }
        const std::uint64_t releasesBeforeStale =
            cache.metrics().explicit_releases;
        if (cache.resolve(transactionalRecreation.handle)
            || cache.touch(transactionalRecreation.handle, 2)
                != ResidencyCode::stale_handle
            || cache.set_pinned(transactionalRecreation.handle, true)
                != ResidencyCode::stale_handle
            || cache.release(transactionalRecreation.handle)
                != ResidencyCode::stale_handle
            || cache.metrics().explicit_releases != releasesBeforeStale)
        {
            return ResidencyContractFailure::stale_handle;
        }

        if (cache.reset_backend_epoch(1)
            || !cache.reset_backend_epoch(2)
            || !cache.snapshots().empty()
            || cache.metrics().active_entries != 0
            || cache.metrics().resident_bytes != 0)
        {
            return ResidencyContractFailure::backend_reset;
        }

        ResidencyRequest recreatedRequest = firstRequest;
        recreatedRequest.frame_sequence = 3;
        const AcquireResult recreated = cache.acquire(recreatedRequest);
        if (!recreated || recreated.code != ResidencyCode::resident
            || recreated.handle == first.handle || cache.backend_epoch() != 2)
        {
            return ResidencyContractFailure::recreation;
        }

        if (cache.release(recreated.handle) != ResidencyCode::resident)
            return ResidencyContractFailure::recreation;
        device.fail_next_upload = true;
        ResidencyRequest failedRequest = firstRequest;
        failedRequest.artifact = detail::contract_artifact(4, 1, 40);
        failedRequest.frame_sequence = 4;
        const AcquireResult failed = cache.acquire(failedRequest);
        if (failed.code != ResidencyCode::backend_upload_failed
            || cache.resolve(failedRequest.artifact))
        {
            return ResidencyContractFailure::upload_failure;
        }

        ResidencyLimits uploadLimits = limits;
        uploadLimits.maximum_upload_bytes_per_frame = 64;
        detail::ResidencyContractDevice uploadDevice{};
        TextureResidencyCache uploadCache{uploadDevice, 1, uploadLimits};
        ResidencyRequest budgetFirst = firstRequest;
        budgetFirst.frame_sequence = 1;
        ResidencyRequest budgetSecond = secondRequest;
        budgetSecond.frame_sequence = 1;
        const AcquireResult budgetResident = uploadCache.acquire(budgetFirst);
        ResidencyRequest payloadFreeReuse = budgetFirst;
        payloadFreeReuse.payload = {};
        const AcquireResult budgetReused = uploadCache.acquire(payloadFreeReuse);
        if (!budgetResident || !budgetReused
            || budgetReused.code != ResidencyCode::reused
            || uploadCache.acquire(budgetSecond).code != ResidencyCode::upload_budget_exceeded)
        {
            return ResidencyContractFailure::upload_budget;
        }

        ResidencyLimits transientLimits = limits;
        transientLimits.maximum_transient_replacement_bytes = 32;
        detail::ResidencyContractDevice transientDevice{};
        TextureResidencyCache transientCache{
            transientDevice,
            1,
            transientLimits};
        const AcquireResult transientResident =
            transientCache.acquire(firstRequest);
        if (!transientResident
            || !transientDevice.mark_not_ready(transientResident.physical))
        {
            return ResidencyContractFailure::recreation;
        }
        const AcquireResult refusedTransient =
            transientCache.acquire(firstRequest);
        if (refusedTransient.code
                != ResidencyCode::resident_budget_exceeded
            || !transientCache.resolve(transientResident.handle)
            || transientDevice.create_count != 1
            || transientDevice.destroy_count != 0)
        {
            return ResidencyContractFailure::recreation;
        }

        detail::ResidencyContractDevice readinessDevice{};
        readinessDevice.fail_next_ready_check = true;
        TextureResidencyCache readinessCache{
            readinessDevice,
            1,
            limits};
        const AcquireResult notReady = readinessCache.acquire(firstRequest);
        const ResidencyMetrics& readinessMetrics = readinessCache.metrics();
        if (notReady.code != ResidencyCode::backend_not_ready
            || readinessMetrics.uploads != 1
            || readinessMetrics.uploaded_bytes != 64
            || readinessMetrics.current_frame_upload_bytes != 64
            || readinessMetrics.upload_failures != 1
            || readinessMetrics.rejected_requests != 1
            || readinessMetrics.active_entries != 0
            || readinessMetrics.resident_bytes != 0
            || readinessDevice.create_count != 1
            || readinessDevice.upload_count != 1
            || readinessDevice.destroy_count != 1)
        {
            return ResidencyContractFailure::upload_failure;
        }

        const ResidencyMetrics& metrics = cache.metrics();
        if (metrics.allocations != 5
            || metrics.recreations != 1
            || metrics.uploads != 5
            || metrics.upload_failures != 3
            || metrics.cache_hits != 1
            || metrics.evictions != 1
            || metrics.explicit_releases != 1
            || metrics.backend_retirements != 1
            || metrics.rejected_requests != 3
            || metrics.acquire_requests != 9
            || metrics.uploaded_bytes != 320
            || metrics.active_entries != 0
            || metrics.resident_bytes != 0
            || metrics.peak_entries != 2
            || metrics.peak_resident_bytes != 128
            || metrics.current_frame != 4
            || metrics.current_frame_upload_bytes != 0
            || metrics.backend_epoch != 2
            || device.destroy_count != 8)
        {
            return ResidencyContractFailure::metrics;
        }

        return ResidencyContractFailure::none;
    }

    [[nodiscard]] bool texture_residency_runtime_contract()
    {
        return texture_residency_runtime_contract_failure()
            == ResidencyContractFailure::none;
    }
}
