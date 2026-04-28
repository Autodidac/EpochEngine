#pragma once

#if defined(__has_include)
#  if __has_include(<version>)
#    include <version>
#  endif
#  if __has_include(<execution>)
#    include <execution>
#  endif
#  if __has_include(<expected>)
#    include <expected>
#  endif
#  if __has_include(<stacktrace>)
#    include <stacktrace>
#  endif
#endif

#if defined(__cpp_contracts) && (__cpp_contracts > 0)
#  define EPOCH_HAS_CONTRACTS 1
#else
#  define EPOCH_HAS_CONTRACTS 0
#endif

#if (defined(__cpp_static_reflection) && (__cpp_static_reflection > 0)) || \
    (defined(__cpp_reflection) && (__cpp_reflection > 0))
#  define EPOCH_HAS_STATIC_REFLECTION 1
#else
#  define EPOCH_HAS_STATIC_REFLECTION 0
#endif

#if defined(__cpp_lib_execution) && (__cpp_lib_execution >= 201603L)
#  define EPOCH_HAS_STD_EXECUTION 1
#else
#  define EPOCH_HAS_STD_EXECUTION 0
#endif

#if defined(__cpp_lib_expected) && (__cpp_lib_expected >= 202202L)
#  define EPOCH_HAS_EXPECTED 1
#else
#  define EPOCH_HAS_EXPECTED 0
#endif

#if defined(__cpp_lib_stacktrace) && (__cpp_lib_stacktrace >= 202011L)
#  define EPOCH_HAS_STACKTRACE 1
#else
#  define EPOCH_HAS_STACKTRACE 0
#endif

namespace epoch::core
{
    inline constexpr bool has_contracts = EPOCH_HAS_CONTRACTS != 0;
    inline constexpr bool has_static_reflection = EPOCH_HAS_STATIC_REFLECTION != 0;
    inline constexpr bool has_std_execution = EPOCH_HAS_STD_EXECUTION != 0;
    inline constexpr bool has_expected = EPOCH_HAS_EXPECTED != 0;
    inline constexpr bool has_stacktrace = EPOCH_HAS_STACKTRACE != 0;
}
