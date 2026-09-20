// Shared by terrain, creatures and water. Pixel colours are transformed in
// place; no blur, bloom, extra screen samples or changes to visibility.
uniform float uDaylight;
uniform float uLightMin;

vec3 shadeWorldColor(vec3 albedo, float skyExposure, float artificialLight, bool discovered)
{
    float sun = clamp(uDaylight * skyExposure, 0.0, 1.0);
    float lamp = clamp(artificialLight, 0.0, 1.0);
    float light = max(sun, lamp);
    float minimum = clamp(uLightMin, 0.0, 1.0);
    float luma = dot(albedo, vec3(0.299, 0.587, 0.114));
    if (!discovered)
    {
        // Preserve the existing undiscovered-tile treatment exactly.
        return mix(vec3(luma), albedo, 0.1 + 0.9 * light)
             * (minimum + (1.0 - minimum) * light);
    }

    // Outdoor night should read as moonlit darkness, not late dusk. Keep the
    // configured floor as the source of the value, but scale it down for the
    // nocturnal pass. The daylight curve then raises it continuously toward the
    // original daytime ambient as the sun comes up.
    float nightAmbient = clamp(minimum * 0.55, 0.08, 0.22);
    float dayAmbient = minimum + (1.0 - minimum) * 0.40 * skyExposure;
    float ambient = mix(nightAmbient, dayAmbient, smoothstep(0.0, 0.80, sun));
    float brightness = mix(ambient, 1.0, pow(light, 0.75));
    float adaptation = (1.0 - light) * mix(0.35, 1.0, skyExposure);
    vec3 readable = mix(albedo, pow(max(albedo, vec3(0.0)), vec3(0.78)), adaptation);
    float saturation = mix(mix(0.62, 0.82, skyExposure), 1.0, light);
    readable = mix(vec3(dot(readable, vec3(0.299, 0.587, 0.114))), readable, saturation);

    vec3 moonTint = mix(vec3(0.90, 0.94, 1.0), vec3(0.80, 0.90, 1.0), skyExposure);
    vec3 tint = mix(moonTint, vec3(1.0), sun);
    // Existing point-light intensity already includes occlusion and falloff.
    // Colour its lit area warmly; zero intensity cannot illuminate a shadow.
    float warmth = smoothstep(0.0, 0.65, lamp) * (1.0 - sun);
    tint = mix(tint, vec3(1.06, 0.97, 0.80), warmth);
    return clamp(readable * brightness * tint, 0.0, 1.0);
}
