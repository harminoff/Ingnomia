#version 430 core

layout(location = 1) flat in uint vFluidLevel;
layout(location = 7) flat in uint vIsSide;

layout(location = 0) out vec4 fColor;

uniform float uDaylight;

void main()
{
	const vec3 shallow = vec3(0.10, 0.42, 0.52);
	const vec3 deep = vec3(0.025, 0.16, 0.28);
	const float depthMix = clamp(float(vFluidLevel) / 10.0, 0.0, 1.0);
	const vec3 color = mix(shallow, deep, depthMix) * mix(0.60, 1.0, uDaylight);
	fColor = vec4(color, vIsSide != 0u ? 0.84 : 0.94);
}
