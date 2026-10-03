# puerts-godot

`puerts-godot` runs ECMAScript and Lua in Godot through Puerts GDExtensions.

It provides:

- V8, Node.js, QuickJS, and Lua backends.
- Multiple independent `PuertsEnvironment` runtimes.
- Godot object bindings and C++ static bindings.

> [!NOTE]
> This project is a community-maintained, third-party implementation of Puerts integration for Godot.

> [!WARNING]
> Experimental, use with caution, and expect breaking changes in the future before the 1.0 release.

## Quick example

```gdscript
extends Node

var env: PuertsEnvironment

func _ready() -> void:
	var pool := PuertsStringNameCachePool.new()
	if pool.initialize() != OK:
		return
	env = PuertsEnvironment.new()
	if env.initialize(PuertsV8Backend.new(), pool) != OK:
		return
	env.set_global(&"answer", 41)
	var result := env.eval("answer + 1")
	if result != null:
		print(result.to_native()) # 42

func _process(_delta: float) -> void:
	if env != null and env.is_alive():
		env.tick()

func _exit_tree() -> void:
	if env != null:
		env.dispose()
```

Keep the environment alive while using its script values. `dispose()` invalidates those values; initialize again only after disposal completes.

## Documentation

- [Getting started](docs/getting-started.md)
- [Backends](docs/backends.md)
- [Conventions](docs/conventions.md)
- [Static binding](docs/static-binding.md)
- [Object allocation and lifetime](docs/object-allocating.md)
- [Advanced usage](docs/advanced.md)
- [V8 inspector](docs/v8-inspector.md)
- [Build guide](docs/build.md)
- [Test runner](docs/testing.md)

## Supported backends

| Platform        | V8+  | Nodejs+ | Quickjs | Lua  |
|-----------------|------|---------|---------|------|
| Windows(x86_64) | Yes  | Yes     | Yes     | Yes  |
| Linux(x86_64)   | Yes  | Yes     | Yes     | Yes  |
| macOS(arm64)    | Yes* | Yes*    | Yes*    | Yes* |
| Android(armv8)  | Yes  | Yes     | Yes     | Yes  |
| iOS             | ?    | ?       | ?       | ?    |
| Web(wasm32)     | x    | x       | Yes     | Yes  |


+: Use one of them, do not use V8 both Nodejs in the same process, especially in linux, it may cause some issues, pr wellcome.

*: CI passes on macOS, but not tested on real devices yet. (no device on hand)

?: Not tested yet due to lack of devices or time

x: No plan to support, V8 and Nodejs cannot run in Web

## Example project

[puerts-godot-demo](https://github.com/realybin/puerts-godot-demo) contains a complete project.

## Roadmap

- [ ] [WebGL support](https://puerts.github.io/en/docs/puerts/unity/knowjs/webgl/)
- [ ] HELP WANTED in technical writing
- [ ] Stable release
- [ ] Better API design
- [ ] Performance optimization
- [ ] V8 bytecode cache guide and support

Under consideration:

- Use as [ScriptLanguage](https://docs.godotengine.org/en/stable/getting_started/step_by_step/scripting_languages.html).
  It means you can use pure ECMAScript without GDScript or other languages.\
  _**Issue:**_
  GDScript is the default scripting language in Godot and already supports a wide range of features.
  However, maintaining a third-party ScriptLanguage integration may pose risks to the community — if it's poorly designed or unstable, it could disrupt the community ecosystem, and we simply don't have enough time to maintain it.
- Python support.\
  _**Issue:**_
  Need to find a good way to distribute Python binaries and its core libraries.


## Contributing

You should set up `prek` for formatting checks

```shell
pip install prek
prek install
```

**You should add license header to all source files, see other source files for examples.**

DCO is required, please sign off your commits with `git commit -s` or `git commit --signoff`.

1. Testing your changes
2. Open an issue before submitting changes
3. Add tests if it finds a bug or adds a new feature if possible
4. clang-format is required, so make a sure format before submitting a PR
5. Separate changes into multiple pull requests if it's too big, we do not accept big PRs
6. We prioritize human-made contributions. Please understand that with our limited time, **AI-generated issues and pull requests may be closed without review**.

## Lifecycle

We follow [Godot’s Release Support Timeline](https://docs.godotengine.org/en/stable/about/release_policy.html#release-support-timeline).

By default, we support versions labeled **Partial support**. Versions marked **No support (end of life)** are not supported.

## Versioning

[Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html)

## Credits

- [Tencent/puerts](https://github.com/Tencent/puerts) Most of the core logic and API design is based on Tencent's puerts project.
- [Godot Engine](https://godotengine.org/) Godot is an open-source game engine that provides a powerful and flexible platform for game development.
