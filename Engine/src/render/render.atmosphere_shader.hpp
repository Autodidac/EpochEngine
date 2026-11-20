// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

#include <string_view>

namespace epochengine::renderatmosphereshader
{
    // The observer and atmospheric thickness are expressed in planet radii.
    // Return straight RGB and coverage so the sky can blend over the target.
    inline constexpr std::string_view fragment_shared = R"glsl(
uniform vec3 uSkyEye,uSkyForward,uSkyRight,uSkyUp,uSkySun;
uniform float uSkyThickness;
uniform bool uSkySunVisible;

vec4 skyAtmosphere(vec2 screen) {
    vec3 ray=normalize(uSkyForward+screen.x*uSkyRight+screen.y*uSkyUp);
    float sunDistance=length(ray-uSkySun);
    float sunEdgeWidth=clamp(0.5*fwidth(sunDistance),0.00005,0.002);
    // Filter the Earth-like solar disk independently of the atmosphere
    // so it remains visible beyond the limb from orbit.
    float sunDisk=uSkySunVisible
        ? 1.0-smoothstep(0.00465-sunEdgeWidth,0.00465+sunEdgeWidth,sunDistance) : 0.0;
    vec3 sunColor=vec3(1.0,0.93,0.74);
    float outer=1.0+uSkyThickness;
    float b=dot(uSkyEye,ray),c=dot(uSkyEye,uSkyEye)-outer*outer;
    float discriminant=b*b-c;
    float halfPath=sqrt(max(0.0,discriminant));
    float first=max(0.0,-b-halfPath),last=-b+halfPath;
    if(uSkyThickness<=0.0||discriminant<=0.0||last<=first)
        return vec4(sunColor,sunDisk);
    float ground=b*b-dot(uSkyEye,uSkyEye)+1.0;
    bool groundOccludesSun=ground>0.0&&-b-sqrt(ground)>0.0;
    if(groundOccludesSun)last=min(last,-b-sqrt(ground));
    if(last<=first)return vec4(0.0);
    vec3 samplePosition=uSkyEye+ray*mix(first,last,0.28);
    float height=max(0.0,length(samplePosition)-1.0);
    float column=(last-first)/uSkyThickness*exp(-2.5*height/uSkyThickness);
    vec3 localUp=normalize(samplePosition);
    float solarElevation=dot(localUp,uSkySun);
    float daylight=smoothstep(-0.12,0.20,solarElevation);
    float twilight=smoothstep(-0.20,-0.04,solarElevation)
        *(1.0-smoothstep(0.05,0.26,solarElevation));
    float horizon=exp(-8.0*abs(dot(ray,localUp)));
    float sunFacing=clamp(dot(ray,uSkySun),0.0,1.0);
    float aureole=pow(sunFacing,14.0);
    vec3 blue=mix(vec3(0.12,0.38,0.76),vec3(0.55,0.73,0.89),1.0-exp(-column*0.40));
    vec3 dusk=mix(vec3(0.18,0.25,0.39),vec3(0.58,0.25,0.30),horizon);
    dusk=mix(dusk,vec3(1.0,0.67,0.34),aureole*(0.35+0.65*horizon));
    float dayWeight=daylight*(1.0-twilight);
    float lightWeight=dayWeight+twilight;
    vec3 skyColor=(blue*dayWeight+dusk*twilight)/max(lightWeight,1e-4);
    float observerHeight=max(0.0,length(uSkyEye)-1.0);
    float nearSurface=1.0-smoothstep(0.0,uSkyThickness,observerHeight);
    float extinction=2.6+3.4*nearSurface*daylight;
    float opacity=(1.0-exp(-extinction*column))*lightWeight;
    if(groundOccludesSun)sunDisk=0.0;
    float skyCoverage=opacity*(1.0-sunDisk);
    float totalCoverage=skyCoverage+sunDisk;
    return vec4((skyColor*skyCoverage+sunColor*sunDisk)/max(totalCoverage,1e-4),totalCoverage);
}
)glsl";
}
