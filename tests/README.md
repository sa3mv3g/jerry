# Modbus Library Tests

This directory contains comprehensive tests for the custom Modbus library.

## Test Structure

```
tests/
├── unit/                    # Unity-based C unit tests
│   ├── CMakeLists.txt       # Build configuration
│   ├── test_runner.c        # Main test runner
│   ├── test_modbus_crc.c    # CRC-16 tests
│   ├── test_modbus_lrc.c    # LRC tests
│   ├── test_modbus_pdu.c    # PDU encoding/decoding tests
│   ├── test_modbus_rtu.c    # RTU framing tests
│   ├── test_modbus_ascii.c  # ASCII framing tests
│   └── test_modbus_tcp.c    # TCP framing tests
├── integration/             # Python pymodbus integration tests
│   ├── conftest.py          # Pytest fixtures
│   ├── test_config.py       # Test configuration
│   ├── test_coils.py        # Coil tests (FC01, FC05, FC15)
│   └── test_holding_registers.py  # Register tests (FC03, FC06, FC16)
└── README.md                # This file
```

## Unit Tests (Unity Framework)

### Prerequisites

- CMake >= 3.22
- C compiler (GCC, Clang, or MSVC)
- Unity test framework (automatically fetched by CMake)

### Building Unit Tests

```bash
# From project root
cmake -S tests/unit -B build_tests
cmake --build build_tests

# Run tests
./build_tests/modbus_tests
```

### Running with CTest

```bash
cd build_tests
ctest --output-on-failure
```

### Test Coverage

| Module | Tests | Description |
|--------|-------|-------------|
| CRC-16 | 8+ | CRC calculation and verification |
| LRC | 10+ | LRC calculation and ASCII conversion |
| PDU | 15+ | PDU encoding/decoding for all function codes |
| RTU | 11+ | RTU frame building, parsing, timing |
| ASCII | 6+ | ASCII frame building and parsing |
| TCP | 6+ | TCP/MBAP frame handling |

## Integration Tests (pymodbus)

### Prerequisites

- Python >= 3.10
- pymodbus >= 3.6.0
- pytest >= 7.0.0
- STM32 target device running Modbus slave

### Installation

```bash
# Using uv (recommended)
uv sync

# Or using pip
pip install pymodbus pytest pytest-asyncio
```

### Configuration

Edit `tests/integration/test_config.py` to match your setup:

```python
MODBUS_HOST = "192.168.1.100"  # STM32 IP address
MODBUS_PORT = 502
MODBUS_UNIT_ID = 1
```

### Running Integration Tests

```bash
# Run all integration tests
pytest tests/integration/ -v

# Run specific test file
pytest tests/integration/test_holding_registers.py -v

# Run with verbose output
pytest tests/integration/ -v --tb=long

# Skip hardware tests (for CI without device)
pytest tests/integration/ --no-hardware

# Generate JUnit XML report
pytest tests/integration/ --junitxml=test_results.xml
```

### Test Categories

| Category | Marker | Description |
|----------|--------|-------------|
| Hardware | `@pytest.mark.hardware` | Requires physical device |
| Slow | `@pytest.mark.slow` | Long-running tests |
| Stress | `@pytest.mark.stress` | Stress/load tests |

### Running Specific Categories

```bash
# Run only hardware tests
pytest tests/integration/ -m hardware

# Skip slow tests
pytest tests/integration/ -m "not slow"

# Run stress tests
pytest tests/integration/ -m stress --timeout=600
```

## Test Plan

See [plans/modbus_library_test_plan.md](../plans/modbus_library_test_plan.md) for the complete test plan including:

- Test architecture
- Detailed test cases
- Test data and vectors
- Pass/fail criteria
- Coverage requirements

## Adding New Tests

### Unit Tests

1. Create a new test file `test_modbus_<module>.c`
2. Include Unity and modbus headers
3. Implement test functions with `void test_<name>(void)` signature
4. Add test declarations to `test_runner.c`
5. Add `RUN_TEST(test_<name>)` calls in main()

### Integration Tests

1. Create a new test file `test_<feature>.py`
2. Import pytest and pymodbus
3. Use the `modbus_client` fixture for connection
4. Mark hardware-dependent tests with `@pytest.mark.hardware`

## Troubleshooting

### Unit Tests

**Unity not found**: CMake will automatically fetch Unity. Ensure internet access.

**Compilation errors**: Check that modbus library headers are in the include path.

### Integration Tests

**Connection refused**: Verify STM32 is running and IP address is correct.

**Timeout errors**: Increase `TIMEOUT` in test_config.py.

**Import errors**: Run `uv sync` or `pip install -r requirements.txt`.

## Continuous Integration

Example GitHub Actions workflow:

```yaml
name: Modbus Tests

on: [push, pull_request]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build unit tests
        run: |
          cmake -S tests/unit -B build_tests
          cmake --build build_tests
      - name: Run unit tests
        run: ./build_tests/modbus_tests

  integration-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: astral-sh/setup-uv@v4
      - name: Install dependencies
        run: uv sync
      - name: Run integration tests (no hardware)
        run: uv run pytest tests/integration/ --no-hardware -v
```
