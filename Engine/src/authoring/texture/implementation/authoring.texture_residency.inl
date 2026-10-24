    PhysicalResidencyPlan plan_physical_residency(
        const CompiledTextureArtifactIdentity& artifact,
        const PhysicalResidencyCapabilities& capabilities,
        const PhysicalResidencyPolicy& policy) noexcept
    {
        PhysicalResidencyPlan result{};
        result.artifact = artifact;
        if (!artifact
            || artifact.profile.compiler_schema_version == 0
            || artifact.estimated_artifact_bytes == 0)
        {
            result.status = ResidencyPlanStatus::invalid_artifact;
            return result;
        }
        if (capabilities.maximum_texture_dimension == 0
            || capabilities.maximum_resident_bytes == 0
            || (capabilities.sparse_images
                && capabilities.sparse_page_extent == 0))
        {
            result.status = ResidencyPlanStatus::invalid_capabilities;
            return result;
        }
        if (artifact.width > capabilities.maximum_texture_dimension
            || artifact.height > capabilities.maximum_texture_dimension)
        {
            result.status = ResidencyPlanStatus::no_compatible_strategy;
            return result;
        }

        std::uint64_t sparsePagePixels{};
        std::uint64_t sparsePageBytes{};
        if (capabilities.sparse_images
            && (!checked_multiply(
                    capabilities.sparse_page_extent,
                    capabilities.sparse_page_extent,
                    sparsePagePixels)
                || !checked_multiply(
                    sparsePagePixels,
                    4,
                    sparsePageBytes)))
        {
            result.status = ResidencyPlanStatus::arithmetic_overflow;
            return result;
        }

        std::vector<std::uint64_t> mipSizes{};
        if (!artifact_mip_sizes(artifact, mipSizes))
        {
            result.status = ResidencyPlanStatus::arithmetic_overflow;
            return result;
        }
        const std::uint64_t calculatedArtifactBytes =
            range_sum(mipSizes, 0, artifact.mip_count);
        if (calculatedArtifactBytes != artifact.estimated_artifact_bytes)
        {
            result.status = ResidencyPlanStatus::invalid_artifact;
            return result;
        }
        const std::uint64_t hardBudget =
            (std::min)(
                capabilities.maximum_resident_bytes,
                policy.target_resident_bytes == 0
                    ? capabilities.maximum_resident_bytes
                    : policy.target_resident_bytes);

        std::array<PhysicalResidencyKind, 7> candidates{
            policy.preferred,
            PhysicalResidencyKind::hybrid_sparse_tail,
            PhysicalResidencyKind::sparse_pages,
            PhysicalResidencyKind::bindless_image,
            PhysicalResidencyKind::atlas_region,
            PhysicalResidencyKind::standalone_image,
            policy.preferred
        };
        if (policy.allow_atlas_for_small_textures
            && artifact.width <= policy.atlas_small_texture_limit
            && artifact.height <= policy.atlas_small_texture_limit)
        {
            candidates[1] = PhysicalResidencyKind::atlas_region;
        }

        bool compatibleButOverBudget{};
        for (std::size_t candidateIndex = 0;
             candidateIndex < candidates.size();
             ++candidateIndex)
        {
            if (candidateIndex != 0 && !policy.allow_fallback)
                break;
            const PhysicalResidencyKind kind = candidates[candidateIndex];
            if (std::ranges::find(
                    candidates.begin(),
                    candidates.begin()
                        + static_cast<std::ptrdiff_t>(candidateIndex),
                    kind)
                != candidates.begin()
                    + static_cast<std::ptrdiff_t>(candidateIndex))
            {
                continue;
            }

            std::vector<ResidencyTier> tiers{};
            std::uint64_t residentBytes{};
            std::uint32_t descriptorCount{};
            bool compatible{};
            if (kind == PhysicalResidencyKind::standalone_image)
            {
                compatible = capabilities.standalone_images;
                residentBytes = artifact.estimated_artifact_bytes;
                descriptorCount = 1;
                if (compatible)
                {
                    tiers.push_back(ResidencyTier{
                        .kind = kind,
                        .first_mip = 0,
                        .mip_count = artifact.mip_count,
                        .logical_tile_count = 1,
                        .estimated_resident_bytes = residentBytes,
                        .descriptor_count = 1
                    });
                }
            }
            else if (kind == PhysicalResidencyKind::atlas_region)
            {
                compatible = capabilities.atlas_regions
                    && policy.allow_atlas_for_small_textures
                    && artifact.width <= capabilities.maximum_atlas_dimension
                    && artifact.height <= capabilities.maximum_atlas_dimension
                    && artifact.width <= policy.atlas_small_texture_limit
                    && artifact.height <= policy.atlas_small_texture_limit;
                residentBytes = artifact.estimated_artifact_bytes;
                descriptorCount = 1;
                if (compatible)
                {
                    tiers.push_back(ResidencyTier{
                        .kind = kind,
                        .first_mip = 0,
                        .mip_count = artifact.mip_count,
                        .logical_tile_count = 1,
                        .estimated_resident_bytes = residentBytes,
                        .descriptor_count = 1
                    });
                }
            }
            else if (kind == PhysicalResidencyKind::bindless_image)
            {
                compatible = capabilities.bindless_images
                    && capabilities.available_bindless_descriptors != 0;
                residentBytes = artifact.estimated_artifact_bytes;
                descriptorCount = 1;
                if (compatible)
                {
                    tiers.push_back(ResidencyTier{
                        .kind = kind,
                        .first_mip = 0,
                        .mip_count = artifact.mip_count,
                        .logical_tile_count = 1,
                        .estimated_resident_bytes = residentBytes,
                        .descriptor_count = 1
                    });
                }
            }
            else if (kind == PhysicalResidencyKind::sparse_pages)
            {
                compatible = capabilities.sparse_images;
                const std::uint64_t logicalPages = compatible
                    ? logical_page_count(
                        artifact,
                        capabilities.sparse_page_extent,
                        0,
                        artifact.mip_count)
                    : 0;
                residentBytes = compatible
                    ? (std::min)(
                        artifact.estimated_artifact_bytes,
                        (std::max)(sparsePageBytes, hardBudget))
                    : 0;
                descriptorCount = 1;
                if (compatible)
                {
                    tiers.push_back(ResidencyTier{
                        .kind = kind,
                        .first_mip = 0,
                        .mip_count = artifact.mip_count,
                        .logical_tile_count = logicalPages,
                        .estimated_resident_bytes = residentBytes,
                        .descriptor_count = 1
                    });
                }
            }
            else
            {
                const std::uint8_t streamedMips = (std::min)(
                    policy.minimum_streamed_mip_count,
                    static_cast<std::uint8_t>(
                        artifact.mip_count > 1
                            ? artifact.mip_count - 1
                            : 0));
                compatible = capabilities.sparse_images
                    && capabilities.standalone_images
                    && streamedMips != 0;
                if (compatible)
                {
                    const std::uint8_t tailMips =
                        artifact.mip_count - streamedMips;
                    const std::uint64_t sparseLogicalBytes =
                        range_sum(mipSizes, 0, streamedMips);
                    const std::uint64_t tailBytes =
                        range_sum(
                            mipSizes,
                            streamedMips,
                            tailMips);

                    const std::uint64_t sparseBudget =
                        hardBudget > tailBytes
                            ? hardBudget - tailBytes
                            : 0;
                    const std::uint64_t sparseResident =
                        (std::min)(
                            sparseLogicalBytes,
                            (std::max)(sparsePageBytes, sparseBudget));
                    residentBytes = tailBytes + sparseResident;
                    descriptorCount = 2;
                    tiers.push_back(ResidencyTier{
                        .kind = PhysicalResidencyKind::sparse_pages,
                        .first_mip = 0,
                        .mip_count = streamedMips,
                        .logical_tile_count = logical_page_count(
                            artifact,
                            capabilities.sparse_page_extent,
                            0,
                            streamedMips),
                        .estimated_resident_bytes = sparseResident,
                        .descriptor_count = 1
                    });
                    tiers.push_back(ResidencyTier{
                        .kind = PhysicalResidencyKind::standalone_image,
                        .first_mip = streamedMips,
                        .mip_count = tailMips,
                        .logical_tile_count = 1,
                        .estimated_resident_bytes = tailBytes,
                        .descriptor_count = 1
                    });
                }
            }

            if (!compatible)
                continue;
            if (residentBytes > hardBudget
                || residentBytes > capabilities.maximum_resident_bytes)
            {
                compatibleButOverBudget = true;
                continue;
            }

            StableHash cacheHash{};
            cacheHash.add_string("epoch.texture.residency.v1");
            cacheHash.add_hash(artifact.key);
            cacheHash.add_integral(kind);
            cacheHash.add_integral(residentBytes);
            cacheHash.add_integral(descriptorCount);
            cacheHash.add_integral(capabilities.sparse_page_extent);
            cacheHash.add_integral(policy.target_resident_bytes);
            for (const ResidencyTier& tier : tiers)
            {
                cacheHash.add_integral(tier.kind);
                cacheHash.add_integral(tier.first_mip);
                cacheHash.add_integral(tier.mip_count);
                cacheHash.add_integral(tier.logical_tile_count);
                cacheHash.add_integral(tier.estimated_resident_bytes);
                cacheHash.add_integral(tier.descriptor_count);
            }
            result.status = ResidencyPlanStatus::ready_for_allocation;
            result.cache_key = cacheHash.finish();
            result.tiers = std::move(tiers);
            result.estimated_resident_bytes = residentBytes;
            result.descriptor_count = descriptorCount;
            return result;
        }

        result.status = compatibleButOverBudget
            ? ResidencyPlanStatus::budget_exceeded
            : ResidencyPlanStatus::no_compatible_strategy;
        return result;
    }

    const char* result_code_name(ResultCode code) noexcept
    {
        switch (code)
        {
        case ResultCode::success: return "success";
        case ResultCode::unchanged: return "unchanged";
        case ResultCode::invalid_document: return "invalid_document";
        case ResultCode::invalid_descriptor: return "invalid_descriptor";
        case ResultCode::invalid_handle: return "invalid_handle";
        case ResultCode::stale_handle: return "stale_handle";
        case ResultCode::invalid_temporal_point:
            return "invalid_temporal_point";
        case ResultCode::temporal_stream_mismatch:
            return "temporal_stream_mismatch";
        case ResultCode::temporal_regression: return "temporal_regression";
        case ResultCode::invalid_operation: return "invalid_operation";
        case ResultCode::unsupported_brush: return "unsupported_brush";
        case ResultCode::layer_limit_exceeded:
            return "layer_limit_exceeded";
        case ResultCode::tile_limit_exceeded: return "tile_limit_exceeded";
        case ResultCode::canonical_memory_budget_exceeded:
            return "canonical_memory_budget_exceeded";
        case ResultCode::history_operation_limit_exceeded:
            return "history_operation_limit_exceeded";
        case ResultCode::history_memory_budget_exceeded:
            return "history_memory_budget_exceeded";
        case ResultCode::checkpoint_limit_exceeded:
            return "checkpoint_limit_exceeded";
        case ResultCode::checkpoint_memory_budget_exceeded:
            return "checkpoint_memory_budget_exceeded";
        case ResultCode::stroke_sample_limit_exceeded:
            return "stroke_sample_limit_exceeded";
        case ResultCode::stroke_tile_limit_exceeded:
            return "stroke_tile_limit_exceeded";
        case ResultCode::nothing_to_undo: return "nothing_to_undo";
        case ResultCode::nothing_to_redo: return "nothing_to_redo";
        case ResultCode::checkpoint_not_found:
            return "checkpoint_not_found";
        case ResultCode::invalid_compile_profile:
            return "invalid_compile_profile";
        case ResultCode::residency_unavailable:
            return "residency_unavailable";
        case ResultCode::allocation_failure: return "allocation_failure";
        case ResultCode::arithmetic_overflow: return "arithmetic_overflow";
        case ResultCode::malformed_payload: return "malformed_payload";
        case ResultCode::unsupported_schema: return "unsupported_schema";
        case ResultCode::serialized_budget_exceeded:
            return "serialized_budget_exceeded";
        case ResultCode::integrity_failure: return "integrity_failure";
        }
        return "unknown";
    }

    const char* residency_kind_name(
        PhysicalResidencyKind kind) noexcept
    {
        switch (kind)
        {
        case PhysicalResidencyKind::standalone_image:
            return "standalone_image";
        case PhysicalResidencyKind::atlas_region: return "atlas_region";
        case PhysicalResidencyKind::bindless_image:
            return "bindless_image";
        case PhysicalResidencyKind::sparse_pages: return "sparse_pages";
        case PhysicalResidencyKind::hybrid_sparse_tail:
            return "hybrid_sparse_tail";
        }
        return "unknown";
    }

    const char* residency_plan_status_name(
        ResidencyPlanStatus status) noexcept
    {
        switch (status)
        {
        case ResidencyPlanStatus::ready_for_allocation:
            return "ready_for_allocation";
        case ResidencyPlanStatus::invalid_artifact:
            return "invalid_artifact";
        case ResidencyPlanStatus::invalid_capabilities:
            return "invalid_capabilities";
        case ResidencyPlanStatus::no_compatible_strategy:
            return "no_compatible_strategy";
        case ResidencyPlanStatus::budget_exceeded:
            return "budget_exceeded";
        case ResidencyPlanStatus::arithmetic_overflow:
            return "arithmetic_overflow";
        }
        return "unknown";
    }