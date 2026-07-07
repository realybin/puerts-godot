# Local Build Guide

More information see GitHub workflows at [make_build.yml](../.github/workflows/make_build.yml).

## Prerequisites

- `git` (with submodule support)
- `python` (3.10+)
- `node` (20+)
- `scons` see [Introduction to the buildsystem](https://docs.godotengine.org/en/stable/engine_details/development/compiling/introduction_to_the_buildsystem.html)
- C/C++ toolchain (platform-specific)

First, go project root.

Initialize submodules first:

```bash
git submodule update --init --recursive
```

Show help:

```bash
python3 scripts/make_puerts.py --help
```

Common arguments:

- `--platform`: `windows|macos|linux|android|ios|web`
- `--arch`: `x86_64|x86_32|arm64|arm32|wasm32`
- `--config`: `Debug|Release`
- `--backends`: comma-separated list, e.g. `core,v8,nodejs,quickjs,lua`

### Make puerts and backends

```bash
# Windows x64 Debug
python scripts/make_puerts.py --platform windows --arch x86_64 --config Debug
# Linux x64 Release, only core + quickjs
python scripts/make_puerts.py --platform linux --arch x86_64 --config Release --backends core,quickjs
# Web wasm32, v8/nodejs will be skipped
python scripts/make_puerts.py --platform web --arch wasm32 --config Release --backends core,v8,nodejs,quickjs,lua
````

### Build GDExtension

```bash
# Windows x64 Debug
scons target=template_debug platform=windows arch=x86_64 precision=single
# Linux x64 Release
scons target=template_release platform=linux arch=x86_64 precision=single
# Web wasm32 Release
scons target=template_release platform=web arch=wasm32 precision=single
```

Default output directory:

- `bin/`

## Platform Notes

### Windows

- Requires Visual Studio C++ toolchain (Visual Studio 2026 recommended).
- puerts Windows debug builds output into `RelWithDebInfo` for better debugging experience, but you can also customize for your own workflow.

### Linux

- Requires GCC/Clang and `build-essential`.
- Some backends may need `libc++-dev` and `libc++abi-dev`.

### Android

- Requires Android NDK (CI currently uses `android-ndk-r27d`).
- You can configure with:
  - `ANDROID_NDK`
  - or `ANDROID_NDK_HOME`

### iOS / macOS

- Requires Xcode and command line tools.
- iOS builds must run on macOS.

### Web

- Requires Emscripten (`emcmake` / `emmake` available in PATH).
- `V8` and `Nodejs` are not supported on Web.
