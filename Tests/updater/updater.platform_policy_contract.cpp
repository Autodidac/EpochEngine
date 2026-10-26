// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <array>
#include <string_view>
import platform.native;
import platform.engine;
import updater.platform_policy;

int main()
{
    using epochengine::platform::RuntimePlatform;
    namespace owner = epochengine::updater::platform_policy;
    constexpr std::array targets{RuntimePlatform::Windows, RuntimePlatform::Linux,
        RuntimePlatform::MacOS, RuntimePlatform::Android, RuntimePlatform::IOS,
        RuntimePlatform::Web, RuntimePlatform::Unknown};
    for (const auto target : targets)
    {
        const bool tar = target == RuntimePlatform::Linux || target == RuntimePlatform::MacOS;
        const auto archive = owner::source_snapshot_archive_for(target);
        const auto compatibility = epochengine::platform::policy::runtime_policy_for(target);
        if (static_cast<int>(archive) != static_cast<int>(compatibility.source_snapshot_archive)) return 8;
        if (owner::updater_shell_backend_for(target) != compatibility.updater_shell_backend) return 9;
        if (owner::source_snapshot_archive_extension_for(archive) !=
            epochengine::platform::policy::source_snapshot_archive_extension_for(compatibility.source_snapshot_archive)) return 10;
        if (owner::source_snapshot_archive_label_for(archive) !=
            epochengine::platform::policy::source_snapshot_archive_label_for(compatibility.source_snapshot_archive)) return 11;
        if ((archive == owner::SourceSnapshotArchive::TarGz) != tar) return 1;
        if (owner::source_snapshot_archive_extension_for(archive) != (tar ? ".tar.gz" : ".zip")) return 2;
        if (owner::source_snapshot_archive_label_for(archive) != (tar
            ? "GitHub source snapshot tarball from main"
            : "GitHub source snapshot zip archive from main")) return 3;
        if (owner::updater_shell_backend_for(target) != (target == RuntimePlatform::Linux ? "opengl" : "software")) return 4;
    }
    const auto current = epochengine::platform::current_platform();
    if (owner::updater_shell_backend_name() != owner::updater_shell_backend_for(current)) return 5;
    const auto archive = owner::source_snapshot_archive_for(current);
    if (owner::source_snapshot_archive_extension() != owner::source_snapshot_archive_extension_for(archive)) return 6;
    if (owner::source_snapshot_archive_label() != owner::source_snapshot_archive_label_for(archive)) return 7;
    return 0;
}
