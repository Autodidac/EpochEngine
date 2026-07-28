/*
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 *
 * Bounded logical audio scheduling manager. A later backend adapter may
 * consume FrameMixPlan, but this module never reports physical playback.
 */
module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

export module audio.manager;

export import audio.types;

export namespace epochengine::audio
{
    struct AudioManagerLimits final
    {
        std::uint32_t maximum_sources{ 1'024 };
        std::uint32_t maximum_buses{ 64 };
        std::uint32_t maximum_pending_commands{ 4'096 };
        std::uint32_t maximum_sources_per_plan{ 1'024 };
    };

    class AudioManager final
    {
    public:
        explicit AudioManager(AudioManagerLimits limits = {});
        ~AudioManager();

        AudioManager(AudioManager&&) noexcept;
        AudioManager& operator=(AudioManager&&) noexcept;

        AudioManager(const AudioManager&) = delete;
        AudioManager& operator=(const AudioManager&) = delete;

        [[nodiscard]] const AudioManagerLimits& limits() const noexcept;
        [[nodiscard]] BusHandle master_bus() const noexcept;
        [[nodiscard]] PhysicalOutputState physical_output_state() const noexcept;

        [[nodiscard]] HandleResult<BusHandle> create_bus(
            const BusDescriptor& descriptor);
        [[nodiscard]] RegistryStatus destroy_bus(BusHandle bus);

        [[nodiscard]] HandleResult<SourceHandle> create_source(
            const SourceDescriptor& descriptor);
        [[nodiscard]] RegistryStatus destroy_source(SourceHandle source);

        [[nodiscard]] bool valid(BusHandle bus) const noexcept;
        [[nodiscard]] bool valid(SourceHandle source) const noexcept;

        [[nodiscard]] std::optional<BusSnapshot> bus_snapshot(
            BusHandle bus) const;
        [[nodiscard]] std::optional<SourceSnapshot> source_snapshot(
            SourceHandle source) const;

        [[nodiscard]] CommandReceipt submit(
            const AudioCommandRequest& request);
        [[nodiscard]] std::vector<CommandReceipt> submit_batch(
            std::span<const AudioCommandRequest> requests);

        [[nodiscard]] FrameMixPlan prepare_frame(const FrameRequest& request);
        [[nodiscard]] AudioManagerMetrics metrics() const noexcept;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation_;
    };

    [[nodiscard]] AudioCommandKind command_kind(
        const AudioCommandPayload& payload) noexcept;

    [[nodiscard]] AudioContractReport run_audio_manager_contract_tests();
}
