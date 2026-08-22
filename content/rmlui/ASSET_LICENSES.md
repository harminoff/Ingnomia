# RmlUi shared asset and license manifest

Scope: Wave 2 shared design-system content under `content/rmlui`.

| Asset | Origin | License/provenance | Runtime status |
|---|---|---|---|
| `styles/*.rcss`, `styles/tokens.json` | Original for Ingnomia | Project license (AGPL-3.0-or-later) | Shared runtime content |
| `templates/*.rml`, `fixtures/components.rml` | Original for Ingnomia | Project license (AGPL-3.0-or-later) | Shared templates and non-player fixture |
| Typographic geometry used as fixture glyphs (`+`, minus, multiplication sign, arrows, check, information mark, diamond) | Original arrangement of ordinary Unicode characters; no icon artwork copied or traced | No separate asset license | Text fallback only; always paired with a visible label or `title` in the fixture |
| `smoke.rml`, `smoke.rcss`, `smoke.tga` | Wave 1 foundation evidence, outside Wave 2 authorship | Existing project worktree provenance | Preserved byte-for-byte by the Wave 2 verifier |

## Fonts

The shared UI uses `LatoLatin-Regular.ttf`, the regular Lato face shipped by the
pinned RmlUi 6.2 source at commit
`2230d1a6e8e0848ed87a5761e2a5160b2a175ba4`. It is copied from
`Samples/assets/LatoLatin-Regular.ttf` without modification. SHA-256:
`D785334AC4E7810F571DEF986BBAD41161F68AC385DB8813F798BF04D71478E1`.
The authoritative RmlUi sample notice is retained at
`fonts/notices/RmlUi-Samples-Lato-OFL-1.1.txt`; it contains the SIL Open Font
License 1.1 text and the Lato copyright/reserved-name terms. This notice must
remain beside the binary in every installed/package output.

The design contract requires a legible UI sans at the named regular/bold roles.
Only the regular face is currently bundled; a bold face must not be referenced
until its binary and matching notice are added with the same provenance and
hash discipline.

## Images and icons

Wave 2 adds no raster image, copied game screenshot, external icon pack, SVG, or
web-fetched texture. Component identity is currently carried by structure,
text, semantic markers, and original typographic geometry. This is deliberate:
the pinned GL3 renderer's guaranteed image route is uncompressed TGA, and the
mission forbids unlicensed or copied third-party art. Future binary additions
must record author/source, license, hash, logical `AssetId`, supported decode
path, native dimensions, and fallback label here before use.

## Dependency notices

The exact notices for the pinned dependencies are staged under `notices/` and
installed with the RmlUi content:

| Notice | Source revision | License | SHA-256 |
|---|---|---|---|
| `notices/RmlUi-LICENSE.txt` | RmlUi `2230d1a6e8e0848ed87a5761e2a5160b2a175ba4` | MIT | `02C6FA90F6A2C1CF69124014FBAAF524AFFC71EBF76DD4176321BC50A5B0746C` |
| `notices/FreeType-LICENSE.TXT` | FreeType `42608f77f20749dd6ddc9e0536788eaad70ea4b5` (2.13.3) | FreeType License or GPLv2, as selected by the product | `87BD7EEA77F6EED44375A8532EADD54E38DBA82FE11A7F9B5613A987E7F9449E` |

These are unmodified upstream notices. Packaging must retain both dependency
notices and the font notice; this manifest does not replace their full text.
