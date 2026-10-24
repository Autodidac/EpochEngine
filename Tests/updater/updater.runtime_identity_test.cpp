#include "../../Engine/include/updater.runtime_identity.hpp"
#include <array>
#include <string>
#include <utility>

int main()
{
    using namespace epochengine::updater::runtime_identity;
    constexpr auto public_id = "epoch.public.github.v1";
    constexpr auto development_id = "epoch.development.site.v1";
    const auto marker = [](const char* edition, const char* version)
    {
        return std::string{"{\"schema\":\"epoch.distribution.v1\",\"edition\":\""}
            + edition + "\",\"version\":\"" + version + "\"}";
    };
    const auto public_marker = marker(public_id, "0.90.183");
    const auto development_marker = marker(development_id, "0.90.183");
    if (!matches(public_marker, "0.90.183", public_id)
        || !matches(development_marker, "0.90.183", development_id)
        || matches(public_marker, "0.90.183", development_id)
        || matches(development_marker, "0.90.183", public_id)
        || matches(public_marker, "0.90.184", public_id)
        || matches(marker(public_id, "garbage"), "garbage", public_id)) return 1;
    constexpr std::array malformed{
        "",
        R"json({"schema":"epoch.distribution.v1","version":"0.90.183"})json",
        R"json({"schema":"epoch.distribution.v2","edition":"epoch.public.github.v1","version":"0.90.183"})json",
        R"json({"schema":"epoch.distribution.v1","edition":null,"version":"0.90.183"})json",
        R"json({"schema":"epoch.distribution.v1","edition":"epoch.public.github.v1","version":183})json",
        R"json({"schema":"epoch.distribution.v1","edition":"epoch.public.github.v1","edition":"epoch.public.github.v1","version":"0.90.183"})json",
        R"json({"schema":"epoch.distribution.v1","edition":"epoch.public.github.v1","ed\u0069tion":"epoch.public.github.v1","version":"0.90.183"})json",
        R"json({"schema":"epoch.distribution.v1","metadata":{"edition":"epoch.public.github.v1"},"version":"0.90.183"})json",
        R"json({"schema":"epoch.distribution.v1","edition":"epoch.public.github.v1","version":"0.90.183","channel":"public"})json",
        R"json({"schema":"epoch.distribution.v1","edition":"epoch.public.github.v1","version":"0.90.183"} trailing)json",
        R"json({"schema":"epoch.distribution.v1","edition":"epoch.public.github.v1","version":"0.90.183")json"
    };
    for (const auto* payload : malformed)
        if (matches(payload, "0.90.183", public_id)) return 2;
    if (matches(std::string(4097u, ' '), "0.90.183", public_id)) return 3;
    if (markerless_allowed(true, "0.90.55")
        || markerless_allowed(true, "0.90.183")
        || !markerless_allowed(false, "0.90.55")
        || markerless_allowed(false, "0.90.183")
        || markerless_allowed(false, "")) return 4;
    return 0;
}
