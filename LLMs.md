# LLMs.md

Neutral entry point for any coding agent working on this repository (Cursor, Gemini, Copilot, ChatGPT, Codex, and others). Claude Code reads `CLAUDE.md` natively; this file gives every other agent the same context.

## What this is

ESP32-C3 (Lolin C3 Mini) Arduino/PlatformIO firmware for an intelligent drone/UAV flight controller. Two build targets:

- `lolin_c3_mini_tmotor` (default) — UAVCAN ESC over CAN bus at 1 Mbps
- `lolin_c3_mini_xag` — PWM-only ESC, analog sensors, no CAN bus

A wireless remote-throttle firmware that pairs with this controller lives in a separate repo: [fly-throttle](https://github.com/rodrigomedeirosbrazil/fly-throttle).

## Read these next

- [`CLAUDE.md`](./CLAUDE.md) — **source of truth**, detailed. Build system, project structure, key patterns, pinouts, power & safety logic, web portal, ESP-NOW remote link. (Other agents: if your tool does not auto-load `CLAUDE.md`, read it manually before doing anything.)
- [`AGENTS.md`](./AGENTS.md) — cross-agent conventions (build commands, coding rules, file discovery). Start here if you are not Claude Code.
- [`README.md`](./README.md) — human-facing overview, system architecture, hardware, features.
- [`platformio.ini`](./platformio.ini) — build environments and library dependencies.

## Per-directory context

When working inside a subdirectory, read its `CLAUDE.md` if present. If a directory has no `CLAUDE.md`, climb one level until you find one.

- [`src/CLAUDE.md`](./src/CLAUDE.md) — firmware source: key files, component pattern, units, conventions, per-component reference.
- [`test/CLAUDE.md`](./test/CLAUDE.md) — host-testable C++ unit tests (no Arduino deps for the pure-logic headers).
- [`docs/CLAUDE.md`](./docs/CLAUDE.md) — user-facing manuals (Portuguese) and hardware schematics.
- [`include/CLAUDE.md`](./include/CLAUDE.md) — currently unused; when to add shared headers.
- [`lib/CLAUDE.md`](./lib/CLAUDE.md) — currently unused; when to add private project libraries.

## Build & test (critical)

**Never** call `pio` or `platformio` directly — it is not on PATH. Always use the full path:

```bash
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor     # build Tmotor
~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag        # build XAG
~/.platformio/penv/bin/pio run -e <env> -t upload          # upload
~/.platformio/penv/bin/pio device monitor -e <env>         # serial monitor
~/.platformio/penv/bin/pio test -e <env> --filter <name>   # run a host test
```

Host tests in `test/` are compiled with `c++ -std=c++17` and never touch hardware — pure logic only.

## Coding conventions (apply to every change)

- All code, comments, commit messages, identifiers: **English**. The only exception is user-facing strings in the web portal (Brazilian Portuguese).
- Build guards: `#if IS_TMOTOR` / `#if IS_XAG` / `#if USES_CAN_BUS` — these are always defined as 0 or 1. **Never** use `#ifdef`.
- No `delay()` in `loop()` — use `millis()` timing.
- Units everywhere: temperatures `int32_t` millicelsius, voltages `uint16_t` millivolts, currents `uint32_t` milliamps, capacity milliamp-hours.
- Naming: camelCase for functions/variables, PascalCase for classes.
- No magic numbers: use `#define` or `const`.

## Working files (never commit)

These are gitignored working documents for agentic workflows — keep local only: `docs/superpowers/`, `docs/*.html`.

## Releases

Pushing a git tag (`YYYY-MM-DD.N` convention, e.g. `2026-05-29.1`) triggers `.github/workflows/build-and-release.yml`, which builds both targets and publishes a GitHub Release with firmware binaries. The tag becomes `APP_VERSION` via a generated `src/Version.h`.
