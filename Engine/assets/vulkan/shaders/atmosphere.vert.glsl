#version 450

layout(location = 0) out vec2 skyCoordinate;

void main() {
    vec2 screen = vec2(gl_VertexIndex == 1 ? 3.0 : -1.0,
                       gl_VertexIndex == 2 ? 3.0 : -1.0);
    gl_Position = vec4(screen, 0.0, 1.0);
    // A positive-height Vulkan viewport maps negative clip Y to its top.
    skyCoordinate = vec2(screen.x, -screen.y);
}
