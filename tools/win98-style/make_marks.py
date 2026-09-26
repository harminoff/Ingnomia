"""Draw the Windows 98 control marks at 1x as a sprite sheet (content/rmlui/icons/w98-marks.tga).

Each cell is exactly the padding box it fills, so the sprite is drawn at an integer scale
(nearest filtering) and stays pixel-exact at 1x, 2x and 3x.
"""
from PIL import Image

BLACK = (0, 0, 0, 255)
GRAY = (128, 128, 128, 255)
WHITE = (255, 255, 255, 255)
CLEAR = (0, 0, 0, 0)

CHECK = ["......#", ".....##", "#...###", "##.###.", "#####..", ".###...", "..#...."]  # 7x7
DOT = [".##.", "####", "####", ".##."]  # 4x4
DOWN = ["#######", ".#####.", "..###..", "...#..."]  # 7x4
UP = DOWN[::-1]
LEFT = ["...#", "..##", ".###", "####", ".###", "..##", "...#"]  # 4x7
RIGHT = [row[::-1] for row in LEFT]
SPIN_UP = ["..#..", ".###.", "#####"]  # 5x3
SPIN_DOWN = SPIN_UP[::-1]
# Farm plot state glyphs (non-colour cues): furrows for tilled ground, a boxed check for ready crops.
FURROW = [("#.#.#.#.#.#.#.#." if r % 4 == 1 else ".#.#.#.#.#.#.#.#" if r % 4 == 2 else "." * 16) for r in range(16)]
READY_BOX = ["#" * 11] + ["#" + "." * 9 + "#" for _ in range(9)] + ["#" * 11]
# Caption button glyphs (the Marlett marks of a 16 x 14 caption button), placed in the 14 x 12 face inside
# the button border: Close, Minimize, Maximize and Restore.
CAP_CLOSE = ["##....##", ".##..##.", "..####..", "...##...", "..####..", ".##..##.", "##....##"]  # 8x7
CAP_MIN = ["######", "######"]  # 6x2
CAP_MAX = ["#########", "#########"] + ["#.......#" for _ in range(6)] + ["#########"]  # 9x9
def _restore():
    grid = [["."] * 8 for _ in range(9)]
    def box(x0, y0):
        for yy in range(6):
            for xx in range(6):
                edge = yy < 2 or yy == 5 or xx == 0 or xx == 5
                if edge:
                    grid[y0 + yy][x0 + xx] = "#"
                elif grid[y0 + yy][x0 + xx] == "#":
                    grid[y0 + yy][x0 + xx] = "."  # the front window hides the back one
    box(2, 0)
    box(0, 3)
    return ["".join(r) for r in grid]
CAP_RESTORE = _restore()  # 8x9
CAP_HELP = ["..####..", ".##..##.", ".....##.", "....##..", "...##...", "...##...", "........", "...##...", "...##..."]  # 8x9, the ? button
CAP_CLOSE_SMALL = ["##...##", ".##.##.", "..###..", "..###..", ".##.##.", "##...##"]  # 7x6, palette caption

# name: (cell width, cell height, pattern, x, y, colour)
CELLS = [
    ("w98m-check", 11, 11, CHECK, 2, 2, BLACK),
    ("w98m-check-disabled", 11, 11, CHECK, 2, 2, GRAY),
    ("w98m-radio", 10, 10, DOT, 3, 3, BLACK),
    ("w98m-radio-disabled", 10, 10, DOT, 3, 3, GRAY),
    ("w98m-down", 14, 14, DOWN, 3, 5, BLACK),
    ("w98m-up", 14, 14, UP, 3, 4, BLACK),
    ("w98m-left", 14, 14, LEFT, 4, 3, BLACK),
    ("w98m-right", 14, 14, RIGHT, 5, 3, BLACK),
    ("w98m-down-disabled", 14, 14, DOWN, 3, 5, GRAY),
    ("w98m-up-disabled", 14, 14, UP, 3, 4, GRAY),
    ("w98m-spin-up", 14, 8, SPIN_UP, 4, 2, BLACK),
    ("w98m-spin-down", 14, 8, SPIN_DOWN, 4, 2, BLACK),
    ("w98m-furrow", 16, 16, FURROW, 0, 0, GRAY),
    ("w98m-ready", 11, 11, READY_BOX, 0, 0, BLACK),
    ("w98m-warning", 32, 32, [], 0, 0, BLACK),
    ("w98m-sort-up", 9, 9, UP[:-1], 1, 3, BLACK),
    ("w98m-sort-down", 9, 9, DOWN[1:], 1, 3, BLACK),
    # Menu marks (PDF p.118-119): a check mark for independent settings, a dot for a choice in a group;
    # white variants for the highlighted item.
    ("w98m-menu-check", 13, 13, CHECK, 3, 3, BLACK),
    ("w98m-menu-check-white", 13, 13, CHECK, 3, 3, WHITE),
    ("w98m-menu-dot", 13, 13, DOT, 4, 4, BLACK),
    ("w98m-menu-dot-white", 13, 13, DOT, 4, 4, WHITE),
    # Toolbar menu button arrow (PDF p.125, p.153), and its unavailable form.
    ("w98m-caret", 5, 3, ["#####", ".###.", "..#.."], 0, 0, BLACK),
    ("w98m-caret-disabled", 5, 3, ["#####", ".###.", "..#.."], 0, 0, GRAY),
    ("w98m-info", 32, 32, [], 0, 0, BLACK),
    ("w98m-cap-close", 14, 12, CAP_CLOSE, 3, 2, BLACK),
    ("w98m-cap-min", 14, 12, CAP_MIN, 3, 8, BLACK),
    ("w98m-cap-max", 14, 12, CAP_MAX, 2, 1, BLACK),
    ("w98m-cap-restore", 14, 12, CAP_RESTORE, 2, 1, BLACK),
    ("w98m-cap-close-small", 11, 9, CAP_CLOSE_SMALL, 2, 1, BLACK),
    # Unavailable left and right scroll arrows (PDF p.101).
    ("w98m-left-disabled", 14, 14, LEFT, 4, 3, GRAY),
    ("w98m-right-disabled", 14, 14, RIGHT, 5, 3, GRAY),
    # Size grip (PDF p.100, p.155): three diagonal ridges toward the lower right corner, each a highlight line
    # and two shadow lines.
    ("w98m-grip", 12, 12, [], 0, 0, BLACK),
    # What's This? title bar button of a secondary window (PDF p.157, p.285).
    ("w98m-cap-help", 14, 12, CAP_HELP, 3, 1, BLACK),
]

width = sum(c[1] for c in CELLS) + len(CELLS)  # 1px gutter between cells
height = max(c[2] for c in CELLS)  # tallest cell (the 32 px message box symbol)
sheet = Image.new("RGBA", (width, height), CLEAR)
x = 0
decl = []
ENGRAVED = {"w98m-down-disabled", "w98m-up-disabled", "w98m-left-disabled", "w98m-right-disabled"}
for name, w, h, pattern, px, py, colour in CELLS:
    # An unavailable arrow is engraved: the gray mark over a white copy one pixel down and to the right.
    if name in ENGRAVED:
        for row, line in enumerate(pattern):
            for col, ch in enumerate(line):
                if ch == "#":
                    sheet.putpixel((x + px + col + 1, py + row + 1), WHITE)
    for row, line in enumerate(pattern):
        for col, ch in enumerate(line):
            if ch == "#":
                sheet.putpixel((x + px + col, py + row), colour)
    if name == "w98m-grip":
        for yy in range(12):
            for xx in range(12):
                s = xx + yy
                if s in (11, 15, 19):
                    sheet.putpixel((x + xx, yy), WHITE)
                elif s in (12, 13, 16, 17, 20, 21):
                    sheet.putpixel((x + xx, yy), GRAY)
    if name == "w98m-ready":
        for yy in range(1, 10):
            for xx in range(1, 10):
                sheet.putpixel((x + xx, yy), WHITE)
        for row, line in enumerate(CHECK):
            for col, ch in enumerate(line):
                if ch == "#":
                    sheet.putpixel((x + 2 + col, 2 + row), BLACK)
    if name == "w98m-warning":
        # Message box Warning symbol: yellow triangle, black outline and exclamation mark, gray shadow.
        YELLOW = (255, 255, 0, 255)
        def inside(px, py, grow=0.0):
            # Triangle with apex (15.5, 1) and base from x=0 to x=31 at y=29.
            if py < 1 - grow or py > 29 + grow:
                return False
            half = (py - 1) * 15.5 / 28 + grow
            return abs(px + 0.5 - 16) <= half
        for py in range(32):
            for px in range(32):
                if inside(px - 2, py - 2) and not inside(px, py, 1.0):
                    sheet.putpixel((x + px, py), GRAY)
        for py in range(32):
            for px in range(32):
                if inside(px, py, 1.0):
                    sheet.putpixel((x + px, py), YELLOW if inside(px, py) and not (not inside(px, py - 1) or not inside(px - 1, py) or not inside(px + 1, py) or not inside(px, py + 1)) else BLACK)
        for py in list(range(9, 21)) + list(range(23, 26)):
            for px in range(14, 18):
                if py < 21 and (px in (14, 17)) and py > 17:
                    continue
                sheet.putpixel((x + px, py), BLACK)
    if name == "w98m-info":
        # Message box Information symbol: white speech balloon with a black outline, gray shadow and a blue "i".
        BLUE = (0, 0, 255, 255)
        def disc(px, py, r, cx=15.0, cy=12.5):
            return (px - cx) ** 2 + (py - cy) ** 2 <= r * r
        def tail(px, py):
            return 20 <= py <= 29 and 6 <= px <= 13 and px - 6 <= ( 29 - py ) * 0.9
        for py in range(32):
            for px in range(32):
                if (disc(px - 2, py - 2, 12.4) or tail(px - 2, py - 2)) and not (disc(px, py, 12.4) or tail(px, py)):
                    sheet.putpixel((x + px, py), GRAY)
        for py in range(32):
            for px in range(32):
                if disc(px, py, 12.4) or tail(px, py):
                    edge = not (disc(px, py, 11.4) or (tail(px, py) and tail(px - 1, py) and tail(px + 1, py) and tail(px, py + 1)))
                    sheet.putpixel((x + px, py), BLACK if edge else WHITE)
        for py in range(5, 8):
            for px in range(14, 17):
                sheet.putpixel((x + px, py), BLUE)
        for py in range(10, 19):
            for px in range(14, 17):
                sheet.putpixel((x + px, py), BLUE)
        for px in range(13, 14):
            sheet.putpixel((x + px, 10), BLUE)
        for px in range(13, 18):
            sheet.putpixel((x + px, 19), BLUE)
    decl.append(f"    {name}: {x}px 0px {w}px {h}px;")
    x += w + 1

out = str(__import__("pathlib").Path(__file__).resolve().parents[2] / "content" / "rmlui" / "icons" / "w98-marks.tga")
sheet.save(out)
print(out, sheet.size)
# Option-set toolbar buttons have a dithered face/highlight background (PDF p.322). The pattern is its own
# 2 x 2 texture so the image decorator can repeat it.
dither = Image.new("RGBA", (2, 2), (192, 192, 192, 255))
dither.putpixel((0, 0), WHITE)
dither.putpixel((1, 1), WHITE)
dither.save(out.replace("w98-marks.tga", "w98-dither.tga"))
print("\n".join(decl))
# The application's small icon for the primary window caption (16 x 16, PDF p.312), reduced from content/icon.png.
root = __import__("pathlib").Path(__file__).resolve().parents[2]
icon = Image.open(root / "content" / "icon.png").convert("RGBA").resize((16, 16), Image.LANCZOS)
icon.save(str(root / "content" / "rmlui" / "icons" / "w98-app-icon.tga"))

