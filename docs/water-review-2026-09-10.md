# Water review and repair — 2026-09-10

The active checkout is `Ingnomia2/Ingnomia`, branch `feat/separate-ui-windows`.
Existing UI, creature-motion and detached-window changes were preserved. The
water changes were applied to the simulation and renderer already in this tree.

## Follow-up: match the authored pixel-art style

Reviewed the terrain and seasonal-grass atlases at native resolution and enlarged
with nearest-neighbour sampling. The stone and grass use crisp pixel clusters and
limited shade ramps. The original `WaterFloor` sprite is exactly four colours:
`#233C86`, `#2D58AF`, `#2482E5`, and `#66BDFF`. Its source rectangle is
`128 0 32 36` in `content/tilesheet/terrain.png`, confirmed by the `BaseSprites`
entry in `content/db/ingnomia.db.sql`.

Reworked `water_f.glsl` around that art direction:

- Use the original four-colour palette, with hard colour bands and small broken
  ripples. Removed the smooth sky/specular/refraction treatment from the previous
  appearance pass. Shallow water favours lighter palette entries.
- Sample on the terrain's native 32x16 isometric pixel grid. The sampling grid is
  anchored to world coordinates and follows camera rotation and zoom, preserving
  sharp pixels and continuous patterns across water cells.
- Animate at eight steps per second, with gentle movement in settled pools and
  faster drift driven by the existing simulation flow flags. Keep bounded,
  overlapping advection phases so different currents do not stretch indefinitely.
- Use sparse blue shoreline wavelets and the same palette in the flat fallback.
  High quality adds fine ripple detail within the same pixel-art treatment.

Only the two water fragment shaders and this report changed in this style pass.
The vertex geometry, depth ordering, simulation, terrain atlases, and gameplay
remain unchanged. Restart the game to load the staged shaders; existing saves work.

Verification evidence is in `.verification/water-style-20260910/`. The
`authored-*-magnified.png` files are enlarged samples used for the art review;
they are not replacement assets. A sampled river interior in the native game
capture contains exactly the same four RGB colours as the original water sprite.
Native animation captures cover settled, eastward, westward, and shallow/high
water, plus all four camera rotations. The paused fixture controls flow flags to
check shader motion independently of gameplay. All four sequences contain 61
frames, exactly the four authored RGB colours in the sampled water interior,
and zero changes in the fixed soil sample. East/west image motion reverses sign.
Each sequence has 12 unchanged frame pairs at 10 captures/second, matching the
intentional eight-step animation cadence. See `animation-results.json` and the
original PNG frames for the measurements.

The generated-map regression still reports 40780 volume, zero flooded dry-land
cells after 600 steps, and level 9 water entering a mined natural bank with volume
conserved. Focused water and main-window CTest checks pass (2/2). Native checks
also cover the flat preset and generated maps at 1.7x, 1x, and 0.5x zoom. Source
and staged shader bytes match, the build reports no pending work, and the checked
runtime logs have no shader compile or GL-invalid-operation errors.

- [Pixel-art river](../.verification/water-style-20260910/generated-first/02-simulated.png)
- [Normal zoom](../.verification/water-style-20260910/generated-final/02-simulated.png)
- [Map overview](../.verification/water-style-20260910/overview/02-simulated.png)
- [Flow animation](../.verification/water-style-20260910/flowing-water.mp4)
- [Settled animation](../.verification/water-style-20260910/settled-water.mp4)

This supersedes the visual treatment in the earlier appearance section below.
Fragment shader SHA256: `ACCCCB89712C4177946241E3E82C10A88B515DA18EF732A772CD359A9A42580D`.
Flat fallback SHA256: `B67FE0D4D86D625D6CDAA58565B401E755AD2CA7C949B0D23453461A40222DD5`.

## Follow-up: visible flowing-water shading

The corrected water surface was still too flat: its previous UV drift was slow,
normal perturbations were weak, and specular highlights rarely appeared. The
fragment shader now uses a continuous world-space wave field with curved crests,
moving normal-based sky highlights, deeper troughs, and broken shoreline foam.
Settled bodies have gentle wind ripples; the existing interpolated simulation
flow flags add faster directional motion. Two overlapping six-second advection
phases prevent unbounded stretching as flow directions vary across the surface.
High quality adds fine normal-map ripples. The flat fallback remains available.

This pass changes shading only. The fixed tile depth, shared water corners,
occupancy rules, generation, and CPU water solver remain unchanged. It does not
displace vertices or reintroduce the incorrect screen-mirrored terrain reflection.
Restart the updated game to load the shader; existing saves are supported.

Validation on the production Windows OpenGL renderer:

- Captured 61 native 1600x900 frames over a full advection cycle for settled water,
  eastward flow, westward flow, and shallow water using high quality. Flow flags
  are controlled in the paused rendering fixture; these are appearance checks,
  not evidence of extra physical simulation in the shader.
- Image motion analysis reverses direction between east and west. Mean RGB
  change per frame in the water interior is 0.66 for settled water, 3.22 for
  eastward flow, and 2.35 for westward flow. The sampled fixed soil region has
  zero pixel changes. These measurements establish visible shader animation,
  not a GPU performance benchmark or a calibrated physical velocity.
- Captured each fixture at all four rotations and inspected concave banks,
  islands, full-depth and half-full water. Also checked the flat preset.
- Generated a river map with seed 1342516210 at close zoom. All 600 water steps
  preserved the earlier containment result: 40780 volume, zero flooded dry-land
  cells. Mining a natural bank admitted water at level 9 and conserved volume.
- MSVC build/link and Qt content staging succeeded. Focused water and main-window
  wiring CTest checks passed (2/2). Checked logs contain no shader compile or
  GL-invalid-operation errors. Source and deployed shader hashes match.

Evidence: `.verification/water-appearance-20260910/`, including
[flowing-water video](../.verification/water-appearance-20260910/flowing-water.mp4),
[settled-water video](../.verification/water-appearance-20260910/settled-water.mp4),
[generated river](../.verification/water-appearance-20260910/generated/02-simulated.png),
`animation-results.json`, and the original native PNG sequences.

The opt-in shoreline probe now also accepts `INGNOMIA_WATER_APPEARANCE_PROBE=1`
to record the animation cycle, plus `INGNOMIA_WATER_SHORELINE_FLOW` (0 for settled,
2 for east, 8 for west). It uses the same isolated profile and copied save as the
shoreline checks below. Normal gameplay does not enable this fixture.

Final shader SHA256: `AF7179F4ACBF5104B16F7C90D7C983CFC0F12D0B612E2DA32A4D94BD2B835E73`.
Updated executable SHA256: `3DD9AA1246A585A05BA1DC05DD1764DD4C829F2016397E32CCA85114DD9ADD79`.

## Follow-up: isometric shoreline rendering

The triangular shoreline cutouts came from mixing two depth conventions. Terrain
sprites use one depth per tile (`z + rotatedX + rotatedY`, with walls/creatures
at `+0.5`). Water interpolated depth across each diamond. That sloped depth plane
crossed the constant-depth bank sprites within a triangle, producing the visible
brown/blue wedges around concave banks and islands.

`water_v.glsl` now sorts each water face at its rotated tile depth plus `0.25`,
between the existing floor and wall layers. Its projected geometry, shared corner
heights, continuous texture coordinates and flow simulation are preserved.

The animated shader also flipped the entire scene vertically as a supposed
reflection. In an isometric view this projects unrelated terrain and tile grids
onto the water. `water_f.glsl` now uses environment lighting, normal-based Fresnel
and specular highlights, plus subtle continuous world-space wave shading. It keeps
shallow refraction and shoreline foam without the false mirrored scene.

Validation used the production Windows OpenGL renderer:

- Reproduced the old triangular island artifact, then verified its disappearance
  with only the vertex-depth fix, before changing the reflection shading.
- Inspected a concave bay, island, front banks and back banks in all four camera
  rotations, with flat shading, animated shading and half-full surface cells.
- Captured a real new world using the user's current settings and seed 1342516210
  at close zoom. After 600 water steps, no previously dry land flooded; volume
  remained 40780. Mining a natural bank admitted water (level 9) and conserved it.
- MSVC build/link and Qt staging passed; final build reports no work pending.
  Source and deployed shader bytes match. GPU runs produced their captures with
  no shader compilation or GL-invalid-operation errors in the checked logs.
- Focused water and main-window wiring CTest checks passed (2/2).

The optional `tests/water/shoreline_probe.h` probe loads a copied save, pauses it,
and constructs the rendering fixture in memory. Set an isolated
`INGNOMIA_DATA_FOLDER`, copied `INGNOMIA_AUTOMATE_LOAD_PATH`, and a fresh
`INGNOMIA_WATER_SHORELINE_PROBE` output directory. Optional
`INGNOMIA_WATER_SHORELINE_LEVEL` controls the top-cell depth (1-10; default 10).
It writes four `rotation-N.png` captures. This is renderer verification, not an
additional gameplay simulation fixture. Normal launches do not enable it.

Evidence is under `.verification/water-shoreline-20260910/`:

- [Before depth correction](../.verification/water-shoreline-20260910/baseline/rotation-0.png)
  and [after depth correction](../.verification/water-shoreline-20260910/fixed-flat/rotation-0.png)
  use the same initial framing; compare the small island's base.
- `flat-framed`, `waves-verified`, and `shallow-verified` contain the complete
  bay's four rotations. Intermediate captures are retained separately.
- [Final generated river](../.verification/water-shoreline-20260910/generated-verified/02-simulated.png)
  and [whole-world results](../.verification/water-shoreline-20260910/generated-verified/result.txt).

**Save scope:** this rendering correction applies to existing saves after loading
the updated build; it does not require generating another world. The separate
new-world requirement below applies only to the earlier terrain-generation fix.

## Follow-up: generated maps flooding on startup

The earlier reservoir fixture did **not** validate generated-map initialization:
it deliberately cleared external water. The user's full-map screenshots exposed
that coverage gap. The generation fixes below supersede that limited acceptance.

The ocean generator filled to hard-coded Z=99 and refused to dig below Z=92.
With the user's 100-level world and ground setting 92, the baseline probe measured
water at Z=98 above dry land at Z=92. Activating that real stored water produced
the flooding; the wall was not required by the solver.

`WorldGenerator` now measures the undisturbed terrain, chooses an ocean surface
below the lowest dry walking surface, and carves a tapered bed below that level.
It clears old water and floors together at river/ocean intersections. Ocean edge
selection now uses the world seed instead of wall-clock time.

Uneven-terrain testing also found river water authored above its local bed, plus
high river sections/sources feeding lower land. Rivers now sample their full width
and banks, start with a common contained surface, and join the ocean at its level.
Each column is filled continuously from a real bed. On hilly maps this can produce
deeper channels through high ground. Aquifers, drains, finite-volume simulation,
and wake-on-mining behavior remain active; water is not frozen to hide the issue.

### Whole-world validation

`tests/water/generation_probe.h` creates a real new game in an isolated profile.
It preserves all generated water, terrain, plants, sources and drains. Each case
runs 400 production flow steps with exact volume conservation, then 200 complete
water steps including sources/drains. It checks every initially dry sunlit floor
for flooding, then mines an actual wet bank and checks entry/volume conservation.
The dry-world case has no wet bank to mine.

| Case | Seed | Depth / ground | Ocean / rivers | Dry land flooded after 600 steps |
| --- | --- | --- | --- | --- |
| User settings | 1239626711 | 100 / 92 | 5 / 3 | 0 |
| River only | 1239626711 | 100 / 92 | 0 / 3 | 0 |
| Original depth | 532879058 | 130 / 100 | 5 / 3 | 0 |
| Hilly terrain, flatness 20 | 42 | 130 / 92 | 10 / 3 | 0 |
| Minimum supported depth | 1 | 71 / 63 | 8 / 0 | 0 |
| Dry world | 7 | 71 / 63 | 0 / 0 | 0 |
| Additional coast seed | 1337 | 100 / 92 | 5 / 3 | 0 |

All six wet cases pass natural-bank mining and volume conservation. The hilly
case failed before the river correction (588 dry land cells flooded), and passes
with the final generation code. The minimum-depth probe initially searched only
for banks to the east; the probe now checks all four directions and its rerun
passes. These are full-world operation tests, not a mouse-driven mining job.

The executable was rebuilt and Qt runtime files staged. Focused water and main
window CTest checks pass (2/2). Final game framebuffer captures were inspected:
[map matching the user's settings after simulation](../.verification/water-generation-20260910/fixed-seed1239626711-final/02-simulated.png),
[river-only result](../.verification/water-generation-20260910/river-only-final/02-simulated.png).
The unchanged earlier isolated-physics results remain documented below.

**Save scope:** these changes fix newly generated maps. Existing saves retain
their old terrain and water, including a previously generated water wall or flood.
No user save was rewritten or drained automatically. Start a new map with this
build to use the corrected generation.

### Reproduce generation checks

Create an isolated profile containing `settings/newgame.json` with the desired
parameters and `settings/config.json` referencing this build's content directory.
Do not enable the separate isolated-reservoir probe or automatic load/new-game
hooks at the same time. This probe dispatches its own new game.

```powershell
$env:INGNOMIA_DATA_FOLDER = '<isolated profile>'
$env:INGNOMIA_WATER_GENERATION_PROBE = '<fresh output directory>'
$env:INGNOMIA_WATER_GENERATION_SEED = '1239626711'
& .\build-wave8-root-msvc-link-priority2\Ingnomia.exe
```

The output directory receives `result.txt` and three framebuffer captures. Final
results live under `.verification/water-generation-20260910/` in
`fixed-seed1239626711-final`, `river-only-final`, `original-depth-final`,
`hills-settled`, `minimum-depth-verified`, `dry-world-final`, and
`coast-seed1337-final`. Earlier failed/interrupted probes are retained separately.
The pre-existing shutdown/retained-process limitation described below remains;
one additional probe executable was retained to release the canonical link path.

## Findings and changes

- `World::initWater()` put loaded water to sleep without checking support or
  gradients. It now evaluates existing water once, then lets settled bodies sleep.
- Pairwise integer flow stopped permanently at staircases such as
  `10,9,8,...,1,0`. Horizontal relaxation now equalizes connected surfaces, retaining
  finite volume and stable integer remainders. Expansion visits only existing wet
  cells and their immediate dry shoreline, advancing into new terrain on later ticks.
- Gravity could move only one tenth of a cell per tick, leaving most water free
  to spread sideways before falling. Gravity now gets a full cell's transfer
  budget first. A spillway receives the last shallow remainder so it can drain.
- A breach now wakes the connected wet interior of a sleeping body. Withdrawals,
  removed plants, mined walls, removed walls and floors wake adjacent water;
  mining also explicitly queues the changed tile for rendering.
- The water pass rendered rectangular sprite bounds as filled surfaces. Those
  bounds overlap in the isometric map. It now projects actual adjoining diamond
  faces with matching world coordinates, corner heights, depth and wave sampling.
- Fluid depth and flow shading are interpolated across shared vertices. Interior
  side faces and excessive bed transparency no longer outline every water tile.
- Loose items and construction previews no longer suppress an entire water
  surface. The renderer receives physical wall/floor topology in two render-only
  flag bits, preserving the existing GPU record size and saved tile format.

This remains a discrete voxel-volume simulation with ten surface units per cell
and the existing saved pressure overflow. It conserves volume during ordinary
flow; aquifers, drains and explicit withdrawals remain intentional sources/sinks.
It is not a momentum-based fluid solver. Simulation remains in the fixed game
tick; rendering does not change gameplay state.

## Validation

| Check | Result |
| --- | --- |
| MSVC `Ingnomia` build, link and Qt runtime staging | Passed |
| Registered `water_flow_tests` CTest target | Passed |
| Long uneven basin, breach extension, gravity, shallow drainage, pressure/floor containment, deterministic ordering, bounded snapshot and irregular stacked-basin conservation regressions | Passed |
| Production `World::mineWall()` opens a sleeping reservoir | Pass: breach level 9 on the next water tick; mass 640 |
| Reservoir fills the far end of an eight-cell channel | Pass: far level 7; mass remains 640; levels differ by at most one stored unit |
| Withdrawal wakes neighboring water | Passed; mass changes only by the requested two units |
| Production `World::removeFloor()` creates a gravity drain | Passed; lower cell fills to 10; total remains 638 |
| `World::initWater()` after redistribution | Passed; total remains 638 |
| Production framebuffer inspection | Passed: contained/breached/settled body and all four camera rotations |
| Focused water and main-window wiring CTest | 2/2 passed |
| Entire currently configured CTest suite | 6/8 passed; two unrelated existing UI tests fail |

The two failures are `ui_foundation_registry` (old registry counts and
`suspend_job` expectation) and `ui_foundation_action_validation`. Their source,
implementations and up-to-date binaries were unchanged by this repair. They were
not adjusted to make the water work appear to pass the entire suite.

The runtime fixture used a copy of `Donkeyprophecy/autosave` with an isolated
`INGNOMIA_DATA_FOLDER`. It cleared external water in memory, created an enclosed
8×8 reservoir and channel, and invoked the production World operations while the
game was paused. These are automated gameplay-operation checks, not mouse-driven
gnome-job completion or a disk save/reload round trip. Real user saves were not
modified. The 80-step fixture settling check took 1 ms in the recorded run; that
includes sleeping steps and is not a whole-world fluid performance benchmark.

The game reached its normal shutdown log, but windowless protected child
processes remained after these probes. Attempts to terminate those recorded test
trees were denied for the children. Clean process teardown is therefore not
claimed; this pre-existing executable-lock behavior remains outside the water
repair. Two earlier probe executables were retained under diagnostic names in
the same build directory to allow relinking the canonical executable.

The build's missing temporary Steam stub/OpenAL import libraries were recreated
inside the retained build directory and the local cache paths updated. The Qt
deployment helper now skips identical DLLs on CMake 3.26+, with its original
behavior retained for older CMake versions, so mapped identical DLLs do not fail
subsequent staging.

## Reproduce

From a Visual Studio developer shell in this checkout:

```powershell
cmake --build build-wave8-root-msvc-link-priority2 --target Ingnomia water_flow_tests -j 4
ctest --test-dir build-wave8-root-msvc-link-priority2 -R water_flow_tests --output-on-failure
```

`tests/water/runtime_probe.h` is an explicit opt-in production probe. Use a copied
save in an isolated data folder, a world at least 66×66×95, and a fresh output
directory. Normal game launches do not enable it.

```powershell
$env:INGNOMIA_DATA_FOLDER = '<isolated profile>'
$env:INGNOMIA_AUTOMATE_LOAD_PATH = '<copied save slot>'
$env:INGNOMIA_WATER_PROBE = '<fresh output directory>'
$env:INGNOMIA_AUTOMATE_TRACE_PATH = "$env:INGNOMIA_WATER_PROBE\automation.txt"
& .\build-wave8-root-msvc-link-priority2\Ingnomia.exe
```

The probe writes `result.txt` (seven PASS checks), six framebuffer captures and
an automation trace. Clear these environment variables before normal play.

## Artifacts

- Executable: `build-wave8-root-msvc-link-priority2/Ingnomia.exe`
- Executable SHA-256: `BAECFC240C44E711DAEB346C81D0B0B714C24FD241C4954D0AB253017C0D4E92`
- Final runtime results: [result.txt](../.verification/water-20260910/runtime-final/result.txt)
- [Contained reservoir](../.verification/water-20260910/runtime-final/01-contained.png)
- [Opened breach](../.verification/water-20260910/runtime-final/02-breach.png)
- [Connected settled body](../.verification/water-20260910/runtime-final/03-settled.png)
- [Rotation 1](../.verification/water-20260910/runtime-final/04-rotation1.png),
  [rotation 2](../.verification/water-20260910/runtime-final/05-rotation2.png),
  [rotation 3](../.verification/water-20260910/runtime-final/06-rotation3.png)
