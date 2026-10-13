# MSVC C++23 logging-display build fix (2026-10-11)

## Cause

The prior console/debugger display formatter in `Engine/src/core/core.log_compact.hpp`
compared `std::string_view` against string literals using unqualified `operator==`
inside the `epochengine` namespace. The project also defines heterogeneous
`epochengine::operator==` overloads for its own string types; those enter
C++23 overload resolution and make the comparisons ambiguous (MSVC C2593).

## Fix

- Replace 12 literal comparisons with `std::string_view::compare(...) == 0`
  to avoid introducing an overload-resolution candidate set.
- Extend `Tests/logging/compact_runtime_logging_test.cpp` to include the
  production `Engine/include/core.stl_types.hpp` first; this reproduces the
  actual overload environment rather than testing the formatter in isolation.
- Keep the **existing core.log logger** and its public API unchanged; its
  console and debugger sink formats remain concise while complete diagnostics
  remain in the existing file sink. No new logger or model subsystem was added.
- The later `LNK1104: cannot open file 'EpochEngine.lib'` in the supplied log
  follows from the failed `EpochEngine` compilation, not a separate dependency
  error in this transcript.

## Validation

- Confirmed the previous header fails under GCC 14 with the real engine string
  definitions, matching the MSVC C2593 cause.
- Updated logging regression test passes under GCC 14 and Clang 17 with
  C++23, `-Wall -Wextra -Werror -pedantic`.
- MSVC rebuild (Debug/Release) is still required on Windows; no Windows
  toolchain or GUI/runtime execution was available for this pass.
