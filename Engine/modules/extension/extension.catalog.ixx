// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

export module extension.catalog;

import capability.profile;
import package.registry;

export namespace epochengine::extension_catalog {
using StableId = std::uint64_t;
using CapabilityMask = std::uint64_t;
using PlatformMask = std::uint32_t;

inline constexpr StableId kInvalidStableId{};
inline constexpr std::size_t kInvalidCatalogIndex =
    (std::numeric_limits<std::size_t>::max)();

enum class ActivationMode : std::uint8_t {
  script,
  package
};

enum class SourceProvenance : std::uint8_t {
  epoch_core,
  operator_owned_reconstruction,
  operator_owned_repository,
  third_party_pinned_source
};

enum class IntegrityRequirement : std::uint8_t {
  core_source_tree,
  manifest_sha256,
  immutable_revision_sha256_license
};

enum class Availability : std::uint8_t { descriptor_only };

enum class Platform : std::uint8_t {
  windows,
  linux,
  macos,
  mobile,
  console,
  web,
  headless
};

enum class Capability : std::uint8_t {
  scripting,
  package_assets,
  temporal_authoring,
  render_to_texture,
  portal_views,
  procedural_generation,
  voxel_fields,
  terrain,
  ocean_simulation,
  audio,
  local_ai_inference,
  guarded_ai_development,
  editor_workspace,
  headless_tools
};

[[nodiscard]] constexpr PlatformMask
platform_bit(const Platform platform) noexcept {
  return PlatformMask{1u} << static_cast<std::uint8_t>(platform);
}

template <typename... Platforms>
[[nodiscard]] constexpr PlatformMask
platform_mask(const Platforms... platforms) noexcept {
  return (PlatformMask{} | ... | platform_bit(platforms));
}

[[nodiscard]] constexpr CapabilityMask
capability_bit(const Capability capability) noexcept {
  return CapabilityMask{1ull} << static_cast<std::uint8_t>(capability);
}

template <typename... Capabilities>
[[nodiscard]] constexpr CapabilityMask
capability_mask(const Capabilities... capabilities) noexcept {
  return (CapabilityMask{} | ... | capability_bit(capabilities));
}

inline constexpr PlatformMask kDesktopPlatforms =
    platform_mask(Platform::windows, Platform::linux, Platform::macos);
inline constexpr PlatformMask kInteractivePlatforms =
    platform_mask(Platform::windows, Platform::linux, Platform::macos,
                  Platform::mobile, Platform::console);

struct SourceIdentity final {
  SourceProvenance provenance{SourceProvenance::operator_owned_reconstruction};
  std::string_view owner{};
  std::string_view repository{};
  std::string_view local_reconstruction_key{};
  std::string_view license_evidence{};
};

struct Entry final {
  StableId stable_id{kInvalidStableId};
  std::string_view package_id{};
  std::string_view display_name{};
  std::string_view summary{};
  CapabilityMask required_capabilities{};
  CapabilityMask optional_capabilities{};
  PlatformMask supported_platforms{};
  capability::Tier minimum_tier{capability::Tier::headless};
  ActivationMode activation{ActivationMode::package};
  SourceIdentity source{};
  IntegrityRequirement integrity{
      IntegrityRequirement::immutable_revision_sha256_license};
  Availability availability{Availability::descriptor_only};
  bool existing_package_registry_entry_required{};
};

enum class ValidationIssue : std::uint64_t {
  none = 0,
  missing_stable_id = 1ull << 0u,
  missing_package_id = 1ull << 1u,
  missing_display_name = 1ull << 2u,
  missing_summary = 1ull << 3u,
  missing_capability_gate = 1ull << 4u,
  missing_platform = 1ull << 5u,
  invalid_tier = 1ull << 6u,
  missing_source_owner = 1ull << 7u,
  missing_source_repository = 1ull << 8u,
  missing_reconstruction_key = 1ull << 9u,
  missing_license_evidence = 1ull << 10u,
  unsafe_integrity_policy = 1ull << 11u,
  mapped_package_missing = 1ull << 12u,
  duplicate_stable_id = 1ull << 13u,
  duplicate_package_id = 1ull << 14u,
  availability_claimed = 1ull << 15u
};

using ValidationIssues = std::uint64_t;

[[nodiscard]] constexpr ValidationIssues
issue_bit(const ValidationIssue issue) noexcept {
  return static_cast<ValidationIssues>(issue);
}

struct ValidationReport final {
  bool ok{};
  std::size_t entry_count{};
  std::size_t registry_mapped_count{};
  std::size_t first_invalid_index{kInvalidCatalogIndex};
  ValidationIssues issues{};
  std::uint64_t deterministic_digest{};
};

enum class ContractFailure : std::uint8_t {
  none,
  canonical_catalog_rejected,
  catalog_digest_unstable,
  lookup_failed,
  package_registry_mapping_failed,
  duplicate_stable_id_not_rejected,
  duplicate_package_id_not_rejected,
  missing_integrity_not_rejected,
  installation_state_exposed
};

inline constexpr std::string_view kExtensionsRepositoryIdentity =
    "Autodidac/EpochEngineExtensions";
inline constexpr std::string_view kOperatorOwner = "Autodidac";
inline constexpr std::string_view kRepositoryLicenseEvidence =
    "EpochEngineExtensions/LICENSE";

inline constexpr StableId kArcadeStableId = 0x4558540000000001ull;
inline constexpr StableId kForestFactoryStableId = 0x4558540000000002ull;
inline constexpr StableId kPlantLabStableId = 0x4558540000000003ull;
inline constexpr StableId kTerrainDemoStableId = 0x4558540000000004ull;
inline constexpr StableId kVoxelDemoStableId = 0x4558540000000005ull;
inline constexpr StableId kOceanDemoStableId = 0x4558540000000006ull;
inline constexpr StableId kPortalDemoStableId = 0x4558540000000007ull;
inline constexpr StableId kLocalAiStableId = 0x4558540000000008ull;

inline constexpr std::array<Entry, 8> kEntries{{
    {
        .stable_id = kArcadeStableId,
        .package_id = package_registry::kEngineArcadePackageId,
        .display_name = "Engine Arcade",
        .summary = "Optional mini-runtime scenes and cabinet assets for "
                   "render-to-texture arcade demonstrations.",
        .required_capabilities =
            capability_mask(Capability::scripting, Capability::package_assets,
                            Capability::render_to_texture),
        .optional_capabilities =
            capability_mask(Capability::audio, Capability::editor_workspace),
        .supported_platforms = kInteractivePlatforms,
        .minimum_tier = capability::Tier::portable_graphics,
        .activation = ActivationMode::package,
        .source = {.provenance =
                       SourceProvenance::operator_owned_reconstruction,
                   .owner = kOperatorOwner,
                   .repository = kExtensionsRepositoryIdentity,
                   .local_reconstruction_key = "packages/arcade",
                   .license_evidence = kRepositoryLicenseEvidence},
        .integrity = IntegrityRequirement::immutable_revision_sha256_license,
        .availability = Availability::descriptor_only,
        .existing_package_registry_entry_required = true,
    },
    {
        .stable_id = kForestFactoryStableId,
        .package_id = package_registry::kEngineForestFactoryPackageId,
        .display_name = "Forest Factory Heavy Assets",
        .summary = "Optional high-detail vegetation assets, voxel LOD "
                   "payloads, and placement libraries above the core Forest "
                   "Factory descriptor lane.",
        .required_capabilities = capability_mask(
            Capability::package_assets, Capability::procedural_generation,
            Capability::voxel_fields),
        .optional_capabilities = capability_mask(Capability::temporal_authoring,
                                                 Capability::editor_workspace),
        .supported_platforms = kInteractivePlatforms,
        .minimum_tier = capability::Tier::portable_graphics,
        .activation = ActivationMode::package,
        .source = {.provenance =
                       SourceProvenance::operator_owned_reconstruction,
                   .owner = kOperatorOwner,
                   .repository = kExtensionsRepositoryIdentity,
                   .local_reconstruction_key =
                       "packages/forest_factory_heavy_assets",
                   .license_evidence = kRepositoryLicenseEvidence},
        .integrity = IntegrityRequirement::immutable_revision_sha256_license,
        .availability = Availability::descriptor_only,
        .existing_package_registry_entry_required = true,
    },
    {
        .stable_id = kPlantLabStableId,
        .package_id = "engine_plant_lab_libraries",
        .display_name = "Plant Lab Libraries",
        .summary = "Reusable temporal L-system, node-graph, foliage, "
                   "vascular-form, and vegetation-authoring libraries for the "
                   "standalone Plant Lab editor.",
        .required_capabilities = capability_mask(
            Capability::package_assets, Capability::temporal_authoring,
            Capability::procedural_generation, Capability::editor_workspace),
        .optional_capabilities = capability_mask(Capability::voxel_fields),
        .supported_platforms = kDesktopPlatforms,
        .minimum_tier = capability::Tier::portable_graphics,
        .activation = ActivationMode::package,
        .source = {.provenance =
                       SourceProvenance::operator_owned_reconstruction,
                   .owner = kOperatorOwner,
                   .repository = kExtensionsRepositoryIdentity,
                   .local_reconstruction_key = "packages/plant_lab_libraries",
                   .license_evidence = kRepositoryLicenseEvidence},
        .integrity = IntegrityRequirement::immutable_revision_sha256_license,
        .availability = Availability::descriptor_only,
    },
    {
        .stable_id = kTerrainDemoStableId,
        .package_id = "demo_tiered_terrain",
        .display_name = "Tiered Terrain Demonstration",
        .summary = "Capability-gated terrain demonstration with headless data "
                   "preparation and portable through advanced renderer "
                   "presentation paths.",
        .required_capabilities =
            capability_mask(Capability::package_assets, Capability::terrain),
        .optional_capabilities = capability_mask(
            Capability::voxel_fields, Capability::procedural_generation),
        .supported_platforms =
            kInteractivePlatforms | platform_bit(Platform::headless),
        .minimum_tier = capability::Tier::headless,
        .activation = ActivationMode::package,
        .source = {.provenance =
                       SourceProvenance::operator_owned_reconstruction,
                   .owner = kOperatorOwner,
                   .repository = kExtensionsRepositoryIdentity,
                   .local_reconstruction_key = "demonstrations/tiered_terrain",
                   .license_evidence = kRepositoryLicenseEvidence},
        .integrity = IntegrityRequirement::immutable_revision_sha256_license,
        .availability = Availability::descriptor_only,
    },
    {
        .stable_id = kVoxelDemoStableId,
        .package_id = "demo_temporal_voxel",
        .display_name = "Temporal Voxel Demonstration",
        .summary =
            "Request-driven voxel field, chunk, ray, and reversible residency "
            "demonstration for CPU and renderer capability tiers.",
        .required_capabilities = capability_mask(Capability::package_assets,
                                                 Capability::voxel_fields),
        .optional_capabilities = capability_mask(
            Capability::temporal_authoring, Capability::procedural_generation),
        .supported_platforms =
            kInteractivePlatforms | platform_bit(Platform::headless),
        .minimum_tier = capability::Tier::headless,
        .activation = ActivationMode::package,
        .source = {.provenance =
                       SourceProvenance::operator_owned_reconstruction,
                   .owner = kOperatorOwner,
                   .repository = kExtensionsRepositoryIdentity,
                   .local_reconstruction_key = "demonstrations/temporal_voxel",
                   .license_evidence = kRepositoryLicenseEvidence},
        .integrity = IntegrityRequirement::immutable_revision_sha256_license,
        .availability = Availability::descriptor_only,
    },
    {
        .stable_id = kOceanDemoStableId,
        .package_id = "demo_fft_ocean",
        .display_name = "FFT Ocean Demonstration",
        .summary = "Optional ocean simulation and renderer comparison scene "
                   "with a deterministic fallback contract.",
        .required_capabilities = capability_mask(Capability::package_assets,
                                                 Capability::ocean_simulation),
        .optional_capabilities =
            capability_mask(Capability::temporal_authoring),
        .supported_platforms = kInteractivePlatforms,
        .minimum_tier = capability::Tier::portable_graphics,
        .activation = ActivationMode::package,
        .source = {.provenance =
                       SourceProvenance::operator_owned_reconstruction,
                   .owner = kOperatorOwner,
                   .repository = kExtensionsRepositoryIdentity,
                   .local_reconstruction_key = "demonstrations/fft_ocean",
                   .license_evidence = kRepositoryLicenseEvidence},
        .integrity = IntegrityRequirement::immutable_revision_sha256_license,
        .availability = Availability::descriptor_only,
    },
    {
        .stable_id = kPortalDemoStableId,
        .package_id = "demo_portal_rtt",
        .display_name = "Portal and RTT Demonstration",
        .summary =
            "Bounded recursive portal-view and render-to-texture demonstration "
            "consuming renderer-owned logical view requests.",
        .required_capabilities = capability_mask(Capability::package_assets,
                                                 Capability::render_to_texture,
                                                 Capability::portal_views),
        .optional_capabilities =
            capability_mask(Capability::temporal_authoring),
        .supported_platforms = kInteractivePlatforms,
        .minimum_tier = capability::Tier::portable_graphics,
        .activation = ActivationMode::package,
        .source = {.provenance =
                       SourceProvenance::operator_owned_reconstruction,
                   .owner = kOperatorOwner,
                   .repository = kExtensionsRepositoryIdentity,
                   .local_reconstruction_key = "demonstrations/portal_rtt",
                   .license_evidence = kRepositoryLicenseEvidence},
        .integrity = IntegrityRequirement::immutable_revision_sha256_license,
        .availability = Availability::descriptor_only,
    },
    {
        .stable_id = kLocalAiStableId,
        .package_id = package_registry::kLocalAiLlamaCppRuntimePackageId,
        .display_name = "Local AI Tooling",
        .summary = "Operator-approved local inference and guarded development "
                   "tooling; no listener, hidden service, model download, or "
                   "installation is implied.",
        .required_capabilities = capability_mask(
            Capability::local_ai_inference,
            Capability::guarded_ai_development),
        .optional_capabilities = capability_mask(Capability::headless_tools,
                                                 Capability::editor_workspace),
        .supported_platforms =
            kDesktopPlatforms | platform_bit(Platform::headless),
        .minimum_tier = capability::Tier::headless,
        .activation = ActivationMode::package,
        .source = {.provenance = SourceProvenance::third_party_pinned_source,
                   .owner = kOperatorOwner,
                   .repository = kExtensionsRepositoryIdentity,
                   .local_reconstruction_key = "tooling/local_ai",
                   .license_evidence = kRepositoryLicenseEvidence},
        .integrity = IntegrityRequirement::immutable_revision_sha256_license,
        .availability = Availability::descriptor_only,
        .existing_package_registry_entry_required = true,
    },
}};

[[nodiscard]] constexpr std::span<const Entry> entries() noexcept {
  return {kEntries.data(), kEntries.size()};
}

[[nodiscard]] constexpr bool
has_capability(const Entry &entry, const Capability capability) noexcept {
  return (entry.required_capabilities & capability_bit(capability)) != 0u ||
         (entry.optional_capabilities & capability_bit(capability)) != 0u;
}

[[nodiscard]] constexpr bool
supports_platform(const Entry &entry, const Platform platform) noexcept {
  return (entry.supported_platforms & platform_bit(platform)) != 0u;
}

[[nodiscard]] constexpr const Entry *find(const StableId stable_id) noexcept {
  for (const Entry &entry : kEntries) {
    if (entry.stable_id == stable_id) {
      return &entry;
    }
  }
  return nullptr;
}

[[nodiscard]] constexpr const Entry *
find(const std::string_view package_id) noexcept {
  for (const Entry &entry : kEntries) {
    if (entry.package_id == package_id) {
      return &entry;
    }
  }
  return nullptr;
}

namespace detail {
[[nodiscard]] constexpr std::uint64_t
digest_byte(std::uint64_t digest, const std::uint8_t value) noexcept {
  return (digest ^ value) * 1099511628211ull;
}

[[nodiscard]] constexpr std::uint64_t
digest_integer(std::uint64_t digest, std::uint64_t value) noexcept {
  for (std::uint8_t byte = 0u; byte < 8u; ++byte) {
    digest = digest_byte(digest, static_cast<std::uint8_t>(value & 0xffu));
    value >>= 8u;
  }
  return digest;
}

[[nodiscard]] constexpr std::uint64_t
digest_text(std::uint64_t digest, const std::string_view text) noexcept {
  for (const char character : text) {
    digest = digest_byte(digest, static_cast<std::uint8_t>(character));
  }
  return digest_byte(digest, 0xffu);
}

constexpr void add_issue(ValidationReport &report, const std::size_t index,
                         const ValidationIssue issue) noexcept {
  report.issues |= issue_bit(issue);
  if (report.first_invalid_index == kInvalidCatalogIndex) {
    report.first_invalid_index = index;
  }
}
} // namespace detail

[[nodiscard]] constexpr ValidationIssues
validate_entry(const Entry &entry) noexcept {
  ValidationIssues issues{};
  const auto add = [&issues](const ValidationIssue issue) constexpr {
    issues |= issue_bit(issue);
  };

  if (entry.stable_id == kInvalidStableId)
    add(ValidationIssue::missing_stable_id);
  if (entry.package_id.empty())
    add(ValidationIssue::missing_package_id);
  if (entry.display_name.empty())
    add(ValidationIssue::missing_display_name);
  if (entry.summary.empty())
    add(ValidationIssue::missing_summary);
  if ((entry.required_capabilities | entry.optional_capabilities) == 0u)
    add(ValidationIssue::missing_capability_gate);
  if (entry.supported_platforms == 0u)
    add(ValidationIssue::missing_platform);
  if (static_cast<std::uint8_t>(entry.minimum_tier) >
      static_cast<std::uint8_t>(capability::Tier::ray_tracing_pipeline)) {
    add(ValidationIssue::invalid_tier);
  }
  if (entry.source.owner.empty())
    add(ValidationIssue::missing_source_owner);
  if (entry.source.repository.empty())
    add(ValidationIssue::missing_source_repository);
  if (entry.source.local_reconstruction_key.empty())
    add(ValidationIssue::missing_reconstruction_key);
  if (entry.source.license_evidence.empty())
    add(ValidationIssue::missing_license_evidence);

  if (entry.source.provenance != SourceProvenance::epoch_core &&
      entry.integrity == IntegrityRequirement::core_source_tree) {
    add(ValidationIssue::unsafe_integrity_policy);
  }
  if (entry.existing_package_registry_entry_required &&
      package_registry::find(entry.package_id) == nullptr) {
    add(ValidationIssue::mapped_package_missing);
  }
  if (entry.availability != Availability::descriptor_only)
    add(ValidationIssue::availability_claimed);
  return issues;
}

[[nodiscard]] constexpr ValidationReport
validate(const std::span<const Entry> catalog) noexcept {
  ValidationReport report{};
  report.entry_count = catalog.size();
  report.deterministic_digest = 14695981039346656037ull;

  for (std::size_t index = 0; index < catalog.size(); ++index) {
    const Entry &entry = catalog[index];
    const ValidationIssues entryIssues = validate_entry(entry);
    if (entryIssues != 0u) {
      report.issues |= entryIssues;
      if (report.first_invalid_index == kInvalidCatalogIndex)
        report.first_invalid_index = index;
    }

    if (entry.existing_package_registry_entry_required)
      ++report.registry_mapped_count;

    report.deterministic_digest =
        detail::digest_integer(report.deterministic_digest, entry.stable_id);
    report.deterministic_digest =
        detail::digest_text(report.deterministic_digest, entry.package_id);
    report.deterministic_digest = detail::digest_integer(
        report.deterministic_digest, entry.required_capabilities);
    report.deterministic_digest = detail::digest_integer(
        report.deterministic_digest, entry.optional_capabilities);
    report.deterministic_digest = detail::digest_integer(
        report.deterministic_digest, entry.supported_platforms);
    report.deterministic_digest =
        detail::digest_integer(report.deterministic_digest,
                               static_cast<std::uint8_t>(entry.minimum_tier));
    report.deterministic_digest =
        detail::digest_integer(report.deterministic_digest,
                               static_cast<std::uint8_t>(entry.activation));
    report.deterministic_digest = detail::digest_text(
        report.deterministic_digest, entry.source.repository);
    report.deterministic_digest = detail::digest_text(
        report.deterministic_digest, entry.source.local_reconstruction_key);
    report.deterministic_digest =
        detail::digest_integer(report.deterministic_digest,
                               static_cast<std::uint8_t>(entry.integrity));
    for (std::size_t previous = 0; previous < index; ++previous) {
      if (catalog[previous].stable_id == entry.stable_id)
        detail::add_issue(report, index, ValidationIssue::duplicate_stable_id);
      if (catalog[previous].package_id == entry.package_id)
        detail::add_issue(report, index, ValidationIssue::duplicate_package_id);
    }
  }

  report.ok = report.entry_count != 0u && report.issues == 0u;
  return report;
}

[[nodiscard]] constexpr ValidationReport validate_catalog() noexcept {
  return validate(entries());
}

[[nodiscard]] ContractFailure run_contract() noexcept;
} // namespace epochengine::extension_catalog
