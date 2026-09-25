"""Regenerate content/rmlui/screens/win98_classic.rcss from win98_classic.template.rcss.

Lengths in the template are written as @N@ (N pixels at 1x). They become N/11 em, because
the Windows 98 sheet font is snapped to 11, 22 or 33 physical pixels, so every length stays a
whole number of pixels at 1x, 2x and 3x. Run: python tools/win98-style/generate.py
"""
import pathlib
import re

here = pathlib.Path(__file__).resolve().parent
root = here.parents[1]
src = (here / "win98_classic.template.rcss").read_text()


def em(match):
    value = float(match.group(1))
    return "0" if value == 0 else f"{value / 11:.4f}".rstrip("0").rstrip(".") + "em"


out = root / "content" / "rmlui" / "screens" / "win98_classic.rcss"
out.write_text(re.sub(r"@(-?\d+(?:\.\d+)?)@", em, src))
print("wrote", out)
