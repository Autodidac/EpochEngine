// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

#include <string_view>

namespace epochengine::rendercloudshader
{
    inline constexpr std::string_view desktop_header = "#version 330 core\n";
    inline constexpr std::string_view gles_header = "#version 300 es\n"
        "precision highp float;\n"
        "precision highp int;\n"
        "precision highp sampler2D;\n";

    // A fullscreen triangle has no edge through the viewport and needs no
    // persistent geometry upload. All resources belong to the native context.
    inline constexpr std::string_view vertex_source=R"glsl(out vec2 vUv;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);vUv=p;gl_Position=vec4(p*2.0-1.0,0.0,1.0);}
)glsl";
    // Adjacent raw literals keep each part inside MSVC's string literal limit.
    inline constexpr std::string_view cloud_trace_source=R"glsl(#ifndef EPOCH_VULKAN_CLOUD
in vec2 vUv;
uniform sampler2D uDepth;
#ifdef GL_ES
// Pixel centers select the same exact opaque texel in the trace and resolve passes.
// Crop draws write directly to a window of the full trace texture.
uniform ivec2 uTraceSize;
uniform float uCloudAtmosphereThickness;
ivec2 cloudTraceDepthPixel(ivec2 tracePixel,ivec2 depthSize,ivec2 traceSize){
 return ((2*tracePixel+ivec2(1))*depthSize)/(2*traceSize);
}
ivec2 cloudPrecedingTracePixel(ivec2 pixel,ivec2 depthSize,ivec2 traceSize){
 return ((2*pixel+ivec2(2))*traceSize-depthSize-ivec2(1))/(2*depthSize);
}
#endif
uniform vec3 uForward,uRight,uUp,uCenter,uSun,uSunColor,uAmbient;
uniform mat3 uWorldToBody;
uniform vec2 uProjection,uNearFar,uWind,uEvolution;
uniform bool uOrthographic;
uniform float uRadius,uWorldRadius,uBase,uThickness,uNoiseScale,uCoverage,uExtinction,uLowerExtinction;
uniform float uViewOccluderRadius;
uniform uint uSeed;
#endif
// The Android 12 emulator's GLES compiler misreads unsigned literals above
// INT_MAX. Assemble the exact CPU lattice multipliers from 16-bit halves.
uint hashWord(uint v){
 v^=v>>16u;v*=0x7feb352du;v^=v>>15u;v*=((0x846cu<<16u)|0xa68bu);return v^(v>>16u);
}
float lattice(ivec3 p,uint seed){
 uint v=uint(p.x)*((0x8da6u<<16u)|0xb343u)
       ^uint(p.y)*((0xd816u<<16u)|0x3841u)
       ^uint(p.z)*((0xcb1au<<16u)|0xb31fu)^seed;
 return float(hashWord(v)>>8u)/16777215.0;
}
float noiseValue(vec3 p,uint seed){
 ivec3 c=ivec3(floor(p));vec3 f=fract(p);f=f*f*(3.0-2.0*f);
 return mix(mix(mix(lattice(c,seed),lattice(c+ivec3(1,0,0),seed),f.x),mix(lattice(c+ivec3(0,1,0),seed),lattice(c+ivec3(1,1,0),seed),f.x),f.y),
 mix(mix(lattice(c+ivec3(0,0,1),seed),lattice(c+ivec3(1,0,1),seed),f.x),mix(lattice(c+ivec3(0,1,1),seed),lattice(c+ivec3(1,1,1),seed),f.x),f.y),f.z);
}
float filteredNoise(vec3 p,uint seed,float footprint){
 float retained=1.0-smoothstep(0.5,1.0,footprint);
 return retained==0.0?0.5:mix(0.5,noiseValue(p,seed),retained);
}
float cloudDensity(vec3 local,float footprint,float rayFootprint){
 float radialDistance=length(local);
 float height=(radialDistance-1.0-uBase)/uThickness;if(height<=0.0||height>=1.0||uCoverage<=0.0)return 0.0;
 bool upper=height>=0.58;
 if(!upper&&height>=0.52)return 0.0;
 vec3 q=uWorldToBody*local;q=vec3(uWind.x*q.x+uWind.y*q.z,q.y,-uWind.y*q.x+uWind.x*q.z)*uNoiseScale;
 footprint*=uNoiseScale;
 float phase=upper?0.6*uEvolution.x+0.8*uEvolution.y:uEvolution.x;
 float crossPhase=upper?0.6*uEvolution.y-0.8*uEvolution.x:uEvolution.y;
 // The two decks advect with the same planet-scale wind. Local flow shears
 // them by different small physical distances without accelerating that wind.
 float shear=(upper?0.55:0.15)*phase;
 q=vec3(q.x+shear*q.z/(radialDistance*uNoiseScale),q.y,q.z-shear*q.x/(radialDistance*uNoiseScale));
 // Use the same footprint for the top, base and in-cloud views.
 vec3 surface=q/radialDistance;
 uint deckSeed=uSeed^(upper?0x57454155u:0x57454154u);
 vec3 synoptic=vec3(surface.x/91.0+11.0,surface.y/68.0,surface.z/91.0+7.0);
 float front=filteredNoise(synoptic,deckSeed,footprint/68.0);
 float bend=sin((surface.x-0.60*surface.z)/49.0+3.0*front);
 float across=upper?14.0:22.0,along=upper?53.0:36.0;
 vec3 cluster=vec3(surface.x/along+1.4*front,surface.y/across+0.84*bend
                   +0.12*crossPhase,surface.z/along-1.2*front);
 vec3 provinces=surface/113.0+vec3(3.2,-7.8,4.6);
 float regime=filteredNoise(provinces,uSeed^0x5245474du,footprint/113.0);
 float broken=1.0-smoothstep(0.16,0.35,regime);
 float wet=smoothstep(0.56,0.77,regime);
 if(wet>0.0){
  float displacement=12.0*wet;
  cluster.x+=displacement*sin((surface.z+0.45*surface.y)/61.0+2.0*front)/along;
  cluster.y+=1.2*displacement*sin((surface.x-0.7*surface.z)/53.0-2.3*front)/across;
  cluster.z+=displacement*sin((surface.y+0.38*surface.x)/68.0+2.8*front)/along;
 }
 float mass=filteredNoise(cluster,deckSeed^0x434c5553u,footprint/across);
 if(broken>0.0){
  vec3 cells=vec3(surface.x/17.0+0.75*bend,surface.y/17.0-0.32*bend,surface.z/17.0+0.7*front);
  mass=mix(mass,filteredNoise(cells,deckSeed^0x43554d55u,footprint/17.0),broken);
 }
 float dry=1.0-smoothstep(0.14,0.37,regime);
 float lobeAlong=upper?13.0:4.5,lobeAcross=upper?10.0:3.2;
 vec3 outline=vec3((surface.x-0.40*surface.z)/lobeAlong,surface.y/lobeAcross+0.8*bend,
                   (surface.z+0.40*surface.x)/lobeAlong);
 float lobes=filteredNoise(outline,deckSeed^0x4c4f4245u,footprint/lobeAcross);
 float regional=(0.29*front+0.56*mass)/0.85;
 float lobeWeight=upper?0.15:mix(0.27,0.04,smoothstep(0.47,0.58,regional));
 float coarseStructure=smoothstep(0.65,2.65,footprint);
 // Keep the optical flank anchored to the same front at all distances.
 float flankWeight=0.0,condensate=0.5;
 if(coarseStructure>0.0){
  float crosswind=surface.y+17.0*bend+39.0*(regime-0.5)+28.0*(front-0.5);
  vec3 flank=vec3((surface.x-0.40*surface.z)/36.0,crosswind/5.5,
                  (surface.z+0.40*surface.x)/34.0);
  condensate=filteredNoise(flank,deckSeed^0x464c414eu,footprint/5.5);
  flankWeight=0.64*coarseStructure*(0.68+0.32*wet);
 }
 float weather=mix(regional,lobes,lobeWeight*(1.0-0.55*coarseStructure));
 float margin=weather-0.12*dry-((upper?0.752:0.725)-0.41*uCoverage);
 // Stretch the coarse edges with the wind-shaped lobes and preserve dense cores.
 float coarseWeather=smoothstep(0.36,1.4,footprint);
 float taper=1.0-smoothstep(0.018,0.105,margin);
 // Wide footprints lose small lobes. Local moisture and the regional
 // front carry the contour shape at that scale.
 float frontShape=1.05*(front-0.5)+0.62*(regime-0.5)-0.33*(mass-0.5);
 float distantStreak=0.14*bend+0.38*(lobes-0.5)+0.66*(condensate-0.5)+frontShape;
 float streak=mix(0.48*bend+1.04*(lobes-0.5),distantStreak,coarseStructure);
 float extension=min(0.14*max(streak,0.0),max(0.075-margin,0.0));
 margin+=coarseWeather*(extension+0.14*taper*min(streak,0.0));
 if(coarseStructure>0.0){
  float coreGuard=1.0-0.60*smoothstep(0.04,0.19,margin);
  margin+=flankWeight*coreGuard*(condensate-0.5)*0.32;
 }
 if(margin<=0.0)return 0.0;
 float core=smoothstep(0.035,0.165,margin);

 // Keep high-frequency erosion at the rim. The interior stays connected.
 float verticalFootprint=footprint/(uNoiseScale*uThickness);
 float detailFootprint=max(max(footprint,rayFootprint*uNoiseScale)*2.1,verticalFootprint*2.8);
 float billowScale=2.1+2.8*height/(radialDistance*uNoiseScale);
 vec3 billows=q*billowScale+vec3(0.31*phase,0.0,0.24*crossPhase);
 float billow=filteredNoise(billows,deckSeed^0x42494c4cu,detailFootprint);
 float erosion=(1.0-core)*0.027*(1.0-billow);
 float erodedMargin=margin-erosion;
 float body=smoothstep(0.0,(upper?0.145:0.105)+0.025*coarseWeather,erodedMargin);
 float profile;
 if(upper){
  float cap=0.80+0.10*core+0.08*billow;
  profile=smoothstep(0.0,0.10,height-0.58)*(1.0-smoothstep(0.0,0.13,height-cap+0.13));
 }else{
  float cellFootprint=max(footprint,rayFootprint*uNoiseScale);
  float resolved=1.0-smoothstep(0.16,0.63,cellFootprint);
  float localBody=smoothstep(0.024,0.084,erodedMargin);
  body=mix(body,localBody,resolved);
  // Body-anchored base relief and smaller cap detail share the same core.
  vec3 cells=vec3((surface.x-0.35*surface.z)/0.46+0.18*phase,
                  surface.y/0.34,(surface.z+0.35*surface.x)/0.46+0.16*crossPhase);
  float cell=filteredNoise(cells,deckSeed^0x42415345u,cellFootprint/0.34);
  float hanging=smoothstep(0.28,0.72,cell);
  float center=0.26+0.04*(hanging-0.5);
  float depth=0.075+0.17*sqrt(body);
  float baseDepth=min(depth*mix(0.55,1.38,hanging),center);
  float capDepth=min(depth*mix(0.99,1.25,billow),0.52-center);
  float crossSection=height<center?(center-height)/baseDepth:(height-center)/capDepth;
  float edgeWidth=mix(0.38,0.24,resolved);
  profile=1.0-smoothstep(1.0-edgeWidth,1.0,crossSection);
 }
 // A coarse column covers both cloud and gaps along its shoulders. Use
 // regional organization to keep the strongest fronts optically thick.
 float organized=smoothstep(0.43,0.73,0.72*regional+0.28*front);
 float columnRelief=1.0-coarseStructure*(upper?0.79:0.78)*(1.0-organized);
 float flankRelief=1.0-flankWeight*(1.0-smoothstep(0.12,0.86,condensate));
 return body*profile*(upper?0.48:1.0)*(0.80+0.20*billow)*columnRelief*flankRelief;
}
float cloudExtinction(vec3 local,float footprint,float rayFootprint){
 float density=cloudDensity(local,footprint,rayFootprint);
 float lowerEnd=1.0+uBase+0.52*uThickness;
 float core=dot(local,local)<lowerEnd*lowerEnd?smoothstep(0.08,0.28,density):0.0;
 return density*mix(uExtinction,uLowerExtinction,core);
}
bool sphere(vec3 o,vec3 d,float r,out vec2 interval){
 float b=dot(o,d),c=dot(o,o)-r*r,h=b*b-c;if(h<0.0)return false;
 float s=sqrt(h),q=-b-(b>=0.0?s:-s);interval=vec2(q,q==0.0?-b:c/q);
 if(interval.x>interval.y)interval=interval.yx;return true;
}
vec3 cloudIllumination(vec3 samplePosition,vec3 ray,float footprint,float localExtinction){
 // Coordinates are centered on the planet and measured in body radii.
 // At cloud altitude the spherical horizon is below the local horizontal.
 float inverseRadius=inversesqrt(dot(samplePosition,samplePosition));
 float sunElevation=dot(samplePosition,uSun)*inverseRadius;
 float horizon=-sqrt(max(0.0,1.0-inverseRadius*inverseRadius));
 // A narrow angular feather gives a continuous transition around tangency.
 float exposure=smoothstep(horizon-0.008,horizon+0.008,sunElevation);
 if(exposure<=0.0)return uAmbient;
 vec3 midpoints=uThickness*vec3(0.10,0.36,0.78);
 vec3 halfWidths=uThickness*vec3(0.10,0.16,0.26);
 vec3 opticalLengths=2.0*halfWidths;
 float lowerEnd=1.0+uBase+0.52*uThickness;
 float radiusSquared=dot(samplePosition,samplePosition);
 if(sunElevation>0.0&&radiusSquared<lowerEnd*lowerEnd){
  // Spend two probes on the remaining lower deck and one on the upper
  // deck. The empty space between the decks does not consume a tap.
  float parallel=dot(samplePosition,uSun);
  float rootOffset=parallel*parallel-radiusSquared;
  vec3 bounds=vec3(lowerEnd,1.0+uBase+0.58*uThickness,1.0+uBase+uThickness);
  vec3 exits=clamp(sqrt(max(vec3(0.0),bounds*bounds+vec3(rootOffset)))-parallel,
    vec3(0.0),vec3(2.0*uThickness));
  float lowerQuarter=0.25*exits.x;
  float upperHalf=0.5*max(0.0,exits.z-exits.y);
  // One upper probe represents the deck's tapered cap and base.
  float blend=smoothstep(0.0,0.10,sunElevation);
  midpoints=mix(midpoints,vec3(lowerQuarter,3.0*lowerQuarter,exits.y+upperHalf),blend);
  halfWidths=mix(halfWidths,vec3(lowerQuarter,lowerQuarter,upperHalf),blend);
  opticalLengths=mix(opticalLengths,vec3(2.0*lowerQuarter,2.0*lowerQuarter,1.32*upperHalf),blend);
 }
 float nearExtinction=cloudExtinction(samplePosition+uSun*midpoints.x,footprint,halfWidths.x);
 float shadow=opticalLengths.x*nearExtinction
             +opticalLengths.y*cloudExtinction(samplePosition+uSun*midpoints.y,footprint,halfWidths.y)
             +opticalLengths.z*cloudExtinction(samplePosition+uSun*midpoints.z,footprint,halfWidths.z);
 float sunVisibility=exp(-shadow*uRadius);
 float facing=max(0.0,sunElevation);
 float phase=0.65+0.35*pow(max(0.0,dot(ray,uSun)),8.0);
 // Clearing the solid horizon is not enough for full-strength sunlight.
 // Low rays also cross dense air, which dims and warms the direct light.
 float highSun=smoothstep(-0.10,0.30,sunElevation);
 vec3 directColor=vec3(uSunColor.x,
  uSunColor.y*(0.58+0.42*highSun),uSunColor.z*(0.32+0.68*highSun));
 float airTransmission=0.18+0.82*highSun;
 // A nearby opening toward the sun lifts diffuse light relative to a
 // blocked one. Reuse the closest shadow tap, with no density or alpha change.
 float height=(radiusSquared*inverseRadius-1.0-uBase)/uThickness;
 float lower=1.0-smoothstep(0.48,0.55,height);
 float resolved=1.0-smoothstep(0.0157,0.173,footprint*uNoiseScale);
 float daylight=smoothstep(0.0,0.12,sunElevation);
 float opening=1.45*(localExtinction-nearExtinction)
    /max(0.00000001,0.12*uLowerExtinction+localExtinction+nearExtinction);
 vec3 ambient=uAmbient*(1.0+clamp(opening,-0.55,0.55)*lower*resolved*daylight);
 return ambient+directColor*(exposure*airTransmission*sunVisibility*(0.25+0.75*facing)*phase);
}
)glsl"
R"glsl(float viewDepth(float z){float n=uNearFar.x,f=uNearFar.y;return uOrthographic?mix(n,f,z):n*f/(f*(1.0-z)+n*z);}
float cloudStepLength(float total,float thickness){
#if defined(EPOCH_VULKAN_CLOUD)
 // The Vulkan subpass traces every output pixel. Bound the initial tier's
 // integration cost without changing the density or depth model.
 return total/clamp(total/max(thickness/10.0,0.00000001),8.0,16.0);
#elif defined(GL_ES)
 return total/clamp(total/max(thickness/10.0,0.00000001),12.0,24.0);
#else
 return total/clamp(total/max(thickness/10.0,0.00000001),16.0,48.0);
#endif
}
vec2 cloudStepSegment(float begin,float end,float seam,bool rear){
 float split=clamp(seam,begin,end);
 return rear?vec2(split,end):vec2(begin,split);
}
#ifdef GL_ES
// Blend distant cloud contrast into the atmosphere already drawn behind it.
// Use the first real cloud interval, which can be the far shell segment
// when the camera is below both decks.
float cloudForegroundTransmission(vec3 origin,vec3 ray,float entry){
 float atmosphere=uCloudAtmosphereThickness;
 float foreground=max(0.0,entry-0.18*atmosphere);
 float observer=max(0.0,length(origin)-1.0);
 float below=1.0-smoothstep(0.42*uBase,0.83*uBase,observer);
 float height=max(0.0,length(origin+ray*(0.5*entry))-1.0);
 float column=foreground/atmosphere*exp(-2.5*height/atmosphere);
 return mix(1.0,exp(-0.30*column),below);
}
#endif
vec4 traceCloud(vec3 origin,vec3 ray,vec3 originDx,vec3 originDy,vec3 rayDx,vec3 rayDy,float depth){
 float rayProjection=max(0.000001,dot(ray,uForward));
 // The fitted solid near/far range is depth decoding metadata, not the
 // atmosphere's visibility range. Clear depth has no opaque occluder.
 float opaque=depth<1.0?viewDepth(depth)/(uWorldRadius*rayProjection):1e30;
 vec2 outer,inner;if(!sphere(origin,ray,1.0+uBase+uThickness,outer))return vec4(0.0);
 // Missing or coarse terrain depth must not expose clouds through the planet.
 // This smaller certified sphere does not define the sunlight horizon.
 vec2 solid;
 if(uViewOccluderRadius>0.0&&sphere(origin,ray,uViewOccluderRadius,solid)
  &&solid.y>max(0.0,solid.x))opaque=min(opaque,max(0.0,solid.x));
 outer=vec2(max(0.0,outer.x),min(opaque,outer.y));if(outer.y<=outer.x)return vec4(0.0);
 vec2 first=outer,second=vec2(0.0);bool split=sphere(origin,ray,1.0+uBase,inner)&&inner.y>outer.x&&inner.x<outer.y;
 if(split){first=vec2(outer.x,min(outer.y,inner.x));second=vec2(max(outer.x,inner.y),outer.y);}
 float firstLength=max(0.0,first.y-first.x),secondLength=max(0.0,second.y-second.x),total=firstLength+secondLength;
 if(total<=0.0)return vec4(0.0);
 float stepLength=cloudStepLength(total,uThickness),transmittance=1.0;vec3 color=vec3(0.0);
#if defined(EPOCH_VULKAN_CLOUD) || defined(GL_ES)
 // Inside the shell, a long grazing ray can turn one sunlit sample into
 // a bright band. Concentrate the same bins near the eye, where extinction
 // contributes most, and fade back to uniform spacing at the shell edges.
 float eyeShellHeight=(length(origin)-1.0-uBase)/uThickness;
 float inShell=smoothstep(-0.12,0.15,eyeShellHeight)*(1.0-smoothstep(0.78,1.08,eyeShellHeight));
 float samplingBias=inShell*smoothstep(4.0*uThickness,16.0*uThickness,total);
#endif
#if defined(EPOCH_VULKAN_CLOUD)
 for(int i=0;i<16;++i){
#elif defined(GL_ES)
 for(int i=0;i<24;++i){
#else
 for(int i=0;i<48;++i){
#endif
  float begin=float(i)*stepLength;
  if(begin>=total||transmittance<0.005)break;
  // A fractional last bin fades in from zero weight instead of moving
  // every sample when the ray's length crosses a whole number of steps.
#if defined(EPOCH_VULKAN_CLOUD)
  float end=i==15?total:min(total,begin+stepLength);
#elif defined(GL_ES)
  float end=i==23?total:min(total,begin+stepLength);
#else
  float end=i==47?total:min(total,begin+stepLength);
#endif
#if defined(EPOCH_VULKAN_CLOUD) || defined(GL_ES)
  // Remap both boundaries so the split and optical weights use the actual
  // length of each bin and the last bin still ends at the clipped surface.
  begin=mix(begin,begin*(begin/total),samplingBias);
  end=mix(end,end*(end/total),samplingBias);
#endif
  for(int part=0;part<2;++part){
   bool rear=part==1;
   vec2 segment=cloudStepSegment(begin,end,firstLength,rear);
   float lengthInShell=segment.y-segment.x;if(lengthInShell<=0.0)continue;
   float midpoint=(segment.x+segment.y)*0.5;
   // A bin spanning the inner gap is integrated on each side in depth order.
   float t=rear?second.x+(midpoint-firstLength):first.x+midpoint;
   float footprint=max(length(originDx+rayDx*t),length(originDy+rayDy*t));
   vec3 samplePosition=origin+ray*t;
   float extinction=cloudExtinction(samplePosition,footprint,lengthInShell*0.5);if(extinction<=0.0)continue;
   float alpha=1.0-exp(-extinction*lengthInShell*uRadius);
   vec3 illumination=cloudIllumination(samplePosition,ray,footprint,extinction);
   color+=transmittance*alpha*illumination;transmittance*=1.0-alpha;
  }
 }
#ifdef GL_ES
 if(depth==1.0&&!uOrthographic&&uCloudAtmosphereThickness>0.0){
  float entry=firstLength>0.0?first.x:second.x;
  vec4 cloud=vec4(color,1.0-transmittance);
  return cloud*cloudForegroundTransmission(origin,ray,entry);
 }
#endif
 return vec4(color,1.0-transmittance);
}
vec4 cloudEncodeSrgb(vec4 value){
 if(value.a>0.00001){
  vec3 color=value.rgb/value.a;
  vec3 srgb=mix(12.92*color,1.055*pow(max(color,vec3(0.0)),vec3(1.0/2.4))-0.055,step(vec3(0.0031308),color));
  value.rgb=srgb*value.a;
 }
 return value;
}
)glsl";
    inline constexpr std::string_view fragment_source=R"glsl(out vec4 outColor;
void main(){
 // Choose the same opaque depth texels as the compositor so a ray on the
 // boundary between pixels always stops at the same surface.
 ivec2 depthSize=textureSize(uDepth,0);
#ifdef GL_ES
 ivec2 tracePixel=ivec2(gl_FragCoord.xy);
 ivec2 depthPixel=cloudTraceDepthPixel(tracePixel,depthSize,uTraceSize);
#else
 ivec2 depthOffset=ivec2(1)-depthSize%ivec2(2);
 ivec2 depthPixel=ivec2(gl_FragCoord.xy)*2+depthOffset;
#endif
 vec2 sampleUv=(vec2(depthPixel)+0.5)/vec2(depthSize);
 vec2 p=sampleUv*2.0-1.0;vec3 origin=vec3(0.0),ray;
 if(uOrthographic){origin=uRight*p.x*uProjection.x+uUp*p.y*uProjection.y;ray=uForward;}
 else ray=normalize(uForward+uRight*p.x*uProjection.x+uUp*p.y*uProjection.y);
 origin/=uWorldRadius;origin-=uCenter;
 // Derivatives must be taken before ray-dependent discard or loop control.
 vec3 originDx=dFdx(origin),originDy=dFdy(origin),rayDx=dFdx(ray),rayDy=dFdy(ray);
 float depth=texelFetch(uDepth,depthPixel,0).r;
 vec4 value=traceCloud(origin,ray,originDx,originDy,rayDx,rayDy,depth);
 if(value.a==0.0)discard;
 outColor=value;
}
)glsl";
    inline constexpr std::string_view composite_source=R"glsl(out vec4 outColor;
// Ray finite differences stay defined inside the selective resolve branch.
// Use the same planet frame, density, and exact opaque stop as accumulation.
vec3 exactDirection(vec2 p){
 return normalize(uForward+uRight*p.x*uProjection.x+uUp*p.y*uProjection.y);
}
vec4 traceExactOpaque(ivec2 targetPixel,ivec2 depthSize,float targetZ){
 vec2 one=2.0/vec2(depthSize);
 vec2 p=(vec2(targetPixel)+0.5)*one-1.0;
 vec3 origin=-uCenter,ray=uForward;
 vec3 originDx=vec3(0.0),originDy=vec3(0.0),rayDx=vec3(0.0),rayDy=vec3(0.0);
 if(uOrthographic){
  origin+=(uRight*p.x*uProjection.x+uUp*p.y*uProjection.y)/uWorldRadius;
  originDx=uRight*(one.x*uProjection.x/uWorldRadius);
  originDy=uUp*(one.y*uProjection.y/uWorldRadius);
 }else{
  ray=exactDirection(p);
  rayDx=exactDirection(p+vec2(one.x,0.0))-ray;
  rayDy=exactDirection(p+vec2(0.0,one.y))-ray;
 }
 return traceCloud(origin,ray,originDx,originDy,rayDx,rayDy,targetZ);
}
uniform sampler2D uCloud;
uniform bool uEncodeSrgb,uRepairEdges;
#ifdef GL_ES
// Match slopes at the actual trace centers. Keep the middle half of
// the interval linear so cloud edges retain their position between rays.
float cloudTraceEdgeFraction(float fraction){
 float distance=min(fraction,1.0-fraction);
 float edge=max(1.0-4.0*distance,0.0);
 float offset=distance*edge*edge;
 return fraction<0.5?fraction-offset:fraction+offset;
}
#endif
void main(){
 ivec2 cloudSize=textureSize(uCloud,0),depthSize=textureSize(uDepth,0);
#ifdef GL_ES
 ivec2 targetPixel=ivec2(gl_FragCoord.xy);
 // The traced window includes both samples that bracket each resolved pixel.
 ivec2 base=clamp(cloudPrecedingTracePixel(targetPixel,depthSize,cloudSize),ivec2(0),cloudSize-ivec2(1));
 ivec2 right=min(base+ivec2(1),cloudSize-ivec2(1));
 ivec2 low=cloudTraceDepthPixel(base,depthSize,cloudSize);
 ivec2 high=cloudTraceDepthPixel(right,depthSize,cloudSize);
 vec2 fraction=clamp(vec2(targetPixel-low)/vec2(max(ivec2(1),high-low)),vec2(0.0),vec2(1.0));
 fraction=vec2(cloudTraceEdgeFraction(fraction.x),cloudTraceEdgeFraction(fraction.y));
#else
 ivec2 depthOffset=ivec2(1)-depthSize%ivec2(2);
 ivec2 targetPixel=clamp(ivec2(vUv*vec2(depthSize)),ivec2(0),depthSize-ivec2(1));
 // Reconstruct from the actual traced pixels. Each sample carries the
 // identical depth texel it used for occlusion in the accumulation pass.
 vec2 p=vec2(targetPixel-depthOffset)*0.5,fraction=fract(p);
 ivec2 base=ivec2(floor(p));
#endif
 bool brightSkyTap=false,compatibleTap=false;
 float targetZ=texelFetch(uDepth,targetPixel,0).r;
 float target=viewDepth(targetZ),weightSum=0.0,nearestDifference=1e30;vec4 sum=vec4(0.0),nearest=vec4(0.0);
 for(int y=0;y<2;++y)for(int x=0;x<2;++x){
  ivec2 sourcePixel=clamp(base+ivec2(x,y),ivec2(0),cloudSize-ivec2(1));
  vec4 value=texelFetch(uCloud,sourcePixel,0);
#ifdef GL_ES
  ivec2 sourceDepthPixel=cloudTraceDepthPixel(sourcePixel,depthSize,cloudSize);
  float sourceZ=texelFetch(uDepth,sourceDepthPixel,0).r;
#else
  float sourceZ=texelFetch(uDepth,sourcePixel*2+depthOffset,0).r;
#endif
  // A sky ray was traced without the opaque target's occluder. It cannot
  // stand in for a thin opaque silhouette, even as the closest fallback.
  if(targetZ<1.0&&sourceZ>=1.0){brightSkyTap=brightSkyTap||value.a>0.00001;continue;}
  compatibleTap=true;
  float difference=abs(viewDepth(sourceZ)-target);
  if(difference<nearestDifference){nearestDifference=difference;nearest=value;}
  float spatial=(x==0?1.0-fraction.x:fraction.x)*(y==0?1.0-fraction.y:fraction.y);
  float weight=spatial*exp(-difference/max(0.01,abs(target)*0.01));sum+=value*weight;weightSum+=weight;
 }
 vec4 value=weightSum>0.00001?sum/weightSum:nearest;
 // A thin silhouette may lie between all four low-resolution rays. Only
 // retrace it if an uncovered neighbor contains cloud worth resolving.
 if(uRepairEdges&&brightSkyTap&&!compatibleTap)
  value=traceExactOpaque(targetPixel,depthSize,targetZ);
 if(uEncodeSrgb)value=cloudEncodeSrgb(value);
 outColor=value;
}
)glsl";
}
