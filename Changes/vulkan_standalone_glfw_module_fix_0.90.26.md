# EpochEngine v0.90.26

The v0.90.25 focus-owned input change accidentally referenced GLFW APIs from `vulkan.context:window` in every build configuration. Normal editor builds do not expose GLFW declarations to that module partition, producing MSVC C2065/C3861 errors for `GLFW_FOCUSED`, `GLFW_TRUE`, `GLFW_KEY_*`, `glfwGetWindowAttrib`, and `glfwGetKey`.

The fix keeps the GLFW-backed mouse callback and standalone camera polling strictly inside `EPOCH_VULKAN_STANDALONE`, and imports `<GLFW/glfw3.h>` only in that partition's global module fragment for the standalone configuration. Integrated editor input continues through Epoch's focus-owned context/input system.
