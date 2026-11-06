# puerts-godot

[English](README.en.md)

`puerts-godot` 让 ECMAScript 与 Lua 通过 Puerts 在 Godot 中运行。

- 支持 V8、Node.js、QuickJS、Lua 及浏览器 JavaScript（Web）后端。
- 除 Web 后端外，可运行多个相互独立的 `PuertsEnvironment` 运行时。
- 提供 Godot 对象绑定与 C++ 静态绑定。

## Showcase

https://github.com/user-attachments/assets/2026e5d9-a95b-466b-8918-cfe784a2e6da

## 快速示例

```gdscript
extends Node

var env: PuertsEnvironment

func _ready() -> void:
	var pool := PuertsStringNameUtf8Cache.new()
	env = PuertsEnvironment.new()
	if env.initialize(PuertsV8Backend.new(), pool) != OK:
		return
	env.set_global(&"answer", 41)
	var result := env.eval("answer + 1")
	if result != null:
		print(result.to_native()) # 42

func _process(_delta: float) -> void:
	if env != null and env.is_ready():
		env.tick()

func _exit_tree() -> void:
	if env != null:
		env.dispose()
```

使用脚本值期间，请保持环境存活。释放规则见[对象生命周期](docs/lifetime.md)。

## 文档

- [入门](docs/getting-started.md)
- [后端](docs/backends.md)
- [约定](docs/conventions.md)
- [静态绑定](docs/static-binding.md)
- [原生 Addon](docs/addons.md)
- [生命周期](docs/lifetime.md)
- [StringName 缓存](docs/u8-cache.md)
- [进阶用法](docs/advanced.md)
- [V8 检查器](docs/v8-inspector.md)
- [IDE](docs/ide.md)
- [构建](docs/build.md)
- [测试](docs/testing.md)

## 支持的后端

| 平台            | V8+  | Nodejs+ | Quickjs | Lua  | 浏览器 JS |
|-----------------|------|---------|---------|------|-----------|
| Windows(x86_64) | Yes  | Yes     | Yes     | Yes  | —         |
| Linux(x86_64)   | Yes  | Yes     | Yes     | Yes  | —         |
| macOS(arm64)    | Yes* | Yes*    | Yes*    | Yes* | —         |
| Android(armv8)  | Yes  | Yes     | Yes     | Yes  | —         |
| iOS             | ?    | ?       | ?       | ?    | —         |
| Web(wasm32)     | x    | x       | Yes     | Yes  | Yes       |


+：二者选其一，请勿在同一进程内同时使用 V8 与 Node.js，在 Linux 上尤其容易出问题；欢迎提交 PR。

*：macOS 上 CI 已通过，但尚未在真机验证（手头暂无设备）。

?：因缺少设备或时间，尚未测试。

x：暂无支持计划；V8 与 Node.js 无法运行于 Web。

`PuertsWebBackend` 使用浏览器的 JavaScript 引擎。参见[后端](docs/backends.md#浏览器-javascript-web)与 [Web 构建](docs/build.md#web-构建)。

## 示例项目

[puerts-godot-demo](https://github.com/realybin/puerts-godot-demo) 提供了一个完整项目示例。

## 反馈

本项目出于个人用途公开，不保证可用性，有问题请及时反馈至 Issue。交流应当清晰，但是请适当使用 LLM，没有描述清楚很难阅读理解。

## 软件生命周期

遵循 [Godot 的发布支持时间线](https://docs.godotengine.org/en/stable/about/release_policy.html#release-support-timeline)。

默认支持标记为 **Partial support** 的版本；**No support (end of life)** 的版本不在支持范围内。

## 版本规范

[Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html)

## 许可证

puerts-godot 采用 [Apache License 2.0](LICENSE.txt)。
第三方依赖的许可证见 [COPYRIGHT.txt](COPYRIGHT.txt)。

## 贡献

暂时不接受功能性的贡献。

先配置 `prek` 用于格式检查

```shell
pip install prek
prek install
```

**所有源文件都需添加许可证头，可参考其他源文件。**

提交需遵循 DCO，请使用 `git commit -s` 或 `git commit --signoff` 签署。

1. 测试你的改动
2. 提交前先创建 issue
3. 修复 bug 或新增特性时，尽量补充测试
4. 必须使用 clang-format，提交 PR 前请确认格式
5. 改动过大请拆分为多个 PR，我们不接受大型 PR
6. 我们优先考虑人工编写的贡献。由于时间有限，**AI 生成的 issue 和 PR 可能未经审查直接关闭**。

## AI 政策

1. 禁止全自动的Agent生成的贡献。
2. 我们接受 AI 生成的贡献，前提是遵循贡献指南。沟通过程中请尽量不要使用智能体自动化。

## 致谢

- [Tencent/puerts](https://github.com/Tencent/puerts)：核心逻辑与 API 设计大多源自腾讯 puerts 项目。
- [Godot Engine](https://godotengine.org/)：Godot 是开源游戏引擎，为游戏开发提供强大而灵活的平台。

## 也可以看看

- [godotjs/GodotJS](https://github.com/godotjs/GodotJS) Add TypeScript/JavaScript Support for Godot 4.x with v8/QuickJS/JavaScriptCore/Browser
- [godothub/gode](https://github.com/godothub/gode) Godot with JavaScript / TypeScript & NodeJS🌟
