# Lighting shader review - 2026-09-10

The lighting pass preserves the current pixel-art terrain and water treatment.
The old world shader reduced unlit tiles to 10% colour saturation and multiplied
all their detail by `lightMin` (0.35 in the user's current settings). This made
nighttime terrain dark and almost grey. Water used a separate fixed day/night
multiplier and did not respond to local light sources.

`content/shaders/lighting.glsl` now provides shared colour grading for the world,
creatures, animated water, and the flat water fallback. Outdoor moonlight lifts
dark mid-tones, retains more colour, and adds a restrained cool tint. Existing
point lights provide warmer colour and brighter falloff. The minimum-light
setting remains the basis of ambient brightness; the user's configuration file
was not changed. Daylit colours pass through unchanged. The undiscovered-tile
branch retains the previous shading calculation.

Water receives the existing per-tile light field through shared wet corners, so
its light response is continuous across connected water. Sky exposure remains
per tile so covered water does not receive outdoor moonlight. Its pixel pattern,
geometry, depth ordering, flow, and containment are preserved.

The renderer expands the single shared lighting include before compiling GLSL
and supplies water with `uLightMin`. No new framebuffer passes, blur, bloom,
textures, or gameplay light sources are introduced. The existing CPU light map
continues to determine occlusion and range. Its intensity is scalar, so the warm
local-light treatment is shared by the existing types of point light.

## Verification

An opt-in `tests/lighting/runtime_probe.h` comparison uses one paused generated
world in an isolated profile. It reloads the old shaders and then the new shaders,
keeping the terrain, objects, camera and gameplay state the same. The scene has
outdoor torches and two roofed underground rooms separated by a view-blocking
wall, plus an undiscovered patch. Shader reloads temporarily use a snapshot of
the old shader directory; the normal content path is restored before shutdown.
Normal launches do not enable this fixture.

The final comparison with the full tree canopy visible measured:

- 100% identical sampled daylit terrain and vegetation pixels (962283 samples).
- Median unlit nighttime luminance in this sample increased from 34.36 to 64.81,
  or 1.89 times the old value; it remained 66% of the daytime median.
- The same 68737 sampled pixels were brightened by the outdoor torches in both
  versions, preserving the footprint of the CPU light field.
- The underground lit neighbour had intensity 18; the tile behind the wall had
  intensity 0. Turning off the light returned the neighbour to 0.
- The sampled shadow-side floor did not change when the torch was switched on.
  The undiscovered patch and surrounding unknown tiles were pixel-identical
  between old and new shaders.
- Both focused CTest checks passed: water flow and main-window renderer wiring.
  The checked native logs have no shader compile/link or GL-invalid-operation
  errors. MSVC compilation, linking, and Qt/content staging passed.

Evidence: `.verification/lighting-20260910/`, including paired `before-*.png` and
`after-*.png`, `result.txt`, `image-results.json`, the old shader snapshot, and the
image measurement script. `full-scene/` contains the comparison with the view
raised to include the full tree canopy; the first run retains the cutaway view.
The final native run completed all eleven captures and shut down its window.
The rebuilt executable SHA-256 for the lighting schedule is
`E50CC18C8E4BA55CFBA09AB3DE0183B5BAF415486A5082968D98BAB87B648E30`.

## Night schedule follow-up - 2026-09-12

The renderer now derives a continuous daylight value from the saved sunrise,
sunset, and in-game clock. Each transition spans 180 in-game minutes with a
smoothstep curve, giving a gradual dawn and dusk while keeping a full dark
interval around midnight. A short 1.5-second render settle only absorbs a
load/pause jump; it does not replace the in-game schedule.

The nocturnal ambient floor is now 55% of the configured `lightMin` (clamped to
0.08-0.22). With the isolated profile's `lightMin=0.35`, the final same-world
capture measured median nighttime terrain luminance 21.64 versus 35.06 with the
previous lighting, or 22.6% of the daytime sample. The roofed torch room still
has a bright local pool while the shadow-side floor and undiscovered patch remain
unchanged. The full-scene captures are in `.verification/lighting-20260912/`.

To reproduce, use an isolated `INGNOMIA_DATA_FOLDER`, set
`INGNOMIA_LIGHTING_PROBE` to a fresh output directory, and set
`INGNOMIA_LIGHTING_BASELINE_CONTENT` to a directory containing the old `shaders/`.
The profile's new-game settings should use a world of at least 70x70 tiles with
at least seven levels below the ground. The probe constructs its test rooms and
lights only in the temporary world and exits after approximately one minute.

The CMake reconfigure needed an unsandboxed build to update the pre-existing
generated version header. Native probe processes retained executable mappings
after window shutdown; exact previous binaries were renamed inside the canonical
build directory to allow relinking. No user-owned game was stopped.
