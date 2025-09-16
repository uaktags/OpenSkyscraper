This repo includes recommended tools to automate the project's coding-style and commit-message rules.

Quick setup
1. Install pre-commit (Python) if not present:

   pip install --user pre-commit

2. Install hooks locally (this will install clang-format hook and wire the local normalization script):

   pre-commit install --hook-type pre-commit

3. Enable the commit-msg hook by configuring Git to use the repository hooks directory:

   git config core.hooksPath .githooks

Notes
- `.pre-commit-config.yaml` runs `clang-format` on C++ files and a local `normalize_copyrights.sh` script.
- `.githooks/commit-msg` enforces a lightweight conventional commit pattern; enable it with `core.hooksPath` as above.

Customizing
- Adjust `clang-format` version or `--style` by adding a `.clang-format` file at repo root.
- Modify `scripts/normalize_copyrights.sh` if you want different header formats.
