#version 450

layout(location = 0) in vec2 skyCoordinate;
layout(location = 0) out vec4 outColor;

layout(constant_id = 0) const bool srgbAttachment = true;

layout(push_constant, std430) uniform SkyConstants {
    vec4 eye;
    vec4 forward;
    vec4 right;
    vec4 up;
    vec4 sun;
} sky;

vec3 attachmentColor(vec3 color) {
    if (!srgbAttachment) return color;
    vec3 lower = color / 12.92;
    vec3 upper = pow((color + 0.055) / 1.055, vec3(2.4));
    return mix(upper, lower, lessThanEqual(color, vec3(0.04045)));
}

void main() {
    vec3 ray = normalize(sky.forward.xyz + skyCoordinate.x * sky.right.xyz
                                        + skyCoordinate.y * sky.up.xyz);
    vec3 observer = sky.eye.xyz;
    vec3 sunDirection = sky.sun.xyz;
    float thickness = sky.eye.w;
    float sunDistance = length(ray - sunDirection);
    float sunEdgeWidth = clamp(0.5 * fwidth(sunDistance), 0.00005, 0.002);
    // Keep the filtered solar disk visible beyond the atmospheric limb.
    float sunDisk = sky.sun.w > 0.5
        ? 1.0 - smoothstep(0.00465 - sunEdgeWidth, 0.00465 + sunEdgeWidth, sunDistance)
        : 0.0;
    vec3 sunColor = vec3(1.0, 0.93, 0.74);

    float outer = 1.0 + thickness;
    float b = dot(observer, ray);
    float c = dot(observer, observer) - outer * outer;
    float discriminant = b * b - c;
    float halfPath = sqrt(max(0.0, discriminant));
    float first = max(0.0, -b - halfPath);
    float last = -b + halfPath;
    if (thickness <= 0.0 || discriminant <= 0.0 || last <= first) {
        if (sunDisk <= 0.0) discard;
        outColor = vec4(attachmentColor(sunColor), sunDisk);
        return;
    }

    float ground = b * b - dot(observer, observer) + 1.0;
    bool groundOccludesSun = ground > 0.0 && -b - sqrt(ground) > 0.0;
    if (groundOccludesSun) last = min(last, -b - sqrt(ground));
    if (last <= first) discard;

    vec3 samplePosition = observer + ray * mix(first, last, 0.28);
    float height = max(0.0, length(samplePosition) - 1.0);
    float column = (last - first) / thickness * exp(-2.5 * height / thickness);
    vec3 localUp = normalize(samplePosition);
    float solarElevation = dot(localUp, sunDirection);
    float daylight = smoothstep(-0.12, 0.20, solarElevation);
    float twilight = smoothstep(-0.20, -0.04, solarElevation)
        * (1.0 - smoothstep(0.05, 0.26, solarElevation));
    float horizon = exp(-8.0 * abs(dot(ray, localUp)));
    float sunFacing = clamp(dot(ray, sunDirection), 0.0, 1.0);
    float aureole = pow(sunFacing, 14.0);
    vec3 blue = mix(vec3(0.12, 0.38, 0.76), vec3(0.55, 0.73, 0.89),
                    1.0 - exp(-column * 0.40));
    vec3 dusk = mix(vec3(0.18, 0.25, 0.39), vec3(0.58, 0.25, 0.30), horizon);
    dusk = mix(dusk, vec3(1.0, 0.67, 0.34), aureole * (0.35 + 0.65 * horizon));
    float dayWeight = daylight * (1.0 - twilight);
    float lightWeight = dayWeight + twilight;
    vec3 skyColor = (blue * dayWeight + dusk * twilight) / max(lightWeight, 1e-4);
    float observerHeight = max(0.0, length(observer) - 1.0);
    float nearSurface = 1.0 - smoothstep(0.0, thickness, observerHeight);
    float extinction = 2.6 + 3.4 * nearSurface * daylight;
    float opacity = (1.0 - exp(-extinction * column)) * lightWeight;
    if (groundOccludesSun) sunDisk = 0.0;
    float skyCoverage = opacity * (1.0 - sunDisk);
    float totalCoverage = skyCoverage + sunDisk;
    vec3 displayColor = (skyColor * skyCoverage + sunColor * sunDisk) / max(totalCoverage, 1e-4);
    outColor = vec4(attachmentColor(displayColor), totalCoverage);
}
