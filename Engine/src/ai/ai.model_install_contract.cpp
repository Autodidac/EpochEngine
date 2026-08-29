/*
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
module;

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

module ai.model_install;

import core.sha256;

namespace epochengine::ai::model_install
{
    namespace detail
    {
        [[nodiscard]] PublishResult publish_verified_snapshot_for_plan(
            const PublishRequest& request);
    }

    namespace
    {
        [[nodiscard]] ArtifactSpec fixture_artifact(
            const std::string_view file,
            const std::string_view bytes)
        {
            constexpr std::string_view source =
                "https://huggingface.co/epoch/fixture";
            constexpr std::string_view revision =
                "0123456789abcdef0123456789abcdef01234567";
            return {
                .file = std::string{file},
                .source_url = std::string{source} + "/resolve/"
                    + std::string{revision} + "/" + std::string{file},
                .bytes = static_cast<std::uint64_t>(bytes.size()),
                .sha256 = core::sha256::hex(core::sha256::hash(bytes))};
        }

        [[nodiscard]] InstallPlan fixture_plan()
        {
            return {
                .package_id = "fixture_model",
                .display_name = "Fixture Model",
                .revision = "0123456789abcdef0123456789abcdef01234567",
                .official_source = "https://example.invalid/fixture-model",
                .artifact_source = "https://huggingface.co/epoch/fixture",
                .artifacts = {
                    fixture_artifact("weights.bin", "exact fixture weights"),
                    fixture_artifact("config.json", "{}\n")}};
        }

        [[nodiscard]] bool write_fixture(
            const std::filesystem::path& root,
            const std::string_view weights,
            const std::string_view config)
        {
            std::error_code error{};
            std::filesystem::create_directories(root, error);
            if (error)
                return false;
            std::ofstream first(root / "weights.bin", std::ios::binary);
            std::ofstream second(root / "config.json", std::ios::binary);
            first.write(weights.data(), static_cast<std::streamsize>(weights.size()));
            second.write(config.data(), static_cast<std::streamsize>(config.size()));
            return static_cast<bool>(first) && static_cast<bool>(second);
        }
    }

    bool run_contract()
    {
        const auto qwen = plan_for("os_model_qwen_3_8_27b");
        const auto nemotron = plan_for("os_model_nemotron_3_nano_4b_bf16");
        if (!qwen || !nemotron || !valid_plan(*qwen) || !valid_plan(*nemotron)
            || qwen->artifacts.size() != 1u
            || nemotron->artifacts.size() != 10u
            || nemotron->revision
                != "dfaf35de3e30f1867dd8dbc38a7fc9fb52d3914f"
            || nemotron->artifacts.front().file != "model.safetensors"
            || nemotron->artifacts.front().bytes != 7'947'142'640ull
            || plan_for("unknown_model"))
            return false;

        const InstallPlan plan = fixture_plan();
        std::error_code error{};
        const std::filesystem::path base =
            std::filesystem::temp_directory_path(error)
            / ("epoch-ai-model-install-contract-"
                + std::to_string(std::chrono::steady_clock::now()
                    .time_since_epoch().count()));
        if (error)
            return false;
        struct Cleanup final
        {
            std::filesystem::path root{};
            ~Cleanup()
            {
                std::error_code ignored{};
                std::filesystem::remove_all(root, ignored);
            }
        } cleanup{base};
        const std::filesystem::path good_cache = base / "good" / "models";
        const std::filesystem::path good_stage = staging_root(
            good_cache, plan, "publish");
        if (good_stage.empty()
            || !write_fixture(good_stage, "exact fixture weights", "{}\n"))
            return false;
        const PublishResult published = detail::publish_verified_snapshot_for_plan({
            .plan = plan,
            .models_cache_root = good_cache,
            .staging_root = good_stage});
        if (!published.accepted() || !published.published
            || published.code != Code::published
            || !receipt_matches(plan,
                [&]
                {
                    std::ifstream input(published.receipt_path, std::ios::binary);
                    return std::string{
                        std::istreambuf_iterator<char>{input},
                        std::istreambuf_iterator<char>{}};
                }()))
            return false;

        std::filesystem::remove(published.receipt_path, error);
        if (error)
            return false;
        const PublishResult recovered = detail::publish_verified_snapshot_for_plan({
            .plan = plan,
            .models_cache_root = good_cache,
            .staging_root = {}});
        if (!recovered.accepted() || !recovered.recovered
            || recovered.code != Code::recovered_receipt)
            return false;

        std::string extra = deterministic_receipt(plan);
        extra.insert(extra.rfind("\n}"), ",\n  \"unexpected\": true");
        if (receipt_matches(plan, extra))
            return false;

        const std::filesystem::path size_cache = base / "bad-size" / "models";
        const std::filesystem::path size_stage = staging_root(
            size_cache, plan, "size");
        if (!write_fixture(size_stage, "short", "{}\n"))
            return false;
        const PublishResult bad_size = detail::publish_verified_snapshot_for_plan({
            .plan = plan,
            .models_cache_root = size_cache,
            .staging_root = size_stage});
        if (bad_size.code != Code::size_mismatch
            || std::filesystem::exists(version_root(size_cache, plan), error))
            return false;

        const std::filesystem::path hash_cache = base / "bad-hash" / "models";
        const std::filesystem::path hash_stage = staging_root(
            hash_cache, plan, "hash");
        if (!write_fixture(hash_stage, "fixture weights exact", "{}\n"))
            return false;
        const PublishResult bad_hash = detail::publish_verified_snapshot_for_plan({
            .plan = plan,
            .models_cache_root = hash_cache,
            .staging_root = hash_stage});
        return bad_hash.code == Code::sha256_mismatch
            && !std::filesystem::exists(version_root(hash_cache, plan), error)
            && !std::filesystem::exists(
                version_root(hash_cache, plan) / receipt_filename, error);
    }
}

#if defined(EPOCH_AI_MODEL_INSTALL_CONTRACT_MAIN)
int main()
{
    return epochengine::ai::model_install::run_contract() ? 0 : 1;
}
#endif
