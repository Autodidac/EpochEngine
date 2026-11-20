#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 drawPosition;
layout(location = 2) flat in float waterDetailTag;

layout(std140, binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 waterCenterScale;
    vec4 waterEye;
    vec4 waterSun;
    vec4 waterWaves[15];
    vec4 waterSlopes[3];
} ubo;

layout(location = 0) out vec4 outColor;

vec3 waterReflection(vec3 up, vec3 normal, vec3 eye, vec4 sun) {
    float facing = clamp(dot(eye, normal), 0.0, 1.0);
    float fresnel = 0.0204 + 0.9796 * pow(1.0 - facing, 5.0);
    float skyElevation = clamp(dot(reflect(-eye, normal), up), 0.0, 1.0);
    float sunElevation = dot(up, sun.xyz);
    float daylight = smoothstep(-0.09, 0.22, sunElevation) * sun.w;
    float twilight = (1.0 - smoothstep(0.02, 0.24, sunElevation))
                   * smoothstep(-0.10, 0.03, sunElevation) * sun.w;
    vec3 sky = mix(vec3(0.13, 0.19, 0.24), vec3(0.045, 0.105, 0.20), skyElevation);
    sky = mix(sky, vec3(0.18, 0.105, 0.055), twilight * (1.0 - skyElevation));
    vec3 halfSum = eye + sun.xyz;
    vec3 halfway = halfSum * inversesqrt(max(dot(halfSum, halfSum), 1e-6));
    float alignment = clamp(dot(normal, halfway), 0.0, 1.0);
    float specular = 0.045 * pow(alignment, 22.0) + 0.23 * pow(alignment, 112.0);
    float visibleSun = smoothstep(0.0, 0.06, sunElevation) * sun.w;
    float masking = smoothstep(0.0, 0.10, facing) * smoothstep(0.0, 0.10, dot(normal, sun.xyz));
    vec3 sunTint = mix(vec3(0.99, 0.61, 0.29), vec3(0.87, 0.94, 1.0),
                       smoothstep(0.04, 0.28, sunElevation));
    return sky * (0.25 + 0.75 * daylight) * fresnel + sunTint * specular * masking * visibleSun;
}

vec3 waterDetail(vec3 linearBase, vec3 dx, vec3 dy) {
    float waterScale = ubo.waterCenterScale.w;
    vec3 p = drawPosition * waterScale;
    vec3 toEye = (ubo.waterEye.xyz - drawPosition) * waterScale;
    float distanceToEye = length(toEye);
    float swellStrength = 1.0 - smoothstep(75.0, 210.0, distanceToEye);
    if (distanceToEye < 1e-4 || swellStrength <= 0.0) return linearBase;
    vec3 directionX = ubo.waterWaves[12].xyz;
    vec3 directionY = ubo.waterWaves[13].xyz;
    vec3 directionZ = ubo.waterWaves[14].xyz;
    vec3 longPhase = vec3(dot(directionX, p) + ubo.waterWaves[12].w,
                         dot(directionY, p) + ubo.waterWaves[13].w,
                         dot(directionZ, p) + ubo.waterWaves[14].w);
    vec3 up = normalize(p - ubo.waterCenterScale.xyz);
    vec3 eye = toEye / distanceToEye;
    vec3 warpRate = cos(longPhase);
    // Resolve the longer swells independently of the short crest spectrum.
    vec3 slope = vec3(0.0);
    for (int i = 0; i < 3; ++i) {
        vec3 wave = ubo.waterWaves[12 + i].xyz;
        float footprint = abs(dot(wave, dx)) + abs(dot(wave, dy));
        float resolved = 1.0 - smoothstep(0.65, 2.45, footprint);
        slope += normalize(wave) * (warpRate[i] * resolved * 0.048 * swellStrength);
    }
    float rippleStrength = 1.0 - smoothstep(40.0, 135.0, distanceToEye);
    if (rippleStrength > 0.0) {
        const vec3 warpX = vec3(0.27, 0.13, -0.21);
        const vec3 warpY = vec3(0.11, -0.23, -0.19);
        const vec3 warpZ = vec3(0.15, 0.09, 0.18);
        vec3 drift = sin(longPhase);
        vec3 warped = p + warpX * drift.x + warpY * drift.y + warpZ * drift.z;
        dx += warpX * (dot(directionX, dx) * warpRate.x)
            + warpY * (dot(directionY, dx) * warpRate.y) + warpZ * (dot(directionZ, dx) * warpRate.z);
        dy += warpX * (dot(directionX, dy) * warpRate.x)
            + warpY * (dot(directionY, dy) * warpRate.y) + warpZ * (dot(directionZ, dy) * warpRate.z);
        for (int i = 0; i < 12; ++i) {
            vec4 wave = ubo.waterWaves[i];
            float footprint = abs(dot(wave.xyz, dx)) + abs(dot(wave.xyz, dy));
            if (footprint >= 2.45) continue;
            float resolved = 1.0 - smoothstep(0.65, 2.45, footprint);
            vec3 gradient = wave.xyz + directionX * dot(wave.xyz, warpX) * warpRate.x
                + directionY * dot(wave.xyz, warpY) * warpRate.y + directionZ * dot(wave.xyz, warpZ) * warpRate.z;
            int band = i / 4;
            float crestEnvelope = 0.44 + 0.56 * smoothstep(-0.65, 0.7,
                cos(longPhase[band] + 0.7 * drift[(band + 1) % 3] + 1.6 * float(i)));
            float crest = ubo.waterSlopes[band][i % 4];
            slope += gradient / length(wave.xyz)
                   * (cos(dot(wave.xyz, warped) + wave.w) * crest * resolved * crestEnvelope * rippleStrength);
        }
    }
    if (dot(slope, slope) == 0.0) return linearBase;
    vec3 normal = normalize(up - (slope - up * dot(up, slope)));
    // The opaque base already includes the broad reflection. Calculate the
    // shared display-space adjustment and restore Vulkan's linear output.
    vec3 delta = waterReflection(up, normal, eye, ubo.waterSun)
               - waterReflection(up, up, eye, ubo.waterSun);
    vec3 base = pow(clamp(linearBase, 0.0, 1.0), vec3(1.0 / 2.2));
    vec3 adjusted = clamp(base + delta, 0.0, 1.0);
    return pow(adjusted, vec3(2.2));
}

void main() {
    vec3 color = fragColor;
    if (ubo.waterCenterScale.w > 0.0) {
        // Take the footprint before the per-primitive shoreline branch.
        vec3 dx = dFdx(drawPosition) * ubo.waterCenterScale.w;
        vec3 dy = dFdy(drawPosition) * ubo.waterCenterScale.w;
        if (waterDetailTag > 0.5) color = waterDetail(color, dx, dy);
    }
    outColor = vec4(color, 1.0);
}
