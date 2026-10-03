# Local Build Guide

Requires `git`, Python 3.10+, Node.js 20+, SCons, and a platform C/C++ toolchain. Run from the repository root. See [make_build.yml](../.github/workflows/make_build.yml) for CI combinations.

Initialize submodules:

```bash
git submodule update --init --recursive
```

Show command options with `python scripts/make_puerts.py --help`.

Common arguments:

- `--platform`: `windows|macos|linux|android|ios|web`
- `--arch`: `x86_64|x86_32|arm64|arm32|wasm32`
- `--config`: `Debug|Release`
- `--backends`: comma-separated list, e.g. `core,v8,nodejs,quickjs,lua,webgl`
- `--threads`: `true|false` for Web; match the SCons `threads` value (default: `true`)

Platform mappings, default architectures, backend names, patch conditions, and extension metadata are in [`scripts/config.json`](../scripts/config.json).

## Build runtime dependencies

```bash
# Windows x64 Debug
python scripts/make_puerts.py --platform windows --arch x86_64 --config Debug
# Linux x64 Release, only core + quickjs
python scripts/make_puerts.py --platform linux --arch x86_64 --config Release --backends core,quickjs
```

## Build GDExtension

Static bindings are versioned `.inc` files. Normal builds do not regenerate them. When changing the API or generator, follow [`puerts-godot-binding-gen`](../tools/puerts-godot-binding-gen/README.md).

Build Core and backend extensions from the same source version so their shared function-table ABI matches.

```bash
# Windows x64 Debug
scons target=template_debug platform=windows arch=x86_64 precision=single api_version=4.7
# Linux x64 Release
scons target=template_release platform=linux arch=x86_64 precision=single api_version=4.7
```

Use `api_version=4.5` for the oldest supported API:

```bash
scons target=template_debug platform=windows arch=x86_64 precision=single api_version=4.5
```

Output is written to `bin/`. Windows uses Visual Studio; Linux uses GCC/Clang; Android uses the NDK (`ANDROID_NDK` or `ANDROID_NDK_HOME`); iOS/macOS uses Xcode; Web uses Emscripten. V8 and Node.js are unavailable on Web.

## Web builds

Build the [browser JavaScript backend](backends.md#browser-javascript-webgl) with Emscripten:

```bash
python scripts/make_puerts.py --platform web --arch wasm32 --config Debug --backends core,webgl --threads false
scons target=template_debug platform=web arch=wasm32 precision=single api_version=4.7 backends=core,webgl threads=false
```

Copy the `.wasm` files from `bin/` to `res://bin/`. Place [`puerts_core.gdextension`](../tests/webgl/puerts_core.gdextension) and [`puerts_webgl.gdextension`](../tests/webgl/puerts_webgl.gdextension) in the project root; both use single precision.

Enable **Extensions Support** in the Web export preset. Keep **Thread Support**, `--threads`, and SCons `threads` consistent.

Threaded exports also require HTTP headers `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp`.

See [testing](testing.md#browser-javascript-backend) to verify the export.
