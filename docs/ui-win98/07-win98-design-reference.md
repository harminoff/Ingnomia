# Windows 98 design reference (Stages 11-21)

This is the design reference for the Windows 98 re-skin of the Ingnomia RmlUi interface. It covers
the stages that remain after the Stage 10 workshop property sheet. The project owner's rule is:
**only use UI that is documented in the sources below.** If a need has no documented pattern, the
implementer uses the closest documented control and records the gap. This document marks every gap
and names the outside reference used to fill it.

Citations such as "PDF p.165" refer to the page numbers printed in the page footer ("Page N of 421") of
`docs/MS-Windows-User-Experience-2001.pdf`. Quotations are kept short. Everything else is a paraphrase.

## 1. Sources

| Key | Source | Use |
| --- | --- | --- |
| Book | *Microsoft Windows User Experience* (Microsoft Press, 2001), `docs/MS-Windows-User-Experience-2001.pdf` | Primary authority. |
| MSDN-VD | MSDN "Design Specifications and Guidelines - Visual Design": [ms997612](https://learn.microsoft.com/en-us/previous-versions/ms997612(v=msdn.10)), with sub-pages [Design of Visual Elements ms997615](https://learn.microsoft.com/en-us/previous-versions/ms997615(v=msdn.10)), [User Interface Text ms997617](https://learn.microsoft.com/en-us/previous-versions/ms997617(v=msdn.10)), [Layout ms997619](https://learn.microsoft.com/en-us/previous-versions/ms997619(v=msdn.10)), [Selection Appearance ms997622](https://learn.microsoft.com/en-us/previous-versions/ms997622(v=msdn.10)), and [Controls ms997492](https://learn.microsoft.com/en-us/previous-versions/ms997492(v=msdn.10)) | Online edition of the same book (Chapters 8 and 14). The text I checked matches the PDF, including the DLU and toolbar tables, so the PDF pages are the citations. |
| W97 | Win32 "How to Create Wizards" (Wizard97 style), https://learn.microsoft.com/en-us/windows/win32/controls/wizards | Wizard page template sizes. The book leaves these out. |
| TBC | Win32 "How to Customize Toolbars", https://learn.microsoft.com/en-us/windows/win32/controls/customize-toolbars | The system Customize Toolbar dialog: two lists, Add/Remove, Move Up/Move Down. The book refers to this dialog on PDF p.153. |
| XL97 | Excel 97 grid model. Selection behavior: https://support.microsoft.com/en-us/office/select-all-cells-on-a-worksheet-1a35f997-7afa-4656-bd06-c8086765fccf, https://support.microsoft.com/en-us/excel/get-started/select-cell-contents-in-excel, https://bettersolutions.com/excel/cells-ranges/selecting-cells.htm, https://distancelearning.institute/research/cell-selection-techniques-excel-97/. Freeze panes: https://support.microsoft.com/en-us/office/freeze-panes-to-lock-rows-and-columns-dab2ffc9-020d-4026-8121-67dd25f2508f | The table and grid model. The book only shows spreadsheet selection in Figure 6.2 (PDF p.56-57). |
| COL | Windows 98 "Windows Standard" scheme values: https://github.com/Abdelrhman-AK/WinPaletter/blob/master/StockThemes/RetroThemesDB.wpdb (Windows Classic 98 entry) and https://github.com/1j01/98 (theme files, e.g. `InfoWindow=255 255 225`) | Concrete hex values. The book names system colors only (PDF p.317, p.358, p.373). |

**Precedence.** Use the Book (or MSDN-VD) first, then the Win32 docs (W97, TBC), then XL97 and COL.
Anything else is a period observation. It must be flagged in the stage record and checked against a
real Windows 98 or Excel 97 screenshot before it is treated as settled.

## 2. Shared rules already adopted (Stages 00-10)

These rules are already built into `content/rmlui/screens/win98_classic.rcss` and the Stage 10
workshop sheet. The remaining stages reuse them unchanged.

### 2.1 Window types

- **Property sheet** for editing an object's properties. Caption: object name plus "Properties", in
  book-title caps. If a sheet covers several objects of one type, use the type name; for mixed types,
  use "Selection" (PDF p.163).
  - Tabs: one row only, never multiple rows (PDF p.147, p.164).
  - Buttons: **OK / Cancel / Apply** sit outside the pages because they apply to the whole window.
    A command button placed on a page acts on that page only (PDF p.165-166, p.346).
  - Changes are pending until OK or Apply. Cancel discards pending changes but does not undo changes
    already applied (PDF p.165).
  - Do not add a Help button (PDF p.166). A Reset button is optional (PDF p.165).
  - The title-bar Close button is **not** Cancel. If changes are pending, ask with a message box that
    has Yes / No / Cancel buttons (PDF p.166).
  - Reopen the sheet on the last page the user viewed (PDF p.159).
- **Pages never scroll.** This is the owner's rule. It follows from the fixed property sheet sizes and
  the advice to split pages or use a subordinate dialog box rather than scroll tabs (PDF p.147,
  p.157). Only a list, list view, or grid control inside a page may scroll.
- **Dialog box** for collecting the parameters of a command.
  - Caption: the command name without its ellipsis (PDF p.169).
  - OK is the default button (PDF p.169).
  - Command buttons are stacked at the top right or placed in a row along the bottom. The default
    button comes first, and OK and Cancel sit next to each other (PDF p.169, p.346).
- **Message box** for errors, warnings, and confirmations (PDF p.182-187). Details are in section
  3.4.
- Secondary windows:
  - have no title-bar icon, and no Minimize or Maximize buttons (PDF p.156);
  - should not include a status bar control (PDF p.157);
  - should be no larger than 263 x 263 DLU (PDF p.157);
  - open centered over the primary window on first use, then reopen where the user last put them
    (PDF p.160).

### 2.2 Metrics (DLU)

One horizontal DLU is 1/4 of the average character width of the system font; one vertical DLU is
1/8 of the character height (PDF p.341).

| Item | Value | Source |
| --- | --- | --- |
| Property sheet sizes (W x H) | 252 x 218, 227 x 215, or 212 x 188. Heights include the 25 DLU button bar. | PDF p.157, p.341 |
| Command button | 50 x 14 | PDF p.341 |
| Text box, drop-down list, drop-down combo | 14 high (the table says 10 for the drop-down controls; MSDN adds that most single-line controls are 14) | PDF p.341-342, MSDN Layout |
| Check box, option button | 10 high | PDF p.341 |
| Text label | 8 per line | PDF p.342 |
| Window margin | 7 on all sides | PDF p.343 |
| Related controls / unrelated controls | 4 / 7 | PDF p.343-344 |
| Label to its control | 3 | PDF p.343 |
| First control in a group box | 11 below the top of the group box | PDF p.344 |
| Group box left inset | 9 | PDF p.344 |
| Last control to group box bottom | 7 | PDF p.344 |
| Smallest gap between controls | 2 | PDF p.344 |
| Text label beside a button | 3 down from the button's top | PDF p.344 |
| Check box, list box, or option button beside a button | 2 down from the button's top | PDF p.344 |

Keep command buttons the same length. Keep tabs the same width in one window (PDF p.343).

### 2.3 Text

- **Book-title caps:** column headings, command buttons, menu items, palette titles, tabs, title
  bars, toolbar buttons, and ToolTips (PDF p.329).
- **Sentence caps:** check box labels, group box labels, list entries, messages, option button labels,
  status bar text, and text box labels (PDF p.329-330).
- **Colons:** a label for a text box, list, spin box, or slider ends with a colon. Button, tab, and
  group box labels never do (PDF p.138, p.142, p.370).
- **Ellipsis** only when the command needs more input before it can finish. Never on Properties,
  Options, Close, or view commands (PDF p.330-331).
- **Access keys** on every label except OK and Cancel, which use Enter and Esc. They must be unique
  within the window, and "A" is reserved for Apply (PDF p.46, p.162, p.328).
- **Font:** MS Sans Serif 8 pt (Windows 98). Only title bars are bold. Avoid italic (PDF p.314-315,
  p.326-327). The project's font for this is "MS W98 UI" (see `06-decisions.md`).
- **Numbers:** right-aligned, and their column headings are right-aligned too (PDF p.143, p.332).

### 2.4 Borders and states (PDF p.316-326)

| Style | Composition | Used by |
| --- | --- | --- |
| Window border | raised outer + raised inner | windows, menus, scroll arrows |
| Button border | raised; the pressed state is sunken and the label moves 1 px right and down | command buttons |
| Field border | sunken outer + sunken inner; interior is the window color (button face when read-only or disabled) | text boxes, list boxes, list views, check boxes, spin boxes, combos |
| Status field | sunken outer only | status bar panes, read-only dynamic fields |
| Grouping | sunken outer + raised inner (etched) | group boxes, menu separators |

- **Default button:** has an extra bold (black) outline (PDF p.160).
- **Input focus:**
  - Text fields show the caret.
  - Other controls get a **dotted rectangle** around the control or its label, with at least one
    border width of clearance (PDF p.325).
- **Unavailable:** the label is engraved (highlight color, overlaid with the shadow color offset by
  1 px) (PDF p.323-324).
- **Mixed value:**
  - Check box: gray check mark on a dithered background.
  - Option buttons: none of them has a dot.
  - Drop-down lists and text fields: blank.
  - (PDF p.126-127, p.322-323)

### 2.5 Colors

The Book defines these colors only as system colors (PDF p.317, p.358, p.373) and says to use each
foreground color with its matching background. The Windows 98 "Windows Standard" values used by
`win98_classic.rcss` are listed below (COL):

| System color | Value |
| --- | --- |
| Button face (3D face), dialog and toolbar background | `#C0C0C0` |
| Button highlight | `#FFFFFF` |
| Button light (inner raised edge) | `#DFDFDF` |
| Button shadow, gray text | `#808080` |
| Dark shadow, window frame, window and button text | `#000000` |
| Window (field interiors) | `#FFFFFF` |
| Highlight / highlight text (selection) | `#000080` / `#FFFFFF` |
| Active caption gradient | `#000080` to `#1084D0`, white bold text |
| Inactive caption gradient | `#808080` to `#C0C0C0`, `#C0C0C0` text |
| ToolTip background / text | `#FFFFE1` / `#000000` |

- Color is never the only cue; always pair it with text, shape, or position (PDF p.313-314, p.347).
- Do not use color to group items (PDF p.346).

### 2.6 Tables and multi-select

- **Tables:** a list view in details view. Column headings sort when clicked and reverse on a
  second click. Users can drag the dividers, and double-clicking a divider may auto-size the column
  (PDF p.136, p.143). Ctrl+Plus on the numeric keypad fits all columns (PDF p.136).
- **Multi-select of independent items:** a multiple-selection list box drawn as a list of **flat**
  check boxes (PDF p.135, p.326). A list view with check box state images is also documented
  (PDF p.136).

## 3. Stage rules

### 3.1 Stage 11: Agriculture (farm, grove, pasture)

**Window type.** A property sheet (section 2.1), sized 252 x 218 DLU, with one tab row.
Suggested pages: General, Plots, Crops (or Trees), Animals, and Food. Show only the pages that apply
to the designation type.

**Plot grid.**

- **Control.** The documented control for a collection of selectable, individually represented items
  is the **list view** (PDF p.136). Use it in icon or small-icon view. In those views the documentation
  allows items to sit at any position (PDF p.136), so each plot can keep its place in the farm. Draw
  the view in the field border. It is the only scrolling region on its page.
- **Selection model.** Book PDF p.49-60, plus XL97 for two-dimensional ranges:
  - Clicking selects one plot and sets the anchor (PDF p.53-54).
  - **Ctrl+click** toggles one plot and moves the anchor. Ctrl+drag applies the state of the first
    plot touched to every plot it covers (PDF p.54-55).
  - **Shift+click** extends the selection from the anchor, and the anchor does not move (PDF p.55).
    In a 2-D grid the range is the rectangle between the anchor and the clicked plot. This is how
    spreadsheet ranges work in the Book's Figure 6.2 (PDF p.56-57) and in Excel 97 (XL97).
  - **Dragging on the background** draws a dotted bounding outline (a marquee). It selects the plots
    it encloses and honors Shift and Ctrl (PDF p.58).
  - **Select All.** Ctrl+A selects every plot (PDF p.399). A Select All command also goes on the shortcut menu (PDF p.390).
    Excel 97 also has a Select All button at the top-left corner where the headings meet (XL97). Use it
    only if the plot grid has row and column headings.
  - **Keyboard:**
    - The arrow keys move the focus and select.
    - Shift+arrow extends the selection from the anchor.
    - Ctrl+arrow moves the focus without changing the selection, and Space then toggles the plot.
    - Shift+F8 is the alternative Add mode.
    - (PDF p.59-60)
  - Right-clicking a selected plot does not change the selection. It opens the shortcut menu
    (PDF p.54).
- **Appearance.**
  - Selected plots use the highlight colors `#000080` and `#FFFFFF`, or an outline in the highlight
    color if the icon has to stay readable (PDF p.357-358).
  - The focused plot also gets the dotted focus rectangle (PDF p.325, p.358).
  - The plot state (untilled, tilled, planted, ready) is shown by an icon or glyph together with a
    text legend or ToolTip, never by color alone (PDF p.314, p.348).

**Crop list and defaults.**

- The farm's default crop is a single choice from a changing list:
  - Use a **drop-down list box** (PDF p.134), or a single-selection list view if the icons and
    columns are needed (PDF p.133, p.136).
  - When the plots in the selection have different crops, the field is blank (mixed value, PDF
    p.134-135).
  - Crops that are not available are **left out** of the list, not shown as disabled (PDF p.131).
- **Per-plot actions and default actions** are separate command buttons, and each is placed next
  to the control it acts on (PDF p.169, p.346). The label names the target: for example,
  "Assign to Selected" beside the grid, and the default selector in its own group box labeled
  "Farm default". Buttons on a page act on that page only (PDF p.165). Commands that act
  immediately (queue or assign) are still command buttons, not settings.
- **Planting queue:** a list view in details view. Queue order editing is covered in section 3.4
  (Move Up / Move Down).

**Pasture.**

- **Animal list:** a list view in details view with columns such as Name, Sex, Age, and Butcher.
  The Butcher mark is a check box state image in the row (PDF p.136). If the list view shows plain
  checks, draw them flat (PDF p.326).
- **Male and female limits:** two **spin boxes** (PDF p.141).
  - Each has a sentence-caps label ending in a colon, placed to the left of or above the box and
    left-aligned with it.
  - The top arrow adds 1 and the bottom arrow subtracts 1.
  - Numbers are left-aligned inside the text box.
  - The Book says "typically" wrap at the ends. For limits, clamping without wrapping is allowed,
    because the Book does not require wrapping.
  - Validate as the user types, and ignore characters that are not digits (PDF p.137, p.162).
- **Food rules:** a multiple-selection list box of flat check boxes (PDF p.135, p.326). Its label is
  something like "Accepted food:" (sentence caps with a colon).

**Persistent options.** Harvest, hay, tame, pick, plant, and fell are **check boxes**, because each
choice has two clear opposite states (PDF p.128). They are pending until Apply. Any control that
depends on a check box comes right after it in the tab order and becomes unavailable when that
check box is cleared (PDF p.130). If there are more than about seven options, use a check-box list
box instead (PDF p.129).

**Statistics.** Read-only numbers are static text (PDF p.142) inside a group box (PDF p.144). If a
value changes while the sheet is open, use the status-field border (PDF p.320).

### 3.2 Stage 12: Population roster, skills, and professions

**Roster.**

- A list view in details view (PDF p.136). Clicking a column heading sorts by that column, and
  clicking it again reverses the order (PDF p.143).
- **Sort indicator.** The Book allows an image in the heading and requires it to be correct: a
  **downward-pointing arrow means descending order** (PDF p.143). Use a small triangle next to the
  heading text and nothing else. Stock Windows 98 list views usually showed no sort arrow at all.
  That is a period observation, so the arrow is optional but allowed.
- **Keyboard alternative.** Column headings cannot take keyboard focus. Put Sort Ascending and Sort
  Descending on the shortcut menu (Shift+F10 or the Application key) as the way to sort from the
  keyboard (PDF p.143).
- **Other rules:**
  - Names are sentence text, left-aligned.
  - Numbers are right-aligned and their headings are right-aligned (PDF p.143, p.332).
  - Typing letters jumps to the matching name (PDF p.131, p.136).
  - Double-click and Enter both run the same default command. The default button in the window must
    match that command (PDF p.137).

**Citizen editor.** A property sheet (section 2.1) with the caption "<Name> Properties". Suggested
pages: General, Skills, and Professions. When several citizens are selected, open **one** sheet
titled with the type name, and show fields whose values differ as mixed values (PDF p.163,
p.167-168).

**Skills.** A list view in details view with check box state images for enabled skills (PDF p.136).
Levels are right-aligned numbers.

**Profession editor: dual list (Add / Remove).**

- **Gap:** the Book has no two-list transfer pattern. The documented system example is the toolbar
  control's **Customize Toolbar dialog**, which the Book mentions (PDF p.153) and TBC describes:
  - The left list holds available items and the right list holds current items.
  - **Add ->** and **<- Remove** buttons sit between the two lists.
  - **Move Up** and **Move Down** buttons sit to the right.
- **Rules from the Book that still apply:**
  - Transfer between list boxes should also work with drag and drop, and with Cut and Paste
    (PDF p.132).
  - Each button sits next to the list it acts on (PDF p.346).
  - After a button runs, focus goes back to the list (PDF p.161).
  - Add and Remove are unavailable when nothing is selected (PDF p.323-324).
  - Labels have no ellipsis (PDF p.330).
- Each list is a list view in details view, or an extended-selection list box that supports
  Shift and Ctrl selection (PDF p.135).

### 3.3 Stage 13: Schedule (24-hour matrix)

**Gap.** The Book has no grid control. It only shows that spreadsheet cells follow the standard
selection model (Figure 6.2, PDF p.56-57), and that clicking a column label can select the whole
column (PDF p.60). The model below is Excel 97 (XL97), limited to the parts that fit the Book.

- **Structure:**
  - Row headings hold the citizen names. Column headings hold the hours 0-23. Both are
    button-face header cells with the button border, like column heading controls (PDF p.143, p.318).
  - The top-left corner cell is the **Select All** button (XL97).
  - Cells have a white interior and gray (`#C0C0C0`) gridlines. The gridline color is a period
    observation of Excel 97's default and has not been verified.
- **Frozen headers.** Row and column headings stay visible while the cells scroll, like Excel 97's
  Window > Freeze Panes (XL97). The grid is the only scrolling region, with scroll bars on its right
  and bottom edges. Disable a scroll arrow when the view reaches that end (PDF p.146).
- **Active cell.** Exactly one cell is active, and it has a heavy black border (XL97).
- **Range selection.** Selected cells take the highlight colors. The active cell keeps its white
  interior and heavy border, so the user can still see where input will go.
  - This also satisfies the Book's rule that a selection needs both the highlight and a focus
    indication (PDF p.358).
  - Excel 97 drew selected ranges in inverse video. Navy is chosen here for consistency with the
    system selection color. This choice is recorded as an adaptation.
- **Mouse:**
  - Click selects one cell. Drag selects a rectangle.
  - Shift+click extends the selection from the anchor. Ctrl+click or Ctrl+drag adds a separate range.
  - Clicking a row or column heading selects the whole row or column. Dragging across headings
    selects several.
  - Clicking the corner selects everything.
  - (PDF p.53-60, XL97)
- **Keyboard:**
  - The arrow keys move the active cell. Shift+arrow extends the selection.
  - Shift+Space selects the row and Ctrl+Space selects the column. Ctrl+A selects all.
  - Home and End, and Ctrl+Home and Ctrl+End, move to the ends of the row or of the grid.
  - (PDF p.50-51, p.399, XL97)
- **Cell content.** Each assignment is a **letter code** in the cell (for example W, E, S, T),
  optionally with a tint. Color alone is not allowed (PDF p.314). Explain the codes in the palette
  labels and ToolTips.
- **Exclusive palette.**
  - For up to 7 activities, use **option buttons** in a group box (PDF p.126). Or use a toolbar
    option-button group:
    - adjacent buttons with **no space between them** (PDF p.345);
    - the set button shows the **option-set appearance**, which is the pressed border plus a
      dithered highlight/face background (PDF p.154, p.322);
    - a ToolTip on each button (PDF p.152).
  - Choosing an activity sets it on the selected cells right away, because toolbar property controls
    act on the current selection (PDF p.152). A "paint" mode, where each click assigns the armed
    activity, is a tool mode:
    - the button stays set and the pointer changes (PDF p.154);
    - Esc cancels the mode (PDF p.48).
- **Bulk scope preview.** Show the selected-cell count as static text, for example
  "12 hours selected for 3 citizens". Use a status-field border if the value updates live
  (PDF p.142, p.320). Do not add floating badges.

### 3.4 Stage 14: Military

- **Squads:**
  - The squad list is a single-selection list box or a list view in details view (PDF p.133, p.136).
  - Squad members are a list view in details view.
  - Squad properties (name, uniform, orders) are a property sheet (section 2.1).
- **Moving citizens between squads** uses the Customize Toolbar two-list pattern from section 3.2
  (TBC), with drag and drop and Cut and Paste between the lists (PDF p.132). If moving is a single
  choice of destination, use a dialog box with a drop-down list labeled "Move to squad:", OK as the
  default button, and Cancel (PDF p.134, p.169).
- **Role and uniform editor.** Use a property sheet page. Each equipment slot is a drop-down list
  box with a sentence-caps label and a colon (PDF p.134). If the selected roles have different
  values, the field is blank (PDF p.135).
- **Target priorities (ordered list):**
  - A list view in details view with **Move Up** and **Move Down** command buttons stacked to the
    right of the list. The pattern comes from TBC; the Book does not document reordering.
  - Each button is unavailable at the end of the list it would move past (PDF p.323).
  - After a move, focus goes back to the list (PDF p.161).
  - Drag and drop inside the list is allowed as a shortcut (PDF p.132).
- **Stance per creature type.** Flee, Defend, Attack, and Hunt are four mutually exclusive choices,
  so they are **option buttons** (PDF p.126):
  - Place them in a group box with a noun label such as "Stance" (sentence caps, no colon;
    PDF p.144).
  - Labels are sentence-caps phrases with no end punctuation (PDF p.127).
  - The arrow keys move between them, and Tab reaches only the one that is set (PDF p.127, p.161).
  - If the options are shown per row inside a list, use **flat** option buttons (PDF p.326).
  - When several types are selected with different stances, no option shows a dot (mixed value,
    PDF p.126-127).
- **Confirmation message boxes** (for example disbanding a squad or removing a role):
  - The title is the object or application name. Never use "Warning" or "Error" (PDF p.182).
  - Use the Warning symbol. The question-mark symbol is not allowed (PDF p.183).
  - Buttons are **Yes / No**. Use specific verbs such as **Disband / Cancel** if Yes/No would be
    ambiguous (PDF p.184-185).
  - The default button is the least destructive choice (PDF p.184).
  - The text is two or three complete sentences. It states the effect rather than asking
    "Are you sure" (PDF p.185-187).
  - Buttons are centered (PDF p.347). The title-bar Close button is enabled only if Cancel is
    present (PDF p.185).

### 3.5 Stage 15: Diplomacy and missions

- **Choose the window type** by what the user is doing:
  - Viewing or editing a faction's standing is a **property sheet**. Pages might be General,
    Relations, and Trade.
  - Planning a mission is a sequence of steps with defaults, so it is a **wizard**. Wizards use the
    property sheet control without tabs, and replace OK/Cancel/Apply with Back, Next or Finish, and
    Cancel (PDF p.145, p.304).
- **Wizard type:**
  - Use a *simple* wizard for three pages or fewer, without Welcome or Completion pages.
  - Use an *advanced* wizard for longer tasks or tasks that branch. It has a Welcome page and a
    Completion page (PDF p.305).
- **Wizard buttons.** **< Back**, **Next >**, **Finish**, and **Cancel** sit along the bottom
  (PDF p.305-306).
  - Back is disabled or removed on the first page.
  - Finish sits right next to Next, on its right, and appears on any page once the defaults are
    enough to finish.
  - On the last page, Finish replaces Next.
  - Never advance to the next page automatically.
  - Give every control a default value.
  - (PDF p.306)
- **Welcome page:**
  - Title: "Welcome to the <Name> Wizard" (book-title caps, no end punctuation).
  - Body: starts "This wizard helps you ..." and ends "To continue, click Next."
  - A graphic sits on the left (PDF p.306-307).
- **Interior pages.** A header area with a book-title-caps title, plus a subtitle written as a
  complete sentence (PDF p.307-308).
- **Completion page:**
  - Title: "Completing the <Name> Wizard".
  - Body: summarizes what was done and ends "To close this wizard, click Finish."
  - If the task did not complete, say so and suggest a fix. Never use the words "failed" or
    "failure" (PDF p.308-309).
- **Sizes (W97; the Book is silent).** The Wizard97 exterior page template is **317 x 193 DLU**.
  The interior page template is **317 x 143 DLU**, with the header drawn outside the template.
  - Keep controls out of the watermark area on the left of exterior pages.
  - The exterior title uses 12 pt Verdana Bold (W97). This is a Windows 2000-era value. For a strict
    Windows 98 look, use the system font in bold, and record that choice.
  - The spacing figure in the Book (Figure 14.29, PDF p.345-346) is an image that has not been read
    here. Check it against the PDF before building the layout.
- **Wizard text.** Conversational, using "you" and questions such as "Which squad do you want to
  send?" (PDF p.309).
- **Unknown values.** The Book gives no treatment for *unknown* values. The nearest documented
  behavior is the mixed-value rule: leave the field blank and show no value (PDF p.323). Use blank
  for editable fields. For read-only facts that the player does not know yet, show the word
  "Unknown" as static text. Do not invent a number, placeholder glyph, or special style.
  Mark this as an adaptation.

### 3.6 Stage 16: Inspectors

- **Window type.** A **property inspector** in a **palette window** (PDF p.167, p.180-181).
  - The inspector always shows the current selection. This is different from a property sheet,
    which stays on the object it was opened for (PDF p.167).
  - Edits apply **immediately** (PDF p.167), so the window has no OK, Cancel, or Apply buttons.
- **Palette window rules:**
  - The title bar is shorter than normal and has only a Close button (PDF p.180).
  - Title: the command or toolbar name, or "<Object> Properties", in book-title caps
    (PDF p.167, p.181).
  - The window may be resizable. Save and restore its size and position (PDF p.181).
  - The shortcut menu offers Close, Move, Size, Always on Top, and Properties. Always on Top is a
    user option (PDF p.158-159, p.181).
  - Alt+F6 switches between the inspector and the main window (PDF p.158).
- **Optional controls:**
  - A drop-down list at the top naming the object being inspected. Choosing another entry shows that
    object's properties (PDF p.167).
  - A "lock" control that keeps the inspector on the current object (PDF p.167).
- **Pages.** Tabs are allowed (PDF p.147). Keep the palette at or under 263 x 263 DLU (PDF p.157).
  Pages still do not scroll; only lists scroll.
- **Hierarchies** (inventory containers, a body-part tree, and similar) use a **tree view**
  (PDF p.137). The keys behave like this:
  - Up and Down move between items.
  - Right expands an item or moves to its first child.
  - Left collapses an item or moves to its parent.
  - The keypad asterisk (\*) expands everything below the item.
  - Typing letters jumps to a matching item.
  - Plus/minus expand buttons and connecting lines are optional.
  - If Enter or double-click opens an item's properties, the default button must be Properties.
- **Several objects selected.** Show one inspector.
  - Fields with differing values show as mixed values (PDF p.167-168).
  - If the objects are of different types, show only the properties they share (PDF p.168).
- **Citizen detail** that the user edits on purpose, rather than live, uses the property sheet from
  section 3.2.

### 3.7 Stage 17: HUD

**Toolbars** (PDF p.151-155, p.319, p.322, p.325, p.342-345, p.352-353).

- **Sizes, in pixels:**
  - Small buttons are 22 x 21 with **16 x 16** images, on a 23 px toolbar.
  - Large buttons are 28 x 26 with **20 x 20** images, on a 28 px toolbar (PDF p.154, p.342).
  - The Book's table of common toolbar images labels the large column **24 x 24** (PDF p.353).
    This conflicts with the 20 x 20 figure, so treat 20 x 20 as the rule.
  - The default display is 16 x 16 images with no text labels (PDF p.155).
- **Spacing:**
  - Adjacent related buttons, such as an option-button group, have no gap (PDF p.345).
  - Other controls are at least 4 DLU apart.
  - Controls sit at least one window border width in from the toolbar edges (PDF p.345).
  - Buttons are left-aligned or top-aligned (PDF p.347).
- **Flat appearance:**
  - A toolbar button has **no border** until the pointer is over it. Then it shows a 1 px raised
    border (PDF p.319, p.325).
  - Grayscale images may change to color on hover (PDF p.154, p.325).
  - Pressed: shadow color on the top and left edges, highlight on the bottom and right (PDF p.319).
- **Toggle and option buttons:**
  - A button that shows a state uses the **option-set** appearance: pressed border plus a dithered
    face/highlight background (PDF p.322).
  - Hovering over a set button shows the face color instead of a border (PDF p.325).
  - An exclusive group behaves like option buttons, and an independent toggle behaves like a check
    box (PDF p.154).
- **Separators** come from the toolbar control (PDF p.153). Draw them in the grouping (etched) style
  (PDF p.320).
- A **grip** at the left end of the toolbar allows docking and floating. A floating toolbar becomes a
  palette window (PDF p.152-153).
- **Split and menu buttons:**
  - A split button runs its default action and also has a drop-down list of related actions
    (PDF p.153).
  - A command button that opens a menu shows a triangular arrow (PDF p.125).
  - Do not use that arrow anywhere else (PDF p.348).
- **Every image-only control has a ToolTip** (PDF p.152). ToolTips:
  - are one or two words in book-title caps with the shortcut key in parentheses;
  - appear after a delay at the lower right of the pointer;
  - disappear when the user clicks or moves away;
  - have a `#FFFFE1` background and black text (COL).
  - (PDF p.148)
- Use the standard images for the functions listed in the Book's common toolbar buttons table only,
  and only for those functions (PDF p.353-355).

**Status bar** (PDF p.151-155, p.288-289, p.320).

- Put it at the bottom of the **primary** window only (PDF p.157).
- Divide it into panes. Each pane has the **status-field** (sunken outer) border (PDF p.320).
- Panes hold read-only static text such as the date, time, or game speed (PDF p.142).
- A size grip in the corner is optional (PDF p.155).
- **Messages:**
  - Start with a present-tense verb, for example "Designates a new farm." (PDF p.289).
  - Show them when the pointer is over a toolbar button or menu item.
  - When an item is unavailable, say why (PDF p.289).
- **Do not put essential information only in the status bar,** because the user may hide it
  (PDF p.152, p.288).
- A **progress indicator** can show a background process in a pane (PDF p.145, p.289).

**Pointer and modes.**

- **Armed tools:**
  - Clicking a tool button sets it (option-set appearance) and **changes the pointer** to show the
    mode (PDF p.154, p.355-356).
  - **Esc** cancels the mode (PDF p.48).
  - A pointer change applies only over the area where it means something (PDF p.356).
- **Hourglass** only while the window cannot respond. For waits longer than a few seconds, use a
  progress indicator (PDF p.356).
- **Hover selection** (Web-style selection) is optional in the Book (PDF p.58-59). Do not use it.
  Keep click selection, because hover selection conflicts with the no-hover rule in section 4.

**Event notification.**

- **Non-critical events** go to the status bar (PDF p.288-289).
- **Balloon tips** (PDF p.148-150) are documented, but they came with Windows 2000/Me. For a strict
  Windows 98 look, prefer the status bar and record the choice.
- **Events that need a decision** use a message box, following the rules in section 3.4:
  - Show only **one** message box per condition, and never a chain of them (PDF p.184).
  - If the game window is inactive, flash its taskbar button first (PDF p.184).
  - Critical errors use the Critical symbol with an OK button (PDF p.183-184).
- **Timing.** Do not show critical feedback briefly and then remove it. Any time-out must be
  adjustable (PDF p.373).

**Menus** (build menus and shortcut menus).

- Use at most one level of cascading submenus (PDF p.116).
- Group commands with separators (PDF p.111).
- Independent settings show a check mark. Choices within a group show an option-button dot
  (PDF p.118-119).
- Unavailable items are grayed out, not removed (PDF p.118).
- Menu bar items hot-track with a 1 px border. Drop-down menu items use the selection highlight
  (PDF p.326).

### 3.8 Stage 18: Main menu, new game, save browser

- **Main menu.** **Gap:** the Book has no game title screen. The nearest documented form is a
  **dialog box**:
  - The commands (New Game, Load Game, Settings, Exit) are 50 x 14 DLU command buttons in
    book-title caps.
  - They are stacked in one column, with the default command first (PDF p.169, p.346).
  - A start-up (splash) window may show the product name and then close by itself (PDF p.184).
    Anything shown there must also be available elsewhere, for example in an About box.
- **New game.** An **advanced wizard** (section 3.5): Welcome, then interior pages (embark
  location, settings, starting gnomes and items), then Completion.
  - Every page has default values, so Finish can appear as soon as the defaults are complete
    (PDF p.306).
  - Presets are a drop-down list (PDF p.134).
  - The world seed is a text box with a colon label (PDF p.138-139).
- **Save browser.** Model it on the common **Open dialog box** (PDF p.170-172) and **Save As**
  (PDF p.172-173):
  - Caption: the command name ("Load Game" or "Save Game"), with no ellipsis (PDF p.169-170).
  - The file list is a list view in details view with book-title-caps headings.
  - The columns **Name, Size, Type, Modified** are the Windows 98 Explorer details columns. This is a
    period observation, not from the Book. Use them, or replace Type with something game-specific
    such as Kingdom.
  - Dates sort newest first when the arrow points down (PDF p.143).
  - A "File name:" text box sits under the list. **Open** (or **Save**) is the default button, next
    to **Cancel** (PDF p.170-173).
  - Double-click opens (PDF p.170).
  - Save As suggests a name ahead of time (PDF p.173).
  - Remember the last folder and view (PDF p.159).
  - Delete sits on the shortcut menu (PDF p.110-111). It confirms with a Yes / No message box that
    states the effect (section 3.4).
  - Overwriting an existing save also asks Yes / No.

### 3.9 Stage 19: Settings, pause, loading

- **Settings.** A **property sheet** with OK / Cancel / Apply (section 2.1). Suggested pages:
  General, Display, Sound, and Controls.
  - The request said the Book might recommend changing Cancel to Close once settings apply
    immediately. **The 2001 Book does not document this.** A search for Close/Cancel wording
    finds only "Avoid interpreting the Close button as Cancel" (PDF p.166).
  - The documented model for settings that apply immediately is the **property inspector**. Its
    changes apply at once and it has only a Close button (PDF p.167, p.180). Controls on toolbars
    and status bars also apply at once (PDF p.152).
  - Use the inspector model only for settings that should take effect live. Everything else stays
    pending until Apply.
- **Sliders** for continuous ranges such as volume (PDF p.146):
  - The label goes to the left or above, in sentence caps with a colon.
  - Range labels are parallel pairs, for example Low / High.
  - The arrow keys move the slider. Tick marks are optional.
  - **The Book does not document a numeric readout.** If the exact value matters, use a **spin box**
    instead, which is the documented alternative (PDF p.146). Or show the value in static text below
    the slider, as the Windows 98 Display Properties "Screen area" slider did. That is a period
    observation, so record it.
- **Unsaved changes** (on quit, load, or closing the sheet):
  - The message box asks "Do you want to save changes to <save name>?" (a question is allowed in
    the Warning type, PDF p.183).
  - Buttons are **Yes / No / Cancel**, where Cancel returns to the game (PDF p.166, p.185).
  - The title is the game or save name (PDF p.182).
- **Pause.** **Gap:** there is no documented pause screen. Use a dialog box with command buttons
  (Resume as the default, then Save, Load, Settings, Quit), captioned with the command name
  (PDF p.169). Esc triggers Resume, because Esc is the Cancel key (PDF p.48, p.162).
- **Loading:**
  - Use a **progress message box**: a solid progress indicator, with text **outside** the bar,
    that closes itself when the load finishes (PDF p.145, p.184).
  - Its button is **Stop**, not Cancel, if the previous state cannot be restored (PDF p.185).
  - Show the hourglass pointer over the window while it cannot respond (PDF p.356).
  - A segmented bar is allowed, but solid is the default (PDF p.145).

### 3.10 Stage 20: Scaling, keyboard, accessibility

- **Tab order:**
  - Left to right, then top to bottom, starting from the main control at the top left. Commit
    buttons (OK, Cancel, Apply) come last (PDF p.161).
  - Tab stops on the set option button of a group, and on every check box (PDF p.161).
  - Tabbing to a label moves focus to its control (PDF p.142, p.370).
  - Tabbing must never trigger an action (PDF p.373).
- **Arrow keys:**
  - move within option-button groups and lists (required);
  - optionally move between text boxes and between check boxes (PDF p.161);
  - stay inside a group box (PDF p.144).
- **Access keys:**
  - Every labeled control gets one. They are unique within the window, and OK and Cancel have none
    (PDF p.46-47, p.328).
  - Alt+letter moves to the control and has the same effect as clicking it.
  - A plain letter works too when the focused control does not accept text (PDF p.161).
- **Enter and Esc.** Enter triggers the default button. Esc cancels, closes a drop-down without
  changing its value, and stops a mode (PDF p.48, p.141, p.162).
- **Tabs:**
  - Ctrl+Tab and Ctrl+Page Down go to the next tab. Ctrl+Page Up goes to the previous one.
  - When a tab has focus, Left and Right move between tabs.
  - (PDF p.147, p.400)
- **Other keys** (PDF p.399-400):
  - Shift+F10 or the Application key opens the shortcut menu.
  - Alt+Enter opens the property sheet.
  - Alt+F6 switches to a modeless window.
  - F1 opens Help.
  - Ctrl+A selects all.
- **Focus.** The dotted focus rectangle is always shown in the active window (PDF p.50, p.325).
  Selection and focus are drawn separately (PDF p.358).
- **Selection in inactive panes.** The Book says to avoid showing a selection in an inactive window
  or pane. If the selection must stay visible, draw an outline in the highlight color (PDF p.357).
  In Windows 98, list views with "show selection always" drew an inactive selection with a gray
  button-face background. That is a period observation and an allowed variant.
- **High Contrast:**
  - Check the setting at startup and whenever settings change, then switch to the high-contrast
    colors.
  - Hide any image drawn behind text.
  - Show icons in monochrome if needed.
  - (PDF p.373-374)
- **Scaling:**
  - Base all measurements on the system font and metrics (DLUs, border widths), not fixed pixels
    (PDF p.317, p.327, p.374).
  - Render at whole-number scales, as in `06-decisions.md`.
  - Check that windows fit on a 640 x 480 screen at 1x (PDF p.342).
- **Mouse.** Every basic function works with a single click. Double-click, drag, and modified clicks
  are only shortcuts (PDF p.374).
- **Color and sound.** Neither is ever the only cue (PDF p.314, p.364-365, p.367).
- **Labels.**
  - Every control has a label, which may be hidden (PDF p.370).
  - Every window has a title (PDF p.372).

### 3.11 Stage 21: Conformance checklist

Check every Windows 98 surface for the following:

- [ ] The window type matches the task:
  - property sheet: pending edits, OK/Cancel/Apply;
  - property inspector or palette: live edits, Close only;
  - dialog box: command parameters;
  - wizard: Back/Next/Finish/Cancel;
  - message box.
- [ ] The caption follows its rule ("X Properties", command name, wizard name, or application
  name). There is no title-bar icon and no Minimize/Maximize on secondary windows.
- [ ] The window size is one of the documented sizes, or at most 263 x 263 DLU. No page scrolls.
- [ ] Margins are 7 DLU, spacing is 4 or 7 DLU, buttons are 50 x 14, and single-line fields are
  14 DLU high.
- [ ] Commit buttons sit outside the pages, at the bottom right. OK and Cancel are adjacent, and
  the default button has its bold outline.
- [ ] Capitalization is correct. Labels end in a colon where required. Ellipses are used only where
  required. Access keys are unique.
- [ ] Only the five border styles are used. There are no rounded corners, soft shadows, or
  gradients (except the caption gradient).
- [ ] The dotted focus rectangle appears on every focusable control. The selection uses
  `#000080` / `#FFFFFF`.
- [ ] Unavailable controls are engraved, not hidden. Crops, items, and similar entries that do not
  apply are left out of lists.
- [ ] Mixed values show as a blank field, a dithered check box, or option buttons with no dot.
- [ ] Tables are list views in details view with correct heading alignment and sorting. Grids
  follow section 3.3.
- [ ] Every image-only control has a ToolTip. Toolbar buttons are flat and have no hover state
  except the 1 px hot-track border.
- [ ] Message boxes have a correct title, symbol, button set, and least-destructive default. The
  words "error" and "failed" do not appear, and there is only one box per condition.
- [ ] The keyboard alone can reach and operate everything. Tab order is correct. Esc and Enter
  behave as documented.
- [ ] High Contrast works. Color is never the only cue. Text uses the system font at whole-number
  scale.
- [ ] Every item in section 4 is absent.

### 3.12 Stage 21a: Stockpile property sheet (replaces the Stage 09 body)

The Stage 09 Stockpile window was built before the owner's documented-only rule. It still has
sentence-case buttons and tabs, section headings, a page that scrolls, header filter combos and
settings that apply immediately. It moves to the Stage 10 property-sheet model.

- **Window:** property sheet, 252 x 218 DLU (384 x 380 px at 1x). Caption "<Name> Properties"
  (PDF p.163). One row of tabs: **Contents**, **Allow List**, **General** (PDF p.147). OK / Cancel /
  Apply outside the pages (PDF p.165-166). The sheet reopens on the last page viewed (PDF p.159).
- **Pending model:** name, priority, the three check boxes on General and every check box on Allow
  List stay pending until Apply or OK. Cancel discards them. Close with pending changes asks
  "Do you want to apply the changes you made to <name>?" Yes / No / Cancel (PDF p.166). Validation
  and conflicts use a message box. Drafts are kept per stockpile ID until the world ends (Stage 10
  decision).
- **Contents page (read-only report):**
  - "Find:" text box and "Category:" drop-down list on one line. Filtering by typing is the Find
    pattern already used by Population (Stage 12); the drop-down list is the documented way to
    choose one of a known set (PDF p.131, p.140).
  - "Items stored here:" list view in details view (PDF p.136, p.143): Item, Material, Stored,
    Total. Numbers and their headings are right-aligned (PDF p.143, p.332). Clicking a heading sorts
    by it; clicking it again reverses the order. Only the list scrolls.
  - One line of static text explains Total ("Total counts every stored item of that kind.").
- **Allow List page:**
  - The same Find / Category line.
  - "Items this stockpile accepts:" list view in details view with a check box on each row (list
    view with check box state images, PDF p.136; flat check boxes, PDF p.135, p.326). Columns Item,
    Material, Type. Space or a click on the check box toggles the pending state. Rows whose state is
    pending are not marked by color alone: the check box itself shows the pending value.
  - **Allow All** and **Block All** act on the rows shown, and the count beside them says how many
    rows that is ("12 items shown"). They only change the pending state, so no confirmation is
    needed: Cancel undoes them.
  - "Template:" drop-down combo box (PDF p.140) with **Load** and **Save** beside it. Templates are
    saved copies held by the game, so both commands act at once on the applied allow list. While rule
    changes are pending they are unavailable (engraved, PDF p.323) and a line of static text says
    "Apply your changes before using templates." Load asks with a message box (Yes / No) before it
    replaces the allow list; Save asks before it replaces a template that already exists.
- **General page:** Name and Priority ("(1 is the highest priority)"), a separator, a "Hauling"
  group with two check boxes, and a "Status" group with "Suspend deliveries" and **Center on Map**,
  exactly like the workshop General page. A static line states what is stored and incoming.
- **Removed:** section headings and help paragraphs, the Excel-style header filter combos, the
  bulk-rule review dialog (replaced by pending Allow All / Block All), "Back to rule search", and the
  separate Apply / Revert pair on the settings page.

### 3.13 Stage 21b: Inventory report window (replaces the Stage 08 body)

The Stage 08 Inventory window predates the rule in the same way: a wide window, header filter
combos, and an in-place detail view with "Back to inventory", which is a web pattern.

- **Window:** 252 x 218 DLU (384 x 380 px at 1x), caption "Inventory", **Close** only, because
  nothing on it is pending (the Population precedent, Stage 12). One page, so no tabs.
- **Items list:** "Find:" and "Category:" as on the Stockpile sheet, then a list view in details
  view: Item, Material, In Stock, Total, sortable by heading. The check box on each row is the watch
  state (list view with check box state images, PDF p.136) and takes effect at once. A status line
  gives "N of M items".
- **Properties** (and Enter or a double-click) opens "<Item> Properties", a subordinate property
  sheet in the same document with Close only (PDF p.147, p.163): **General** (counts and stockpile
  locations), **Recipes** ("Made by:" and "Used to make:" lists), **History**. This replaces the
  in-place detail view.

### 3.14 Stage 21c: Localized text and access keys

- Every string a player can read comes from the localization catalog, never from literal RML text
  (PDF p.378-383). No text is built at run time by joining sentence fragments (PDF p.383); messages
  are whole strings with named arguments.
- **Access keys travel with the string.** As in Windows resources and registry verbs, the access
  character is marked with an ampersand ("&Name:", PDF p.214), so a translation can choose its own
  letter. The runtime turns "&X" into the underlined letter and the `accesskey` attribute; "&&" is a
  literal ampersand. OK and Cancel have none, and "A" stays reserved for Apply (PDF p.46, p.162,
  p.328).
- Labels, buttons and tabs leave room for about 30 percent longer text (PDF p.380-381). The
  qps-long pseudo-locale is the check for this.

## 4. Not documented: do not use

These are common in game interfaces but have no basis in the sources above. Do not use them.

- Rounded corners, pill shapes, cards, tiles, or card grids. Use group boxes, list views, and
  static text instead.
- Drop shadows, glows, blur, translucent panels, and gradients anywhere except the active and
  inactive caption.
- **Hover highlights on command buttons, check boxes, option buttons, list items, or tabs.**
  Windows 98 command buttons have no hover state. Hot tracking is documented only for flat toolbar
  buttons and the menu bar, and drop-down menu items use the selection highlight (PDF p.319,
  p.325-326).
- Colored badges, chips, tags, notification dots, or count bubbles. Use static text or a
  status-bar pane.
- Color-coded state without text or shape, including red/green alone (PDF p.314).
- Hiding or showing controls when the pointer moves over them, apart from ToolTips (PDF p.373).
- Toasts or notifications that disappear on their own, and chains of popups (PDF p.184, p.373).
- Scrolling property pages, several rows of tabs, scrolling tab strips, and vertical side-tab
  navigation (PDF p.147, p.164).
- Icon-only command buttons without a ToolTip, and image buttons outside toolbars (PDF p.152).
- Custom message-box icons, the question-mark icon, and "Warning" or "Error" as a title
  (PDF p.182-183).
- Hyperlinks used in place of OK/Cancel or other actions, and "click here" link text (PDF p.162).
- Bold or italic interface text other than title bars, wizard titles, and default menu commands
  (PDF p.315, p.327).
- Animated transitions, sliding panels, and pulsing highlights. The Book allows animation only to
  show how a tool works or to reflect a state. It must never be the only carrier of information,
  must not block interaction, and must be interruptible (PDF p.363).
- Toggle switches, segmented controls, steppers other than the spin box, floating action buttons,
  and hamburger menus.
- Placeholder text inside text boxes as a substitute for a label (PDF p.138).
- Invented symbols for unknown or empty values. Use a blank mixed-value field or the word "Unknown"
  (section 3.5).

## 5. Open questions and uncertain points

1. **Excel 97 visual details.** These come from period observation and have not been verified
   against a real Excel 97 screenshot: the gridline color, the heavy active-cell border, whether
   selected ranges were drawn inverse or highlighted, and whether headings were emphasized for the
   selection. Section 3.3 uses navy for consistency with the Book.
2. **Move Up / Move Down and the Add/Remove dual list.** Neither is in the Book. Both come from the
   system Customize Toolbar dialog (TBC). The Book refers to that dialog only in passing (PDF p.153).
3. **Changing Cancel to Close after an immediate apply.** This is not in the 2001 Book. The
   documented alternative is the property inspector (Close only). Settings stay OK/Cancel/Apply.
4. **Wizard sizes and the 12 pt Verdana title** come from the Win32 Wizard97 documentation, which
   dates from Windows 2000. The layout figure in the Book (Figure 14.29) is an image and has not been
   transcribed here.
5. **Toolbar images.** The Book gives 16x16 and 20x20 in its text and tables (PDF p.154, p.342,
   p.352). The common toolbar images table is labeled 24x24 (PDF p.353). Use 16 and 20.
6. **Balloon tips** are in the Book but did not exist on Windows 98. Use them only if the owner
   accepts it.
7. **Hex values** for system colors come from Windows 98 scheme data (COL), not the Book. The
   `#DFDFDF` button-light value is the Windows 98 value; Windows 95 used `#C0C0C0`.
8. **Save-browser columns** (Name, Size, Type, Modified) and the **Display Properties slider
   readout** are Windows 98 period observations with no book citation.
