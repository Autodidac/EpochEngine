# Epoch C++23 Module Rules

This repo uses real mixed C++23 module units, not toy examples. These are the
rules that keep them building across MSVC, Clang, Windows SDK headers, and the
engine's platform glue.

## The hard rule

If a module unit needs classic `#include` headers before the named module
declaration, it must start with:

```cpp
module;
```

That opens the global module fragment. Put legacy headers, Windows/X11/OpenGL
SDK headers, and preprocessor-driven setup there.

Then declare the actual module:

```cpp
export module my.module;
```

After that, you are in the named module.

## Correct shape

```cpp
module;

#if defined(_WIN32)
#include "framework.hpp"
#endif
#include <string>

export module epoch.example;

import engine.platform;

export namespace epoch::example
{
    export constexpr bool some_platform_switch = true;
}
```

## What we learned the hard way

### 1. `module;` comes first

If you need a global module fragment at all, put `module;` at the top. Do not
start with random includes and try to retrofit a named module later.

### 2. Macros are not your public interface

Macros in the global module fragment are useful for controlling textual headers,
but they are not the right public API for importers.

If a compile-time switch matters to code outside the module, mirror it after
`export module ...` with exported `constexpr` values, enums, or functions.

That is the "double compile-time macro" lesson:
- one side controls old-school headers during preprocessing
- the other side exposes the same decision through the actual module interface

### 3. Do not rely on `import std;` in mixed units

In clean module units, `import std;` may be fine.

In mixed units that already needed global-fragment includes, it has been safer
in this repo to keep standard library usage textual with classic headers
instead of trying to import `std` afterward. Toolchain/header interactions have
been fragile there.

Practical rule:
- clean unit: `export module ...; import std;`
- mixed unit with `module;` and classic includes: prefer classic standard
  headers too

### 4. Platform headers stay in the global fragment

Anything that is macro-heavy, order-sensitive, or hostile to modules belongs
before the named module declaration:
- `windows.h`
- X11 / GLX headers
- OpenGL loader headers
- old C libraries that expect textual inclusion

### 5. Keep the named module clean

After `export module ...`, prefer:
- `import` for real engine modules
- exported `constexpr` / types / functions for feature visibility
- minimal dependency leakage

## Header-only to module traps

Converting old header-only code into a module is not just "wrap it in
`export module ...`". A lot of header-era behavior depends on textual inclusion
and quietly breaks when the code becomes a compiled module boundary.

### 1. Transitive includes stop saving you

Header-only code often "worked" because some other include happened to pull in:
- `<string>`
- `<vector>`
- `<algorithm>`
- platform headers

In a module, be explicit. Import or include what the unit actually needs.

### 2. Importers do not see your macros

Header-only code often expects the includer to set macros first.

That pattern does not translate cleanly to modules:
- importer macros do not behave like textual header configuration
- global-fragment macros are implementation detail, not exported API

If consumers need configuration, expose it as:
- exported `constexpr`
- enums
- traits
- explicit build flags wired into the module implementation

### 3. Include-order tricks die here

Old header-only utilities often relied on:
- "include platform header first"
- "include config before this file"
- "define this switch before include"

If that ordering still matters, keep it in the global module fragment with
`module;` and make the named module boundary explicit afterward.

### 4. Internal-linkage behavior changes

Header-only code may rely on `static`, anonymous namespaces, or inline globals
behaving one way per translation unit.

Once moved into a module, you are compiling an actual unit, not text-pasting
into every consumer. Re-check:
- global state lifetime
- `static` helper visibility
- inline variable ownership
- one-time initialization assumptions

### 5. Macro-based feature toggles should become module-facing symbols

If a header-only system used:

```cpp
#if EPOCH_FEATURE_X
```

do not stop halfway when converting it.

The implementation may still need the macro, but the imported interface should
publish the result in a stable C++ form:

```cpp
export constexpr bool feature_x_enabled = EPOCH_FEATURE_X != 0;
```

### 6. Header fallbacks and module interfaces are not the same thing

Some files in this repo still act as compatibility wrappers for older builds.
That is fine, but keep the roles clear:
- compatibility header: shim or fallback
- module interface: real ownership surface

Do not let a shim header become the accidental source of truth again.

### 7. `import std;` is not a magic cleanup button

If the old header-only code depended on textual standard headers, macros, or
hostile SDK interactions, `import std;` will not automatically rescue the
conversion. In mixed units, it can make the situation worse.

Prefer:
- classic standard headers in the global fragment for mixed/platform-heavy units
- `import std;` only in genuinely clean module units

## Output and logging policy

Epoch is a C++23 engine, so new code must not fall back to iostream-style
diagnostic output.

Use:
- `core.logger` for engine, editor, runtime, backend, AI, capture, updater, and
  project-generation code paths
- visible editor/evidence surfaces for operator-facing workflow status
- `core_log_write` / `core.log` for engine-branded smoke tools or lightweight
  validation binaries that cannot import the full logger facade

Avoid:
- `std::cout` / `std::cerr` in new C++ code
- C `printf` / `fprintf` / `puts` style output in new C++ code
- direct `std::print` / `std::println` in Epoch-branded runtime, editor, smoke,
  backend, AI, capture, updater, or project-generation code
- ad hoc console diagnostics where an engine logger category or evidence panel
  should own the message

The logger implementation itself may use C++23 print facilities or platform/file
sinks internally. That keeps console/file output centralized while preserving
logger ownership for the rest of the engine. Non-engine helper utilities may use
`<print>` directly only when they are deliberately outside Epoch runtime/tooling
ownership.

## Good Epoch pattern

Use:
- global module fragment for platform setup and toxic headers
- named module for the real interface
- exported constants for feature state
- imported Epoch modules for engine-owned dependencies

Avoid:
- treating macros as exported API
- mixing heavy SDK includes into the named module body
- assuming `import std;` will save a unit that already depends on textual setup
- assuming header-only behavior survives unchanged after crossing a module boundary
- hand-rolling new Win32 macro/include blocks in unrelated source files when
  Epoch already has an audited platform wrapper or backend-local owner for that
  SDK surface

## Snapshot summary

When in doubt:

1. Put `module;` first.
2. Put classic includes and macro setup in the global fragment.
3. Declare `export module ...`.
4. Expose compile-time decisions again through exported module-facing symbols.
5. Prefer classic standard headers over `import std;` in those mixed units.
6. When converting header-only code, remove transitive/include-order assumptions
   instead of preserving them by accident.
