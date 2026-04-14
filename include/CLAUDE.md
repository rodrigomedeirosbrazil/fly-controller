# include/

This is the PlatformIO standard location for project-wide header files shared across multiple source files.

## Current state

This directory is currently unused. All headers in this project are co-located with their corresponding `.cpp` implementation files under `src/ComponentName/` (e.g., `src/Imu/Imu.h` lives next to `src/Imu/Imu.cpp`).

## When to add files here

Place a header here only when it needs to be shared across multiple unrelated components and has no natural owner in `src/`:

- Global type definitions or enums used project-wide (e.g., `types.h`)
- Shared constants or configuration macros that span subsystems
- Abstract interfaces (pure virtual base classes) that multiple components implement

If a header belongs to a single component, keep it in that component's `src/ComponentName/` directory instead.
