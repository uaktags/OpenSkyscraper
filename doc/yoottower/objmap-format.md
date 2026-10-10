# YootTower Plugin OBJMAP Format

**Status:** Reverse-engineered, confidence HIGH.
**Method:** Hex-dumped OBJMAP 100/1000/1100 from `Stair.t2p`, `Burger.t2p`, `Condo.t2p`, `Cathedral.t2p`, `StdElev.t2p` via `scratch/objmap_dump.py`, then read the bytes. The format is plain ASCII text — no binary struct to decode.
**Calibration oracle:** `scratch/extract_t2p_stairs.py:11-13` knows the Stair.t2p spiral-staircase rects (`Rect 0,0,64,60`, `Rect 0,60,64,156`, `Rect 0,156,64,288`). The OBJMAP 1000 dump contains those exact strings verbatim. **The format hypothesis reproduces the oracle.**

## TL;DR

OBJMAP is a **tab-separated text format** with `\r\n` line endings. It is essentially an INI-meets-SDL declaration of how a plugin's RT_BITMAP sprite sheet is sliced into named parts (toolbox icons, primary views, walking-people frames, etc.). A trivial line-based parser is sufficient; no binary reverse engineering is required.

## Encoding

- 8-bit ASCII (no UTF-8 BOM; high bit is never set in observed samples).
- Field separator inside a line: `\t` (tab).
- Line terminator: `\r\n` (CRLF, Windows convention).
- Block separator: a blank `\r\n` (i.e. two consecutive CRLFs).
- File terminator: the literal word `End\r\n`.

## Block grammar

```
DefParts\tName\t<semantic-name>\r\n          (block header)
\t\t<key>\t<value>\r\n                        (zero or more attribute lines)
\t\tList\tRect x1,y1,x2,y2 [flags...]\r\n    (the sprite rectangle)
\r\n                                          (blank line ends block)
```

### Keys observed in attribute lines

| Key       | Meaning                                                                                    |
|-----------|--------------------------------------------------------------------------------------------|
| `ID`      | Numeric type id. `101` = toolbox icons, `100` = primary views, `103`/`1003` = SHIL frames. The `Name` field is a more reliable semantic — IDs vary slightly across plugins. |
| `Pattern` | Sub-id within a type. For `View` blocks this is the frame index (100, 101, 102, ...).     |
| `Span`    | Only used for multi-story items (Stairs): 2/3/4 for the 2/3/4-story variant. Mutually exclusive with `Pattern`. |
| `List`    | The actual sprite data line. Always `Rect x1,y1,x2,y2 [flags]`.                           |

### Rect semantics

`Rect x1,y1,x2,y2` is in pixel coordinates inside the RT_BITMAP sheet whose id matches the OBJMAP id (i.e. OBJMAP 1000 describes RT_BITMAP 1000, OBJMAP 100 describes RT_BITMAP 100, OBJMAP 200 describes RT_BITMAP 200). The rect is **(left, top, right, bottom)**, not (left, top, width, height). Sprite width = `x2 - x1`, height = `y2 - y1`.

### Rect flags

| Flag          | Meaning                                                                                         |
|---------------|-------------------------------------------------------------------------------------------------|
| `Transparent` | The sheet uses color-key transparency. The transparent color is NOT declared here; it has to be discovered per-plugin by inspecting the bitmap (e.g. the magenta `Color(105, 33, 104)` used by `Condo.t2p`). |
| `Halftone`    | The rect is a master/cursor rect covering the full sheet (typically the first block, named `Csr`). |

## Per-OBJMAP-id semantics

Empirically verified across 5 calibration plugins. The ids are stable across the plugin set but not all plugins have all ids.

### OBJMAP id=100 — toolbox icons

Always 5 blocks, always references RT_BITMAP id=100 (which is consistently 26×130 across plugins).

- 3 blocks named `Category` with `ID=101`, `Pattern=200..202` (or 300..302, 500..502, ...): the parent category's icon variants.
- 2 blocks named `<Item Name>` with `ID=101`, `Pattern=0` and `Pattern=1`: the item's own toolbox icon in unselected and selected states.

This is the data needed to populate `ToolboxWindow` for a descriptor-driven item.

### OBJMAP id=200 — SHIL (shadow) people frames

Observed on transport plugins (Stairs). Hundreds of small (8×24 or 16×24) `Transparent` rects covering a tall narrow sheet (Stair's RT_BITMAP 200 is 32×768). The pattern ids encode direction and frame:

- `Pattern 1000-1990` (odd/even pairs): one walking direction
- `Pattern 2000-2990`: second direction
- `Pattern 3000-3311`: third direction

Use these to drive the people-on-stairs animation. Only present for transport plugins that show people walking (Stairs, Escalator). Not present for Burger/Condo/Cathedral.

### OBJMAP id=1000 — primary sprite layout

The most important OBJMAP. Describes the main item sprite(s) inside RT_BITMAP 1000. Two variants:

**Multi-span form** (Stair): blocks use `Span N` instead of `Pattern N`. Each block is a different story-count variant of the item. Used for stairs and possibly elevators.

**Multi-pattern form** (Burger, Condo, Cathedral): blocks use `Pattern 100, 101, 102, ...`. Each block is a different visual state of the item:
- Burger: 4 patterns (likely open/closed/day/night).
- Condo: 15 patterns (3 variants × 5 lighting states — empty, day, lit, night, dark — matching the YootCondo `frameIndex = variant * 5 + stateIndex` math in `source/Item/YootCondo.cpp:210`).
- Cathedral: 4 patterns.

A first block named `Csr` with no `ID`/`Pattern` declares the master rect (always covers the full sheet, marked `Halftone`).

### OBJMAP id=1100 — shop-specific SHIL

Observed on commercial plugins (Burger). A small set of 6 SHIL frames for the in-shop walking animation. Same grammar as OBJMAP 200. Not present on Stair (Stair uses id=200 instead) or Cathedral (no people anim).

## Coverage of the calibration set

| Plugin        | OBJMAP 100 (icons) | OBJMAP 200 (stair-SHIL) | OBJMAP 1000 (primary) | OBJMAP 1100 (shop-SHIL) |
|---------------|:---:|:---:|:---:|:---:|
| Stair.t2p     | 5 blocks   | **9295 b** (96 frames) | 3 spans (calibration oracle ✓) | — |
| Burger.t2p    | 5 blocks   | — | 4 patterns (128×24 each, sheet 128×96) | 6 frames |
| Condo.t2p     | 5 blocks   | — | 15 patterns (128×24 each, sheet 128×360) | — |
| Cathedral.t2p | 5 blocks   | — | 4 patterns | — |
| StdElev.t2p   | 5 blocks   | — | (multi-span) | — |

## Population-wide verification

`scratch/objmap_verify.py` sweeps every PE-format plugin (105 of them; the other 51 are SP-format and out of scope for OBJMAP) and asserts the format claims hold across the whole population. Results on first run:

- **105 / 105 plugins** start with the `DefParts` magic. ✓
- **101 plugins** have an OBJMAP 100.
- **104 plugins** have an OBJMAP 1000.
- **5 edge cases** (none of them blockers, all explained):
  - `Floor.t2p`, `KFloor.t2p` — OBJMAP 100 has 7 blocks instead of 5. Floor is a category parent in the toolbox (it expands into Floor + bare structure variants), so it carries 2 extra category-icon blocks.
  - `DefaultToolPalette.t2p` — OBJMAP 1000 has no View blocks. This plugin is toolbox UI chrome, not a buildable item.
  - `Hawai.t2p`, `Tokyo.t2p` — OBJMAP 1000 has no View blocks. These are *scenario/location* plugins (different tower settings), not items.

**Conclusion: the format is consistent across every gameplay-item plugin.** The 5 edge cases are non-item plugins that can be filtered out by the audit by checking for "at least one View block in OBJMAP 1000" — which is also the right shape for "is this a buildable item?" detection.

## Implementation guidance for the C++ parser (D.2)

A 100-line line-based parser is sufficient:

1. Verify the resource length is non-zero and the first 8 bytes decode as ASCII `"DefParts"`. If not, return empty (defensive — future plugins might have a binary variant).
2. Split the input on `\r\n`. Iterate lines, tracking whether we're inside a `DefParts` block.
3. A line starting with `DefParts\tName\t` opens a new block and records the semantic name.
4. A line starting with `\t\t` is an attribute; split on `\t`, key = field 1, value = field 2.
5. A `List\tRect a,b,c,d [flags]` line is the rect; parse the four comma-separated integers and the trailing flags.
6. An empty line closes the current block.
7. `End` terminates the file.

Output shape: a vector of `Block { std::string name; int id; int pattern; int span; Rect rect; std::vector<std::string> flags; }`. The audit tool can then group by `name`, count patterns, and answer the coverage questions without needing to know the per-plugin idiosyncrasies.

## Open questions for the long tail

These don't block Phase D.2/D.4 but should be revisited when descriptor-driven items land:

- **The transparent color** isn't in OBJMAP. Per-plugin discovery requires either scanning the bitmap corners or hand-curating a small override table. Today only `Condo.t2p` (`Color(105, 33, 104)`) and the resident overlay (`Color::White`) are known and hard-coded in `YootCondo.cpp:loadTextures`.
- **Animation timings** (how many milliseconds per SHIL frame) aren't in OBJMAP either. SimTower's 30 Hz constant is the obvious first guess.
- **The `Csr` / Halftone block's purpose** is unclear beyond "this is the master rect". Probably used as the in-place cursor hit area; can be ignored for the audit, used for the eventual descriptor-driven rendering.

## Acceptance proof

For `Stair.t2p` OBJMAP id=1000, the parser must produce three `View` blocks with these rects (matching `extract_t2p_stairs.py:11-13`):

```
Span 2: Rect(0, 0,   64, 60)
Span 3: Rect(0, 60,  64, 156)
Span 4: Rect(0, 156, 64, 288)
```

The raw bytes decoded above contain the literal strings `"Rect 0,0,64,60"`, `"Rect 0,60,64,156"`, `"Rect 0,156,64,288"`, so a correct parser cannot fail this.
