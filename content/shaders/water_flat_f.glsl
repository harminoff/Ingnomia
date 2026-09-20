#version 430 core

layout(location = 1) noperspective in float vFluidLevel;
layout(location = 9) noperspective in float vWaterLight;
layout(location = 10) flat in float vWaterSky;
layout(location = 0) out vec4 fColor;
#include "lighting.glsl"

void main()
{
    // Match the authored Water sprite even when animated shading is disabled.
    const vec3 deep = vec3(35.0, 60.0, 134.0) / 255.0;
    const vec3 body = vec3(45.0, 88.0, 175.0) / 255.0;
    float shallow = 1.0 - clamp(vFluidLevel / 10.0, 0.0, 1.0);
    fColor = vec4(shadeWorldColor(mix(deep, body, shallow), vWaterSky, vWaterLight, true), 1.0);
}
