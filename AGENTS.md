# AGENTS.md

Cross-agent conventions for contributing to Fly Controller. **For full context read [`CLAUDE.md`](./CLAUDE.md)** — it is the source of truth and is kept more detailed than this file.

Non-Claude agents (Cursor, Gemini, Copilot, Codex, ChatGPT, and others) can start at [`LLMs.md`](./LLMs.md) for a neutral overview.

## Build system

PlatformIO on ESP32-C3 (`lolin_c3_mini`), Arduino framework. **Never** call `pio` directly — it is not on PATH:

```bash
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor     # default, CAN/UAVCAN
~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag        # PWM-only, no CAN
~/.platformio/penv/bin/pio run -e <env> -t upload
~/.platformio/penv/bin/pio device monitor -e <env>
~/.platformio/penv/bin/pio test -e <env> --filter <name>
```

Host tests under `test/` compile with `c++ -std=c++17` and never touch hardware.

## Build targets

| Environment | `CONTROLLER_TYPE` | Macros | CAN bus |
|---|---|---|---|
| `lolin_c3_mini_tmotor` (default) | 3 | `IS_TMOTOR=1` | 1 Mbps |
| `lolin_c3_mini_xag` | 1 | `IS_XAG=1` | None |

Value `2` is a retired Hobbywing type. `USES_CAN_BUS` is 1 for Tmotor, 0 for XAG.

## Coding conventions

- All code, comments, commit messages, identifiers: **English**. Only exception: user-facing strings in the web portal (Brazilian Portuguese).
- Build guards: `#if IS_TMOTOR` / `#if IS_XAG` / `#if USES_CAN_BUS` — always defined as 0 or 1. **Never** use `#ifdef`.
- No `delay()` in `loop()` — use `millis()` timing.
- Units: temperatures millicelsius (`int32_t`), voltages millivolts (`uint16_t`), currents milliamps (`uint32_t`), capacity milliamp-hours.
- Naming: camelCase for functions/variables, PascalCase for classes.
- No magic numbers: use `#define` or `const`.

## Component pattern

Each feature lives in `src/ComponentName/ComponentName.h` and `.cpp`. To add one:

1. Create `src/ComponentName/ComponentName.h` and `.cpp`.
2. Add `extern ComponentName componentName;` to `config.h` (guarded by `#if` if build-specific).
3. Instantiate in `config.cpp`.
4. Call `componentName.setup()` in `setup()` and `componentName.handle()` in `loop()`.

Globals: only `config.cpp` instantiates global objects (`config.h` holds `extern` declarations). Exception: `ControllerWebServer webServer` and `Logger logger` live in `main.cpp`; `Telemetry telemetry` is defined at the bottom of `Telemetry/Telemetry.cpp`.

## File discovery hierarchy

When working inside a subdirectory, read its `CLAUDE.md` if present. If a directory has none, climb one level until you find one. There are `CLAUDE.md` files at the repo root, `src/`, `test/`, `docs/`, `include/`, and `lib/`.

## Working files (never commit)

Gitignored working documents: `docs/superpowers/`, `docs/*.html`. Keep them local only.
