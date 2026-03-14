#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragPlanePos;

layout(location = 0) out vec4 outColor;

float grid_line(vec2 coord) {
    vec2 derivative = max(fwidth(coord), vec2(1e-4));
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    return 1.0 - min(min(grid.x, grid.y), 1.0);
}

float axis_line(float coord) {
    float derivative = max(fwidth(coord), 1e-4);
    return 1.0 - min(abs(coord) / derivative, 1.0);
}

void main() {
    vec3 baseColor = vec3(0.10, 0.13, 0.18);
    vec3 minorColor = vec3(0.15, 0.18, 0.24);
    vec3 majorColor = vec3(0.26, 0.29, 0.36);
    vec3 xAxisColor = vec3(0.95, 0.30, 0.28);
    vec3 zAxisColor = vec3(0.33, 0.58, 0.98);

    float minorGrid = grid_line(fragPlanePos);
    float majorGrid = grid_line(fragPlanePos / 4.0);
    float xAxis = axis_line(fragPlanePos.y);
    float zAxis = axis_line(fragPlanePos.x);

    vec3 color = baseColor;
    color = mix(color, minorColor, minorGrid * 0.45);
    color = mix(color, majorColor, majorGrid * 0.75);
    color = mix(color, xAxisColor, xAxis);
    color = mix(color, zAxisColor, zAxis);

    vec3 lightDir = normalize(vec3(0.4, 0.8, 0.6));
    float diff = max(dot(normalize(fragNormal), lightDir), 0.55);
    outColor = vec4(color * diff, 1.0);
}
