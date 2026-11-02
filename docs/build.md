# Build

Prerequisites

* git
* Python 3.11+
* Node.js 20+
* SCons
* C/C++ toolchain
* Emscripten (for Web)

Initialize submodules:

```bash
git submodule update --init --recursive
```

See command options with `python misc/scripts/make_puerts.py --help`.

Common arguments:

- `--platform`: `windows|macos|linux|android|ios|web`
- `--arch`: `x86_64|x86_32|arm64|arm32|wasm32`
- `--config`: `Debug|Release`
- `--backends`: `core,v8,nodejs,quickjs,lua,web`
- `--threads`: Web `true|false`
- `--thread-safe`: `yes|no`, default `yes`; match SCons `thread_safe=yes|no`

Metadata: [`manifest.toml`](../manifest.toml).

## Dependencies

puerts-godot depends on [puerts](https://github.com/Tencent/puerts), so puerts must be built in advance.

```bash
# Windows x64 Debug
python misc/scripts/make_puerts.py --platform windows --arch x86_64 --config Debug
# Linux x64 Release, only core + quickjs
python misc/scripts/make_puerts.py --platform linux --arch x86_64 --config Release --backends core,quickjs
```

## Build

We generate the C++ static bindings for the core API from Godot's API metadata: [`puerts-godot-binding-gen`](../tools/puerts-godot-binding-gen/README.md)

```bash
# Windows x64 Debug
scons target=template_debug platform=windows arch=x86_64 precision=single api_version=4.7
# Linux x64 Release
scons target=template_release platform=linux arch=x86_64 precision=single api_version=4.7
```

Output goes to `bin/`.

Thread synchronization defaults to `thread_safe=yes`. To disable it, pass `--thread-safe no` to `make_puerts.py` and `thread_safe=no` to SCons. Rebuild both the native libraries and GDExtension when changing this option; SCons checks the native build metadata and rejects a mismatch. Non-threaded Web builds compile out synchronization regardless of this option.

## Web builds

```bash
python misc/scripts/make_puerts.py --platform web --arch wasm32 --config Debug --backends core,web --threads false
scons target=template_debug platform=web arch=wasm32 precision=single api_version=4.7 backends=core,web threads=false
```

Also set the following headers:
* `Cross-Origin-Opener-Policy: same-origin`
* `Cross-Origin-Embedder-Policy: require-corp`

## CI builds

[Build GDExtension](https://github.com/realybin/puerts-godot/actions/workflows/make_build.yml)
produces component packages for the selected platforms, Godot API versions, build target, and precision.
See [Getting Started](getting-started.md) for installation.
