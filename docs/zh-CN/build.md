# 构建

前置条件

* git
* Python 3.11+
* Node.js 20+
* SCons
* C/C++ Toolchain
* Emscripten (for Web)

初始化子模块：

```bash
git submodule update --init --recursive
```

用 `python misc/scripts/make_puerts.py --help` 查看命令选项。

常用参数：

- `--platform`：`windows|macos|linux|android|ios|web`
- `--arch`：`x86_64|x86_32|arm64|arm32|wasm32`
- `--config`：`Debug|Release`
- `--backends`：`core,v8,nodejs,quickjs,lua,web`
- `--threads`：Web `true|false`

元数据 [`manifest.toml`](../../manifest.toml)。

## 依赖

puerts-godot 依赖于 [puerts](https://github.com/Tencent/puerts)，所以我们需要预先构建 puerts。

```bash
# Windows x64 Debug
python misc/scripts/make_puerts.py --platform windows --arch x86_64 --config Debug
# Linux x64 Release，仅 core + quickjs
python misc/scripts/make_puerts.py --platform linux --arch x86_64 --config Release --backends core,quickjs
```

## 构建

我们通过 godot 的 api 元数据生成核心 API 的C++静态绑定 [`puerts-godot-binding-gen`](../../tools/puerts-godot-binding-gen/README.md)

```bash
# Windows x64 Debug
scons target=template_debug platform=windows arch=x86_64 precision=single api_version=4.7
# Linux x64 Release
scons target=template_release platform=linux arch=x86_64 precision=single api_version=4.7
```

输出 `bin/`

## Web 构建

```bash
python misc/scripts/make_puerts.py --platform web --arch wasm32 --config Debug --backends core,web --threads false
scons target=template_debug platform=web arch=wasm32 precision=single api_version=4.7 backends=core,web threads=false
```

另外还要调整为
* `Cross-Origin-Opener-Policy: same-origin`
* `Cross-Origin-Embedder-Policy: require-corp`

## 发布包

Build GDExtension 会将所选构建打包，上传为工作流产物
`puerts-godot-packages`。每个组件的压缩包命名为
`puerts-godot-<component>-v<version>-godot<api>-<single|double>.zip`。
包内只包含本次运行所选的平台、构建目标和 Web 线程模式。

将 core 包和所需后端包解压到项目目录，确保项目版本、Godot API 版本及浮点精度一致。
只有 core 包包含 README、许可证和文档。

在本地打包已下载的构建产物时，请保留各产物的目录名称：

```bash
python misc/scripts/package_release.py --artifacts artifacts --output dist
```
