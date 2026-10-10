---
name: "opensky-crossport"
description: "Porting changes between the OpenSky C++ and OpenSkyWeb JS repos, editing their sources, or running OpenSkyTests — produces verified ports with green tests."
---

# OpenSky Cross-Porting

Port changes between the sibling repos `~/Projects/OpenSky` (C++ SimTower remake) and `~/Projects/OpenSkyWeb` (pure-JS 1:1 conversion, Bun). The web repo was converted FROM the C++ but has diverged toward authentic SIMTOWER.EXE behavior — when porting web → C++, cite the web commit or `docs/issues/ISSUE-*.md` as evidence. `docs/issues/` doubles as the web repo's changelog; `docs/specs/` holds its porting bibles.

## 1. Scope the port

- List candidate improvements: `git log --oneline` and `docs/issues/` in the source repo.
- For each candidate, verify in BOTH codebases before porting:
  - The target actually lacks the behavior (grep the target's equivalent file).
  - The source fix is live code, not dead or render-only. Check guards that short-circuit before the interesting branch (example: the web's multi-story-lobby auto-connection is dead because `Lobby.canHaulPeople()` returns false and `gamemap.addNode` returns early for non-haulers).
  - Web-only bugs (JS porting artifacts) need no C++ fix; only port behaviors, self-heals, and data changes that make sense natively.
- Completion: a written port list with target file paths and the web commit/issue for each item.

## 2. Respect the dirty tree

- Run `git status --short` first. The C++ repo typically carries large in-flight uncommitted work on `feature/*` branches.
- Touch only the files your port needs; never stage, commit, revert, or line-ending-rewrite unrelated files. Leave your changes uncommitted unless the operator asks.
- Completion: you can list every file you intend to modify before editing.

## 3. Edit C++ sources with ending-aware patching

- Check endings per file: `file <path>` — the repos mix LF, CRLF, and files with BOTH endings line-by-line. The `edit` tool only matches LF-clean exact spans; multi-line edits on CRLF/mixed files fail or clip braces.
- For CRLF/mixed files use a line-based Python patcher: read with `splitlines(keepends=True)`, match blocks by whitespace-collapsed keys (`''.join(line.split())`), splice replacements using the anchor line's own EOL, and assert every block matched exactly once before writing.
- Keep needles complete: include trailing same-line comments and exact indentation (tabs vs 4/16 spaces vary per file).
- Write replacement blocks with their final indentation already applied — the patcher splices lines verbatim and does not re-flow. Unindented inserts compile but leave mangled formatting that costs a re-indent pass (repeated twice in one session before this rule landed).
- Completion: grep confirms each inserted marker (new field, branch, call) in place; `git diff --stat` shows no whole-file churn.

## 4. Build and test

- Full game: `cmake --build build -j$(nproc)` — fixes compile errors the test target cannot catch (test target compiles only a seam subset: Time, Route, SaveHash, LevelUp, plugins, ...).
- Tests: `cmake --build build --target OpenSkyTests`, then run `./OpenSkyTests` **from inside `build/`** — the tests resolve fixtures via `../data/...` relative paths, so running from the repo root produces dozens of phantom failures.
- FAIL lines go to stderr; pipe `2>&1` when counting.
- When a port deliberately changes behavior (pricing, LevelUp tables, unlock mapping), update the affected test expectations in the same change and note why.
- New pure-logic modules (e.g. `source/SaveHash.cpp`) must be added to the `OpenSkyTests` target in CMakeLists.txt; the main executable globs `source/*.cpp` automatically.
- Register new tests by hand in `tests/test_main.cpp`: define `static void testX()` and call it explicitly from `main()` — nothing auto-discovers tests, so an unregistered test never runs and "All tests passed" can be silently green. After registering, confirm the new PASS lines actually appear in the output.
- Runtime smoke-load (the seam tests cannot do this: they don't link Game): from the repo root run `timeout 90 xvfb-run -a ./build/OpenSkyscraper --load-game <save>` and confirm the log line `Game loaded from <save>`. The game keeps running until the timeout kills it, so never omit the timeout; CWD must be the repo root so `data/` (SIMTOWER.EXE, fonts) resolves.
- Any save-format, load-path, or generated-save change gets this check even with green tests — OpenSkyTests passed on a save whose real-binary load aborted (astar-search fixed-node-allocator assert).
- A load that never prints `Game loaded` before the timeout is a hang, not a crash: suspect eager per-item work during decode or layout update (Game::updateRoutes once ran one lobby A* per room, freezing full-lot saves for minutes). Layout-wide recomputation must stay O(N) invalidation; searches go through the time-budgeted `Game::pumpRouteBuilds()` queue drained from `Game::advance`.
- A crash during the operator's interactive playtest rarely reproduces headlessly (mouse drags cannot be scripted) and leaves a core: `coredumpctl list` shows recent OpenSkyscraper entries, and `coredumpctl gdb <PID>` with a `bt` command returns the exact failing frame and source line — read it before hunting call sites statically.
- Completion: full build succeeds and `./OpenSkyTests` from `build/` prints "All tests passed".

## 5. Baseline suspicious failures

- If tests fail and you suspect pre-existing causes, build a baseline in a temp worktree: `git worktree add /tmp/<name> HEAD`, `cmake -S . -B build`, build `OpenSkyTests`, run from its `build/`, compare FAIL sets, then `git worktree remove --force /tmp/<name>`.
- Attribute failures before changing code: dirty-tree deletions (e.g. removed `data/bitmaps/unhandled/*`) and wrong CWD both produce failures your edits did not cause.
- Completion: every remaining failure is either explained as pre-existing or fixed.

## 6. Document the port

- Append a dated section to `TODO.md` ("Ported from OpenSkyWeb (YYYY-MM-DD)") listing each port with its source commit/issue, plus explicitly NOT-ported candidates and why (dead code, render-only, missing items).
- Record follow-up candidates there so the next pass can start from the list.
- Completion: TODO.md section exists and the final report names touched files and commit posture.

## 7. Ship the ports (commit, push, PR)

- When the operator asks to commit a tree that mixes their in-flight work with port changes, split by pathspec excludes: stage their work first with `git add -A -- ':(exclude)<each port file>'`, commit, then a bare `git add -A` for the ports commit. Order commits so the LAST tree is the state that was built and tested.
- Attribute shared files (CMakeLists.txt, tests/test_main.cpp, SimTowerLoader.cpp, TODO.md) to whichever side dominates and note the ride-along in the message; do not attempt hunk-level separation.
- `git add` aborts entirely if any pathspec names an ignored path (`scratch/` is gitignored) — never exclude ignored dirs.
- Keep `memory/` out of the repo (agent session notes, not game code): gitignore it before staging — the final bare `git add -A` re-sweeps untracked meta dirs otherwise; `git rm --cached` + `git commit --amend` if one slipped into a commit.
- The PR base is `master` (there is no `main`); push is a plain fast-forward to the tracked `origin/feature/*` branch, then `gh pr create --base master` (gh is authenticated as uaktags). Note submodule pointer bumps in the PR body.
- Completion: `git log` shows the split commits, push fast-forwards, and the PR URL is returned.
