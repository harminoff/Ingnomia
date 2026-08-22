# Tilesheet asset provenance

These seventeen PNG tilesheets are the repository-owned assets published by the
upstream `origin/gh-pages` branch at commit
`21e1c36ac4da28f107d125231b3fbfe95a731d0a` (the branch's deployed asset tree).
They were extracted from the exact Git tree without conversion or recompression
and are consumed by `SpriteFactory` through the `BaseSprites.Tilesheet` values
in `content/db/ingnomia.db.sql`.

| File | Git blob | Bytes | SHA-256 | Dimensions / format |
|---|---|---:|---|---|
| `terrain.png` | `0c85b87c9df5032fe353016062fe1012daf54418` | 99446 | `0392F6587AD4201197A8DEF3C7642742ABC6B3CA32BDB3CE88879A9D7AF7125F` | 1024x1152 RGBA PNG |
| `default.png` | `5bafb6e128e82d4d5f610fe16853f561e733f8bf` | 75442 | `127B8F9FD7F061CF82B554B546F32921296A319C666F844EAE5CAF3F7D7C69EE` | 1024x1152 RGBA PNG |
| `furniture.png` | `bb47462ab59f27c0b42113c64a653ff21ff1dc66` | 42813 | `37D3EDB2E1639A11D435A50C2BCAB940B14592A4A89AEB2550FE7EF6672A12F2` | 512x576 RGBA PNG |
| `workshops.png` | `1d2f40d6ace090d8ef93a3ebe20eaca020e18435` | 46871 | `8ED31957304B029B7057C589C93FC072B7A279983453ACC74D43DF4DA71E34B6` | 416x540 RGBA PNG |
| `animals.png` | `3d7ad97bd019fd094b8b0d7934c66c981dd041eb` | 46343 | `C0547C1DEADA14A13C821422C0D761620B7A7FF25EF58D5FB5804C3FEEA143A1` | 512x576 RGBA PNG |
| `multitrees.png` | `ad4462cd52a4d024306dbed2c4720bd7f0c6cb8c` | 52978 | `FBC70591D5548B848BA7DE1889582FB7D889D6970736D52E57591572D2CE1BD8` | 640x576 RGBA PNG |
| `automatons.png` | `b82f95ea26856baec067c4af277b3e75237960ab` | 1873 | `2EAAE64E987222143CE86C066559974FDC0AFE05B888A81D228312E5702C6F9E` | 256x108 RGBA PNG |
| `food_drink_ingredients.png` | `102770826a605d3a15c1c1e6620499e1d8019203` | 6801 | `CDC02061DC855A8625910DCFD6D038DFEE15BF67B7A6848FED413003CB9C43DB` | 384x144 RGBA PNG |
| `gnomes.png` | `3cdd644439135920a294c8d26a8ad0d6e7e7065e` | 28697 | `881EA6DCFF6057F91125ABE391FE73A56075E404AFD737E9BCA12DF9613128A0` | 768x540 RGBA PNG |
| `goblin.png` | `79c9fec679779d6ff79fd37fb271951f730bab7f` | 4940 | `86EC4C948F8999F70044159EB5EBA043C9332070AEE7D5DA8AB682577D55D85A` | 480x288 RGBA PNG |
| `mushroom_biome_grass.png` | `821531b047556322876e565d0fb6854769f6aece` | 14930 | `4374750180E308DDFE89FA6E6113C05E26D47469AEE090823392384FD9820174` | 256x288 RGBA PNG |
| `mushrooms.png` | `ca7c19739af3066b7ed3b3bed452af83c39b7ee4` | 14052 | `651C1F64DFEF1E8D7F152B0BF47E85F124CF68E13550592AA9C3ED8F8BDC769B` | 1056x108 RGBA PNG |
| `plants.png` | `f7008425fb170e50d8e2273f81f902736b98ff7f` | 89863 | `BC795D3C000E0D32CD5091B1FA44D9E53B82D4B649FFBAC4B0DC9486F884CCF7` | 1024x1152 RGBA PNG |
| `seasonalgrass.png` | `92f9e3c7f596ea5d9a73f3aadbd7e5a4b5bedcff` | 250678 | `D9E77301A054E6592DBD04FB835681B76CD322532D4FEF23F04C2E4AD504B8BD` | 416x1116 RGBA PNG |
| `traps_mechanism.png` | `d35ee58f217510bda6d192852e16cd8b4be27990` | 43978 | `331AD59C4D605649BD00E0045ACB854601CC8477896A9E8BEDC068FF014CA0DC` | 576x432 RGBA PNG |
| `weapons_armour.png` | `10363134bf7d5cd8f25160815bd99660f8ffe194` | 4940 | `2ED4E93F399FC43D17C1EBDBE2C53A8CB362D62693EA6108632448591964C48F` | 384x288 RGBA PNG |
| `windmill.png` | `c36b8c8c6979abe2dcac75965168818d113d033f` | 13937 | `6A29885B26E4D97F723038605FF752382F26033F0B7781003285909495411823` | 256x324 RGBA PNG |

The gh-pages tree has no separate asset license/notice file. The files are
therefore retained as upstream Ingnomia repository assets under the repository
licensing context, but a package/release owner should confirm that provenance
and any contributor artwork terms before redistributing a final artifact.

The same gh-pages directory also contains `icon.png`; it is a repository icon,
not a `BaseSprites.Tilesheet` value, so it is intentionally outside this
runtime tilesheet package.

## DB references without an available gh-pages sheet

The current `BaseSprites.Tilesheet` values also name the following files, but
none exists under `origin/gh-pages:assets/` at the pinned commit. They are
intentionally not fabricated or substituted:

`magic.png`, `mobs.png`, `multicreatures.png`, `mushroom_biome_slopes.png`,
`mushroom_biome_zygs.png`, `seasonaldetails.png`, `seasonalslopes.png`, and
`weapons-armour-UI-large.png`.

## RmlUi build thumbnails

`build_*.tga` (333 files) are deterministic, uncompressed RGBA crops made by
`scripts/generate_build_icons.py` from the DB `BaseSprites.SourceRectangle`
values and the PNG sheets above. They are runtime copies, not new artwork:
the pinned RmlUi GL3 backend accepts TGA textures, while the source tilesheets
remain PNGs. Regenerate with `python scripts/generate_build_icons.py` after a
DB or sheet change; the generated files are staged by the existing `*.tga`
content glob.

`plants.tga` is the uncompressed RGBA companion for `plants.png` (1024x1152,
4,718,636 bytes, SHA-256
`A5D1C05BB5B6537FFC5A0C1D00C504157818B8DD3F4DEBA963977C6B0E3780CD`). It is
used by the RmlUi inventory rows because the pinned GL3 renderer accepts TGA,
not PNG, and the inventory adapter crops the authoritative plant rectangles
from this companion at render time.
