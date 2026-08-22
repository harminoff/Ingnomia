#!/usr/bin/env python3
"""Build a deterministic inventory of the current upstream Noesis/XAML UI.

The output is evidence for the migration map, not a claim that a binding or
command was exercised at runtime. Run from anywhere inside the checkout:

    python docs/ui-migration/tools/inventory_current_ui.py
    python docs/ui-migration/tools/inventory_current_ui.py --check
"""

from __future__ import annotations

import argparse
import io
import json
import re
import subprocess
import sys
import tarfile
import tempfile
from collections import Counter
from pathlib import Path


SCRIPT_PATH = Path(__file__).resolve()
CHECKOUT_ROOT = SCRIPT_PATH.parents[3]
ROOT = CHECKOUT_ROOT
BASELINE_COMMIT = "4f99266c0f95faa847ca0db2af18cf59aff07f4b"
DEFAULT_OUTPUT = CHECKOUT_ROOT / "docs" / "ui-migration" / "current-ui-inventory.json"


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def line_number(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def balanced_markup_extensions(text: str, prefix: str) -> list[tuple[int, str]]:
    """Return balanced ``{Binding ...}``-style expressions and offsets."""
    output: list[tuple[int, str]] = []
    start = 0
    while True:
        start = text.find(prefix, start)
        if start < 0:
            break
        depth = 0
        quote: str | None = None
        end = start
        while end < len(text):
            char = text[end]
            if quote:
                if char == quote:
                    quote = None
            elif char in ("'", '"'):
                quote = char
            elif char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0:
                    end += 1
                    break
            end += 1
        output.append((start, text[start:end]))
        start = max(end, start + 1)
    return output


def binding_path(raw: str) -> str:
    body = raw[len("{Binding") : -1].strip()
    match = re.search(r"(?:^|,)\s*Path\s*=\s*([^,}]+)", body)
    if match:
        return match.group(1).strip()
    if body and "=" not in body.split(",", 1)[0]:
        return body.split(",", 1)[0].strip()
    return ""


def attributes(text: str) -> list[dict[str, object]]:
    pattern = re.compile(r"(?P<name>[A-Za-z_][\w:.-]*)\s*=\s*(?P<quote>['\"])(?P<value>.*?)(?P=quote)", re.S)
    return [
        {
            "attribute": match.group("name"),
            "value": match.group("value"),
            "line": line_number(text, match.start()),
        }
        for match in pattern.finditer(text)
    ]


def xaml_inventory(path: Path) -> dict[str, object]:
    text = path.read_text(encoding="utf-8-sig", errors="replace")
    attrs = attributes(text)
    root_match = re.search(r"<(?!\?|!--)([A-Za-z_][\w:.-]*)\b", text)
    bindings = [
        {
            "path": binding_path(raw),
            "raw": raw,
            "line": line_number(text, offset),
        }
        for offset, raw in balanced_markup_extensions(text, "{Binding")
    ]
    command_attrs = {
        "Command",
        "CommandParameter",
        "Click",
        "Checked",
        "Unchecked",
        "SelectionChanged",
        "Selected",
        "Loaded",
        "MouseDown",
        "MouseUp",
        "MouseDoubleClick",
        "TextChanged",
        "ValueChanged",
    }
    visibility = [
        attr
        for attr in attrs
        if attr["attribute"] == "Visibility"
        or (attr["attribute"] == "Property" and attr["value"] == "Visibility")
    ]
    resources = sorted(
        {
            str(attr["value"])
            for attr in attrs
            if attr["attribute"] == "Source"
            and (str(attr["value"]).lower().endswith(".xaml") or "{StaticResource" in str(attr["value"]))
        }
    )
    images = sorted(
        {
            str(attr["value"])
            for attr in attrs
            if attr["attribute"] == "Source"
            and re.search(r"\.(?:png|jpg|jpeg|bmp|gif)$", str(attr["value"]), re.I)
        }
    )
    converters = sorted(
        set(re.findall(r"Converter=\{StaticResource\s+([^}\s]+)", text))
        | set(re.findall(r"<(?:\w+:)?([A-Za-z_][\w]*Converter\w*)\b", text))
    )
    return {
        "path": rel(path),
        "root": root_match.group(1) if root_match else "",
        "is_resource_dictionary": bool(root_match and root_match.group(1).endswith("ResourceDictionary")),
        "bindings": bindings,
        "commands_and_events": [attr for attr in attrs if attr["attribute"].split(":")[-1] in command_attrs],
        "visibility_conditions": visibility,
        "merged_resource_sources": resources,
        "converter_resources": converters,
        "image_sources": images,
        "static_resource_references": sorted(set(re.findall(r"\{StaticResource\s+([^},\s]+)", text))),
        "defined_resource_keys": sorted(set(re.findall(r"\bx:Key\s*=\s*['\"]([^'\"]+)['\"]", text))),
        "data_templates": [
            {
                "key": match.group(1),
                "line": line_number(text, match.start()),
            }
            for match in re.finditer(r"<DataTemplate\b[^>]*\bx:Key\s*=\s*['\"]([^'\"]+)['\"]", text, re.S)
        ],
    }


def source_occurrences(pattern: re.Pattern[str], paths: list[Path], group: int = 0) -> list[dict[str, object]]:
    output: list[dict[str, object]] = []
    for path in paths:
        text = path.read_text(encoding="utf-8-sig", errors="replace")
        for match in pattern.finditer(text):
            output.append(
                {
                    "path": rel(path),
                    "line": line_number(text, match.start()),
                    "value": match.group(group),
                }
            )
    return output


def declarations_in_block(text: str, label: str) -> list[dict[str, object]]:
    block = re.search(
        rf"^[ \t]*{re.escape(label)}[ \t]*:\s*(.*?)(?=^[ \t]*(?:public|private|protected|signals)(?:[ \t]+slots)?[ \t]*:|^\s*\}};)",
        text,
        re.M | re.S,
    )
    if not block:
        return []
    output: list[dict[str, object]] = []
    for match in re.finditer(r"^[ \t]*(?:virtual\s+)?(?:[\w:<>,*&]+\s+)+(?P<name>\w+)\s*\((?P<args>[^;{}]*)\)\s*(?:const\s*)?;", block.group(1), re.M):
        output.append(
            {
                "name": match.group("name"),
                "arguments": " ".join(match.group("args").split()),
                "line": line_number(text, block.start(1) + match.start()),
            }
        )
    return output


def qt_bridge_inventory(headers: list[Path], sources: list[Path]) -> list[dict[str, object]]:
    bridges: list[dict[str, object]] = []
    for header in headers:
        text = header.read_text(encoding="utf-8-sig", errors="replace")
        if "Q_OBJECT" not in text:
            continue
        # Comments sometimes discuss a "class" before the real declaration. Strip
        # them for class-name discovery so prose cannot be mistaken for C++.
        declaration_text = re.sub(r"//.*?$|/\*.*?\*/", "", text, flags=re.M | re.S)
        class_match = re.search(
            r"\bclass\s+(\w+)\b[^;{]*:\s*public\s+QObject\s*\{.*?\bQ_OBJECT\b",
            declaration_text,
            re.S,
        )
        if not class_match:
            class_match = re.search(r"\bclass\s+(\w+)\b[^;{]*\{", declaration_text)
        stem = header.stem.lower()
        peers = [source for source in sources if source.stem.lower() == stem]
        connections: list[dict[str, object]] = []
        for source in peers:
            source_text = source.read_text(encoding="utf-8-sig", errors="replace")
            for match in re.finditer(r"connect\s*\((.*?)\)\s*;", source_text, re.S):
                connections.append(
                    {
                        "path": rel(source),
                        "line": line_number(source_text, match.start()),
                        "expression": " ".join(match.group(1).split()),
                    }
                )
        bridges.append(
            {
                "class": class_match.group(1) if class_match else header.stem,
                "header": rel(header),
                "signals": declarations_in_block(text, "signals"),
                "public_slots": declarations_in_block(text, "public slots"),
                "private_slots": declarations_in_block(text, "private slots"),
                "connections": connections,
            }
        )
    return bridges


def enum_inventory(paths: list[Path]) -> list[dict[str, object]]:
    output: list[dict[str, object]] = []
    for path in paths:
        text = path.read_text(encoding="utf-8-sig", errors="replace")
        for match in re.finditer(r"enum\s+(?:class\s+)?(?P<name>\w+)\s*(?::[^\{]+)?\{(?P<body>.*?)\}\s*;", text, re.S):
            values = []
            body = re.sub(r"//.*?$|/\*.*?\*/", "", match.group("body"), flags=re.M | re.S)
            for value in body.split(","):
                name = value.split("=", 1)[0].strip()
                if re.fullmatch(r"[A-Za-z_]\w*", name):
                    values.append(name)
            output.append(
                {
                    "path": rel(path),
                    "line": line_number(text, match.start()),
                    "name": match.group("name"),
                    "values": values,
                }
            )
    return output


def build_inventory_from_root() -> dict[str, object]:
    xaml_paths = sorted(ROOT.rglob("*.xaml"), key=rel)
    cpp_paths = sorted(
        [*ROOT.glob("src/**/*.h"), *ROOT.glob("src/**/*.hpp"), *ROOT.glob("src/**/*.cpp")],
        key=rel,
    )
    # Freeze this evidence to the upstream UI. Replacement sources are added
    # below src/gui/ui during migration and must not mutate the baseline audit.
    cpp_paths = [path for path in cpp_paths if not path.is_relative_to(ROOT / "src" / "gui" / "ui")]
    ui_cpp_paths = [path for path in cpp_paths if path.is_relative_to(ROOT / "src" / "gui")]
    headers = [path for path in ui_cpp_paths if path.suffix in (".h", ".hpp")]
    sources = [path for path in ui_cpp_paths if path.suffix == ".cpp"]

    noesis_includes = source_occurrences(
        re.compile(r"^\s*#\s*include\s*[<\"]((?:Ns|Noesis)[^>\"]+)[>\"]", re.M),
        cpp_paths,
        1,
    )
    reflection = source_occurrences(re.compile(r"\b(NS_(?:DECLARE|IMPLEMENT)[A-Z0-9_]*)\s*\("), ui_cpp_paths, 1)
    reflected_properties = source_occurrences(re.compile(r"\bNsProp\s*\(\s*\"([^\"]+)\""), ui_cpp_paths, 1)
    component_registrations = source_occurrences(re.compile(r"RegisterComponent\s*<\s*([^>]+)\s*>"), ui_cpp_paths, 1)
    noesis_types = source_occurrences(
        re.compile(
            r"\b((?:Noesis|NoesisApp)::[A-Za-z_]\w*|ObservableCollection\s*<[^;(){}]+?>|DelegateCommand|NotifyPropertyChangedBase)\b"
        ),
        ui_cpp_paths,
        1,
    )
    type_counts = Counter(str(item["value"]).replace(" ", "") for item in noesis_types)

    keybinding_path = ROOT / "keybindings.json"
    keybindings = json.loads(keybinding_path.read_text(encoding="utf-8-sig")) if keybinding_path.exists() else []

    return {
        "schema_version": 1,
        "scope": "Current upstream Noesis/XAML UI; static source inventory only",
        "baseline_commit": BASELINE_COMMIT,
        "summary": {
            "xaml_documents": len(xaml_paths),
            "content_xaml_documents": sum(path.is_relative_to(ROOT / "content" / "xaml") for path in xaml_paths),
            "bindings": sum(len(item["bindings"]) for item in map(xaml_inventory, xaml_paths)),
            "noesis_include_occurrences": len(noesis_includes),
            "reflection_macro_occurrences": len(reflection),
            "reflected_property_occurrences": len(reflected_properties),
        },
        "xaml_documents": [xaml_inventory(path) for path in xaml_paths],
        "noesis_cpp": {
            "includes": noesis_includes,
            "reflection_macros": reflection,
            "reflected_properties": reflected_properties,
            "component_registrations": component_registrations,
            "type_counts": dict(sorted(type_counts.items())),
            "type_occurrences": noesis_types,
        },
        "qt_ui_bridges": qt_bridge_inventory(headers, sources),
        "state_enums": enum_inventory(headers),
        "configured_keybindings": keybindings,
    }


def build_inventory() -> dict[str, object]:
    """Inventory the recorded upstream commit, independent of worktree edits."""
    global ROOT
    archive = subprocess.run(
        ["git", "-C", str(CHECKOUT_ROOT), "archive", "--format=tar", BASELINE_COMMIT],
        check=True,
        stdout=subprocess.PIPE,
    ).stdout
    with tempfile.TemporaryDirectory(prefix="ingnomia-ui-baseline-") as temporary:
        baseline_root = Path(temporary)
        with tarfile.open(fileobj=io.BytesIO(archive), mode="r:") as baseline_tar:
            baseline_tar.extractall(baseline_root, filter="data")
        previous_root = ROOT
        ROOT = baseline_root
        try:
            return build_inventory_from_root()
        finally:
            ROOT = previous_root


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--check", action="store_true", help="Fail if the checked-in JSON is stale")
    args = parser.parse_args()

    data = build_inventory()
    rendered = json.dumps(data, indent=2, ensure_ascii=False) + "\n"
    output = args.output if args.output.is_absolute() else CHECKOUT_ROOT / args.output
    if args.check:
        if not output.exists() or output.read_text(encoding="utf-8") != rendered:
            print(f"stale inventory: {output.relative_to(CHECKOUT_ROOT).as_posix()}", file=sys.stderr)
            return 1
        print(f"inventory is current: {output.relative_to(CHECKOUT_ROOT).as_posix()}")
        return 0

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(rendered, encoding="utf-8", newline="\n")
    print(f"wrote {output.relative_to(CHECKOUT_ROOT).as_posix()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
