# AGENTS.md

This file provides guidance to agents when working with code in this repository.

## Build/Lint/Test Commands

**Build firmware:**
```bash
cmake -S . -B build -G Ninja -DCPPCHECK_USE_ADDONS=ON
cmake --build build --target jerry_app
```

**Format code (C/C++ & Python):**
```bash
cmake --build build --target format
```

**Run all quality checks:**
```bash
cmake --build build --target lint
```

**Run unit tests (Modbus library):**
```bash
# From project root
cmake -S tests/unit -B build_tests
cmake --build build_tests
./build_tests/modbus_tests
```

**Run specific unit test:**
```bash
# Build first if needed
cmake --build build_tests
# Then run specific test executable (requires modifying test_runner.c temporarily)
# Better approach: Add new test to test_runner.c and rebuild
```

**Run integration tests (requires STM32 device):**
```bash
uv sync
pytest tests/integration/ -v
# Skip hardware tests: pytest tests/integration/ --no-hardware
```

**Run Python unit tests (code generator):**
```bash
pytest tests/unit_python/ -v
```

## Code Style Guidelines

**C/C++ (based on .clang-format):**
- BasedOnStyle: Google with specific offsets:
  - AccessModifierOffset: -1
  - ConstructorInitializerIndentWidth: 4
  - ContinuationIndentWidth: 4
  - IndentWidth: 4
  - TabWidth: 8 (UseTab: Never)
- ColumnLimit: 80
- BreakBeforeBraces: Allman
- AllowShortFunctionsOnASingleLine: All
- AllowShortIfStatementsOnASingleLine: WithoutElse
- AllowShortLoopsOnASingleLine: true
- AlwaysBreakBeforeMultilineStrings: true
- PointerAlignment: Left
- SortIncludes: CaseSensitive
- IncludeCategories: System headers first (<ext/>, then <>), then ""

**Python (based on pylintrc & ruff.toml):**
- max-line-length = 80
- indent-width = 4
- indent-string = '    ' (4 spaces)
- quote-style = "double"
- indent-style = "space"
- line-ending = "auto"
- target-version = "py310"

**Project-Specific Patterns:**
- TrustZone SysCTL handling: Secure world disables SysTick before jumping to non-secure (`SysTick->CTRL = 0`)
- FreeRTOS static allocation: No dynamic memory allocation anywhere in firmware
- Modbus callback architecture: Hardware state updates happen in callbacks (modbus_device_callbacks.c)
- Code generation: Modbus registers auto-generated from JSON via Jinja2 templates
- Static analysis: CppCheck with MISRA addons requires specific setup (see README.md MISRA section)
- UV Python manager: All Python tooling managed via `uv` (not pip directly)