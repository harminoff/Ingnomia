#!/usr/bin/env python3
"""Generate small RmlUi-compatible build thumbnails from the game DB.

The checked-in PNG tilesheets remain the source of truth. RmlUi's pinned GL3
backend loads uncompressed TGA, so each thumbnail is cropped once into a
small TGA and can be rendered without background-position or CSS sprite
support. The script is deterministic and safe to rerun.
"""

from pathlib import Path
import re
import sqlite3

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
DB_SQL = ROOT / "content" / "db" / "ingnomia.db.sql"
SHEET_DIR = ROOT / "content" / "tilesheet"

# Composite/null Sprite records still need a single inventory thumbnail. Keep
# these IDs in the generator so the authoritative component crops are staged
# alongside the normal DB-resolved crops.
FALLBACK_BASE_IDS = {
    "AlarmBellBase", "Meat", "AutomatonTorsoFR", "HammerHead", "StrawBed",
    "BellowsFL", "Blackberry", "BigTorchBase", "BookshelfFR", "BrazierBase",
    "FancyWoodBedFrameFR", "FellingAxeHead", "KnifeBlade", "Strawberry",
    "GearBoxBaseItem", "GoblinTorso", "PineTree", "PineTreeNoLeaves", "LeverOffFR",
    "Painting1", "PickaxeHead", "PumpBase", "SteamEngineBoilerFR", "SwordBlade",
    "GroundTorchBase", "Carrot", "Axle", "WallTorchBaseFR", "WarhammerHead"
}


def values(connection: sqlite3.Connection, table: str, column: str):
    return {
        str(row[0])
        for row in connection.execute(f'SELECT "{column}" FROM "{table}"')
        if row[0]
    }


def resolve_base_ids(connection: sqlite3.Connection, sprite_id: str, base_ids: set[str], seen: set[str]):
    if not sprite_id or sprite_id in seen:
        return set()
    seen.add(sprite_id)
    if sprite_id in base_ids:
        return {sprite_id}

    resolved: set[str] = set()
    for row in connection.execute('SELECT "Sprite" FROM "Sprites_ByMaterialTypes" WHERE "ID" = ?', (sprite_id,)):
        resolved |= resolve_base_ids(connection, row[0], base_ids, seen)
    for row in connection.execute('SELECT "BaseSprite" FROM "Sprites_Rotations" WHERE "ID" = ?', (sprite_id,)):
        resolved |= resolve_base_ids(connection, row[0], base_ids, seen)
    for row in connection.execute('SELECT "BaseSprite" FROM "Sprites" WHERE "ID" = ?', (sprite_id,)):
        resolved |= resolve_base_ids(connection, row[0], base_ids, seen)
    return resolved


def main() -> int:
    connection = sqlite3.connect(":memory:")
    connection.executescript(DB_SQL.read_text(encoding="utf-8-sig"))
    base_rows = {
        str(row[0]): (str(row[1] or ""), str(row[2] or ""))
        for row in connection.execute('SELECT "ID", "SourceRectangle", "Tilesheet" FROM "BaseSprites"')
    }
    direct_ids: set[str] = set()
    for table, column in (
        ("Items", "SpriteID"),
        ("Workshops", "Icon"),
        ("Workshops_Components", "SpriteID"),
        ("Workshops_Components", "SpriteID2"),
        ("Containers_Tiles", "SpriteID"),
        ("Constructions_Sprites", "SpriteID"),
        ("Constructions_Sprites", "SpriteIDOverride"),
        ("ItemGrouping_Groups", "SpriteID"),
    ):
        direct_ids |= values(connection, table, column)

    resolved: set[str] = set()
    for sprite_id in sorted(direct_ids):
        resolved |= resolve_base_ids(connection, sprite_id, set(base_rows), set())
    resolved |= FALLBACK_BASE_IDS

    generated = 0
    skipped = 0
    for base_id in sorted(resolved):
        rect, sheet = base_rows[base_id]
        source = SHEET_DIR / sheet
        match = re.fullmatch(r"\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s*", rect)
        if not source.exists() or not match:
            skipped += 1
            continue
        x, y, width, height = (int(part) for part in match.groups())
        with Image.open(source).convert("RGBA") as image:
            if x < 0 or y < 0 or x + width > image.width or y + height > image.height:
                skipped += 1
                continue
            crop = image.crop((x, y, x + width, y + height))
            output = SHEET_DIR / f"build_{re.sub(r'[^A-Za-z0-9_-]', '_', base_id)}.tga"
            crop.save(output, format="TGA")
            generated += 1
    print(f"generated={generated} skipped={skipped} resolved={len(resolved)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
