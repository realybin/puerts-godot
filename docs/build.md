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
```

### Build GDExtension

The `api_version` SCons option selects the Godot GDExtension API used to build
`godot-cpp`. This project currently supports Godot API versions `4.5`, `4.6`,
and `4.7`. Godot `4.7` is the default; select `4.5` explicitly when producing
a backwards-compatible build.

Static bindings for Godot `4.5`, `4.6`, and `4.7` are generated as separate,
versioned `.inc` files. Each generated file is
guarded by `GODOT_VERSION_MAJOR` and `GODOT_VERSION_MINOR`; the C++ entry point
includes only the files matching the selected `api_version`. Normal builds do
not run the binding generator. When updating the API files or generator, follow
the three generation commands in the
[`puerts-godot-binding-gen` README](../tools/puerts-godot-binding-gen/README.md).

```bash
# Windows x64 Debug
scons target=template_debug platform=windows arch=x86_64 precision=single api_version=4.7
# Linux x64 Release
scons target=template_release platform=linux arch=x86_64 precision=single api_version=4.7
# Web wasm32 Release
scons target=template_release platform=web arch=wasm32 precision=single api_version=4.7
```

To target the oldest supported API, change the option explicitly:

```bash
scons target=template_debug platform=windows arch=x86_64 precision=single api_version=4.5
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
