// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#pragma once

#include <complex>
#include <cstdint>
#include <span>
#include <vector>

namespace epoch::extensions::fft_ocean
{
    struct OceanSettings
    {
        std::uint32_t resolution = 64;
        float tileLengthMeters = 128.0f;
        float windSpeedMetersPerSecond = 18.0f;
        float windDirectionRadians = 0.65f;
        float amplitude = 0.00035f;
        float choppiness = 1.25f;
        float normalStrength = 1.0f;
        float foamSlopeThreshold = 0.72f;
        float gravity = 9.80665f;
        std::uint32_t seed = 0xE903'F00Du;
    };

    struct OceanSample
    {
        float xMeters = 0.0f;
        float zMeters = 0.0f;
        float heightMeters = 0.0f;
        float displacementXMeters = 0.0f;
        float displacementZMeters = 0.0f;
        float normalX = 0.0f;
        float normalY = 1.0f;
        float normalZ = 0.0f;
        float foam = 0.0f;
    };

    enum class OceanCode : std::uint8_t
    {
        ready, invalid_settings, limit_exceeded, invalid_time,
        non_finite_output, allocation_failed
    };

    struct OceanResult
    {
        OceanCode code{OceanCode::ready};
        [[nodiscard]] explicit operator bool() const noexcept { return code == OceanCode::ready; }
    };

    struct OceanLimits
    {
        std::uint32_t maximumResolution{512};
        std::uint64_t maximumWorkingBytes{64u * 1024u * 1024u};
    };

    // Vector payload only; allocator bookkeeping is not included. Replacement
    // assumes an equally sized retained tile; reset checks its actual capacities.
    struct OceanMemory
    {
        std::uint64_t residentBytes{}, evaluationPeakBytes{}, replacementPeakBytes{};
    };
    struct OceanAdmission
    {
        OceanCode code{OceanCode::invalid_settings};
        OceanMemory memory{};
        [[nodiscard]] explicit operator bool() const noexcept { return code == OceanCode::ready; }
    };
    [[nodiscard]] OceanAdmission admit(const OceanSettings&, OceanLimits = {}) noexcept;

    class OceanTile
    {
    public:
        explicit OceanTile(OceanSettings settings = {}, OceanLimits limits = {});

        // Failure retains the last complete settings/samples. Construction
        // retains its original throwing contract because it cannot return a code.
        OceanResult reset(OceanSettings settings, OceanLimits limits = {}) noexcept;
        OceanResult evaluate(float timeSeconds) noexcept;
        // Double absolute time for Water/timeline consumers. The original
        // float entry retains its established numerical output.
        OceanResult evaluateAbsolute(double timeSeconds) noexcept;

        [[nodiscard]] const OceanSettings& settings() const noexcept;
        [[nodiscard]] std::uint32_t resolution() const noexcept;
        [[nodiscard]] float tileLengthMeters() const noexcept;
        [[nodiscard]] std::span<const OceanSample> samples() const noexcept;
        // Non-finite or unrepresentable coordinates return the neutral sample.
        [[nodiscard]] OceanSample sample(float xMeters, float zMeters) const noexcept;

    private:
        struct Empty {};
        explicit OceanTile(Empty) noexcept {}
        OceanSettings settings_{};
        OceanLimits limits_{};
        std::vector<OceanSample> samples_{};
        std::vector<std::complex<float>> spectrum0_{};
        std::vector<std::complex<float>> frequencyDomain_{};

        [[nodiscard]] bool rebuildSpectrum() noexcept;
        OceanResult evaluateTime(double timeSeconds, bool legacyFloat) noexcept;
        [[nodiscard]] std::uint64_t retainedPayloadBytes() const noexcept;
        [[nodiscard]] std::uint32_t wrappedIndex(std::int32_t x, std::int32_t z) const noexcept;
    };
}
