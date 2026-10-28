# puerts-godot

`puerts-godot` 让 ECMAScript 与 Lua 通过 Puerts 在 Godot 中运行。

主要特性：

- 支持 V8、Node.js、QuickJS、Lua 及浏览器 JavaScript（Web）后端。
- 除 Web 后端外，可运行多个相互独立的 `PuertsEnvironment` 运行时。
- 提供 Godot 对象绑定与 C++ 静态绑定。

> [!NOTE]
> 本项目由社区维护，是面向 Godot 的第三方 Puerts 集成实现。

> [!WARNING]
> 项目仍处于实验阶段，请谨慎使用；1.0 发布前可能发生破坏性变更。

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

使用脚本值期间，请保持环境存活。释放规则见[对象生命周期](docs/zh-CN/lifetime.md)。

## 文档

- [入门](docs/zh-CN/getting-started.md)
- [后端](docs/zh-CN/backends.md)
- [约定](docs/zh-CN/conventions.md)
- [静态绑定](docs/zh-CN/static-binding.md)
- [生命周期](docs/zh-CN/lifetime.md)
- [StringName 缓存](docs/zh-CN/u8-cache.md)
- [进阶用法](docs/zh-CN/advanced.md)
- [V8 检查器](docs/zh-CN/v8-inspector.md)
- [IDE](docs/zh-CN/ide.md)
- [构建](docs/zh-CN/build.md)
- [测试](docs/zh-CN/testing.md)

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

`PuertsWebBackend` 使用浏览器的 JavaScript 引擎。参见[后端](docs/zh-CN/backends.md#浏览器-javascript-web)与 [Web 构建](docs/zh-CN/build.md#web-构建)。

## 示例项目

[puerts-godot-demo](https://github.com/realybin/puerts-godot-demo) 提供了一个完整项目示例。

## Roadmap

- [x] [支持 Web](docs/zh-CN/backends.md#浏览器-javascript-web)
- [x] Working on technical writing
- [ ] 稳定版本发布
- [ ] API 设计改进
- [ ] 性能优化
- [ ] V8 字节码缓存指南与支持

考虑中的方向：

- 作为 [ScriptLanguage](https://docs.godotengine.org/en/stable/getting_started/step_by_step/scripting_languages.html) 使用。
  这样便可只用纯 ECMAScript，无需 GDScript 或其他语言。\
  _**问题：**_
  GDScript 是 Godot 的默认脚本语言，已支持大量特性。
  但维护第三方 ScriptLanguage 集成对社区存在风险——若设计不佳或不够稳定，可能扰乱社区生态，而我们确实没有足够时间维护。
- 支持 Python。\
  _**问题：**_
  需要找到分发 Python 二进制及核心库的合适方式。


## 参与贡献

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

## 生命周期

遵循 [Godot 的发布支持时间线](https://docs.godotengine.org/en/stable/about/release_policy.html#release-support-timeline)。

默认支持标记为 **Partial support** 的版本；**No support (end of life)** 的版本不在支持范围内。

## 版本规范

[Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html)

## 致谢

- [Tencent/puerts](https://github.com/Tencent/puerts)：核心逻辑与 API 设计大多源自腾讯 puerts 项目。
- [Godot Engine](https://godotengine.org/)：Godot 是开源游戏引擎，为游戏开发提供强大而灵活的平台。

## 也可以看看

- [godotjs/GodotJS](https://github.com/godotjs/GodotJS) Add TypeScript/JavaScript Support for Godot 4.x with v8/QuickJS/JavaScriptCore/Browser
- [godothub/gode](https://github.com/godothub/gode) Godot with JavaScript / TypeScript & NodeJS🌟
