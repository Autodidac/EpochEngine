// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <array>

module extension.catalog;

import package.registry;

namespace epochengine::extension_catalog {
namespace {
[[nodiscard]] constexpr ContractFailure evaluate_contract() noexcept {
  const ValidationReport first = validate_catalog();
  const ValidationReport second = validate_catalog();
  if (!first.ok || first.entry_count != kEntries.size() ||
      first.native_entry_count != 1u || first.restart_entry_count != 1u ||
      first.registry_mapped_count != 3u) {
    return ContractFailure::canonical_catalog_rejected;
  }
  if (first.deterministic_digest == 0u ||
      first.deterministic_digest != second.deterministic_digest) {
    return ContractFailure::catalog_digest_unstable;
  }

  const Entry *arcade = find(kArcadeStableId);
  const Entry *portal = find("demo_portal_rtt");
  const Entry *localAi =
      find(package_registry::kLocalAiLlamaCppRuntimePackageId);
  if (arcade == nullptr || portal == nullptr || localAi == nullptr ||
      !has_capability(*arcade, Capability::render_to_texture) ||
      !has_capability(*portal, Capability::portal_views) ||
      !supports_platform(*localAi, Platform::headless)) {
    return ContractFailure::lookup_failed;
  }
  if (package_registry::find(arcade->package_id) == nullptr ||
      package_registry::find(package_registry::kEngineForestFactoryPackageId) ==
          nullptr ||
      package_registry::find(localAi->package_id) == nullptr) {
    return ContractFailure::package_registry_mapping_failed;
  }

  std::array<Entry, 2> duplicateStable{kEntries[0], kEntries[1]};
  duplicateStable[1].stable_id = duplicateStable[0].stable_id;
  const ValidationReport duplicateStableReport = validate(duplicateStable);
  if (duplicateStableReport.ok ||
      (duplicateStableReport.issues &
       issue_bit(ValidationIssue::duplicate_stable_id)) == 0u) {
    return ContractFailure::duplicate_stable_id_not_rejected;
  }

  std::array<Entry, 2> duplicatePackage{kEntries[0], kEntries[1]};
  duplicatePackage[1].package_id = duplicatePackage[0].package_id;
  duplicatePackage[1].existing_package_registry_entry_required = false;
  const ValidationReport duplicatePackageReport = validate(duplicatePackage);
  if (duplicatePackageReport.ok ||
      (duplicatePackageReport.issues &
       issue_bit(ValidationIssue::duplicate_package_id)) == 0u) {
    return ContractFailure::duplicate_package_id_not_rejected;
  }

  Entry unsafeNative = *localAi;
  unsafeNative.integrity = IntegrityRequirement::manifest_sha256;
  unsafeNative.editor_restart_required = false;
  const ValidationIssues unsafeNativeIssues = validate_entry(unsafeNative);
  if ((unsafeNativeIssues &
       issue_bit(ValidationIssue::unsafe_integrity_policy)) == 0u ||
      (unsafeNativeIssues &
       issue_bit(ValidationIssue::native_activation_without_restart)) == 0u) {
    return ContractFailure::unsafe_native_entry_not_rejected;
  }

  Entry missingIntegrity = *portal;
  missingIntegrity.integrity = IntegrityRequirement::core_source_tree;
  if ((validate_entry(missingIntegrity) &
       issue_bit(ValidationIssue::unsafe_integrity_policy)) == 0u) {
    return ContractFailure::missing_integrity_not_rejected;
  }

  for (const Entry &entry : entries()) {
    if (entry.availability != Availability::descriptor_only)
      return ContractFailure::installation_state_exposed;
  }
  return ContractFailure::none;
}

static_assert(evaluate_contract() == ContractFailure::none);
} // namespace

ContractFailure run_contract() noexcept { return evaluate_contract(); }
} // namespace epochengine::extension_catalog
