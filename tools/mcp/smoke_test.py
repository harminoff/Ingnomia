#!/usr/bin/env python3
"""Protocol-level smoke test for the local Ingnomia MCP server."""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys


SERVER = Path(__file__).with_name("ingnomia_mcp_server.py")


def call(process: subprocess.Popen[str], request_id: int, method: str, params: dict | None = None) -> dict:
    payload = {"jsonrpc": "2.0", "id": request_id, "method": method}
    if params is not None:
        payload["params"] = params
    assert process.stdin is not None
    assert process.stdout is not None
    process.stdin.write(json.dumps(payload) + "\n")
    process.stdin.flush()
    line = process.stdout.readline()
    if not line:
        raise RuntimeError("MCP server closed stdout")
    response = json.loads(line)
    if response.get("id") != request_id:
        raise RuntimeError(f"unexpected response id: {response}")
    return response


def main() -> int:
    process = subprocess.Popen(
        [sys.executable, str(SERVER)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
    )
    try:
        initialize = call(process, 1, "initialize", {"protocolVersion": "2024-11-05", "capabilities": {}, "clientInfo": {"name": "smoke-test", "version": "1"}})
        assert initialize["result"]["serverInfo"]["name"] == "ingnomia-game-dev"

        tools = call(process, 2, "tools/list", {})
        names = {tool["name"] for tool in tools["result"]["tools"]}
        expected = {
            "project_info", "build_game", "run_game", "game_process", "stop_game", "run_ui_checks", "capture_inspector", "capture_ui_fixture", "capture_ui_matrix", "read_trace",
            "capture_screenshot", "read_game_log", "run_scenario", "performance_snapshot", "crash_report", "image_compare",
            "renderdoc_capture", "save_fixture", "world_query", "run_all",
        }
        assert expected <= names, names

        info = call(process, 3, "tools/call", {"name": "project_info", "arguments": {}})
        assert info["result"]["isError"] is False

        checks = call(process, 4, "tools/call", {"name": "run_ui_checks", "arguments": {}})
        if checks["result"].get("isError"):
            raise RuntimeError(checks["result"]["content"][0]["text"])

        build = call(process, 5, "tools/call", {"name": "build_game", "arguments": {"timeout_seconds": 600}})
        if build["result"].get("isError"):
            raise RuntimeError(build["result"]["content"][0]["text"])

        process_state = call(process, 6, "tools/call", {"name": "game_process", "arguments": {}})
        assert process_state["result"]["isError"] is False
        log = call(process, 7, "tools/call", {"name": "read_game_log", "arguments": {}})
        assert log["result"]["isError"] is False
        performance = call(process, 8, "tools/call", {"name": "performance_snapshot", "arguments": {"sample_seconds": 0.1}})
        assert performance["result"]["isError"] is False
        crash = call(process, 9, "tools/call", {"name": "crash_report", "arguments": {}})
        assert crash["result"]["isError"] is False
        image = call(process, 10, "tools/call", {"name": "image_compare", "arguments": {"baseline": "build-wave8-root-msvc-link-priority2/preview-multiple2.png", "candidate": "build-wave8-root-msvc-link-priority2/preview-multiple.png"}})
        assert image["result"]["isError"] is False
        renderdoc = call(process, 11, "tools/call", {"name": "renderdoc_capture", "arguments": {"mode": "probe"}})
        assert renderdoc["result"]["isError"] is False
        fixture = call(process, 12, "tools/call", {"name": "save_fixture", "arguments": {"action": "hash", "name": "smoke", "source": "build-wave8-root-msvc-link-priority2/Ingnomia.exe"}})
        assert fixture["result"]["isError"] is False
        world = call(process, 13, "tools/call", {"name": "world_query", "arguments": {"query": "summary"}})
        assert world["result"]["isError"] is True
        screenshot = call(process, 14, "tools/call", {"name": "capture_screenshot", "arguments": {}})
        assert "isError" in screenshot["result"]
        all_steps = call(process, 15, "tools/call", {"name": "run_all", "arguments": {}})
        assert all_steps["result"]["isError"] is False
        print(json.dumps({"passed": True, "tool_count": len(names), "game_process": process_state["result"]["content"][0]["text"], "renderdoc": renderdoc["result"]["content"][0]["text"], "run_all": all_steps["result"]["content"][0]["text"]}, indent=2))
        return 0
    finally:
        if process.stdin:
            process.stdin.close()
        process.terminate()
        process.wait(timeout=10)


if __name__ == "__main__":
    raise SystemExit(main())
