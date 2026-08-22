# Test and validation report

This report records the bounded evidence available for the RmlUi migration at the
upstream baseline `4f99266c0f95faa847ca0db2af18cf59aff07f4b`.

## Passing focused gates

- UI foundation: 5/5 tests in the explicit MSVC build (`build-ui-foundation-final`).
- Accessibility/navigation: 3/3 tests in `build-ui-accessibility-final`; the accessibility and released-literal verifiers pass.
- Management 6A, 6B, and 6C focused suites: 3/3 each in their final isolated builds.
- Shell, HUD, inspector, management 6A, management 6B, and management 6C static RML contracts pass.
- All 16 repository `verify-*.cmake` contracts pass after the cleanup.
- Design-system verifier passes, including token/style/template checks.
- Native RmlUi/Qt/OpenGL spikes pass their lifecycle and GL-state checks; the retained asset spike exits 0 with a 1200x675 framebuffer and Lato loaded.
- Management 6A/6B/6C and developer-UI proof logs record 100 lifecycle cycles, listener teardown, typed callbacks, and zero GL warnings where the proof gate requires them.

## Dependency and configure boundary

The normal application was configured with Qt 6.8.3, MSVC/Ninja, the
repository-published Steamworks SDK 1.64 archive, OpenAL 1.25.1, and verified
offline FreeType/RmlUi source trees at the pinned revisions. CMake completed and
the `Ingnomia` target built and linked successfully. Qt deployment plus
`steam_api64.dll` and `OpenAL32.dll` were staged beside the executable.
The downloaded archive hashes are Steamworks `55C525B61DE0FC820099B84EBD2CBD3890B9378DD3D12909C0F33D74EA05243C`
and OpenAL `BC6A1E07BA1E5FF5ACCE874F085CC0093004A6605272C04BB6222327E87A81B0`.

## Visual evidence

Retained spike captures include the shell/HUD/inspector/management surfaces and
the Lato asset proof. The 6C lower-section capture retains a documented clipped
cyan footer artifact for later polish; it is not presented as a clean final
gallery frame. The full executable also launched from the deployed output and
remained alive for an 8-second bounded startup smoke check; it was then stopped
by its exact process ID. This is startup evidence, not a clean shutdown or
whole-game gameplay proof.

## Remaining acceptance work

The active-source Noesis/XAML audit reaches zero outside historical migration
records and the recoverable quarantine. The full configure/build gate now
passes with the downloaded dependencies. Remaining release gates are clean
shutdown, real gameplay mutations, save/reload, physical input, and final
package/runtime acceptance; isolated spikes and bounded startup do not prove
those behaviors.
