// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec2 cloudCoordinate;
layout(location = 0) out vec4 outColor;
layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput opaqueDepth;
layout(set = 0, binding = 1, std140) uniform CloudParameters {
    vec4 forward;
    vec4 right;
    vec4 up;
    vec4 center;
    vec4 sun;
    vec4 sunColor;
    vec4 ambient;
    mat3 worldToBody;
    vec4 projectionNearFar;
    vec4 windEvolution;
    vec4 radiiBaseThickness;
    vec4 density;
    float viewOccluderRadius;
    uint orthographic;
    uint seed;
    uint encodeSrgb;
} cloud;

#define EPOCH_VULKAN_CLOUD
#define uForward cloud.forward.xyz
#define uRight cloud.right.xyz
#define uUp cloud.up.xyz
#define uCenter cloud.center.xyz
#define uSun cloud.sun.xyz
#define uSunColor cloud.sunColor.xyz
#define uAmbient cloud.ambient.xyz
#define uWorldToBody cloud.worldToBody
#define uProjection cloud.projectionNearFar.xy
#define uNearFar cloud.projectionNearFar.zw
#define uWind cloud.windEvolution.xy
#define uEvolution cloud.windEvolution.zw
#define uRadius cloud.radiiBaseThickness.x
#define uWorldRadius cloud.radiiBaseThickness.y
#define uBase cloud.radiiBaseThickness.z
#define uThickness cloud.radiiBaseThickness.w
#define uNoiseScale cloud.density.x
#define uCoverage cloud.density.y
#define uExtinction cloud.density.z
#define uLowerExtinction cloud.density.w
#define uViewOccluderRadius cloud.viewOccluderRadius
#define uOrthographic (cloud.orthographic != 0u)
#define uSeed cloud.seed
#include "cloud_trace.generated.glsl"

void main() {
    vec3 offset = uRight * (cloudCoordinate.x * uProjection.x)
        + uUp * (cloudCoordinate.y * uProjection.y);
    vec3 origin = uOrthographic ? offset / uWorldRadius - uCenter : -uCenter;
    vec3 ray = uOrthographic ? uForward : normalize(uForward + offset);
    // Derivatives are evaluated before the shared march can diverge.
    vec3 originDx = dFdx(origin), originDy = dFdy(origin);
    vec3 rayDx = dFdx(ray), rayDy = dFdy(ray);
    float depth = subpassLoad(opaqueDepth).r;
    vec4 value = traceCloud(origin, ray, originDx, originDy, rayDx, rayDy, depth);
    if (value.a <= 0.0) discard;
    outColor = cloud.encodeSrgb != 0u ? cloudEncodeSrgb(value) : value;
}
