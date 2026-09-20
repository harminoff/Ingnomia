# Underground excavation placement correction

## Reproduced cause

The preview and the job used the same world position, but the picker inverted
the hidden **floor** of a solid underground tile. The visible top of that tile
is 16 game pixels higher. Consequently, a floor-shaped stair outline could sit
under the pointer while the actual excavated cube appeared one diagonal tile
behind it. Adjusting job X/Y coordinates would only conceal this disagreement.

The sprite atlas provides the actual anchors: terrain sprites receive 16 rows
of padding; floor diamonds are centred on atlas row 40 and wall-top diamonds
on row 24. The vertex projection `(64 - row) - 12` therefore gives heights
12 and 28. The previous floor constant, 10.4, was the quad's bounding-box
centre, not the visible diamond's centre.

## Change

- Pick a solid wall's visible top before testing the floor at each ray depth.
- Keep lowered-wall mode and open-floor picking on the floor plane.
- For downward excavation, keep the entry-surface diamond and the stairs/ramp
  item preview together. Lift the item visually so its top meets the diamond;
  keep its actual component position and job depth unchanged. Dig hole has no
  item and retains the diamond alone. Do not draw the old empty wire cube.
- Keep Selection, JobManager, and the worker's job position authoritative;
  do not add a compensating tile offset when committing.

## Evidence

An opt-in probe generates an isolated world, submits mouse coordinates through
AggregatorSelection, clicks through Selection, then executes the actual
CanWork task using a test worker. Captures come from the production OpenGL
framebuffer. No user saves are used or changed.

At 3x zoom, underground stairs reproduce and correct the reported mismatch:

| Version | Pointer | Selected/job tile | Visible excavated surface centre |
| --- | --- | --- | --- |
| Before | 648,339 | 50,48,95 | 648,291 |
| After | 648,339 | 51,49,95 | 648,339 |

The change in tile coordinates is the result of picking the visible cube, not
an offset applied to the job. The after capture shows the opening directly
inside the upper outline diamond.

Evidence folders under `.verification/`:

- `placement-20260913-baseline-debug`: reproduced misplaced underground stairs.
- `placement-20260913-fixed-debug`: corrected underground stairs, real task
  succeeded, with matching pointer and projected target surface.
- `placement-20260913-floor-direct`: ordinary floor stairs still align and complete.
- `placement-20260913-mine-direct`: Mine aligns and removes the selected wall.
- `placement-20260913-hole-direct`: Dig hole aligns and removes the selected
  floor and wall below.
- `placement-20260913-stairs-final`: final rebuilt executable, normal launch
  without the debugger, underground stairs aligned and completed; exit code 0.

Each includes `result.txt` and preview/job/completed framebuffer captures.
The initial before/after comparison was run under the debugger; floor, Mine,
hole and the final underground stairs check also ran directly. These are
scripted runtime checks, not physical mouse acceptance. Hidden-window probe
attempts crashed before capture; the renderer is initialized on exposure, so
use the verified normal-launch path rather than a hidden window.

Original placement-fix executable (before the preview follow-up):
`build-wave8-root-msvc-link-priority2/Ingnomia.exe`, built
2026-09-13 at 17:11:44 local time. SHA-256:
`CFC5415889496523983667A17D660F664645D1F4488F003602F4DCDE0ECC4899`.

`ui_foundation_placement_math` passes with both surface anchors, four camera
rotations, multiple depths/zooms, an explicit diagonal-offset regression,
and samples immediately before/after a tile edge (not just at tile centres).

## Repeat

Build `Ingnomia` and `ui_foundation_placement_math` in the canonical
`build-wave8-root-msvc-link-priority2` directory. Run the math check with:

```powershell
ctest --test-dir build-wave8-root-msvc-link-priority2 -R placement_math --output-on-failure
```

For the runtime probe, set `INGNOMIA_PLACEMENT_PROBE` to a fresh absolute
output directory and `INGNOMIA_DATA_FOLDER` to an isolated profile directory,
then launch the built game normally. Optional `INGNOMIA_PLACEMENT_ACTION`
values are `DigStairsDown` (default), `DigRampDown`, `Mine`, and `DigHole`.
`INGNOMIA_PLACEMENT_ROTATION=0..3` selects the item orientation.
`INGNOMIA_PLACEMENT_SURFACE=floor` selects an open-floor fixture; otherwise
the fixture is solid underground terrain. The probe exits after capturing
the completed task. Check for `PASS alignment`, a matching `JOB` position,
`EXECUTE success=1`, and `COMPLETE` in `result.txt`.

## Top-square preview follow-up

After the placement correction was confirmed, the requested preview was
simplified to the top square only. This is presentation-only: the picker,
selected position, job offsets and excavation tasks are unchanged.

`SelectionWallTop` previously referenced artwork containing full cube sides.
Its sprite definition now reuses `SelectionFloorTop` with a `0 -16` image
offset. Downward-excavation previews emit a single marker at the selected
entry tile instead of the root cube plus buried stairs/ramp components.

The runtime probe also checks that the emitted marker is at the selected
position and that its actual opaque sprite pixels occupy only the top-face
rows. This guards against misleading asset names hiding cube geometry.

Follow-up captures are in `.verification/placement-top-only-solid` and
`.verification/placement-top-only-floor`. Both passed the single-marker,
sprite-pixel, targeting and completed-excavation checks. The two initial test
runs needed cleanup after shutdown; their test observer now holds only a weak
reference so it cannot keep the test worker alive past the game.

Follow-up executable built 2026-09-13 at 17:27:55 local time, SHA-256:
`C2E98F4881AE0C1955D160B214C17CDD8645541039FD07FC21F19744BCEC6FD4`.
The final rerun in `.verification/placement-top-only-final` passed all checks
and exited normally with code 0; the placement math test also passes.

## Item-preview restoration

The top-only pass hid the stairs themselves. The final requested presentation
keeps the flat outline AND the item. `SelectionData::previewYOffset` is an
unscaled, render-only vertical lift, applied before the camera transform.
The ghost still carries its real component position below the entry tile.
For a component one level down, the lift is 20 pixels on a wall top and 4 on
an open floor; the item's top face then coincides with the existing marker.
Rotation, validation tint, ray picking, job coordinates and worker task
offsets remain unchanged. Other selection sprites have zero lift by default.

`ui_foundation_placement_math` passes, including lift checks across depths and
zoom levels. Runtime captures in `.verification/placement-item-preview-solid`
and `.verification/placement-item-preview-floor-rotated` show the item beneath
the flat outline without an empty gap. Both report two preview sprites,
matching marker/item top heights, correct rotation, matching pointer/job
positions, and successful real stair construction. The rotated ramp preview
also passed in `.verification/placement-item-preview-ramp`; that solid fixture
is ineligible for a ramp job, so it is not evidence of ramp completion.
These captured test processes required cleanup after their shutdown sequence;
the evidence above concerns rendering and placement, not clean-exit acceptance.

Executable built 2026-09-13 at 17:34:05 local time, SHA-256:
`B6349C693A97152D4C60CFF89B1B0C8EB038CE4ED92CCE9B98EE17F5A2567621`.
