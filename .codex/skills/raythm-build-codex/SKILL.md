---
name: raythm-build-codex
description: Build and smoke-test the raythm repository from Codex on Windows using cmake-build-codex, MinGW Makefiles, and the local w64devkit compiler. Use when the task needs repo-local build verification or when a command needs the exact Codex build setup.
---

# raythm-build-codex

Use this skill when you need to build `raythm` from the Codex desktop environment on this machine.

## Configure

Run this once when `cmake-build-codex` does not exist yet or when the CMake cache needs to be recreated.

```powershell
cmake -S . -B cmake-build-codex -G "MinGW Makefiles" `
  -DCMAKE_C_COMPILER=C:/Users/rento/Documents/w64devkit/bin/gcc.exe `
  -DCMAKE_CXX_COMPILER=C:/Users/rento/Documents/w64devkit/bin/g++.exe
```

## Main Build

For the app:

```powershell
cmake --build cmake-build-codex --target raythm -j 2
```

For common editor checks:

```powershell
cmake --build cmake-build-codex --target raythm editor_state_smoke editor_panel_controller_smoke -j 2
```

## Smoke Tests

PowerShell sometimes fails to run `.\cmake-build-codex\...exe` directly in this environment. Prefer absolute paths with `&`.

```powershell
& 'C:\Users\rento\CLionProjects\raythm\cmake-build-codex\editor_state_smoke.exe'
& 'C:\Users\rento\CLionProjects\raythm\cmake-build-codex\editor_panel_controller_smoke.exe'
```

Example for a new smoke target:

```powershell
cmake --build cmake-build-codex --target editor_meter_map_smoke -j 2
& 'C:\Users\rento\CLionProjects\raythm\cmake-build-codex\editor_meter_map_smoke.exe'
```

## Notes

- Work from the repo root: `C:\Users\rento\CLionProjects\raythm`
- The generated build tree is `cmake-build-codex`
- `raylib` CMake warnings about build type are expected here
- If a new smoke target fails to compile due to missing headers, add `target_include_directories(<target> PRIVATE ${RAYTHM_INCLUDE_DIRS})` in [CMakeLists.txt](C:/Users/rento/CLionProjects/raythm/CMakeLists.txt)
