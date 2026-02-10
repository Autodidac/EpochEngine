//#include "main.h"
//
//#include <_epoch.stl_types.hpp>
//#include <print>
//
//import core.assert;
//import core.env;
//import core.error;
//import core.format;
//import core.id;
//import core.math;
//import core.path;
//import core.string;
//import core.time;
//
//import runtime;
//
//int main()
//{
//    using epoch::core::asserts::that;
//    using namespace std::literals;
//
//    // core.math
//    that(epoch::core::math::clamp(5, 0, 3) == 3);
//    that(epoch::core::math::lerp(0.0, 10.0, 0.5) == 5.0);
//    std::println("[OK] core.math");
//
//    // core.string
//    // trim(...) returns epoch::string_view (or epoch::string). Convert before comparing.
//    that(epoch::to_std(epoch::core::string::trim("  hi  ")) == "hi"sv);
//    std::println("[OK] core.string");
//
//    // core.id (basic compile-time sanity)
//    struct TagA {};
//    epoch::core::id::strong_id<TagA> a{ 42 };
//    that(a.value == 42);
//    std::println("[OK] core.id");
//
//    // core.env
//    (void)epoch::core::env::set("DEMO_TEST_ENV", "123");
//    auto v = epoch::core::env::get("DEMO_TEST_ENV");
//    that(v.has_value() && epoch::to_std(*v) == "123"sv);
//    (void)epoch::core::env::unset("DEMO_TEST_ENV");
//    std::println("[OK] core.env");
//
//    // core.path (best-effort existence)
//    auto exe = epoch::core::path::executable_path();
//    that(!exe.empty());
//    std::println("[OK] core.path");
//
//    // core.error (source_location present)
//    auto e = epoch::core::error::failed("x");
//    that((bool)e);
//    std::println("[OK] core.error");
//
//    return runtime::run();
//}
