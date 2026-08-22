# Ingnomia game-development MCP server

`ingnomia_mcp_server.py` is a dependency-free MCP server for the local Windows development loop. It uses stdio and keeps all file/process operations inside this checkout and the selected build directory.

It exposes:

- `project_info`, `build_game` — selected build/worktree state and CMake builds.
- `run_game`, `game_process`, `stop_game` — exact executable launch and native process/window inspection.
- `run_ui_checks`, `run_all` — static contracts, whitespace checks, build, and optional scenario orchestration.
- `capture_inspector`, `capture_ui_fixture`, `run_scenario` — saved-game automation plus deterministic UI fixtures, including independent creature windows and build/mine/settings probes.
- `capture_screenshot` — native window capture through Windows PrintWindow.
- `read_trace`, `read_game_log`, `crash_report` — bounded diagnostics, error filtering, exit status, and local dump discovery.
- `performance_snapshot` — sampled CPU, memory, handles, threads, and responsiveness. Frame/FPS metrics remain explicitly marked as not instrumented by the current game build.
- `image_compare` — PNG pixel comparison and optional diff output using Windows System.Drawing.
- `renderdoc_capture` — RenderDoc CLI detection and best-effort captured launch. It reports unavailable cleanly when RenderDoc is not installed.
- `save_fixture` — snapshot, hash, and recoverable restore of checkout-local save fixtures.
- `world_query` — read-only queries against an explicit JSON world snapshot; it never invents live tile/entity state.

## Run directly

From the `Ingnomia` checkout:

```powershell
python .\tools\mcp\ingnomia_mcp_server.py
```

The server writes only JSON-RPC messages to stdout. Diagnostics go to stderr so an MCP client can use the process as a normal stdio server.

## Generic stdio registration

MCP clients generally accept a server entry equivalent to:

```json
{
  "mcpServers": {
    "ingnomia-game-dev": {
      "command": "python",
      "args": ["C:/Programming/Repos/Ingnomia2/Ingnomia/tools/mcp/ingnomia_mcp_server.py"]
    }
  }
}
```

The default build is `build-wave8-root-msvc-link-priority2`. A different checkout is supported by placing a copy of the server under that checkout; `INGNOMIA_VCVARS_PATH` can select a different `vcvars64.bat` location. The server intentionally rejects paths outside the checkout.

## Typical development loop

1. Call `project_info` to confirm the executable and dirty worktree.
2. Call `run_ui_checks` for fast contract checks.
3. Call `build_game`.
4. Call `run_game` with `wait_seconds` long enough for the native window to appear.
5. Call `game_process` to verify the real window handle/title/responding state.
6. For the inspector, call `capture_inspector` with a creature tile id and optionally `second_tile_id`; the PNG and trace paths are returned.
7. For layout work that should not depend on a saved tile, call `capture_ui_fixture` with `fixture: "creature_profile"`; it opens the synthetic gnome, activates `Full profile`, and returns a real framebuffer PNG plus trace.
8. Call `stop_game` with the PID returned by `run_game` when the diagnostic session is complete.

For repeatable evidence, `run_all` runs the UI contracts and build first, then optionally launches a named scenario. `save_fixture` uses `tools/mcp/fixtures` and moves replaced content into timestamped backups instead of deleting it.

`run_game` will not replace a running matching executable unless `replace_existing: true` is explicitly supplied. `capture_inspector` leaves the game visible after capture so the result can be inspected interactively.

## UI capture matrix

`capture_ui_matrix` launches each requested surface in isolation, captures a real framebuffer PNG, records an automation trace, and stops the probe before moving to the next surface. Omit `surfaces` for the complete supported matrix, or pass a short list such as `['build_menu', 'mine_menu', 'creature_profile']`. Outputs are written under `mcp-ui-matrix` by default.

## Smoke test

The protocol-level smoke test starts the server as a child process and calls the MCP handshake, tool discovery, project inspection, UI checks, build, and process inspection:

```powershell
python .\tools\mcp\smoke_test.py
```
