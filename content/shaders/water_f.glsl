#version 430 core

layout(location = 0) noperspective in vec2 vTexCoords;
layout(location = 1) flat in uint vFluidLevel;
layout(location = 2) flat in uint vWaterFlow;
layout(location = 3) flat in uvec4 vNeighborLevels;
layout(location = 4) flat in uvec3 vTile;
layout(location = 5) in vec2 vScreenUv;
layout(location = 6) flat in vec3 vBaseNormal;
layout(location = 7) flat in uint vIsSide;
layout(location = 8) noperspective in vec2 vWaterUv;

layout(location = 0) out vec4 fColor;

uniform sampler2D uSceneColor;
uniform sampler2D uSceneDepth;
uniform sampler2D uDuDvMap;
uniform sampler2D uNormalMap;
uniform vec2 uViewportSize;
uniform float uWaterTime;
uniform int uWaterQuality;
uniform float uDaylight;

vec2 flowDirection(uint flow)
{
	vec2 direction = vec2(0.0);
	if ((flow & 0x01u) != 0u) direction += vec2(0.0, -1.0);
	if ((flow & 0x02u) != 0u) direction += vec2(1.0, 0.0);
	if ((flow & 0x04u) != 0u) direction += vec2(0.0, 1.0);
	if ((flow & 0x08u) != 0u) direction += vec2(-1.0, 0.0);
	return length(direction) > 0.0 ? normalize(direction) : vec2(0.15, 0.07);
}

void main()
{
	vec2 screenUv = gl_FragCoord.xy / max(uViewportSize, vec2(1.0));
	float depthMix = clamp(float(vFluidLevel) / 10.0, 0.0, 1.0);
	vec3 waterDeep = vec3(0.025, 0.16, 0.28);
	vec3 waterShallow = vec3(0.10, 0.42, 0.52);
	if (uWaterQuality == 0)
	{
		vec3 flatColor = mix(waterShallow, waterDeep, depthMix) * mix(0.60, 1.0, uDaylight);
		fColor = vec4(flatColor, vIsSide != 0u ? 0.78 : 0.70);
		return;
	}

	vec2 flow = flowDirection(vWaterFlow);
	// vWaterUv is continuous across the tessellated water surface.  Sampling
	// from tile-local coordinates was the source of the visible grid: every
	// cell started a new DuDv/normal pattern and a new refracted patch.
	vec2 worldUv = vWaterUv;
	// Use broad, overlapping waves.  The generated DuDv map is tileable, but
	// sampling it at its native frequency made the diagonal isometric surface
	// look like a stack of narrow strips.
	vec2 dudvA = texture(uDuDvMap, worldUv * 0.58 + flow * uWaterTime * 0.018).rg * 2.0 - 1.0;
	vec2 dudvB = texture(uDuDvMap, worldUv * 0.91 - flow.yx * uWaterTime * 0.013 + vec2(0.37, 0.61)).rg * 2.0 - 1.0;
	// Two low-amplitude, world-space layers keep the surface alive without
	// making the wave pattern track the tile grid.
	float broadWave = 0.5 + 0.5 * sin(dot(worldUv, vec2(19.0, 13.0)) + uWaterTime * 0.42);
	vec2 distortion = (dudvA + dudvB) * (0.004 + broadWave * 0.002);
	vec2 refractUv = clamp(screenUv + distortion, vec2(0.002), vec2(0.998));
	vec3 scene = texture(uSceneColor, refractUv).rgb;
	vec2 reflectionUv = clamp(vec2(screenUv.x, 1.0 - screenUv.y) + distortion * 1.6, vec2(0.002), vec2(0.998));
	vec3 reflectedScene = texture(uSceneColor, reflectionUv).rgb;
	vec3 environment = mix(vec3(0.08, 0.14, 0.18), vec3(0.34, 0.52, 0.62), uDaylight);
	float reflectionValidity = smoothstep(0.02, 0.16, min(reflectionUv.y, 1.0 - reflectionUv.y));
	vec3 reflection = mix(environment, reflectedScene, reflectionValidity * 0.35);
	vec3 mappedNormal = texture(uNormalMap, worldUv * 0.72 + dudvA * 0.06).rgb * 2.0 - 1.0;
	vec3 normal = normalize(vBaseNormal + vec3(mappedNormal.xy * 0.32, mappedNormal.z * 0.18));
	vec3 viewDirection = normalize(vec3(-0.45, -0.55, 0.70));
	vec3 lightDirection = normalize(vec3(-0.35, -0.25, 0.90));
	float fresnel = 0.12 + 0.58 * pow(1.0 - clamp(dot(normal, viewDirection), 0.0, 1.0), 3.0);
	float specular = pow(max(dot(reflect(-lightDirection, normal), viewDirection), 0.0), 32.0) * uDaylight;
	vec3 base = mix(waterShallow, waterDeep, depthMix);
	// Keep the river bed as a subtle refraction cue, not as a texture visible
	// through every cell.  This lets one continuous water body read above the
	// authored sand/soil floor.
	vec3 color = mix(base, scene, 0.04);
	color = mix(color, reflection, fresnel);
	color += vec3(0.72, 0.88, 0.92) * specular * 0.55;

	// Foam belongs on a real fluid/land boundary only.  The old expression
	// applied an edge highlight to every cell edge, which outlined the water
	// grid even inside a large body.
	float shore = 0.0;
	if (vNeighborLevels.x == 0u) shore = max(shore, 1.0 - smoothstep(0.0, 0.16, vTexCoords.y));
	if (vNeighborLevels.y == 0u) shore = max(shore, 1.0 - smoothstep(0.0, 0.16, 1.0 - vTexCoords.x));
	if (vNeighborLevels.z == 0u) shore = max(shore, 1.0 - smoothstep(0.0, 0.16, 1.0 - vTexCoords.y));
	if (vNeighborLevels.w == 0u) shore = max(shore, 1.0 - smoothstep(0.0, 0.16, vTexCoords.x));
	float foamNoise = clamp(0.5 + 0.5 * dudvB.x, 0.0, 1.0);
	float foam = shore * (1.0 - depthMix) * (0.22 + 0.20 * foamNoise);
	color = mix(color, vec3(0.72, 0.90, 0.88), clamp(foam, 0.0, 0.45));

	float sceneDepth = texture(uSceneDepth, screenUv).r;
	float depthSeparation = clamp(abs(sceneDepth - gl_FragCoord.z) * 80.0, 0.0, 1.0);
	// Full water should read as one continuous body. Preserve a little
	// transparency for shallow edges, but make a full cell effectively opaque.
	float alpha = mix(0.99, 0.975, depthMix) * mix(0.94, 1.0, depthSeparation);
	if (vIsSide != 0u)
		alpha = max(alpha, 0.78);
	color *= mix(0.60, 1.0, uDaylight);
	fColor = vec4(color, alpha);
}
