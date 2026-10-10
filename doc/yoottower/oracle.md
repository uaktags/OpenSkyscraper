# YootTower dynamic oracle (original T2.exe under Wine, headless)

The original YootTower (OPeNBooK9003 / Sega, 1999, Win32 + MFC42) runs under the local
Wine on a headless Xvfb, so agents can start it, load a `.bld`, screenshot it and click
around. The original files in `~/Downloads` are only read. Everything else lives in
`~/yoottower-decomp/`.

| Path | Contents |
|---|---|
| `tools/yoottower/oracle/run_yoot.sh` | low-level: setup / start / shot / click / key / stop |
| `tools/yoottower/oracle/drive.sh` | flows: title, new, open, load, pause |
| `tools/yoottower/oracle/xinput.py` | xdotool replacement (python-xlib + XTEST) and screenshots |
| `~/yoottower-decomp/game/YOOT/` | working copy of the game (C:\YOOT is a symlink to it) |
| `~/yoottower-decomp/wineprefix/` | dedicated Wine prefix |
| `~/yoottower-decomp/oracle-state/` | PID files, NOTES.md |
| `~/yoottower-decomp/oracle-shots/` | screenshots |
| `~/yoottower-decomp/logs/` | wine and Xvfb logs (`game.log`) |

## Quick start

```sh
O=tools/yoottower/oracle
$O/run_yoot.sh setup                      # idempotent; start runs it if needed
PAUSE=1 $O/drive.sh load "Sample Tokyo Map.bld" ~/yoottower-decomp/oracle-shots/tokyo.png
$O/run_yoot.sh click 525 130              # Control palette: run/pause toggle
$O/run_yoot.sh shot ~/yoottower-decomp/oracle-shots/x.png
$O/run_yoot.sh stop                       # kills T2, this prefix's wineserver, Xvfb
```

### run_yoot.sh

```
setup                        prefix + game copy + C:\YOOT link + mfc42.dll + registry
start [save]                 Xvfb :87 (1024x768x24) + T2.exe [save]; waits until the UI is up
                             (the title dialog, or "<save> - T2" with the "Loading" window gone)
shot OUT.png                 full-screen PNG
click X Y [btn] | dclick X Y | rclick X Y | move X Y | drag X1 Y1 X2 Y2
key KEY...                   Return Escape alt+f ctrl+o F1 ...
type "text"                  ASCII text, including C:\paths
windows                      visible windows: id WxH +X+Y title
wait-window REGEX [t]        wait-gone REGEX [t]
status | log | stop
```

A save can be given as a bare name in the game dir (`Yooooot.bld`), a Windows path
(`C:\YOOT\x.bld`), or any host path. Host paths are converted to `Z:\...` with `winepath`.
You can change these with environment variables: `YOOT_ROOT`, `YOOT_SRC`, `YOOT_DISPLAY`
(default `:87`), `YOOT_SCREEN` (default `1024x768x24`), `MFC42_DLL`, `WINEDEBUG`.

### drive.sh

```
title [png]          stop, fresh start, click through the intro, stop at the NEW/OPEN/QUIT dialog
new [png]            (from title) NEW -> "World Select Dialog"
open <save> [png]    (from title) OPEN -> type the path in the file dialog -> Return -> wait for load
load <save> [png]    restart with the save on the command line (fastest; preferred)
pause | resume       toggle the Control palette run/pause button
geom REGEX           "X Y W H" of a window by title
```

`PAUSE=1` with `load`/`open` pauses the clock as soon as the save has loaded.

## What it took

* **Wine**: Fedora `wine-11.0 (Staging)`. This build is pure wow64, so `WINEARCH=win32` is
  rejected and the prefix is a default wow64 one. 32-bit code runs fine.
* **MFC42.DLL**: T2.exe and T2DLL.dll import `MFC42.DLL`, and Wine has no builtin version.
  `setup` copies a local VC6 redistributable `mfc42.dll` into `syswow64`. It comes from the
  user's Bottles "YootTower" bottle (vcredist6sp6, md5 `48b74cc8...`) or from `$MFC42_DLL`.
  Nothing is downloaded. The fallback is `winetricks mfc42`. msvcrt is Wine's builtin.
* **Other imports**: WINMM, ole32, DINPUT, DSOUND, GDI32, USER32, ADVAPI32. There is **no
  DirectDraw**: rendering is GDI only, so no DLL overrides are needed. With no audio device,
  DirectSound fails quietly.
* **CD check**: T2.exe@0x40d7b4 calls `GetVolumeInformationA` and compares the volume label
  with `"YootTower"`. In this copy the branch at `0x40d7ee` is already `jmp 0x40d81d`, so the
  "requires the Yoot Tower CD-ROM" box can never appear. The EN and RIP T2.exe/T2DLL.dll are
  byte-identical, and both are this patched "CD-rip". No CD image is needed. `mcicda` is
  probed once (CD audio) and then unloaded, which is harmless.
* **256-colour check**: `GetDeviceCaps(RASTERCAPS) & RC_PALETTE` fails on a true-colour
  display, so the game shows "Please set the display mode to 256 colors". `setup` suppresses
  this with
  `HKCU\Software\OPeNBooK9003\YOOT Tower\Settings  CheckDisplay = REG_DWORD 0`.
  The game itself writes this value if you tick "Message will not be displayed hereafter".
  The other game keys are under `HKCU\Software\OPeNBooK9003\YOOT Tower\` (`Recent File List`,
  `Status`, `Settings`).
* **Display**: use Xvfb depth **24**. With depth 8 the game runs, but the colours are
  garbage. Wine's palette emulation on 24-bit looks correct.
* **No virtual desktop**: with `wine explorer /desktop=...` the OPeNBooK splash shows, but
  the title dialog and main window never paint (they stay blank white). Plain top-level X
  windows on Xvfb with no window manager work, so `start` runs `wine C:\YOOT\T2.exe` directly.
* **QuickTime / movies**: nothing is needed to start or to load saves. The theatre movies
  (`Movie/*.MOV`) go through "Active Movie" (DirectShow/quartz), not QTML. Movie playback
  is **untested**; Wine's quartz probably cannot decode these QuickTime .MOVs.
* **Plugin paths in saves**: the saves embed absolute plugin paths
  (`C:\PROGRAM FILES\YOOTTOWER\YOOTTOWER\PLUGINS\HOUSESALE.T2P`, `C:\YOOT\PLUGINS\ALOHA.T2P`).
  Both saves load correctly from `C:\YOOT`, so plugins are not resolved from those absolute
  paths, or a mismatch is not fatal.
* **Loading saves**: `T2.exe "C:\YOOT\<name>.bld"` opens the document directly (MFC
  command-line open). The title-screen OPEN file dialog works too.
* **Input**: xdotool is not installed. `xinput.py` uses python-xlib (user site-packages)
  with XTEST, and the PIL `get_image` for screenshots (ImageMagick `import -window root`
  also works). Run it with `python3 -P`, not `-I`, because `-I` hides user site-packages.

## Startup sequence

1. "Presented by OPeNBooK9003" splash (full screen, black).
2. Intro animation in an untitled 640x480 window at (-5,-5). Any click skips it.
3. Main window "Untitled - T2" (768x576 at 0,0, blank) and the **"YOOT TOWER"** title dialog.
4. NEW opens **"World Select Dialog"** (494x388 at +265+190; Hawaii $5,000,000, 45 above /
   6 below, 400 m; other worlds with the arrows). OPEN opens the common file dialog
   "Open", which starts in C:\YOOT.
5. While a save loads, a "Loading" window (306x124) is shown.

## Screen coordinates (1024x768, absolute)

| UI | Window geometry | Points |
|---|---|---|
| Title dialog "YOOT TOWER" | 466x344 +279+212 | NEW (635,295), OPEN (635,413), QUIT (635,503) |
| File "Open" dialog | 422x241 +173+181 | File name field (380,370) has focus; Open (545,372); Cancel (545,400) |
| "Information" palette | 523x70 +0+0 | clock (27,34) + AM/PM (27,55); date (120,35); FUND (255,35); POPULATION (380,35); message line (250,56); rating star (480,45) |
| "Control" palette | 133x76 +500+100 | run/pause (525,130): red "‖" = running, green "▶" = paused; speed slider (585,128); Main (526,149) Pricing (566,148) Eval (607,148) Hotel (526,165) Traffic (566,165) Outside (607,165) |
| "Tools" palette | 92x359 +100+100 | tool icons from (120,140); it covers the left end of the main menu bar |
| Main window | depends on the save: Tokyo 760x524 +48+74, Yooooot 760x503 +114+140 | menu bar about 11 px below the top (File, Function, Favorite, Window, Help); horizontal scrollbar at the bottom |

Get live positions with `run_yoot.sh windows` or `drive.sh geom REGEX`. drive.sh computes
clicks relative to the window, not from the fixed numbers above.

## Verified stages (screenshots in ~/yoottower-decomp/oracle-shots/)

* `stage_a_splash_openbook.png`, `stage_a_intro.png`: splash and intro.
* `stage_b_title.png`: NEW/OPEN/QUIT dialog. `new.png`: World Select (Hawaii).
* `stage_c_tokyo_via_open.png`: *Sample Tokyo Map.bld* loaded through the OPEN dialog,
  paused. YEAR 1 Jan. WD, about 09:38 AM, FUND $7,877,000, POPULATION 2 (56 after about
  40 s of running). It shows a 10-storey Tokyo tower: a 3-storey atrium lobby with a red
  sculpture, an ice-cream shop on 4F, offices on 5F-7F, empty floors with a "For RENT" sign
  on 8F-10F, an elevator shaft with floor numbers, stairs at both ends, and a city skyline
  behind.
* `stage_c_yooooot.png`: *Yooooot.bld* via the command line, paused. YEAR 1 Feb. WD, about
  10:51 AM, FUND $5,000,000, POPULATION 0. It is the Hawaii (Aloha) world with an **empty
  lot**: a Waikiki/Diamond Head backdrop with palm trees, B1 floor slab, and ocean with a
  dolphin at B2-B3 on the left. It is effectively a fresh Hawaii start.

## Limitations

* The clock runs during and right after loading, so the first readable time is a few game
  minutes past the saved time. Loading an empty tower runs very fast (hours per second).
  Use `PAUSE=1` and read values after pausing. Exact saved values should come from the
  .bld decoder.
* There is no window manager, so windows have no real frames and cannot be moved with the
  mouse by title bar. The game's own palettes have captions.
* Only one instance per display/prefix. `stop` uses `wineserver -k` for this prefix only.
  For concurrent runs (e.g. several agents), give each caller its own instance:
  `YOOT_INSTANCE=econ tools/yoottower/oracle/run_yoot.sh start ...`. That uses a private
  prefix, game copy, state dir and X display under `$YOOT_ROOT/instances/econ/`, so one
  caller's `stop` can't kill another's session or clobber its saves.
* No sound. Movies and CD music are untested.
* The Tools palette hides the File and Function menus on saves whose main window sits at
  x=114 (Yooooot.bld). `key alt+f` did **not** open the menu in testing, because keyboard
  focus stays on the palette after a click. Close the Tools palette (its "x" at (179,111))
  or load through the title dialog. Keyboard shortcuts may need focus on the main window
  first; this is untested.
