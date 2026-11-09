/*
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
module;

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include "../../include/core.utf8.hpp"

module ai.image_generation;

import ai.model_install;

namespace epochengine::ai::image_generation
{
    namespace
    {
        [[nodiscard]] bool valid_text(const std::string_view value,
            const std::size_t maximum, const bool required) noexcept
        {
            return value.size() <= maximum && core::utf8::valid(value)
                && value.find('\0') == std::string_view::npos
                && (!required || std::any_of(value.begin(), value.end(), [](const char ch)
                    { return ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n'; }));
        }

        [[nodiscard]] bool exact_path(const std::filesystem::path& path)
        {
            if (path.empty() || !path.is_absolute() || path == path.root_path()
                || path.lexically_normal() != path)
                return false;
            std::error_code error{};
            return std::filesystem::canonical(path, error) == path && !error;
        }

        [[nodiscard]] std::string utf8_path(const std::filesystem::path& path)
        {
            const auto bytes = path.generic_u8string();
            return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
        }

        [[nodiscard]] bool vacant(const std::filesystem::path& path)
        {
            std::error_code error{};
            const auto status = std::filesystem::symlink_status(path, error);
            return (!error || error == std::errc::no_such_file_or_directory)
                && status.type() == std::filesystem::file_type::not_found;
        }

        [[nodiscard]] bool within(const std::filesystem::path& child,
            const std::filesystem::path& parent)
        {
            auto at = child.begin();
            for (const auto& component : parent)
            {
                if (at == child.end() || *at++ != component)
                    return false;
            }
            return true;
        }
    }

    namespace detail
    {
        // Production supplies only the three immutable compiled component plans.
        [[nodiscard]] Invocation prepare_for_plans(const Request& request,
            const Configuration& configuration,
            const std::filesystem::path& owned_work_directory,
            const std::array<model_install::InstallPlan, 3u>& plans)
        {
            const auto fail = [](std::string diagnostic)
            { return Invocation{.diagnostic = std::move(diagnostic)}; };
            if (const auto diagnostic = request_diagnostic(request); !diagnostic.empty())
                return fail(diagnostic);
            if (!exact_path(configuration.executable)
                || !configuration.expected_executable.valid())
                return fail("Select the pinned native image executable with its exact expected size and SHA-256.");
            const model_install::ArtifactSpec executable{
                .bytes = configuration.expected_executable.size_bytes,
                .sha256 = configuration.expected_executable.sha256};
            if (!model_install::verify_file(configuration.executable, executable).verified())
                return fail("The native image executable does not match the selected verified identity.");
            std::error_code error{};
            if (!exact_path(owned_work_directory)
                || !std::filesystem::is_directory(owned_work_directory, error) || error
                || within(owned_work_directory, configuration.models_cache_root)
                || within(owned_work_directory, configuration.executable.parent_path()))
                return fail("Use an existing, normalized, nonredirected owned job directory outside model and executable inputs.");
            // The pinned CLI supports printf-style output sequences. A percent
            // in the absolute output path would change that path's interpretation.
            const auto work = utf8_path(owned_work_directory);
            if (!valid_text(work, 8192u, true) || work.find('%') != std::string::npos)
                return fail("The owned job path is not a valid literal native output path.");
            const auto output = owned_work_directory / "generated.png";
            const auto log = owned_work_directory / "generation.log";
            if (!vacant(output) || !vacant(log))
                return fail("The job output or log already exists; use a fresh owned job directory.");

            std::array<std::filesystem::path, 3u> models{};
            for (std::size_t index{}; index < plans.size(); ++index)
            {
                if (plans[index].artifacts.size() != 1u)
                    return fail("Each image component must have one exact immutable artifact.");
                const auto verified = model_install::verify_snapshot(
                    configuration.models_cache_root, plans[index]);
                if (!verified.accepted())
                    return fail(plans[index].display_name + ": " + verified.diagnostic);
                models[index] = verified.installed_root / plans[index].artifacts.front().file;
            }

            Invocation result{};
            result.output = output;
            result.launch.executable = configuration.executable;
            result.launch.expected_executable = configuration.expected_executable;
            result.launch.working_directory = owned_work_directory;
            result.launch.merged_output_path = log;
            result.launch.arguments = {
                "--mode", "img_gen", "--diffusion-model", utf8_path(models[0u]),
                "--llm", utf8_path(models[1u]), "--vae", utf8_path(models[2u]),
                "--prompt", request.prompt, "--negative-prompt", request.negative_prompt,
                "--seed", std::to_string(request.seed), "--width", std::to_string(request.width),
                "--height", std::to_string(request.height), "--steps", std::to_string(request.steps),
                "--batch-count", "1", "--cfg-scale", "1.0", "--sampling-method", "euler",
                "--offload-to-cpu", "--output", utf8_path(output)};
            result.launch.correlation_key = "image-generation:" + work;
            result.launch.exclusive_group = "native-image-generation";
            result.launch.display_name = "Local FLUX.2 Klein 4B image generation";
            result.launch.window_mode = platform::child_process::WindowMode::hidden;
            result.launch.disconnect_standard_input = true;
            result.diagnostic = "Pinned Klein diffusion, Qwen image encoder and complete small VAE snapshots verified; native invocation prepared.";
            result.ready = true;
            return result;
        }
    }

    std::string request_diagnostic(const Request& request)
    {
        if (!valid_text(request.prompt, 8192u, true)
            || !valid_text(request.negative_prompt, 4096u, false))
            return "Use a nonempty UTF-8 prompt up to 8192 bytes and a negative prompt up to 4096 bytes; embedded NUL is refused.";
        if (request.width < 256u || request.height < 256u
            || request.width > 2048u || request.height > 2048u
            || request.width % 16u != 0u || request.height % 16u != 0u
            || static_cast<std::uint64_t>(request.width) * request.height > 1024u * 1024u
            || request.steps == 0u || request.steps > 8u)
            return "Use dimensions 256–2048 in multiples of 16, at most one megapixel, and 1–8 steps.";
        return {};
    }

    Invocation prepare(const Request& request, const Configuration& configuration,
        const std::filesystem::path& owned_work_directory)
    {
        std::array<model_install::InstallPlan, 3u> plans{};
        const std::array identifiers{diffusion_package, text_encoder_package, decoder_package};
        for (std::size_t index{}; index < plans.size(); ++index)
        {
            const auto plan = model_install::plan_for(identifiers[index]);
            if (!plan)
                return {.diagnostic = "The native image component has no compiled install plan."};
            plans[index] = *plan;
        }
        return detail::prepare_for_plans(request, configuration, owned_work_directory, plans);
    }
}
