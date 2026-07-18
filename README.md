# puerts-godot

> This project is a community-maintained, third-party implementation of Puerts integration for Godot.

> Experimental, use with caution, and expect breaking changes in the future before the 1.0 release.

`puerts-godot` is a Godot GDExtension integration for puerts.

Showcase:

https://github.com/user-attachments/assets/2026e5d9-a95b-466b-8918-cfe784a2e6da

See [puerts-godot-demo](https://github.com/realybin/puerts-godot-demo) for a complete example.

It provides:

- Run ECMAScript, Lua, or other supported language scripts in Godot through `puerts`
- Multiple ScriptEngine backends: V8, Nodejs, Quickjs, Lua
- Support multiple instances of ScriptEngine with different backends in the same process
- Static binding support based on C++ templates

```gdscript
# Example usage
extends Node3D

var env:PuertsEnvironment

func _ready() -> void:

	env = PuertsEnvironment.new()
	var backend = PuertsV8Backend.new()
	var pool = PuertsStringNameCachePool.new()
	pool.initialize(PuertsStringNameCachePool.POLICY_HASH_MAP)

	env.initialize(backend,pool)
	env.open_debugger(9229)

	env.eval("""
	console.log("hello world")
	""", "chunk.js")

	env.set_global("myGlobal", 123)
	var result = env.eval("myGlobal + 1", "chunk2.js")
	print(result.to_int()) # 124

	var print_func: Callable = func (message):
		print(message)

	env.set_global("print", print_func)
	env.eval("""
	print.call("hello from js")
	""", "chunk3.js")
	var ret = env.eval("""(function () {
	const Vector2 = new load_type("Vector2")
	const v = new Vector2(3.0, 4.0).length()
	return v; })()
	"""
	, "chunk4.js") # returns 5.0

	print(ret.to_int())

func _process(delta: float) -> void:
	env.debugger_tick()
```


## Documentation

- [Getting Started](docs/getting-started.md)
- [Build Guide](docs/build.md)
- [Static Binding](docs/static-binding.md)
- [Object Allocation and Lifetime](docs/object-allocating.md)
- [Test Runner Notes](docs/testing.md)

## Example

* [puerts-godot-demo](https://github.com/realybin/puerts-godot-demo): Basic esm support and sample usage of puerts-godot.

## Supported backends

| Backend         | V8+  | Nodejs+ | Quickjs | Lua  |
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

## Roadmap

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
