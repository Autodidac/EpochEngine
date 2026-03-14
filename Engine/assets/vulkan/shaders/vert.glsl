#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragPlanePos;

void main() {
    vec4 worldPos = ubo.model * vec4(inPos, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;
    fragUV = inUV;
    fragNormal = inNormal;
    fragPlanePos = worldPos.xz;
}
