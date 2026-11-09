# Third-party material and dependency provenance

The default build uses the official Microsoft vcpkg baseline **9e593bb18ea69cc5095e012465dcd675a822ed0d** (2026.07.29). This is an intentionally fixed baseline, not a statement that its dependency versions are the newest available. The helper verifies its managed checkout's commit. The July 2026 tooling avoids bootstrapping a pre-Visual-Studio-2026 tool; VS environment setup preserves an explicitly supplied VCPKG_ROOT. Linux explicitly enables the loader's xlib/xcb WSI features, plus wayland when selected. It never resets a checkout supplied through VCPKG_ROOT. Version constraints alone are not a bit-for-bit compiler/OS reproducibility guarantee.

- GLFW: zlib/libpng license. The pinned baseline has the 3.4 port.
- Vulkan headers/loader: upstream Apache-2.0/MIT license notices, as applicable. The pinned loader port is 1.4.350.1.
- shaderc and its SPIR-V/glslang dependencies: respective Apache-2.0/BSD/upstream licenses; the exact notices come from the resolved ports.
- MoltenVK on macOS: external runtime deployment; consult its Apache-2.0 and SDK redistribution requirements. It is not silently added as a nonexistent port in this baseline.
- CMake, Ninja, Git, Python and compilers are build prerequisites, not bundled application source.

No dependency source or binary is bundled in the source ZIP. Preserve the installed `vcpkg_installed/<triplet>/share/<port>/copyright` notices when distributing dependency binaries. The CMake installer copies available vcpkg copyright notices into its documentation tree when built through the manifest. System-dependency builds need their own packaging/license audit. Vulkan GPU drivers remain system components.

The untouched user-supplied original project ZIP is under `legacy/`. No new licensing assumption is imposed on that original. `src/core/voxel_terrain.cpp` derives from that input and retains the original user's licensing status; establish that provenance before a public redistribution. Replacement project files have a separate permissive license in LICENSE.

No No Man's Sky or Light No Fire assets, code, SDKs, trademarks used as product branding, or screenshots are included. The games are mentioned only to identify the intended exploration/scale direction.

## Primary implementation references

- https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html
- https://www.glfw.org/docs/latest/vulkan_guide.html
- https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html
- https://cmake.org/cmake/help/latest/generator/Ninja%20Multi-Config.html
- https://github.com/microsoft/vcpkg/tree/9e593bb18ea69cc5095e012465dcd675a822ed0d
- https://github.com/microsoft/vcpkg/blob/9e593bb18ea69cc5095e012465dcd675a822ed0d/ports/shaderc/usage

- https://learn.microsoft.com/en-us/vcpkg/users/triplets
- https://github.com/microsoft/vcpkg/releases/tag/2026.07.29
