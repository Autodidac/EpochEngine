/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cstdint>
#include <limits>

export module render.canvas2d_limits;

import platform.budgets;
import perf.tier;
import capability.profile;
import render.device;
import render.canvas2d;
import render.canvas2d_cpu;
import render.texture_residency;

export namespace epochengine::canvas2d::limits
{
    struct NativeExecutionLimits final
    {
        CanvasLimits canvas{};
        cpu::RasterLimits raster{};
        texture_residency::ResidencyLimits residency{};
        std::uint64_t maximum_native_canvas_bytes{};
        bool enabled{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return enabled
                && epochengine::canvas2d::valid(canvas)
                && cpu::valid(raster)
                && texture_residency::valid(residency)
                && maximum_native_canvas_bytes != 0u
                && maximum_native_canvas_bytes
                    <= residency.maximum_single_texture_bytes
                && maximum_native_canvas_bytes
                    <= residency.maximum_upload_bytes_per_frame;
        }
    };

    [[nodiscard]] constexpr NativeExecutionLimits from_budgets(
        const Budgets& budgets) noexcept
    {
        NativeExecutionLimits output{};
        if (!budgets.canvas2d.valid()
            || budgets.max_w == 0u || budgets.max_h == 0u
            || budgets.vram_budget_bytes < 8u
            || budgets.upload_budget_bytes < sizeof(cpu::Rgba8))
        {
            return output;
        }

        const std::uint64_t residentBudget = (std::min)(
            budgets.canvas2d.maximum_logical_texture_bytes,
            budgets.vram_budget_bytes);
        const std::uint64_t transactionalSingleBudget = residentBudget / 2u;
        const std::uint64_t uploadBudget = (std::min)(
            budgets.upload_budget_bytes,
            transactionalSingleBudget);
        const std::uint64_t uploadPixels =
            uploadBudget / sizeof(cpu::Rgba8);
        const std::uint64_t canvasPixels = (std::min)(
            budgets.canvas2d.maximum_logical_canvas_pixels,
            uploadPixels);
        if (canvasPixels == 0u
            || canvasPixels
                > (std::numeric_limits<std::uint64_t>::max)()
                    / sizeof(cpu::Rgba8))
        {
            return output;
        }

        const std::uint64_t canvasBytes =
            canvasPixels * sizeof(cpu::Rgba8);
        const std::uint64_t maximumDimension64 = (std::max)(
            static_cast<std::uint64_t>(budgets.max_w),
            static_cast<std::uint64_t>(budgets.max_h));
        if (maximumDimension64
            > (std::numeric_limits<std::uint32_t>::max)())
        {
            return output;
        }

        output.canvas.maximum_canvas_dimension =
            static_cast<std::uint32_t>(maximumDimension64);
        output.canvas.maximum_canvas_pixels = canvasPixels;

        output.raster.maximum_canvas_pixels = canvasPixels;
        const std::uint64_t spriteTriangles =
            budgets.canvas2d.maximum_visible_sprites
                > (std::numeric_limits<std::uint64_t>::max)() / 2u
            ? (std::numeric_limits<std::uint64_t>::max)()
            : budgets.canvas2d.maximum_visible_sprites * 2u;
        output.raster.maximum_triangles = (std::min)(
            output.raster.maximum_triangles,
            spriteTriangles);

        output.residency.maximum_entries = 2u;
        output.residency.maximum_resident_bytes = residentBudget;
        output.residency.maximum_single_texture_bytes = canvasBytes;
        output.residency.maximum_transient_replacement_bytes = canvasBytes;
        output.residency.maximum_upload_bytes_per_frame = canvasBytes;
        output.residency.maximum_evictions_per_acquire = 1u;
        output.maximum_native_canvas_bytes = canvasBytes;
        output.enabled = true;
        if (!output.valid())
            return {};
        return output;
    }

    [[nodiscard]] constexpr NativeExecutionLimits for_backend(
        RendererBackendKind backend) noexcept
    {
        return from_budgets(
            capability::renderer_profile_for(backend).recommended_budgets);
    }

    enum class ContractFailure : std::uint8_t
    {
        none,
        invalid_budget,
        mobile_mapping,
        transaction_budget,
        raster_budget
    };

    [[nodiscard]] constexpr const char* contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "pass";
        case ContractFailure::invalid_budget: return "invalid_budget";
        case ContractFailure::mobile_mapping: return "mobile_mapping";
        case ContractFailure::transaction_budget: return "transaction_budget";
        case ContractFailure::raster_budget: return "raster_budget";
        }
        return "unknown";
    }

    [[nodiscard]] constexpr ContractFailure runtime_contract_failure() noexcept
    {
        Budgets invalid = platform::recommended_budgets_for_tier(
            perf::tier::mobile_30);
        invalid.upload_budget_bytes = 0u;
        if (from_budgets(invalid).valid())
            return ContractFailure::invalid_budget;

        const Budgets mobile = platform::recommended_budgets_for_tier(
            perf::tier::mobile_30);
        const NativeExecutionLimits mapped = from_budgets(mobile);
        constexpr std::uint64_t expectedPixels = 1280ull * 720ull;
        constexpr std::uint64_t expectedBytes =
            expectedPixels * sizeof(cpu::Rgba8);
        if (!mapped.valid()
            || mapped.canvas.maximum_canvas_dimension != 1280u
            || mapped.canvas.maximum_canvas_pixels != expectedPixels
            || mapped.raster.maximum_canvas_pixels != expectedPixels
            || mapped.maximum_native_canvas_bytes != expectedBytes
            || mapped.residency.maximum_entries != 2u
            || mapped.residency.maximum_single_texture_bytes != expectedBytes
            || mapped.residency.maximum_upload_bytes_per_frame != expectedBytes)
        {
            return ContractFailure::mobile_mapping;
        }
        if (mapped.residency.maximum_resident_bytes
                < mapped.residency.maximum_single_texture_bytes * 2u
            || mapped.residency.maximum_transient_replacement_bytes
                != mapped.residency.maximum_single_texture_bytes)
        {
            return ContractFailure::transaction_budget;
        }
        if (mapped.raster.maximum_triangles
                != mobile.canvas2d.maximum_visible_sprites * 2u
            || mapped.canvas.maximum_surface_pixels
                != CanvasLimits{}.maximum_surface_pixels
            || mapped.raster.maximum_presentation_pixels
                != cpu::RasterLimits{}.maximum_presentation_pixels)
        {
            return ContractFailure::raster_budget;
        }
        if (!for_backend(RendererBackendKind::opengl).valid()
            || !for_backend(RendererBackendKind::sdl3).valid()
            || !for_backend(RendererBackendKind::sfml3).valid()
            || !for_backend(RendererBackendKind::raylib3).valid()
            || !for_backend(RendererBackendKind::vulkan).valid()
            || !for_backend(RendererBackendKind::directx).valid()
            || !for_backend(RendererBackendKind::software).valid())
        {
            return ContractFailure::invalid_budget;
        }
        return ContractFailure::none;
    }
}
