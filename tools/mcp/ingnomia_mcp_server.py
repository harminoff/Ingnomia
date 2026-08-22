#!/usr/bin/env python3
"""Small, dependency-free MCP server for the local Ingnomia development loop.

The server speaks MCP over stdio.  It deliberately limits file/process actions
to this checkout and the selected build directory so it can be registered with
an MCP client without becoming a general-purpose shell bridge.
"""

from __future__ import annotations

import json
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from typing import Any


SERVER_NAME = "ingnomia-game-dev"
SERVER_VERSION = "0.3.0"
DEFAULT_CONFIGURATION = "RelWithDebInfo"
DEFAULT_BUILD_DIRECTORY = "build-wave8-root-msvc-link-priority2"
DEFAULT_VCVARS_CANDIDATES = (
    Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"),
    Path(r"C:\Program Files\Microsoft Visual Studio\17\Community\VC\Auxiliary\Build\vcvars64.bat"),
)


def _project_root() -> Path:
    # tools/mcp/ingnomia_mcp_server.py -> Ingnomia/
    return Path(__file__).resolve().parents[2]


PROJECT_ROOT = _project_root()
LAUNCHED_PIDS: set[int] = set()
LAUNCHED_PROCESSES: dict[int, subprocess.Popen[Any]] = {}


TOOLS: list[dict[str, Any]] = [
    {
        "name": "project_info",
        "description": "Report the Ingnomia checkout, selected build, executable, and git worktree state.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
    {
        "name": "build_game",
        "description": "Build the Ingnomia executable with the existing CMake build directory.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "build_dir": {"type": "string", "description": "Build directory relative to the checkout."},
                "configuration": {"type": "string", "description": "CMake configuration, normally RelWithDebInfo."},
                "target": {"type": "string", "description": "CMake target, normally Ingnomia."},
                "timeout_seconds": {"type": "integer", "minimum": 30, "maximum": 1800},
            },
            "additionalProperties": False,
        },
    },
    {
        "name": "run_game",
        "description": "Launch the exact built Ingnomia executable and report its process/window state.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "build_dir": {"type": "string"},
                "args": {"type": "array", "items": {"type": "string"}},
                "env": {"type": "object", "additionalProperties": {"type": "string"}, "description": "Only INGNOMIA_* overrides are accepted."},
                "wait_seconds": {"type": "number", "minimum": 0, "maximum": 60},
                "replace_existing": {"type": "boolean", "description": "Stop matching existing game processes before launch."},
            },
            "additionalProperties": False,
        },
    },
    {
        "name": "game_process",
        "description": "Inspect visible Ingnomia processes whose executable path matches the selected build.",
        "inputSchema": {"type": "object", "properties": {"build_dir": {"type": "string"}}, "additionalProperties": False},
    },
    {
        "name": "stop_game",
        "description": "Stop one matching Ingnomia process by PID after verifying its executable path.",
        "inputSchema": {"type": "object", "properties": {"pid": {"type": "integer", "minimum": 1}, "build_dir": {"type": "string"}}, "required": ["pid"], "additionalProperties": False},
    },
    {
        "name": "run_ui_checks",
        "description": "Run the repository's static inspector contracts and git whitespace check.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
    {
        "name": "capture_inspector",
        "description": "Launch the saved-game inspector automation and produce a PNG with one or two independent creature windows.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "tile_id": {"type": "string", "description": "Creature tile id used by the existing automation seam."},
                "second_tile_id": {"type": "string", "description": "Optional second creature tile id."},
                "build_dir": {"type": "string"},
                "capture_path": {"type": "string", "description": "PNG path relative to the build directory or checkout."},
                "trace_path": {"type": "string", "description": "Automation trace path relative to the build directory or checkout."},
                "load_path": {"type": "string", "description": "Optional explicit save path; otherwise the last saved game is loaded."},
                "view_level": {"type": "integer"},
                "wait_seconds": {"type": "number", "minimum": 1, "maximum": 90},
                "replace_existing": {"type": "boolean"},
            },
            "required": ["tile_id"],
            "additionalProperties": False,
        },
    },
    {
        "name": "capture_ui_fixture",
        "description": "Launch a deterministic synthetic UI fixture, open its full creature profile, and return the PNG and automation trace.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "fixture": {"type": "string", "enum": ["creature_profile"], "description": "Synthetic UI state to render."},
                "build_dir": {"type": "string"},
                "capture_path": {"type": "string", "description": "PNG path relative to the build directory or checkout."},
                "trace_path": {"type": "string", "description": "Automation trace path relative to the build directory or checkout."},
                "wait_seconds": {"type": "number", "minimum": 12, "maximum": 90},
                "replace_existing": {"type": "boolean"},
            },
            "required": ["fixture"],
            "additionalProperties": False,
        },
    },
    {
        "name": "capture_ui_matrix",
        "description": "Capture a repeatable matrix of reachable Ingnomia UI surfaces as separate framebuffer PNGs and traces.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "surfaces": {"type": "array", "items": {"type": "string", "enum": [
                    "main_menu", "load_game", "setup_world", "setup_settlement", "setup_terrain", "setup_review", "settings",
                    "hud", "pause_menu", "build_menu", "mine_menu", "agriculture_menu", "designations_menu", "jobs_menu",
                    "kingdom_panel", "inventory", "military", "population", "missions", "event_prompt", "creature_profile",
                ]}, "description": "Surfaces to capture; omit to capture the complete supported matrix."},
                "build_dir": {"type": "string"},
                "output_dir": {"type": "string", "description": "Directory relative to the checkout or build directory for PNGs and traces."},
                "build_first": {"type": "boolean", "description": "Build the selected executable before launching the matrix."},
                "wait_seconds": {"type": "number", "minimum": 1, "maximum": 90, "description": "Minimum wait per surface; slower surfaces retain their own safety floor."},
                "replace_existing": {"type": "boolean"},
            },
            "additionalProperties": False,
        },
    },
    {
        "name": "read_trace",
        "description": "Read the tail of a game or automation trace inside the checkout.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": {"type": "string", "description": "Relative path from the checkout or an absolute path inside it."},
                "tail_lines": {"type": "integer", "minimum": 1, "maximum": 1000},
            },
            "required": ["path"],
            "additionalProperties": False,
        },
    },
    {
        "name": "capture_screenshot",
        "description": "Capture the selected build's native game window to a PNG using Windows PrintWindow.",
        "inputSchema": {
            "type": "object",
            "properties": {"build_dir": {"type": "string"}, "pid": {"type": "integer"}, "path": {"type": "string"}},
            "additionalProperties": False,
        },
    },
    {
        "name": "read_game_log",
        "description": "Find the newest checkout-local game log and return recent or pattern-matched lines.",
        "inputSchema": {
            "type": "object",
            "properties": {"build_dir": {"type": "string"}, "path": {"type": "string"}, "pattern": {"type": "string"}, "tail_lines": {"type": "integer", "minimum": 1, "maximum": 1000}},
            "additionalProperties": False,
        },
    },
    {
        "name": "run_scenario",
        "description": "Launch one repeatable game scenario using the existing INGNOMIA_AUTOMATE_* seams.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "scenario": {"type": "string", "enum": ["load", "inspector", "inspector_multi", "build_menu", "mine_menu", "settings", "custom"]},
                "tile_id": {"type": "string"},
                "second_tile_id": {"type": "string"},
                "build_dir": {"type": "string"},
                "capture_path": {"type": "string"},
                "trace_path": {"type": "string"},
                "env": {"type": "object", "additionalProperties": {"type": "string"}},
                "wait_seconds": {"type": "number", "minimum": 1, "maximum": 90},
                "replace_existing": {"type": "boolean"},
            },
            "required": ["scenario"],
            "additionalProperties": False,
        },
    },
    {
        "name": "performance_snapshot",
        "description": "Sample CPU time, working set, handles, threads, and responsiveness for the matching game process.",
        "inputSchema": {
            "type": "object",
            "properties": {"build_dir": {"type": "string"}, "pid": {"type": "integer"}, "sample_seconds": {"type": "number", "minimum": 0.1, "maximum": 10}},
            "additionalProperties": False,
        },
    },
    {
        "name": "crash_report",
        "description": "Summarize tracked process exit state, recent error log lines, traces, and local dump artifacts.",
        "inputSchema": {
            "type": "object",
            "properties": {"build_dir": {"type": "string"}, "pid": {"type": "integer"}, "log_path": {"type": "string"}, "trace_paths": {"type": "array", "items": {"type": "string"}}, "tail_lines": {"type": "integer", "minimum": 1, "maximum": 300}},
            "additionalProperties": False,
        },
    },
    {
        "name": "image_compare",
        "description": "Compare two PNGs with Windows System.Drawing and optionally write a visual diff image.",
        "inputSchema": {
            "type": "object",
            "properties": {"baseline": {"type": "string"}, "candidate": {"type": "string"}, "diff": {"type": "string"}},
            "required": ["baseline", "candidate"],
            "additionalProperties": False,
        },
    },
    {
        "name": "renderdoc_capture",
        "description": "Probe for RenderDoc and, when renderdoccmd is installed, attempt a best-effort captured launch.",
        "inputSchema": {
            "type": "object",
            "properties": {"build_dir": {"type": "string"}, "args": {"type": "array", "items": {"type": "string"}}, "output": {"type": "string"}, "mode": {"type": "string", "enum": ["probe", "launch"]}, "wait_seconds": {"type": "number", "minimum": 1, "maximum": 180}},
            "additionalProperties": False,
        },
    },
    {
        "name": "save_fixture",
        "description": "Snapshot, hash, or restore a checkout-local save fixture with recoverable backups on replacement.",
        "inputSchema": {
            "type": "object",
            "properties": {"action": {"type": "string", "enum": ["snapshot", "hash", "restore"]}, "name": {"type": "string"}, "source": {"type": "string"}, "target": {"type": "string"}, "overwrite": {"type": "boolean"}},
            "required": ["action", "name"],
            "additionalProperties": False,
        },
    },
    {
        "name": "world_query",
        "description": "Query an explicit JSON world snapshot for tiles or entities without fabricating live game state.",
        "inputSchema": {
            "type": "object",
            "properties": {"build_dir": {"type": "string"}, "path": {"type": "string"}, "query": {"type": "string", "enum": ["summary", "tile", "entity", "search"]}, "x": {"type": "integer"}, "y": {"type": "integer"}, "z": {"type": "integer"}, "id": {"type": "string"}, "term": {"type": "string"}},
            "required": ["query"],
            "additionalProperties": False,
        },
    },
    {
        "name": "run_all",
        "description": "Run UI checks, build the game, and optionally launch a named scenario as one evidence-producing workflow.",
        "inputSchema": {
            "type": "object",
            "properties": {"build_dir": {"type": "string"}, "scenario": {"type": "string"}, "tile_id": {"type": "string"}, "second_tile_id": {"type": "string"}, "wait_seconds": {"type": "number", "minimum": 1, "maximum": 90}, "replace_existing": {"type": "boolean"}},
            "additionalProperties": False,
        },
    },
]


def _json_text(value: Any) -> str:
    return json.dumps(value, indent=2, ensure_ascii=False, default=str)


def _ok(value: Any) -> dict[str, Any]:
    return {"content": [{"type": "text", "text": _json_text(value)}], "isError": False}


def _error(message: str, details: Any | None = None) -> dict[str, Any]:
    payload: dict[str, Any] = {"error": message}
    if details is not None:
        payload["details"] = details
    return {"content": [{"type": "text", "text": _json_text(payload)}], "isError": True}


def _inside(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def _safe_project_path(value: str | None, default: Path, *, must_exist: bool = False) -> Path:
    candidate = Path(value) if value else default
    if not candidate.is_absolute():
        candidate = PROJECT_ROOT / candidate
    candidate = candidate.resolve()
    if not _inside(candidate, PROJECT_ROOT):
        raise ValueError(f"path must stay inside {PROJECT_ROOT}")
    if must_exist and not candidate.exists():
        raise FileNotFoundError(candidate)
    return candidate


def _build_directory(arguments: dict[str, Any]) -> Path:
    value = arguments.get("build_dir", DEFAULT_BUILD_DIRECTORY)
    return _safe_project_path(str(value), PROJECT_ROOT / DEFAULT_BUILD_DIRECTORY, must_exist=True)


def _find_executable(build_dir: Path) -> Path:
    candidates = [build_dir / "Ingnomia.exe", build_dir / DEFAULT_CONFIGURATION / "Ingnomia.exe"]
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    raise FileNotFoundError(f"Ingnomia.exe was not found in {build_dir}")


def _run(command: list[str], *, cwd: Path, timeout: int = 120, env: dict[str, str] | None = None) -> dict[str, Any]:
    started = time.monotonic()
    try:
        completed = subprocess.run(
            command,
            cwd=str(cwd),
            env=env,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
            check=False,
        )
        output = completed.stdout or ""
        if len(output) > 16000:
            output = "...<trimmed>...\n" + output[-16000:]
        return {
            "command": command,
            "cwd": str(cwd),
            "exit_code": completed.returncode,
            "elapsed_seconds": round(time.monotonic() - started, 2),
            "output": output,
        }
    except subprocess.TimeoutExpired as exc:
        output = (exc.stdout or "") if isinstance(exc.stdout, str) else ""
        return {
            "command": command,
            "cwd": str(cwd),
            "exit_code": None,
            "timed_out": True,
            "elapsed_seconds": round(time.monotonic() - started, 2),
            "output": output[-16000:],
        }


def _vcvars_path() -> Path | None:
    configured = os.environ.get("INGNOMIA_VCVARS_PATH")
    candidates = [Path(configured)] if configured else []
    candidates.extend(DEFAULT_VCVARS_CANDIDATES)
    return next((candidate for candidate in candidates if candidate and candidate.is_file()), None)


def _short_windows_path(path: Path) -> str:
    """Return an 8.3 path for cmd.exe batch invocation when available."""
    if os.name != "nt":
        return str(path)
    import ctypes

    buffer = ctypes.create_unicode_buffer(32768)
    length = ctypes.windll.kernel32.GetShortPathNameW(str(path), buffer, len(buffer))
    return buffer.value if length else str(path)


def _build_command(build_dir: Path, configuration: str, target: str) -> list[str]:
    cmake_args = ["cmake", "--build", str(build_dir), "--config", configuration, "--target", target]
    vcvars = _vcvars_path()
    if os.name == "nt" and vcvars:
        # Passing a nested quoted batch path through subprocess' Windows
        # argument quoting is parsed differently by cmd.exe than an interactive
        # shell command. The short path keeps the `call` operand unambiguous.
        command = "call {} && cmake --build {} --config {} --target {}".format(
            _short_windows_path(vcvars),
            _short_windows_path(build_dir),
            configuration,
            target,
        )
        # Do not add cmd.exe's /s quote-stripping mode here. The command contains
        # quoted paths with spaces, and /s changes how the nested vcvars call is
        # parsed when subprocess passes /c as a distinct argument.
        return ["cmd.exe", "/d", "/c", command]
    return cmake_args


def _powershell_quote(value: str) -> str:
    return "'" + value.replace("'", "''") + "'"


def _processes(build_dir: Path) -> list[dict[str, Any]]:
    executable = _find_executable(build_dir)
    if os.name != "nt":
        return [{"pid": pid, "executable": str(executable)} for pid in sorted(LAUNCHED_PIDS) if _pid_alive(pid)]

    script = """
$expected = [IO.Path]::GetFullPath({expected})
Get-Process -Name Ingnomia -ErrorAction SilentlyContinue |
  ForEach-Object {{
    $path = $null
    try {{ $path = $_.Path }} catch {{}}
    if ($path -and ([IO.Path]::GetFullPath($path) -ieq $expected)) {{
      [PSCustomObject]@{{
        pid = $_.Id
        title = $_.MainWindowTitle
        window_handle = [Int64]$_.MainWindowHandle
        responding = $_.Responding
        executable = $path
      }}
    }}
  }} | ConvertTo-Json -Compress
""".format(expected=_powershell_quote(str(executable)))
    result = subprocess.run(
        ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script],
        cwd=str(PROJECT_ROOT),
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if result.returncode != 0 or not result.stdout.strip():
        return []
    try:
        decoded = json.loads(result.stdout)
    except json.JSONDecodeError:
        return []
    if isinstance(decoded, dict):
        return [decoded]
    return decoded if isinstance(decoded, list) else []


def _pid_alive(pid: int) -> bool:
    if os.name == "nt":
        result = subprocess.run(["tasklist", "/FI", f"PID eq {pid}", "/NH"], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, encoding="utf-8", errors="replace", check=False)
        return result.returncode == 0 and str(pid) in result.stdout
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


def _stop_matching(pid: int, build_dir: Path) -> dict[str, Any]:
    matches = {int(item["pid"]): item for item in _processes(build_dir) if str(item.get("pid", "")).isdigit()}
    if pid not in matches:
        return {"stopped": False, "pid": pid, "reason": "PID is not an Ingnomia process from the selected build."}
    if os.name == "nt":
        completed = subprocess.run(["taskkill.exe", "/PID", str(pid), "/T"], cwd=str(PROJECT_ROOT), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace", check=False)
        return {"stopped": completed.returncode == 0, "pid": pid, "output": completed.stdout[-4000:]}
    os.kill(pid, 15)
    return {"stopped": True, "pid": pid}


def _git_status() -> str:
    result = _run(["git", "status", "--short", "--branch"], cwd=PROJECT_ROOT, timeout=30)
    return result.get("output", "").strip()


def _tool_project_info(_: dict[str, Any]) -> dict[str, Any]:
    build_dir = _safe_project_path(DEFAULT_BUILD_DIRECTORY, PROJECT_ROOT / DEFAULT_BUILD_DIRECTORY, must_exist=True)
    try:
        executable = _find_executable(build_dir)
        exe_info: dict[str, Any] = {"path": str(executable), "size": executable.stat().st_size, "last_write_time": executable.stat().st_mtime}
    except FileNotFoundError as exc:
        exe_info = {"error": str(exc)}
    return _ok({"project_root": str(PROJECT_ROOT), "default_build_dir": str(build_dir), "executable": exe_info, "git_status": _git_status()})


def _tool_build_game(arguments: dict[str, Any]) -> dict[str, Any]:
    try:
        build_dir = _build_directory(arguments)
        configuration = str(arguments.get("configuration", DEFAULT_CONFIGURATION))
        target = str(arguments.get("target", "Ingnomia"))
        timeout = max(30, min(1800, int(arguments.get("timeout_seconds", 120))))
        result = _run(_build_command(build_dir, configuration, target), cwd=PROJECT_ROOT, timeout=timeout)
        result["executable"] = str(_find_executable(build_dir)) if target == "Ingnomia" and any((build_dir / candidate).is_file() for candidate in ("Ingnomia.exe", f"{configuration}/Ingnomia.exe")) else None
        return _ok(result) if result.get("exit_code") == 0 else _error("build failed", result)
    except (ValueError, FileNotFoundError, OSError) as exc:
        return _error("build could not start", str(exc))


def _validated_overrides(overrides: Any) -> dict[str, str]:
    validated: dict[str, str] = {}
    if overrides is None:
        return validated
    if not isinstance(overrides, dict):
        raise ValueError("env must be an object")
    for key, value in overrides.items():
        if not isinstance(key, str) or not key.startswith("INGNOMIA_"):
            raise ValueError("only INGNOMIA_* environment overrides are allowed")
        if not isinstance(value, str):
            raise ValueError(f"environment value for {key} must be a string")
        validated[key] = value
    return validated


def _validated_environment(overrides: Any) -> dict[str, str]:
    environment = os.environ.copy()
    environment.update(_validated_overrides(overrides))
    return environment


def _launch(executable: Path, arguments: list[str], environment: dict[str, str]) -> subprocess.Popen[Any]:
    creation_flags = getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0) if os.name == "nt" else 0
    return subprocess.Popen(
        [str(executable), *arguments],
        cwd=str(executable.parent),
        env=environment,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        creationflags=creation_flags,
        close_fds=True,
    )


def _tool_run_game(arguments: dict[str, Any]) -> dict[str, Any]:
    try:
        build_dir = _build_directory(arguments)
        executable = _find_executable(build_dir)
        existing = _processes(build_dir)
        if existing and arguments.get("replace_existing", False):
            for item in existing:
                _stop_matching(int(item["pid"]), build_dir)
            time.sleep(0.5)
            existing = _processes(build_dir)
        if existing:
            return _error("matching Ingnomia is already running; use replace_existing or stop_game", {"processes": existing})

        raw_args = arguments.get("args", [])
        if not isinstance(raw_args, list) or not all(isinstance(item, str) for item in raw_args):
            raise ValueError("args must be an array of strings")
        environment = _validated_environment(arguments.get("env"))
        process = _launch(executable, raw_args, environment)
        LAUNCHED_PIDS.add(process.pid)
        LAUNCHED_PROCESSES[process.pid] = process
        wait_seconds = max(0.0, min(60.0, float(arguments.get("wait_seconds", 2.0))))
        if wait_seconds:
            time.sleep(wait_seconds)
        return _ok({"pid": process.pid, "executable": str(executable), "processes": _processes(build_dir), "running": process.poll() is None})
    except (ValueError, FileNotFoundError, OSError) as exc:
        return _error("game could not launch", str(exc))


def _tool_game_process(arguments: dict[str, Any]) -> dict[str, Any]:
    try:
        return _ok({"processes": _processes(_build_directory(arguments))})
    except (ValueError, FileNotFoundError, OSError) as exc:
        return _error("could not inspect game process", str(exc))


def _tool_stop_game(arguments: dict[str, Any]) -> dict[str, Any]:
    try:
        pid = int(arguments["pid"])
        result = _stop_matching(pid, _build_directory(arguments))
        if result.get("stopped"):
            LAUNCHED_PIDS.discard(pid)
            LAUNCHED_PROCESSES.pop(pid, None)
        return _ok(result) if result.get("stopped") else _error("game was not stopped", result)
    except (KeyError, ValueError, FileNotFoundError, OSError) as exc:
        return _error("game could not be stopped", str(exc))


def _tool_run_ui_checks(_: dict[str, Any]) -> dict[str, Any]:
    commands = [
        ["cmake", "-P", "tests/ui-inspector/verify-inspector-rml.cmake"],
        ["cmake", "-P", "tests/ui-inspector/verify-inspector-integration.cmake"],
        ["git", "diff", "--check"],
    ]
    results = [_run(command, cwd=PROJECT_ROOT, timeout=120) for command in commands]
    failed = [result for result in results if result.get("exit_code") != 0]
    return _ok({"passed": not failed, "checks": results}) if not failed else _error("one or more UI checks failed", {"checks": results})


def _tool_capture_inspector(arguments: dict[str, Any]) -> dict[str, Any]:
    try:
        tile_id = str(arguments["tile_id"])
        if not tile_id:
            raise ValueError("tile_id cannot be empty")
        build_dir = _build_directory(arguments)
        capture_path = _safe_project_path(arguments.get("capture_path"), build_dir / "mcp-inspector.png")
        trace_path = _safe_project_path(arguments.get("trace_path"), build_dir / "mcp-inspector-automation.log")
        environment: dict[str, str] = {
            "INGNOMIA_AUTOMATE_LOAD": "1",
            "INGNOMIA_AUTOMATE_CENTER_VIEW": "1",
            "INGNOMIA_AUTOMATE_INSPECTOR_TILE_ID": tile_id,
            "INGNOMIA_AUTOMATE_INSPECTOR_ELEMENT": "tile_open_creature",
            "INGNOMIA_AUTOMATE_INSPECTOR_GL_CAPTURE_PATH": str(capture_path),
            "INGNOMIA_AUTOMATE_TRACE_PATH": str(trace_path),
        }
        if arguments.get("second_tile_id"):
            environment["INGNOMIA_AUTOMATE_INSPECTOR_SECOND_TILE_ID"] = str(arguments["second_tile_id"])
        if arguments.get("load_path"):
            environment.pop("INGNOMIA_AUTOMATE_LOAD", None)
            environment["INGNOMIA_AUTOMATE_LOAD_PATH"] = str(_safe_project_path(str(arguments["load_path"]), PROJECT_ROOT, must_exist=True))
        if arguments.get("view_level") is not None:
            environment["INGNOMIA_AUTOMATE_VIEW_LEVEL"] = str(int(arguments["view_level"]))

        launch_result = _tool_run_game({
            "build_dir": str(build_dir),
            "env": environment,
            "wait_seconds": arguments.get("wait_seconds", 20),
            "replace_existing": arguments.get("replace_existing", False),
        })
        if launch_result.get("isError"):
            return launch_result
        payload = json.loads(launch_result["content"][0]["text"])
        payload["capture"] = {"path": str(capture_path), "exists": capture_path.is_file(), "size": capture_path.stat().st_size if capture_path.is_file() else 0}
        payload["trace"] = {"path": str(trace_path), "exists": trace_path.is_file(), "size": trace_path.stat().st_size if trace_path.is_file() else 0}
        return _ok(payload)
    except (KeyError, ValueError, FileNotFoundError, OSError) as exc:
        return _error("inspector capture could not start", str(exc))


def _tool_capture_ui_fixture(arguments: dict[str, Any]) -> dict[str, Any]:
    try:
        fixture = str(arguments["fixture"])
        if fixture != "creature_profile":
            raise ValueError("unsupported UI fixture")
        build_dir = _build_directory(arguments)
        capture_path = _safe_project_path(arguments.get("capture_path"), build_dir / "mcp-ui-fixture.png")
        trace_path = _safe_project_path(arguments.get("trace_path"), build_dir / "mcp-ui-fixture.trace.txt")
        environment = {
            "INGNOMIA_AUTOMATE_LOAD": "1",
            "INGNOMIA_AUTOMATE_UI_FIXTURE": fixture,
            "INGNOMIA_AUTOMATE_UI_FIXTURE_CAPTURE_PATH": str(capture_path),
            "INGNOMIA_AUTOMATE_TRACE_PATH": str(trace_path),
        }
        launch_result = _tool_run_game({
            "build_dir": str(build_dir),
            "env": environment,
            "wait_seconds": max(12.0, min(90.0, float(arguments.get("wait_seconds", 16.0)))),
            "replace_existing": arguments.get("replace_existing", False),
        })
        if launch_result.get("isError"):
            return launch_result
        payload = json.loads(launch_result["content"][0]["text"])
        payload["fixture"] = fixture
        payload["capture"] = {"path": str(capture_path), "exists": capture_path.is_file(), "size": capture_path.stat().st_size if capture_path.is_file() else 0}
        payload["trace"] = {"path": str(trace_path), "exists": trace_path.is_file(), "size": trace_path.stat().st_size if trace_path.is_file() else 0}
        return _ok(payload)
    except (KeyError, ValueError, FileNotFoundError, OSError) as exc:
        return _error("UI fixture capture could not start", str(exc))


UI_SURFACE_PLANS: dict[str, dict[str, Any]] = {
    "main_menu": {"wait": 4, "env": {"capture_kind": "startup"}},
    "load_game": {"wait": 5, "env": {"capture_kind": "shell", "shell_element": "shell-load"}},
    "setup_world": {"wait": 5, "env": {"capture_kind": "shell", "shell_element": "shell-new-setup"}},
    "setup_settlement": {"wait": 8, "env": {"capture_kind": "shell", "shell_element": "shell-new-setup", "shell_second_element": "new-tab-settlement", "shell_second_delay": "4500"}},
    "setup_terrain": {"wait": 8, "env": {"capture_kind": "shell", "shell_element": "shell-new-setup", "shell_second_element": "new-tab-terrain", "shell_second_delay": "4500"}},
    "setup_review": {"wait": 8, "env": {"capture_kind": "shell", "shell_element": "shell-new-setup", "shell_second_element": "new-tab-review", "shell_second_delay": "4500"}},
    "settings": {"wait": 5, "env": {"capture_kind": "shell", "shell_element": "shell-settings"}},
    "hud": {"wait": 10, "env": {"capture_kind": "loaded_world"}},
    "pause_menu": {"wait": 10, "env": {"capture_kind": "hud", "hud_element": "hud_pause"}},
    "build_menu": {"wait": 11, "env": {"capture_kind": "hud", "hud_element": "hud_tool_build"}},
    "mine_menu": {"wait": 11, "env": {"capture_kind": "hud", "hud_element": "hud_tool_mine"}},
    "agriculture_menu": {"wait": 11, "env": {"capture_kind": "hud", "hud_element": "hud_tool_agriculture"}},
    "designations_menu": {"wait": 11, "env": {"capture_kind": "hud", "hud_element": "hud_tool_designations"}},
    "jobs_menu": {"wait": 11, "env": {"capture_kind": "hud", "hud_element": "hud_tool_jobs"}},
    "kingdom_panel": {"wait": 11, "env": {"capture_kind": "hud", "hud_element": "hud_open_kingdom"}},
    "inventory": {"wait": 11, "env": {"capture_kind": "hud", "hud_element": "hud_open_inventory"}},
    "military": {"wait": 11, "env": {"capture_kind": "hud", "hud_element": "hud_open_military"}},
    "population": {"wait": 11, "env": {"capture_kind": "hud", "hud_element": "hud_open_population"}},
    "missions": {"wait": 11, "env": {"capture_kind": "hud", "hud_element": "hud_open_diplomacy"}},
    "event_prompt": {"wait": 15, "env": {"capture_kind": "event_prompt"}},
    "creature_profile": {"wait": 16, "env": {"capture_kind": "fixture"}},
}


def _matrix_capture_metadata(capture_path: Path, trace_path: Path) -> dict[str, Any]:
    return {
        "capture": {"path": str(capture_path), "exists": capture_path.is_file(), "size": capture_path.stat().st_size if capture_path.is_file() else 0},
        "trace": {"path": str(trace_path), "exists": trace_path.is_file(), "size": trace_path.stat().st_size if trace_path.is_file() else 0},
    }


def _stop_matrix_process(payload: dict[str, Any], build_dir: Path) -> dict[str, Any]:
    pid = int(payload.get("pid", 0) or 0)
    if pid <= 0:
        return {"stopped": False, "reason": "surface launch did not return a PID"}
    stopped = _stop_matching(pid, build_dir)
    if stopped.get("stopped"):
        LAUNCHED_PIDS.discard(pid)
        LAUNCHED_PROCESSES.pop(pid, None)
        return stopped
    owned = LAUNCHED_PROCESSES.get(pid)
    if owned is not None and owned.poll() is None:
        try:
            owned.terminate()
            owned.wait(timeout=5)
            LAUNCHED_PIDS.discard(pid)
            LAUNCHED_PROCESSES.pop(pid, None)
            return {"stopped": True, "pid": pid, "method": "owned_process"}
        except OSError:
            pass
    return stopped


def _tool_capture_ui_matrix(arguments: dict[str, Any]) -> dict[str, Any]:
    try:
        build_dir = _build_directory(arguments)
        requested = arguments.get("surfaces")
        if requested is not None and (not isinstance(requested, list) or not all(isinstance(item, str) for item in requested)):
            raise ValueError("surfaces must be an array of strings")
        surfaces = list(UI_SURFACE_PLANS) if requested is None else [str(item) for item in requested]
        if not surfaces:
            raise ValueError("surfaces cannot be empty")
        unknown = [surface for surface in surfaces if surface not in UI_SURFACE_PLANS]
        if unknown:
            raise ValueError(f"unknown UI surface(s): {', '.join(unknown)}")

        output_dir = _safe_project_path(arguments.get("output_dir"), build_dir / "mcp-ui-matrix")
        output_dir.mkdir(parents=True, exist_ok=True)
        if arguments.get("build_first"):
            build = _tool_build_game({"build_dir": str(build_dir), "timeout_seconds": 1800})
            if build.get("isError"):
                return build

        requested_wait = float(arguments.get("wait_seconds", 0))
        results: list[dict[str, Any]] = []
        for surface in surfaces:
            capture_path = output_dir / f"{surface}.png"
            trace_path = output_dir / f"{surface}.trace.txt"
            _ensure_parent(capture_path)
            plan = UI_SURFACE_PLANS[surface]
            plan_env = plan["env"]
            wait_seconds = max(float(plan["wait"]), requested_wait, 1.0)
            if plan_env["capture_kind"] == "fixture":
                launched = _tool_capture_ui_fixture({
                    "fixture": "creature_profile",
                    "build_dir": str(build_dir),
                    "capture_path": str(capture_path),
                    "trace_path": str(trace_path),
                    "wait_seconds": wait_seconds,
                    "replace_existing": arguments.get("replace_existing", False),
                })
            else:
                environment: dict[str, str] = {"INGNOMIA_AUTOMATE_TRACE_PATH": str(trace_path)}
                capture_kind = plan_env["capture_kind"]
                if capture_kind == "startup":
                    environment.update({"INGNOMIA_UI_CAPTURE": str(capture_path), "INGNOMIA_UI_CAPTURE_FRAME": "1"})
                elif capture_kind == "loaded_world":
                    environment.update({"INGNOMIA_AUTOMATE_LOAD": "1", "INGNOMIA_AUTOMATE_CENTER_VIEW": "1", "INGNOMIA_UI_CAPTURE": str(capture_path), "INGNOMIA_UI_CAPTURE_AFTER_LOAD": "1", "INGNOMIA_UI_CAPTURE_DELAY_MS": "1500"})
                elif capture_kind == "shell":
                    environment.update({"INGNOMIA_AUTOMATE_SHELL_ELEMENT": plan_env["shell_element"], "INGNOMIA_AUTOMATE_SHELL_ELEMENT_DELAY_MS": plan_env.get("shell_delay", "1500"), "INGNOMIA_AUTOMATE_SHELL_CAPTURE_PATH": str(capture_path)})
                    if plan_env.get("shell_second_element"):
                        environment["INGNOMIA_AUTOMATE_SHELL_SECOND_ELEMENT"] = plan_env["shell_second_element"]
                        environment["INGNOMIA_AUTOMATE_SHELL_SECOND_ELEMENT_DELAY_MS"] = plan_env.get("shell_second_delay", "4500")
                elif capture_kind == "hud":
                    environment.update({"INGNOMIA_AUTOMATE_LOAD": "1", "INGNOMIA_AUTOMATE_CENTER_VIEW": "1", "INGNOMIA_AUTOMATE_HUD_ELEMENT": plan_env["hud_element"], "INGNOMIA_AUTOMATE_HUD_CAPTURE_PATH": str(capture_path)})
                elif capture_kind == "event_prompt":
                    environment.update({"INGNOMIA_AUTOMATE_LOAD": "1", "INGNOMIA_AUTOMATE_CENTER_VIEW": "1", "INGNOMIA_AUTOMATE_EVENT_PROMPT": "1", "INGNOMIA_AUTOMATE_EVENT_PROMPT_KIND": "ack", "INGNOMIA_AUTOMATE_EVENT_PROMPT_CAPTURE_PATH": str(capture_path)})
                launched = _tool_run_game({
                    "build_dir": str(build_dir),
                    "env": environment,
                    "wait_seconds": min(90.0, wait_seconds),
                    "replace_existing": arguments.get("replace_existing", False),
                })

            if launched.get("isError"):
                results.append({"surface": surface, "passed": False, "error": launched.get("content", [{}])[0].get("text", "launch failed")})
                continue
            payload = json.loads(launched["content"][0]["text"])
            stopped = _stop_matrix_process(payload, build_dir)
            metadata = _matrix_capture_metadata(capture_path, trace_path)
            trace_text = trace_path.read_text(encoding="utf-8", errors="replace") if trace_path.is_file() else ""
            action_ok = "activated=false" not in trace_text and "capture armed=true" in trace_text or plan_env["capture_kind"] in ("startup", "loaded_world") and "ui_capture_requested" in trace_text
            passed = metadata["capture"]["exists"] and metadata["trace"]["exists"] and action_ok
            results.append({"surface": surface, "passed": passed, "action_ok": action_ok, **metadata, "launch": {"pid": payload.get("pid"), "executable": payload.get("executable")}, "stop": stopped})

        response = {"build_dir": str(build_dir), "output_dir": str(output_dir), "surfaces": results, "passed": all(item["passed"] for item in results)}
        return _ok(response) if response["passed"] else _error("one or more UI surfaces could not be captured", response)
    except (KeyError, TypeError, ValueError, FileNotFoundError, OSError) as exc:
        return _error("UI matrix capture could not start", str(exc))


def _tool_read_trace(arguments: dict[str, Any]) -> dict[str, Any]:
    try:
        path = _safe_project_path(str(arguments["path"]), PROJECT_ROOT, must_exist=True)
        if not path.is_file():
            raise ValueError("trace path is not a file")
        tail_lines = max(1, min(1000, int(arguments.get("tail_lines", 120))))
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        return _ok({"path": str(path), "line_count": len(lines), "tail": lines[-tail_lines:]})
    except (KeyError, ValueError, FileNotFoundError, OSError) as exc:
        return _error("trace could not be read", str(exc))


def _ensure_parent(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def _powershell(command: str, *, timeout: int = 120) -> dict[str, Any]:
    return _run(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", command], cwd=PROJECT_ROOT, timeout=timeout)


def _process_for_window(build_dir: Path, requested_pid: Any = None) -> dict[str, Any]:
    matches = _processes(build_dir)
    if requested_pid is not None:
        matches = [item for item in matches if int(item.get("pid", -1)) == int(requested_pid)]
    if not matches:
        raise RuntimeError("no matching Ingnomia process is running")
    with_window = [item for item in matches if int(item.get("window_handle", 0) or 0) > 0]
    process = (with_window or matches)[0]
    if int(process.get("window_handle", 0) or 0) <= 0:
        raise RuntimeError(f"matching process {process.get('pid')} has no native window handle yet")
    return process


def _tool_capture_screenshot(arguments: dict[str, Any]) -> dict[str, Any]:
    try:
        build_dir = _build_directory(arguments)
        process = _process_for_window(build_dir, arguments.get("pid"))
        output = _safe_project_path(arguments.get("path"), build_dir / "mcp-window.png")
        _ensure_parent(output)
        hwnd = int(process["window_handle"])
        capture_script = r"""
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class IngnomiaMcpWindowCapture {
    [StructLayout(LayoutKind.Sequential)]
    private struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint flags);

    public static bool Save(IntPtr hWnd, string filePath) {
        RECT rect;
        if (!GetWindowRect(hWnd, out rect)) return false;
        int width = Math.Max(1, rect.Right - rect.Left);
        int height = Math.Max(1, rect.Bottom - rect.Top);
        using (var bitmap = new Bitmap(width, height, PixelFormat.Format32bppArgb))
        using (var graphics = Graphics.FromImage(bitmap)) {
            IntPtr hdc = graphics.GetHdc();
            bool printed = PrintWindow(hWnd, hdc, 2);
            graphics.ReleaseHdc(hdc);
            if (!printed) graphics.CopyFromScreen(rect.Left, rect.Top, 0, 0, bitmap.Size);
            bitmap.Save(filePath, ImageFormat.Png);
        }
        return true;
    }
}
'@
if (-not [IngnomiaMcpWindowCapture]::Save([IntPtr]::new(@@HWND@@), @@PATH@@)) { exit 2 }
""".replace("@@HWND@@", str(hwnd)).replace("@@PATH@@", _powershell_quote(str(output)))
        result = _powershell(capture_script, timeout=60)
        payload = {"pid": process["pid"], "window_handle": hwnd, "path": str(output), "exists": output.is_file(), "size": output.stat().st_size if output.is_file() else 0, "output": result["output"]}
        return _ok(payload) if result.get("exit_code") == 0 and output.is_file() else _error("window capture failed", payload)
    except (ValueError, FileNotFoundError, RuntimeError, OSError) as exc:
        return _error("screenshot could not be captured", str(exc))


def _log_candidates(build_dir: Path) -> list[Path]:
    candidates = [build_dir / "log.txt", PROJECT_ROOT / "log.txt"]
    if build_dir.is_dir():
        candidates.extend(build_dir.glob("*.log"))
    return sorted({path.resolve() for path in candidates if path.is_file() and _inside(path.resolve(), PROJECT_ROOT)}, key=lambda path: path.stat().st_mtime, reverse=True)


def _read_log_data(path: Path, pattern: str | None, tail_lines: int) -> dict[str, Any]:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    if pattern:
        try:
            selected = [line for line in lines if re.search(pattern, line, re.IGNORECASE)]
        except re.error as exc:
            raise ValueError(f"invalid log pattern: {exc}") from exc
    else:
        selected = lines[-tail_lines:]
    return {"path": str(path), "line_count": len(lines), "matched_count": len(selected), "lines": selected[-tail_lines:]}


def _tool_read_game_log(arguments: dict[str, Any]) -> dict[str, Any]:
    try:
        build_dir = _build_directory(arguments)
        if arguments.get("path"):
            path = _safe_project_path(str(arguments["path"]), build_dir / "log.txt", must_exist=True)
        else:
            candidates = _log_candidates(build_dir)
            if not candidates:
                raise FileNotFoundError("no checkout-local log.txt or *.log file found")
            path = candidates[0]
        tail_lines = max(1, min(1000, int(arguments.get("tail_lines", 120))))
        return _ok(_read_log_data(path, arguments.get("pattern"), tail_lines))
    except (KeyError, ValueError, FileNotFoundError, OSError) as exc:
        return _error("game log could not be read", str(exc))


def _unwrap_result(result: dict[str, Any]) -> dict[str, Any]:
    if result.get("isError"):
        return {"is_error": True, "error": result.get("content", [{}])[0].get("text", "")}
    try:
        return {"is_error": False, "value": json.loads(result["content"][0]["text"])}
    except (KeyError, TypeError, json.JSONDecodeError):
        return {"is_error": False, "value": result}


SCENARIO_ENV: dict[str, dict[str, str]] = {
    "load": {"INGNOMIA_AUTOMATE_LOAD": "1", "INGNOMIA_AUTOMATE_CENTER_VIEW": "1"},
    "inspector": {"INGNOMIA_AUTOMATE_LOAD": "1", "INGNOMIA_AUTOMATE_CENTER_VIEW": "1", "INGNOMIA_AUTOMATE_INSPECTOR_ELEMENT": "tile_open_creature"},
    "inspector_multi": {"INGNOMIA_AUTOMATE_LOAD": "1", "INGNOMIA_AUTOMATE_CENTER_VIEW": "1", "INGNOMIA_AUTOMATE_INSPECTOR_ELEMENT": "tile_open_creature"},
    "build_menu": {"INGNOMIA_AUTOMATE_LOAD": "1", "INGNOMIA_AUTOMATE_CENTER_VIEW": "1", "INGNOMIA_AUTOMATE_BUILD_MENU": "1"},
    "mine_menu": {"INGNOMIA_AUTOMATE_LOAD": "1", "INGNOMIA_AUTOMATE_CENTER_VIEW": "1", "INGNOMIA_AUTOMATE_MINE_MENU": "1"},
    "settings": {"INGNOMIA_AUTOMATE_LOAD": "1", "INGNOMIA_AUTOMATE_SETTINGS_UI": "1"},
    "custom": {},
}


def _tool_run_scenario(arguments: dict[str, Any]) -> dict[str, Any]:
    try:
        scenario = str(arguments["scenario"])
        if scenario not in SCENARIO_ENV:
            raise ValueError(f"unknown scenario: {scenario}")
        build_dir = _build_directory(arguments)
        environment = dict(SCENARIO_ENV[scenario])
        if scenario in ("inspector", "inspector_multi"):
            if not arguments.get("tile_id"):
                raise ValueError("tile_id is required for inspector scenarios")
            environment["INGNOMIA_AUTOMATE_INSPECTOR_TILE_ID"] = str(arguments["tile_id"])
            if scenario == "inspector_multi":
                if not arguments.get("second_tile_id"):
                    raise ValueError("second_tile_id is required for inspector_multi")
                environment["INGNOMIA_AUTOMATE_INSPECTOR_SECOND_TILE_ID"] = str(arguments["second_tile_id"])
        if arguments.get("trace_path"):
            environment["INGNOMIA_AUTOMATE_TRACE_PATH"] = str(_safe_project_path(str(arguments["trace_path"]), build_dir / "mcp-scenario.log"))
        capture_path: Path | None = None
        if arguments.get("capture_path"):
            capture_path = _safe_project_path(str(arguments["capture_path"]), build_dir / "mcp-scenario.png")
            _ensure_parent(capture_path)
            if scenario in ("inspector", "inspector_multi"):
                environment["INGNOMIA_AUTOMATE_INSPECTOR_GL_CAPTURE_PATH"] = str(capture_path)
            elif scenario == "settings":
                environment["INGNOMIA_AUTOMATE_SETTINGS_UI_CAPTURE_PATH"] = str(capture_path)
            elif scenario == "load":
                environment["INGNOMIA_UI_CAPTURE"] = str(capture_path)
                environment["INGNOMIA_UI_CAPTURE_AFTER_LOAD"] = "1"
                environment["INGNOMIA_UI_CAPTURE_DELAY_MS"] = "1500"
        environment.update(_validated_overrides(arguments.get("env")))
        launch = _tool_run_game({"build_dir": str(build_dir), "env": environment, "wait_seconds": arguments.get("wait_seconds", 12), "replace_existing": arguments.get("replace_existing", False)})
        unwrapped = _unwrap_result(launch)
        if unwrapped["is_error"]:
            return launch
        value = unwrapped["value"]
        if capture_path and scenario not in ("inspector", "inspector_multi", "settings", "load"):
            screenshot = _tool_capture_screenshot({"build_dir": str(build_dir), "path": str(capture_path)})
            value["screenshot"] = _unwrap_result(screenshot)
        if capture_path:
            value["capture"] = {"path": str(capture_path), "exists": capture_path.is_file(), "size": capture_path.stat().st_size if capture_path.is_file() else 0}
        return _ok({"scenario": scenario, **value})
    except (KeyError, ValueError, FileNotFoundError, RuntimeError, OSError) as exc:
        return _error("scenario could not run", str(exc))


def _process_metrics(pid: int) -> dict[str, Any] | None:
    script = """
$process = Get-Process -Id {pid} -ErrorAction SilentlyContinue
if ($process) {{
  [PSCustomObject]@{{
    pid = $process.Id
    cpu_seconds = $process.TotalProcessorTime.TotalSeconds
    working_set_bytes = $process.WorkingSet64
    private_bytes = $process.PrivateMemorySize64
    handles = $process.Handles
    threads = $process.Threads.Count
    responding = $process.Responding
    title = $process.MainWindowTitle
  }} | ConvertTo-Json -Compress
}}
""".format(pid=int(pid))
    result = _powershell(script, timeout=30)
    if result.get("exit_code") != 0 or not result.get("output", "").strip():
        return None
    try:
        decoded = json.loads(result["output"])
        return decoded if isinstance(decoded, dict) else None
    except json.JSONDecodeError:
        return None


def _tool_performance_snapshot(arguments: dict[str, Any]) -> dict[str, Any]:
    try:
        build_dir = _build_directory(arguments)
        matches = _processes(build_dir)
        if arguments.get("pid") is not None:
            matches = [item for item in matches if int(item.get("pid", -1)) == int(arguments["pid"])]
        if not matches:
            raise RuntimeError("no matching Ingnomia process is running")
        sample_seconds = max(0.1, min(10.0, float(arguments.get("sample_seconds", 1.0))))
        samples: list[dict[str, Any]] = []
        for item in matches:
            pid = int(item["pid"])
            first = _process_metrics(pid)
            time.sleep(sample_seconds)
            second = _process_metrics(pid)
            if not first or not second:
                samples.append({"pid": pid, "alive": False, "window": item})
                continue
            cpu_delta = float(second.get("cpu_seconds", 0)) - float(first.get("cpu_seconds", 0))
            samples.append({"pid": pid, "alive": True, "window": item, "cpu_percent_estimate": round(max(0.0, cpu_delta) / sample_seconds / max(1, os.cpu_count() or 1) * 100.0, 2), "working_set_mb": round(int(second.get("working_set_bytes", 0)) / 1048576.0, 2), "private_memory_mb": round(int(second.get("private_bytes", 0)) / 1048576.0, 2), "handles": second.get("handles"), "threads": second.get("threads"), "sample_seconds": sample_seconds})
        return _ok({"processes": samples, "logical_processors": os.cpu_count() or 1, "frame_metrics": "not instrumented by the current game build"})
    except (ValueError, FileNotFoundError, RuntimeError, OSError) as exc:
        return _error("performance snapshot could not be collected", str(exc))


def _tool_crash_report(arguments: dict[str, Any]) -> dict[str, Any]:
    try:
        build_dir = _build_directory(arguments)
        matches = _processes(build_dir)
        pid = int(arguments["pid"]) if arguments.get("pid") is not None else None
        if pid is not None:
            matches = [item for item in matches if int(item.get("pid", -1)) == pid]
        tracked = LAUNCHED_PROCESSES.get(pid) if pid is not None else None
        exit_code = tracked.poll() if tracked else None
        log_path = _safe_project_path(str(arguments["log_path"]), build_dir / "log.txt", must_exist=True) if arguments.get("log_path") else (_log_candidates(build_dir)[0] if _log_candidates(build_dir) else None)
        tail_lines = max(1, min(300, int(arguments.get("tail_lines", 80))))
        errors = _read_log_data(log_path, r"fatal|critical|exception|assert|crash|error", tail_lines) if log_path else None
        traces: list[dict[str, Any]] = []
        for raw_path in arguments.get("trace_paths", []) or []:
            path = _safe_project_path(str(raw_path), build_dir / "trace.log", must_exist=True)
            traces.append(_read_log_data(path, None, tail_lines))
        dumps = []
        for pattern in ("*.dmp", "*.mdmp"):
            dumps.extend({path.resolve() for path in build_dir.rglob(pattern) if path.is_file()})
        dumps = sorted(dumps, key=lambda path: path.stat().st_mtime, reverse=True)[:20]
        report = {"pid": pid, "live_processes": matches, "tracked_exit_code": exit_code, "abnormal_exit": exit_code is not None and exit_code != 0, "log": errors, "traces": traces, "dump_artifacts": [{"path": str(path), "size": path.stat().st_size, "last_write_time": path.stat().st_mtime} for path in dumps]}
        return _ok(report)
    except (KeyError, ValueError, FileNotFoundError, RuntimeError, OSError) as exc:
        return _error("crash report could not be collected", str(exc))


def _tool_image_compare(arguments: dict[str, Any]) -> dict[str, Any]:
    try:
        baseline = _safe_project_path(str(arguments["baseline"]), PROJECT_ROOT, must_exist=True)
        candidate = _safe_project_path(str(arguments["candidate"]), PROJECT_ROOT, must_exist=True)
        if not baseline.is_file() or not candidate.is_file():
            raise ValueError("baseline and candidate must be files")
        diff = _safe_project_path(str(arguments["diff"]), PROJECT_ROOT / "build-wave8-root-msvc-link-priority2" / "mcp-image-diff.png") if arguments.get("diff") else None
        if diff:
            _ensure_parent(diff)
        if hashlib.sha256(baseline.read_bytes()).digest() == hashlib.sha256(candidate.read_bytes()).digest():
            return _ok({"identical_bytes": True, "different_pixels": 0, "diff": str(diff) if diff else None, "baseline": str(baseline), "candidate": str(candidate)})

        diff_path = _powershell_quote(str(diff)) if diff else "$null"
        compare_script = r"""
Add-Type -AssemblyName System.Drawing
$a = [Drawing.Bitmap]::new(@@BASELINE@@)
$b = [Drawing.Bitmap]::new(@@CANDIDATE@@)
$width = [Math]::Min($a.Width, $b.Width)
$height = [Math]::Min($a.Height, $b.Height)
$different = 0L
$maxDelta = 0
$diffBitmap = $null
if (@@DIFF@@ -ne $null) { $diffBitmap = [Drawing.Bitmap]::new($width, $height, [Drawing.Imaging.PixelFormat]::Format32bppArgb) }
for ($y = 0; $y -lt $height; $y++) {
  for ($x = 0; $x -lt $width; $x++) {
    $ca = $a.GetPixel($x, $y)
    $cb = $b.GetPixel($x, $y)
    $delta = [Math]::Max([Math]::Abs($ca.R - $cb.R), [Math]::Max([Math]::Abs($ca.G - $cb.G), [Math]::Max([Math]::Abs($ca.B - $cb.B), [Math]::Abs($ca.A - $cb.A))))
    if ($delta -gt 0) {
      $different++
      if ($delta -gt $maxDelta) { $maxDelta = $delta }
      if ($diffBitmap) { $diffBitmap.SetPixel($x, $y, [Drawing.Color]::FromArgb(255, 255, 0, 0)) }
    } elseif ($diffBitmap) { $diffBitmap.SetPixel($x, $y, [Drawing.Color]::FromArgb(255, 0, 0, 0)) }
  }
}
if ($diffBitmap) { $diffBitmap.Save(@@DIFF@@, [Drawing.Imaging.ImageFormat]::Png); $diffBitmap.Dispose() }
$a.Dispose(); $b.Dispose()
[PSCustomObject]@{ width_a = $a.Width; height_a = $a.Height; width_b = $b.Width; height_b = $b.Height; compared_width = $width; compared_height = $height; different_pixels = $different; max_channel_delta = $maxDelta; diff = @@DIFF@@ } | ConvertTo-Json -Compress
""".replace("@@BASELINE@@", _powershell_quote(str(baseline))).replace("@@CANDIDATE@@", _powershell_quote(str(candidate))).replace("@@DIFF@@", diff_path)
        result = _powershell(compare_script, timeout=180)
        if result.get("exit_code") != 0:
            return _error("image comparison failed", result)
        try:
            comparison = json.loads(result["output"].strip().splitlines()[-1])
        except (json.JSONDecodeError, IndexError):
            comparison = {"output": result["output"]}
        comparison.update({"identical_bytes": False, "baseline": str(baseline), "candidate": str(candidate)})
        return _ok(comparison)
    except (KeyError, ValueError, FileNotFoundError, OSError) as exc:
        return _error("images could not be compared", str(exc))


def _renderdoc_command() -> str | None:
    for name in ("renderdoccmd.exe", "renderdoccmd"):
        found = shutil.which(name)
        if found:
            return found
    candidates = [
        Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "RenderDoc" / "renderdoccmd.exe",
        Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "RenderDoc" / "renderdoccmd.exe",
        Path(os.environ.get("LOCALAPPDATA", "")) / "Programs" / "RenderDoc" / "renderdoccmd.exe",
    ]
    return next((str(path) for path in candidates if path.is_file()), None)


def _tool_renderdoc_capture(arguments: dict[str, Any]) -> dict[str, Any]:
    command = _renderdoc_command()
    probe = {"renderdoccmd": command, "available": command is not None, "note": "RenderDoc CLI was not found on this machine." if command is None else "renderdoccmd detected."}
    if str(arguments.get("mode", "probe")) == "probe":
        return _ok(probe)
    if command is None:
        return _error("RenderDoc is unavailable", probe)
    try:
        build_dir = _build_directory(arguments)
        executable = _find_executable(build_dir)
        output = _safe_project_path(arguments.get("output"), build_dir / "mcp-renderdoc.rdc")
        _ensure_parent(output)
        raw_args = arguments.get("args", [])
        if not isinstance(raw_args, list) or not all(isinstance(item, str) for item in raw_args):
            raise ValueError("args must be an array of strings")
        # This is RenderDoc's documented command-line shape in current builds;
        # the result reports the exact invocation if a local version differs.
        capture_command = [command, "capture", "--capture-file", str(output), str(executable), *raw_args]
        result = _run(capture_command, cwd=build_dir, timeout=max(1, min(180, int(arguments.get("wait_seconds", 60)))))
        payload = {"probe": probe, "capture": result, "output": str(output), "exists": output.is_file(), "size": output.stat().st_size if output.is_file() else 0}
        return _ok(payload) if result.get("exit_code") == 0 and output.is_file() else _error("RenderDoc capture did not produce an .rdc file", payload)
    except (ValueError, FileNotFoundError, OSError) as exc:
        return _error("RenderDoc capture could not start", str(exc))


def _fixture_root() -> Path:
    return PROJECT_ROOT / "tools" / "mcp" / "fixtures"


def _fixture_path(name: str) -> Path:
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,80}", name):
        raise ValueError("fixture name must contain only letters, digits, dot, underscore, or hyphen")
    return _fixture_root() / name


def _hash_path(path: Path) -> str:
    digest = hashlib.sha256()
    if path.is_file():
        digest.update(path.name.encode("utf-8"))
        digest.update(path.read_bytes())
        return digest.hexdigest()
    for child in sorted(item for item in path.rglob("*") if item.is_file()):
        digest.update(str(child.relative_to(path)).replace("\\", "/").encode("utf-8"))
        digest.update(child.read_bytes())
    return digest.hexdigest()


def _backup_path(path: Path, name: str) -> Path:
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    backup = _fixture_root() / "backups" / f"{name}-{stamp}"
    _ensure_parent(backup)
    shutil.move(str(path), str(backup))
    return backup


def _copy_path(source: Path, destination: Path) -> None:
    if source.is_dir():
        shutil.copytree(source, destination)
    else:
        _ensure_parent(destination)
        shutil.copy2(source, destination)


def _tool_save_fixture(arguments: dict[str, Any]) -> dict[str, Any]:
    try:
        action = str(arguments["action"])
        name = str(arguments["name"])
        fixture = _fixture_path(name)
        fixture_root = _fixture_root()
        if action == "snapshot":
            if not arguments.get("source"):
                raise ValueError("source is required for snapshot")
            source = _safe_project_path(str(arguments["source"]), PROJECT_ROOT, must_exist=True)
            if fixture.exists():
                if not arguments.get("overwrite", False):
                    raise FileExistsError(fixture)
                backup = _backup_path(fixture, name)
            else:
                backup = None
            _copy_path(source, fixture)
            return _ok({"action": action, "fixture": str(fixture), "source": str(source), "sha256": _hash_path(fixture), "backup": str(backup) if backup else None})
        if action == "hash":
            source = _safe_project_path(str(arguments.get("source") or fixture), fixture, must_exist=True)
            return _ok({"action": action, "path": str(source), "sha256": _hash_path(source)})
        if action == "restore":
            if not fixture.exists():
                raise FileNotFoundError(fixture)
            if not arguments.get("target"):
                raise ValueError("target is required for restore")
            target = _safe_project_path(str(arguments["target"]), PROJECT_ROOT)
            if target.exists():
                if not arguments.get("overwrite", False):
                    raise FileExistsError(f"target exists: {target}; set overwrite=true for a recoverable backup")
                backup = _backup_path(target, f"{name}-target")
            else:
                backup = None
            _copy_path(fixture, target)
            return _ok({"action": action, "fixture": str(fixture), "target": str(target), "sha256": _hash_path(target), "backup": str(backup) if backup else None})
        raise ValueError(f"unknown fixture action: {action}")
    except (KeyError, ValueError, FileNotFoundError, FileExistsError, OSError) as exc:
        return _error("fixture operation failed", str(exc))


def _snapshot_candidates(build_dir: Path) -> list[Path]:
    return [build_dir / "world_snapshot.json", build_dir / "debug" / "world.json", _fixture_root() / "world_snapshot.json"]


def _items_from_snapshot(data: dict[str, Any], keys: tuple[str, ...]) -> list[dict[str, Any]]:
    for key in keys:
        raw = data.get(key)
        if isinstance(raw, list):
            return [item for item in raw if isinstance(item, dict)]
        if isinstance(raw, dict):
            return [{"id": item_id, **item} if isinstance(item, dict) else {"id": item_id, "value": item} for item_id, item in raw.items()]
    return []


def _tool_world_query(arguments: dict[str, Any]) -> dict[str, Any]:
    try:
        build_dir = _build_directory(arguments)
        if arguments.get("path"):
            snapshot_path = _safe_project_path(str(arguments["path"]), build_dir / "world_snapshot.json", must_exist=True)
        else:
            snapshot_path = next((path for path in _snapshot_candidates(build_dir) if path.is_file()), None)
        if not snapshot_path:
            raise FileNotFoundError("no world snapshot found; provide path to an explicit JSON snapshot")
        data = json.loads(snapshot_path.read_text(encoding="utf-8"))
        if not isinstance(data, dict):
            raise ValueError("world snapshot root must be a JSON object")
        query = str(arguments["query"])
        if query == "summary":
            return _ok({"path": str(snapshot_path), "keys": sorted(data.keys()), "tile_count": len(_items_from_snapshot(data, ("tiles", "tile"))), "entity_count": len(_items_from_snapshot(data, ("entities", "creatures", "animals", "monsters")))})
        if query == "search":
            term = str(arguments.get("term", "")).lower()
            if not term:
                raise ValueError("term is required for search")
            matches: list[Any] = []
            for key, value in data.items():
                if term in json.dumps({key: value}, ensure_ascii=False).lower():
                    matches.append({key: value})
                    if len(matches) >= 50:
                        break
            return _ok({"path": str(snapshot_path), "query": query, "matches": matches})
        collection = _items_from_snapshot(data, ("tiles", "tile")) if query == "tile" else _items_from_snapshot(data, ("entities", "creatures", "animals", "monsters"))
        selected: list[dict[str, Any]] = []
        for item in collection:
            position = item.get("position") if isinstance(item.get("position"), dict) else item
            if arguments.get("id") is not None and str(item.get("id", item.get("tile_id", item.get("entity_id", "")))) != str(arguments["id"]):
                continue
            if any(arguments.get(axis) is not None and int(position.get(axis, -999999)) != int(arguments[axis]) for axis in ("x", "y", "z")):
                continue
            selected.append(item)
        return _ok({"path": str(snapshot_path), "query": query, "matches": selected[:100], "match_count": len(selected)})
    except (KeyError, ValueError, FileNotFoundError, json.JSONDecodeError, OSError) as exc:
        return _error("world query failed", str(exc))


def _tool_run_all(arguments: dict[str, Any]) -> dict[str, Any]:
    steps: list[dict[str, Any]] = []
    checks = _tool_run_ui_checks({})
    steps.append({"name": "run_ui_checks", "result": _unwrap_result(checks)})
    build_args = {"build_dir": arguments.get("build_dir", DEFAULT_BUILD_DIRECTORY)}
    build = _tool_build_game(build_args)
    steps.append({"name": "build_game", "result": _unwrap_result(build)})
    if arguments.get("scenario"):
        scenario_args: dict[str, Any] = {"scenario": arguments["scenario"], "build_dir": arguments.get("build_dir", DEFAULT_BUILD_DIRECTORY), "wait_seconds": arguments.get("wait_seconds", 12), "replace_existing": arguments.get("replace_existing", False)}
        for key in ("tile_id", "second_tile_id"):
            if arguments.get(key) is not None:
                scenario_args[key] = arguments[key]
        scenario = _tool_run_scenario(scenario_args)
        steps.append({"name": "run_scenario", "result": _unwrap_result(scenario)})
    failed = any(step["result"].get("is_error") for step in steps)
    payload = {"passed": not failed, "steps": steps}
    return _ok(payload) if not failed else _error("run_all reported a failure", payload)


DISPATCH = {
    "project_info": _tool_project_info,
    "build_game": _tool_build_game,
    "run_game": _tool_run_game,
    "game_process": _tool_game_process,
    "stop_game": _tool_stop_game,
    "run_ui_checks": _tool_run_ui_checks,
    "capture_inspector": _tool_capture_inspector,
    "capture_ui_fixture": _tool_capture_ui_fixture,
    "capture_ui_matrix": _tool_capture_ui_matrix,
    "read_trace": _tool_read_trace,
    "capture_screenshot": _tool_capture_screenshot,
    "read_game_log": _tool_read_game_log,
    "run_scenario": _tool_run_scenario,
    "performance_snapshot": _tool_performance_snapshot,
    "crash_report": _tool_crash_report,
    "image_compare": _tool_image_compare,
    "renderdoc_capture": _tool_renderdoc_capture,
    "save_fixture": _tool_save_fixture,
    "world_query": _tool_world_query,
    "run_all": _tool_run_all,
}


def _read_message(stream: Any) -> dict[str, Any] | None:
    while True:
        first = stream.readline()
        if not first:
            return None
        if first.strip() == b"":
            continue
        if first.lstrip().startswith(b"{"):
            return json.loads(first.decode("utf-8"))
        if first.lower().startswith(b"content-length:"):
            length = int(first.split(b":", 1)[1].strip())
            while True:
                header = stream.readline()
                if not header or header in (b"\r\n", b"\n"):
                    break
            payload = stream.read(length)
            if len(payload) != length:
                raise EOFError("truncated MCP message")
            return json.loads(payload.decode("utf-8"))
        raise ValueError("unsupported MCP stdio framing")


def _write_message(stream: Any, message: dict[str, Any]) -> None:
    encoded = (json.dumps(message, ensure_ascii=False, separators=(",", ":")) + "\n").encode("utf-8")
    stream.write(encoded)
    stream.flush()


def _response(request_id: Any, result: Any | None = None, error: dict[str, Any] | None = None) -> dict[str, Any]:
    response: dict[str, Any] = {"jsonrpc": "2.0", "id": request_id}
    if error is not None:
        response["error"] = error
    else:
        response["result"] = result
    return response


def _handle(message: dict[str, Any]) -> dict[str, Any] | None:
    method = message.get("method")
    request_id = message.get("id")
    if method == "notifications/initialized" or method == "notifications/cancelled":
        return None
    if method == "ping":
        return _response(request_id, {})
    if method == "initialize":
        requested = message.get("params", {}).get("protocolVersion", "2024-11-05")
        return _response(request_id, {
            "protocolVersion": requested,
            "capabilities": {"tools": {"listChanged": False}},
            "serverInfo": {"name": SERVER_NAME, "version": SERVER_VERSION},
            "instructions": "Use build_game before run_game; game_process and stop_game are scoped to the selected build executable.",
        })
    if method == "tools/list":
        return _response(request_id, {"tools": TOOLS})
    if method == "tools/call":
        params = message.get("params") or {}
        name = params.get("name")
        handler = DISPATCH.get(name)
        if handler is None:
            return _response(request_id, error={"code": -32602, "message": f"unknown tool: {name}"})
        try:
            result = handler(params.get("arguments") or {})
        except Exception as exc:  # Keep the stdio server alive after one bad tool call.
            result = _error(f"tool {name} failed unexpectedly", repr(exc))
        return _response(request_id, result)
    if request_id is not None:
        return _response(request_id, error={"code": -32601, "message": f"method not found: {method}"})
    return None


def main() -> int:
    input_stream = sys.stdin.buffer
    output_stream = sys.stdout.buffer
    while True:
        try:
            message = _read_message(input_stream)
            if message is None:
                return 0
            response = _handle(message)
            if response is not None:
                _write_message(output_stream, response)
        except Exception as exc:
            print(f"ingnomia-mcp: {exc!r}", file=sys.stderr, flush=True)
            return 1


if __name__ == "__main__":
    raise SystemExit(main())
