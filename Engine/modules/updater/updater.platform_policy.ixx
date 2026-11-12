// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Engine update choices belong here, not in the reusable native platform API.
// Platform's published compatibility exports remain unchanged for old consumers.
module;
#include <string_view>
export module updater.platform_policy;
import platform.native;

export namespace epochengine::updater::platform_policy
{
    enum class SourceSnapshotArchive { Zip, TarGz };

    [[nodiscard]] constexpr SourceSnapshotArchive source_snapshot_archive_for(
        platform::RuntimePlatform target) noexcept
    {
        return target == platform::RuntimePlatform::Linux
            || target == platform::RuntimePlatform::MacOS
            ? SourceSnapshotArchive::TarGz : SourceSnapshotArchive::Zip;
    }

    [[nodiscard]] constexpr std::string_view source_snapshot_archive_extension_for(
        SourceSnapshotArchive archive) noexcept
    {
        return archive == SourceSnapshotArchive::TarGz ? ".tar.gz" : ".zip";
    }

    [[nodiscard]] constexpr std::string_view source_snapshot_archive_label_for(
        SourceSnapshotArchive archive) noexcept
    {
        return archive == SourceSnapshotArchive::TarGz
            ? "GitHub source snapshot tarball from main"
            : "GitHub source snapshot zip archive from main";
    }

    [[nodiscard]] constexpr std::string_view updater_shell_backend_for(
        platform::RuntimePlatform target) noexcept
    {
        return target == platform::RuntimePlatform::Linux ? "opengl" : "software";
    }

    [[nodiscard]] constexpr std::string_view updater_shell_backend_name() noexcept
    {
        return updater_shell_backend_for(platform::current_platform());
    }

    [[nodiscard]] constexpr std::string_view source_snapshot_archive_extension() noexcept
    {
        return source_snapshot_archive_extension_for(
            source_snapshot_archive_for(platform::current_platform()));
    }

    [[nodiscard]] constexpr std::string_view source_snapshot_archive_label() noexcept
    {
        return source_snapshot_archive_label_for(
            source_snapshot_archive_for(platform::current_platform()));
    }
}
