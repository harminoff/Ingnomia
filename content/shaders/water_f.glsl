#version 430 core

layout(location = 0) noperspective in vec2 vTexCoords;
layout(location = 1) noperspective in float vFluidLevel;
layout(location = 2) noperspective in vec2 vWaterFlow;
layout(location = 3) flat in uvec4 vNeighborLevels;
layout(location = 7) flat in uint vIsSide;
layout(location = 8) noperspective in vec2 vWaterUv;

layout(location = 9) noperspective in float vWaterLight;
layout(location = 10) flat in float vWaterSky;
layout(location = 0) out vec4 fColor;

uniform sampler2D uDuDvMap;
uniform float uWaterTime;
uniform int uWaterQuality;
uniform int uWorldRotation;
#include "lighting.glsl"

vec2 rotateDirection(vec2 p)
{
    if (uWorldRotation == 1) return vec2(-p.y, p.x);
    if (uWorldRotation == 2) return -p;
    if (uWorldRotation == 3) return vec2(p.y, -p.x);
    return p;
}

vec2 unrotateDirection(vec2 p)
{
    if (uWorldRotation == 1) return vec2(p.y, -p.x);
    if (uWorldRotation == 2) return -p;
    if (uWorldRotation == 3) return vec2(-p.y, p.x);
    return p;
}

// Terrain floors are drawn from 32x16-pixel diamonds. Sample on that same
// native pixel grid, anchored in the world so zooming/panning cannot make the
// water grain slide over the shore. Keep geometry and its fixed depth intact.
vec2 surfacePixel(vec2 world)
{
    vec2 rotated = rotateDirection(world);
    vec2 pixel = floor(vec2(16.0 * (rotated.x - rotated.y),
                            8.0 * (rotated.x + rotated.y))) + 0.5;
    return unrotateDirection(vec2(pixel.x / 32.0 + pixel.y / 16.0,
                                  pixel.y / 16.0 - pixel.x / 32.0));
}

float ripples(vec2 p, float time)
{
    vec2 warp = texture(uDuDvMap, p * 0.12 + vec2(0.005, -0.004) * time).rg * 2.0 - 1.0;
    p += warp * 0.30;
    // Short, broken wavelets similar in scale to the authored water sprite,
    // rather than long specular ribbons. Palette bands give crisp pixel edges.
    return sin(dot(p, vec2(4.7, 2.3))) * 0.55
         + sin(dot(p, vec2(-2.1, 5.3)) + 1.7) * 0.30
         + sin(dot(p, vec2(8.1, -3.5)) + 4.1) * 0.15;
}

void main()
{
    // The four opaque colours of terrain.png's authored WaterFloor sprite
    // (source rectangle 128 0 32 36), before the game's daylight tint.
    const vec3 deep = vec3(35.0, 60.0, 134.0) / 255.0;
    const vec3 body = vec3(45.0, 88.0, 175.0) / 255.0;
    const vec3 ripple = vec3(36.0, 130.0, 229.0) / 255.0;
    const vec3 glint = vec3(102.0, 189.0, 255.0) / 255.0;
    float shallow = 1.0 - clamp(vFluidLevel / 10.0, 0.0, 1.0);
    if (uWaterQuality == 0)
    {
        fColor = vec4(shadeWorldColor(mix(deep, body, shallow), vWaterSky, vWaterLight, true), 1.0);
        return;
    }

    vec2 world = surfacePixel(vWaterUv / 0.045);
    // Eight small animation steps per second fit the sprite animation style.
    float time = floor(uWaterTime * 8.0) / 8.0;
    vec2 velocity = vec2(0.10, -0.045) + vWaterFlow * 0.50;
    // Reset each bounded offset while its contribution is invisible. Shared
    // coordinates/flow keep adjacent tiles part of one continuous water body.
    float phaseA = fract(time / 6.0);
    float phaseB = fract(time / 6.0 + 0.5);
    float weightA = 1.0 - abs(phaseA * 2.0 - 1.0);
    float wave = mix(ripples(world - velocity * phaseB * 6.0, time),
                     ripples(world - velocity * phaseA * 6.0, time), weightA);
    if (uWaterQuality > 1)
        wave += 0.06 * sin(dot(world, vec2(17.0, 11.0)) - time * 1.5);

    float tone = wave + shallow * 0.15;
    vec3 color = deep;
    if (tone > 0.05) color = body;
    if (tone > 0.57) color = ripple;
    if (tone > 0.83) color = glint;

    // Sparse, one-pixel wavelets at real shores; never outline wet/wet edges.
    float shore = 1.0;
    if (vNeighborLevels.x == 0u) shore = min(shore, vTexCoords.y);
    if (vNeighborLevels.y == 0u) shore = min(shore, 1.0 - vTexCoords.x);
    if (vNeighborLevels.z == 0u) shore = min(shore, 1.0 - vTexCoords.y);
    if (vNeighborLevels.w == 0u) shore = min(shore, vTexCoords.x);
    if (vIsSide == 0u && shore < 0.055 && wave > 0.55) color = ripple;
    if (vIsSide != 0u) color = wave > 0.25 ? body : deep;

    fColor = vec4(shadeWorldColor(color, vWaterSky, vWaterLight, true), 1.0);
}
