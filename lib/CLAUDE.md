# lib/

This is the PlatformIO standard location for private project libraries — code compiled as a static library and linked into the firmware.

## Current state

This directory is currently unused. All external dependencies are managed via `lib_deps` in `platformio.ini` (fetched from the PlatformIO registry or Git). Project source code lives in `src/`.

## When to add files here

Add a library here when you have self-contained code that:

- Is reusable across projects but not suitable for publishing to the PlatformIO registry
- Has a clean, stable API that should be isolated from the main `src/` build (e.g., a custom HAL wrapper, a signal processing utility)
- Needs its own `library.json` for custom build flags or dependency declarations

Each library should live in its own subdirectory: `lib/LibraryName/LibraryName.h` and `lib/LibraryName/LibraryName.cpp`.

If the code is tightly coupled to the flight controller and unlikely to be reused, keep it in `src/ComponentName/` instead.
