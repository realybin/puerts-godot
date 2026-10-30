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

## Web builds

```bash
python misc/scripts/make_puerts.py --platform web --arch wasm32 --config Debug --backends core,web --threads false
scons target=template_debug platform=web arch=wasm32 precision=single api_version=4.7 backends=core,web threads=false
```

Also set the following headers:
* `Cross-Origin-Opener-Policy: same-origin`
* `Cross-Origin-Embedder-Policy: require-corp`


## Release packages

Build GDExtension packages the selected builds and uploads them as the
`puerts-godot-packages` workflow artifact. Each component archive is named
`puerts-godot-<component>-v<version>-godot<api>-<single|double>.zip`.
Archives contain only the platforms, targets, and Web thread variants selected for that run.

Extract the core archive and the desired backend archives into your project directory.
Use matching project versions, Godot API versions, and float precision.
Only core includes the README files, licenses, and documentation.

To package downloaded build artifacts locally, preserve their artifact directory names:

```bash
python misc/scripts/package_release.py --artifacts artifacts --output dist
```
