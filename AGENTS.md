# AGENTS.md — Jerry Firmware

Agent-facing guide for the Jerry firmware project. Canonical details live in the referenced files; this file only consolidates the commands and constraints an agent needs day-to-day.

## Repo essentials

- **App:** C + Assembly, FreeRTOS (static allocation), no dynamic memory allocation.
- **Build:** CMake → Ninja, single entry point `tools/build.py` (run via `uv run python tools/build.py`).
- **Tools:** Python (>=3.10), `uv`-managed, declared in `pyproject.toml`.
- **Tests:** Unity (C, `tests/unit/`), pytest + pymodbus (Python, `tests/integration/`).
- **Standards:** Google C++ style (`config/.clang-format`), Google Python style (`config/pylintrc`, `config/ruff.toml`), Google Shell style (`refs/shell_style_guild.md`).
- **MCP:** `.vscode/mcp.json` (currently `context7`); preserve existing entries when adding servers.

## Commands (run from repo root)

| Task | Command |
|------|---------|
| Debug build | `uv run python tools/build.py` |
| Release build | `uv run python tools/build.py --profile release` |
| Configure only | `uv run python tools/build.py configure --profile <debug\|release>` |
| Clean one profile | `uv run python tools/build.py clean --profile <profile>` |
| Clean all | `uv run python tools/build.py clean-all` |
| Format (clang-format) | `uv run python tools/build.py format` |
| Lint all | `uv run python tools/build.py lint` |
| Package (CPack ZIP) | `uv run python tools/build.py package` |
| Flash | `uv run python tools/build.py flash --profile <profile> [--sn <serial>]` |
| Build C unit tests | `cmake -S tests/unit -B build_tests && cmake --build build_tests` |
| Run C unit tests | `./build_tests/modbus_tests` (or `ctest --output-on-failure` in `build_tests/`) |
| Run Python integration tests | `uv run python -m pytest tests/integration/` |

## Key files to reference (do not duplicate)

- `README.md` — project overview, features, prerequisites, build instructions.
- `project_requirements.md` — requirements matrix, directory structure, constraints.
- `CONTEXT.md` — ubiquitous language glossary.
- `pyproject.toml` — Python project deps & pytest config.
- `CMakeLists.txt` — root CMake, app + test targets, lint/format custom targets.
- `tools/build.py` — single source of truth for build/flash/lint/format/package.
- `tools/flash_nucleo.py` — STM32CubeCLT flashing (TrustZone + secure/non-secure apps).
- `config/.clang-format` — C/C++ formatting (BasedOnStyle: Google).
- `config/ruff.toml` — Python lint/format (line-length 80, py310).
- `config/pylintrc` — Python linting (max-line-length 80).
- `refs/cpp_coding_style.md`, `refs/python_coding_style.md`, `refs/shell_style_guild.md` — coding standards.
- `refs/misra_c_2012__headlines_for_cppcheck.txt` — MISRA C:2012 headlines.
- `refs/commit_message_generation_prompt.md` — commit message rules.
- `.gitignore` — `build/`, `.venv/`, `.vscode/`, caches, 3p deps are ignored.

## Constraints (agent must respect)

- Windows cmd.exe: avoid `sed`/`grep`/`awk`/`cat`/`rm`/`cp`/`mv`; use PowerShell or built-ins.
- cmd.exe line-length limit (~8191 chars): batch long file lists (see `tools/format.py`).
- No dynamic memory allocation in the app.
- All warnings enabled and treated as errors for every compiler.
- Release builds enforce clang-format pre-build; cppcheck/MISRA + lizard post-build (warnings, non-fatal by default).
- Build artifacts (`build/`, `.venv/`, `.vscode/`, caches) are gitignored — do not commit.
- 3p dependencies under `application/dependencies/` are auto-pulled and gitignored.
