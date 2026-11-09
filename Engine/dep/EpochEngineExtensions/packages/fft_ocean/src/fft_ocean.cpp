// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/fft_ocean/fft_ocean.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <new>
#include <numbers>
#include <stdexcept>

namespace epoch::extensions::fft_ocean
{
    namespace
    {
        using Complex = std::complex<float>;

        constexpr float kTwoPi = std::numbers::pi_v<float> * 2.0f;

        [[nodiscard]] bool is_power_of_two(const std::uint32_t value) noexcept
        {
            return value != 0u && (value & (value - 1u)) == 0u;
        }

        [[nodiscard]] std::uint32_t next_u32(std::uint32_t& state) noexcept
        {
            state = state * 1664525u + 1013904223u;
            return state;
        }

        [[nodiscard]] float uniform01(std::uint32_t& state) noexcept
        {
            return static_cast<float>((next_u32(state) >> 8u) & 0x00FF'FFFFu) / static_cast<float>(0x0100'0000u);
        }

        [[nodiscard]] float gaussian(std::uint32_t& state) noexcept
        {
            const float u0 = std::max(uniform01(state), 0.000001f);
            const float u1 = uniform01(state);
            return std::sqrt(-2.0f * std::log(u0)) * std::cos(kTwoPi * u1);
        }

        [[nodiscard]] Complex initial_spectrum_value(
            const OceanSettings& settings,
            const float kx,
            const float kz,
            std::uint32_t& randomState) noexcept
        {
            const float k2 = kx * kx + kz * kz;
            if (k2 < 0.000001f)
                return {};

            const float k = std::sqrt(k2);
            const float windX = std::cos(settings.windDirectionRadians);
            const float windZ = std::sin(settings.windDirectionRadians);
            const float kDotWind = (kx * windX + kz * windZ) / k;
            const float windLength = settings.windSpeedMetersPerSecond * settings.windSpeedMetersPerSecond / settings.gravity;
            const float damping = windLength * 0.001f;
            const float damping2 = damping * damping;

            const float phillips =
                settings.amplitude
                * std::exp(-1.0f / ((k * windLength) * (k * windLength)))
                / (k2 * k2)
                * (kDotWind * kDotWind)
                * std::exp(-k2 * damping2);

            const float scale = std::sqrt(std::max(phillips, 0.0f) * 0.5f);
            return { gaussian(randomState) * scale, gaussian(randomState) * scale };
        }

        void fft_1d(std::vector<Complex>& values, const bool inverse)
        {
            const std::size_t n = values.size();
            for (std::size_t i = 1u, j = 0u; i < n; ++i)
            {
                std::size_t bit = n >> 1u;
                for (; (j & bit) != 0u; bit >>= 1u)
                    j ^= bit;
                j ^= bit;
                if (i < j)
                    std::swap(values[i], values[j]);
            }

            for (std::size_t length = 2u; length <= n; length <<= 1u)
            {
                const float angle = (inverse ? kTwoPi : -kTwoPi) / static_cast<float>(length);
                const Complex root{ std::cos(angle), std::sin(angle) };
                for (std::size_t i = 0u; i < n; i += length)
                {
                    Complex w{ 1.0f, 0.0f };
                    const std::size_t half = length >> 1u;
                    for (std::size_t j = 0u; j < half; ++j)
                    {
                        const Complex even = values[i + j];
                        const Complex odd = values[i + j + half] * w;
                        values[i + j] = even + odd;
                        values[i + j + half] = even - odd;
                        w *= root;
                    }
                }
            }

            if (inverse)
            {
                const float invN = 1.0f / static_cast<float>(n);
                for (Complex& value : values)
                    value *= invN;
            }
        }

        void inverse_fft_2d(std::vector<Complex>& values, const std::uint32_t resolution)
        {
            std::vector<Complex> scratch(resolution);

            for (std::uint32_t z = 0u; z < resolution; ++z)
            {
                for (std::uint32_t x = 0u; x < resolution; ++x)
                    scratch[x] = values[z * resolution + x];
                fft_1d(scratch, true);
                for (std::uint32_t x = 0u; x < resolution; ++x)
                    values[z * resolution + x] = scratch[x];
            }

            for (std::uint32_t x = 0u; x < resolution; ++x)
            {
                for (std::uint32_t z = 0u; z < resolution; ++z)
                    scratch[z] = values[z * resolution + x];
                fft_1d(scratch, true);
                for (std::uint32_t z = 0u; z < resolution; ++z)
                    values[z * resolution + x] = scratch[z];
            }
        }

        [[nodiscard]] float clamp01(const float value) noexcept
        {
            return std::clamp(value, 0.0f, 1.0f);
        }
    }

    OceanAdmission admit(const OceanSettings& settings, const OceanLimits limits) noexcept
    {
        constexpr std::uint64_t absoluteBytes = 64u * 1024u * 1024u;
        if (!is_power_of_two(limits.maximumResolution) || limits.maximumResolution < 8u ||
            limits.maximumResolution > 512u || limits.maximumWorkingBytes == 0u ||
            limits.maximumWorkingBytes > absoluteBytes)
            return {OceanCode::limit_exceeded};
        if (!is_power_of_two(settings.resolution) || settings.resolution < 8u)
            return {OceanCode::invalid_settings};
        if (settings.resolution > limits.maximumResolution)
            return {OceanCode::limit_exceeded};
        for (const float value : {settings.tileLengthMeters, settings.windSpeedMetersPerSecond,
            settings.windDirectionRadians, settings.amplitude, settings.choppiness,
            settings.normalStrength, settings.foamSlopeThreshold, settings.gravity})
            if (!std::isfinite(value)) return {OceanCode::invalid_settings};
        if (settings.tileLengthMeters <= 0.0f || settings.windSpeedMetersPerSecond < 0.0f ||
            settings.amplitude < 0.0f || settings.choppiness < 0.0f ||
            settings.normalStrength <= 0.0f || settings.foamSlopeThreshold < 0.0f || settings.gravity <= 0.0f)
            return {OceanCode::invalid_settings};
        const float maximumK = kTwoPi * static_cast<float>(settings.resolution / 2u) / settings.tileLengthMeters;
        const float k2 = maximumK * maximumK * 2.0f;
        const float windLength = settings.windSpeedMetersPerSecond * settings.windSpeedMetersPerSecond / settings.gravity;
        const float damping = windLength * 0.001f;
        if (!std::isfinite(k2 * k2) || !std::isfinite(settings.gravity * std::sqrt(k2)) ||
            !std::isfinite(damping * damping) || !std::isfinite(windLength) ||
            !std::isfinite(settings.normalStrength * settings.normalStrength) ||
            settings.normalStrength * settings.normalStrength == 0.0f ||
            settings.tileLengthMeters / settings.resolution == 0.0f)
            return {OceanCode::invalid_settings};
        const std::uint64_t count = std::uint64_t(settings.resolution) * settings.resolution;
        OceanMemory memory;
        memory.residentBytes = count * (sizeof(OceanSample) + 2u * sizeof(Complex));
        const auto temporary = count * (sizeof(OceanSample) + sizeof(Complex)) +
            std::uint64_t(settings.resolution) * sizeof(Complex);
        memory.evaluationPeakBytes = memory.residentBytes + temporary;
        memory.replacementPeakBytes = memory.residentBytes + memory.evaluationPeakBytes;
        if (memory.evaluationPeakBytes > limits.maximumWorkingBytes)
            return {OceanCode::limit_exceeded, memory};
        return {OceanCode::ready, memory};
    }

    OceanTile::OceanTile(OceanSettings settings, OceanLimits limits)
    {
        const auto result = reset(settings, limits);
        if (result) return;
        if (result.code == OceanCode::allocation_failed) throw std::bad_alloc{};
        throw std::invalid_argument("fft_ocean construction refused invalid or over-budget settings");
    }

    std::uint64_t OceanTile::retainedPayloadBytes() const noexcept
    {
        return std::uint64_t(samples_.capacity()) * sizeof(OceanSample) +
            (std::uint64_t(spectrum0_.capacity()) + frequencyDomain_.capacity()) * sizeof(Complex);
    }

    OceanResult OceanTile::reset(OceanSettings settings, OceanLimits limits) noexcept
    {
        const auto admission = admit(settings, limits);
        if (!admission) return {admission.code};
        if (retainedPayloadBytes() > limits.maximumWorkingBytes - admission.memory.evaluationPeakBytes)
            return {OceanCode::limit_exceeded};
        try
        {
            OceanTile replacement{Empty{}};
            replacement.settings_ = settings;
            replacement.limits_ = limits;
            const std::size_t count = std::size_t(settings.resolution) * settings.resolution;
            replacement.samples_.resize(count);
            replacement.spectrum0_.resize(count);
            replacement.frequencyDomain_.resize(count);
            if (!replacement.rebuildSpectrum()) return {OceanCode::non_finite_output};
            const auto evaluated = replacement.evaluate(0.0f);
            if (!evaluated) return evaluated;
            settings_ = settings;
            limits_ = limits;
            samples_.swap(replacement.samples_);
            spectrum0_.swap(replacement.spectrum0_);
            frequencyDomain_.swap(replacement.frequencyDomain_);
            return {};
        }
        catch (...) { return {OceanCode::allocation_failed}; }
    }

    bool OceanTile::rebuildSpectrum() noexcept
    {
        std::uint32_t randomState = settings_.seed;
        const float length = settings_.tileLengthMeters;
        const std::int32_t resolution = static_cast<std::int32_t>(settings_.resolution);

        for (std::int32_t z = 0; z < resolution; ++z)
        {
            const std::int32_t shiftedZ = z < resolution / 2 ? z : z - resolution;
            const float kz = kTwoPi * static_cast<float>(shiftedZ) / length;

            for (std::int32_t x = 0; x < resolution; ++x)
            {
                const std::int32_t shiftedX = x < resolution / 2 ? x : x - resolution;
                const float kx = kTwoPi * static_cast<float>(shiftedX) / length;
                const auto value = initial_spectrum_value(settings_, kx, kz, randomState);
                if (!std::isfinite(value.real()) || !std::isfinite(value.imag())) return false;
                spectrum0_[static_cast<std::size_t>(z) * settings_.resolution + static_cast<std::size_t>(x)] = value;
            }
        }
        return true;
    }

    OceanResult OceanTile::evaluate(const float timeSeconds) noexcept
    {
        return evaluateTime(timeSeconds,true);
    }

    OceanResult OceanTile::evaluateAbsolute(const double timeSeconds) noexcept
    {
        return evaluateTime(timeSeconds,false);
    }

    OceanResult OceanTile::evaluateTime(const double timeSeconds,const bool legacyFloat) noexcept
    {
        if (!std::isfinite(timeSeconds)) return {OceanCode::invalid_time};
        const auto admission = admit(settings_, limits_);
        if (!admission) return {admission.code};
        const auto count = std::size_t(settings_.resolution) * settings_.resolution;
        if (samples_.size() != count || spectrum0_.size() != count || frequencyDomain_.size() != count)
            return {OceanCode::invalid_settings};
        const auto temporary = admission.memory.evaluationPeakBytes - admission.memory.residentBytes;
        if (retainedPayloadBytes() > limits_.maximumWorkingBytes - temporary)
            return {OceanCode::limit_exceeded};
        const float maximumK = kTwoPi * static_cast<float>(settings_.resolution / 2u) / settings_.tileLengthMeters;
        const float maximumOmega = std::sqrt(settings_.gravity * std::sqrt(maximumK * maximumK * 2.0f));
        if (!std::isfinite(maximumOmega * timeSeconds)
            ||(legacyFloat&&!std::isfinite(maximumOmega*static_cast<float>(timeSeconds))))
            return {OceanCode::invalid_time};
        try
        {
            const std::uint32_t n = settings_.resolution;
            const float length = settings_.tileLengthMeters;
            std::vector<OceanSample> nextSamples(samples_.size());

            for (std::uint32_t z = 0u; z < n; ++z)
            {
                const std::int32_t shiftedZ = z < n / 2u ? static_cast<std::int32_t>(z) : static_cast<std::int32_t>(z) - static_cast<std::int32_t>(n);
                const float kz = kTwoPi * static_cast<float>(shiftedZ) / length;

                for (std::uint32_t x = 0u; x < n; ++x)
                {
                    const std::int32_t shiftedX = x < n / 2u ? static_cast<std::int32_t>(x) : static_cast<std::int32_t>(x) - static_cast<std::int32_t>(n);
                    const float kx = kTwoPi * static_cast<float>(shiftedX) / length;
                    const float k = std::sqrt(kx * kx + kz * kz);
                    const float omega = std::sqrt(settings_.gravity * k);
                    const double angle = legacyFloat?double(omega*static_cast<float>(timeSeconds))
                        :double(omega)*timeSeconds;
                    if (!std::isfinite(angle)) return {OceanCode::invalid_time};
                    const Complex phase = legacyFloat
                        ?Complex{std::cos(static_cast<float>(angle)),std::sin(static_cast<float>(angle))}
                        :Complex{static_cast<float>(std::cos(angle)),static_cast<float>(std::sin(angle))};

                    const std::uint32_t oppositeX = (n - x) & (n - 1u);
                    const std::uint32_t oppositeZ = (n - z) & (n - 1u);
                    const Complex h0 = spectrum0_[z * n + x];
                    const Complex h0Opposite = std::conj(spectrum0_[oppositeZ * n + oppositeX]);
                    frequencyDomain_[z * n + x] = h0 * phase + h0Opposite * std::conj(phase);
                }
            }

            std::vector<Complex> spatial(frequencyDomain_.size());
            for (std::size_t index = 0; index < frequencyDomain_.size(); ++index)
                spatial[index] = frequencyDomain_[index];
            inverse_fft_2d(spatial, n);

            // Phillips is a continuous spectral density. Each discrete mode
            // represents a frequency cell of area deltaK squared, so its
            // amplitude carries deltaK. Our inverse FFT divides by N twice;
            // cancel that numerical normalization when producing metres.
            // Without this factor, increasing resolution makes the sea flatter
            // and the default 18 m/s ocean produces centimetre-scale waves.
            const float spectrumToMeters = static_cast<float>(n) * static_cast<float>(n) * kTwoPi / length;

            for (std::uint32_t z = 0u; z < n; ++z)
            {
                for (std::uint32_t x = 0u; x < n; ++x)
                {
                    const float u = static_cast<float>(x) / static_cast<float>(n);
                    const float v = static_cast<float>(z) / static_cast<float>(n);
                    OceanSample& out = nextSamples[z * n + x];
                    out.xMeters = (u - 0.5f) * length;
                    out.zMeters = (v - 0.5f) * length;
                    out.heightMeters = spatial[z * n + x].real() * spectrumToMeters;
                }
            }

            const float cellSize = length / static_cast<float>(n);
            for (std::uint32_t z = 0u; z < n; ++z)
            {
                for (std::uint32_t x = 0u; x < n; ++x)
                {
                    OceanSample& out = nextSamples[z * n + x];
                    const float hL = nextSamples[wrappedIndex(static_cast<std::int32_t>(x) - 1, static_cast<std::int32_t>(z))].heightMeters;
                    const float hR = nextSamples[wrappedIndex(static_cast<std::int32_t>(x) + 1, static_cast<std::int32_t>(z))].heightMeters;
                    const float hD = nextSamples[wrappedIndex(static_cast<std::int32_t>(x), static_cast<std::int32_t>(z) - 1)].heightMeters;
                    const float hU = nextSamples[wrappedIndex(static_cast<std::int32_t>(x), static_cast<std::int32_t>(z) + 1)].heightMeters;

                    const float slopeX = (hR - hL) / (2.0f * cellSize);
                    const float slopeZ = (hU - hD) / (2.0f * cellSize);
                    const float invLength = 1.0f / std::sqrt(slopeX * slopeX + settings_.normalStrength * settings_.normalStrength + slopeZ * slopeZ);
                    if (!std::isfinite(invLength) || invLength <= 0.0f)
                        return {OceanCode::non_finite_output};

                    out.displacementXMeters = -slopeX * settings_.choppiness;
                    out.displacementZMeters = -slopeZ * settings_.choppiness;
                    out.normalX = -slopeX * invLength;
                    out.normalY = settings_.normalStrength * invLength;
                    out.normalZ = -slopeZ * invLength;
                    out.foam = clamp01((std::sqrt(slopeX * slopeX + slopeZ * slopeZ) - settings_.foamSlopeThreshold) * 4.0f);
                    for (const float value : {out.heightMeters, out.displacementXMeters, out.displacementZMeters,
                        out.normalX, out.normalY, out.normalZ, out.foam})
                        if (!std::isfinite(value)) return {OceanCode::non_finite_output};
                }
            }
            samples_.swap(nextSamples);
            return {};
        }
        catch (...) { return {OceanCode::allocation_failed}; }
    }

    const OceanSettings& OceanTile::settings() const noexcept
    {
        return settings_;
    }

    std::uint32_t OceanTile::resolution() const noexcept
    {
        return settings_.resolution;
    }

    float OceanTile::tileLengthMeters() const noexcept
    {
        return settings_.tileLengthMeters;
    }

    std::span<const OceanSample> OceanTile::samples() const noexcept
    {
        return samples_;
    }

    OceanSample OceanTile::sample(const float xMeters, const float zMeters) const noexcept
    {
        if (samples_.size() != std::size_t(settings_.resolution) * settings_.resolution) return {};
        const float n = static_cast<float>(settings_.resolution);
        const float u = (xMeters / settings_.tileLengthMeters + 0.5f) * n;
        const float v = (zMeters / settings_.tileLengthMeters + 0.5f) * n;
        if (!std::isfinite(u) || !std::isfinite(v) ||
            double(std::floor(u)) < double((std::numeric_limits<std::int32_t>::min)()) ||
            double(std::floor(v)) < double((std::numeric_limits<std::int32_t>::min)()) ||
            double(std::floor(u)) >= double((std::numeric_limits<std::int32_t>::max)()) ||
            double(std::floor(v)) >= double((std::numeric_limits<std::int32_t>::max)())) return {};
        const auto x0 = static_cast<std::int32_t>(std::floor(u));
        const auto z0 = static_cast<std::int32_t>(std::floor(v));
        const float tx = u - std::floor(u);
        const float tz = v - std::floor(v);

        const OceanSample a = samples_[wrappedIndex(x0, z0)];
        const OceanSample b = samples_[wrappedIndex(x0 + 1, z0)];
        const OceanSample c = samples_[wrappedIndex(x0, z0 + 1)];
        const OceanSample d = samples_[wrappedIndex(x0 + 1, z0 + 1)];

        const auto lerp = [](const float lhs, const float rhs, const float t) noexcept
        {
            return lhs + (rhs - lhs) * t;
        };

        OceanSample out{};
        out.xMeters = xMeters;
        out.zMeters = zMeters;
        out.heightMeters = lerp(lerp(a.heightMeters, b.heightMeters, tx), lerp(c.heightMeters, d.heightMeters, tx), tz);
        out.displacementXMeters = lerp(lerp(a.displacementXMeters, b.displacementXMeters, tx), lerp(c.displacementXMeters, d.displacementXMeters, tx), tz);
        out.displacementZMeters = lerp(lerp(a.displacementZMeters, b.displacementZMeters, tx), lerp(c.displacementZMeters, d.displacementZMeters, tx), tz);
        out.normalX = lerp(lerp(a.normalX, b.normalX, tx), lerp(c.normalX, d.normalX, tx), tz);
        out.normalY = lerp(lerp(a.normalY, b.normalY, tx), lerp(c.normalY, d.normalY, tx), tz);
        out.normalZ = lerp(lerp(a.normalZ, b.normalZ, tx), lerp(c.normalZ, d.normalZ, tx), tz);
        out.foam = lerp(lerp(a.foam, b.foam, tx), lerp(c.foam, d.foam, tx), tz);
        return out;
    }

    std::uint32_t OceanTile::wrappedIndex(const std::int32_t x, const std::int32_t z) const noexcept
    {
        const auto n = static_cast<std::int32_t>(settings_.resolution);
        const auto wrap = [n](const std::int32_t value) noexcept
        {
            const std::int32_t mod = value % n;
            return mod < 0 ? mod + n : mod;
        };
        return static_cast<std::uint32_t>(wrap(z) * n + wrap(x));
    }
}
