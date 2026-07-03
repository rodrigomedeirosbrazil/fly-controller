# test/ Directory

This directory contains unit tests for the fly-controller firmware. It is the standard PlatformIO test directory, as described in the [PlatformIO Unit Testing docs](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html).

## Contents

- `PowerTest.cpp` - Unit tests for the `Power` class, covering battery limit calculation, motor temperature limiting, combined power limiting, and PWM output mapping.
- `README` - Default PlatformIO README for the test directory.

## How PlatformIO Testing Works

PlatformIO's test runner compiles and uploads test code to the target device (ESP32-C3), runs the tests, and reports results over serial. Tests in this project use the **Unity** testing framework, which is bundled with PlatformIO.

Since the ESP32-C3 is a microcontroller without a standard OS, tests must either:
- Run natively on the device (flashed and executed via serial), or
- Run on the host (native environment) by mocking or reimplementing hardware-dependent code.

Currently, `PowerTest.cpp` uses the **native host approach**: hardware dependencies (`Canbus`, `Throttle`, `MotorTemp`) are replaced with plain C++ mock structs, and Arduino-specific functions (`map`, `constrain`) are reimplemented locally. This allows the tests to compile and run without a connected device.

## Running Tests

```bash
# Run all tests (using PlatformIO from the project root)
~/.platformio/penv/bin/pio test

# Run tests for a specific environment
~/.platformio/penv/bin/pio test -e lolin_c3_mini

# Run a specific test file
~/.platformio/penv/bin/pio test --filter PowerTest
```

Tests must be run from the project root (where `platformio.ini` lives), not from within `test/`.

## Writing Tests for Embedded/Arduino Firmware

### File naming and structure

- Each test file should be a self-contained `.cpp` file in `test/`.
- Name test files after the class or module under test (e.g., `PowerTest.cpp`).

### Mocking hardware dependencies

The firmware depends on hardware peripherals (CAN bus, ADC, sensors). Tests must not rely on actual hardware. Instead:

- Define lightweight mock structs that replicate only the interface methods the class under test calls.
- Embed mocks directly in the test file or a shared `test/mocks/` header.
- Copy and adapt the class under test into the test file if it cannot be compiled without the full Arduino/ESP-IDF environment, removing hardware-specific includes.

### Reimplementing Arduino built-ins

Arduino's `map()` and `constrain()` are not available on the host. Reimplement them as static methods or free functions within the test file, matching Arduino's integer semantics exactly.

### Assertions

- Use `assert()` from `<cassert>` for host-native tests.
- For on-device PlatformIO/Unity tests, use `TEST_ASSERT_*` macros from `unity.h` and wrap tests in `setUp()`/`tearDown()`/`RUN_TEST()` as required by the Unity framework.

### Test function conventions

- Prefix each test function with `test_` followed by the method or scenario name (e.g., `test_calcBatteryLimit`).
- Each test function should cover one logical behavior, including boundary conditions and failure cases.
- Print a confirmation message at the end of each test function (`std::cout << "test_X passed\n"`) for easy diagnosis when running natively.
- Call all test functions from `main()` and return `0` on success.

### Avoid global state between tests

- Instantiate a fresh object at the start of each test function, or explicitly reset all relevant state, to prevent one test from influencing another.
