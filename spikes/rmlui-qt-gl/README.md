# Isolated Qt/OpenGL RmlUi integration spike

This standalone target exercises the architecture in `docs/ui-migration/03-rmlui-architecture.md` without changing the root game build. It is intentionally not included by the top-level `CMakeLists.txt`.

It pins RmlUi 6.2 to commit `2230d1a6e8e0848ed87a5761e2a5160b2a175ba4` and FreeType 2.13.3 to commit `42608f77f20749dd6ddc9e0536788eaad70ea4b5`, creates a Qt-owned OpenGL 4.3 core context, reuses Ingnomia's GLAD loader, compiles the tagged official GL3 renderer with a custom-loader shim, and renders a moving OpenGL background behind RmlUi.

## Configure and build

```powershell
cmake -S spikes/rmlui-qt-gl -B build-rmlui-qt-spike `
  -DQt6_DIR=C:/path/to/Qt/6.x/msvc2022_64/lib/cmake/Qt6 `
  -DCMAKE_C_COMPILER=cl `
  -DCMAKE_CXX_COMPILER=cl
cmake --build build-rmlui-qt-spike --config Debug --parallel
build-rmlui-qt-spike/ingnomia_rmlui_qt_spike.exe --self-test
```

`--self-test` renders 60 frames, writes `spike-framebuffer.bmp` next to the executable, and closes itself with process exit code 0 on success. The normal interactive launch remains required for physical input checks.

The spike stages the shared `content/rmlui` tree, including
`fonts/LatoLatin-Regular.ttf` from the pinned RmlUi 6.2 Samples/assets tree.
The binary is loaded before the document as `fonts/LatoLatin-Regular.ttf` and
uses the `LatoLatin` family. The accompanying SIL Open Font License 1.1 notice
is `fonts/notices/RmlUi-Samples-Lato-OFL-1.1.txt`; dependency notices are kept
under `notices/` and are copied into the runtime asset root as well.

For an offline build, populate verified source trees first and add both overrides:

```powershell
-DFETCHCONTENT_SOURCE_DIR_RMLUI=C:/path/to/RmlUi-2230d1a `
-DFETCHCONTENT_SOURCE_DIR_FREETYPE=C:/path/to/freetype-42608f77
```

Always configure into a fresh build directory when changing compiler families. A cached MinGW or Clang compiler is not ABI-compatible with an MSVC Qt package.

The dependency-free source-contract check can still run:

```powershell
cmake -P spikes/rmlui-qt-gl/verify-spike.cmake
```

## Runtime checks

1. Confirm the log reports RmlUi 6.2 and the actual GL 4.3 vendor/renderer.
2. Confirm the moving amber world bar remains visible and animated behind the panel.
3. Click the button and confirm its data-bound count increments.
4. Hover, click, Tab, and use arrow navigation; verify visible focus.
5. Enter ASCII and non-ASCII committed text in the input.
6. Wheel the clipped panel and verify it does not invoke world fallback.
7. Resize to 1200x675 and 1920x1080.
8. Press F9/F10 to vary UI scale; test 100%, 125%, 150%, and 200% where available.
9. Confirm the image, rounded clipping, transform, and shadow render correctly.
10. Press F6 and confirm 100 document load/unload cycles pass.
11. Press F7 to toggle the development debugger.
12. Close the window and confirm process exit code 0 and no GL-state mismatch/error logs.
