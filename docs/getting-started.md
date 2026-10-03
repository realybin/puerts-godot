# Getting Started

`puerts-godot` needs Godot 4.5 or later, the core extension, and one backend extension. Use binaries built for the same Godot version, platform, architecture, and precision. See the [build guide](build.md) or [GitHub Actions](https://github.com/realybin/puerts-godot/actions/workflows/make_build.yml).

Put the extensions and libraries in the project:

```text
res://bin/
res://puerts_core.gdextension
res://puerts_v8.gdextension
```

Use `PuertsNodejsBackend`, `PuertsQuickjsBackend`, or `PuertsLuaBackend` with its matching extension when V8 is not appropriate. Each `.gdextension` must list its libraries and any native dependencies for the target platform.

## First runtime

```gdscript
extends Node

var _env: PuertsEnvironment

func _ready() -> void:
	var pool := PuertsStringNameCachePool.new()
	if pool.initialize() != OK:
		return
	_env = PuertsEnvironment.new()
	_env.set_error_callback(func(message: String): push_error(message))
	if _env.initialize(PuertsV8Backend.new(), pool) != OK:
		return
	var value := _env.eval("40 + 2", &"bootstrap.js")
	if value != null:
		print(value.to_native()) # 42

func _process(_delta: float) -> void:
	if _env != null and _env.is_alive():
		_env.tick() # V8 and Node.js

func _exit_tree() -> void:
	if _env != null:
		_env.dispose()
```

Share an environment when scripts span scenes. `dispose()` is safe to call repeatedly and invalidates all `PuertsScriptValue` wrappers and script callables from that runtime. Reinitialize only after the current operation has returned; old wrappers cannot be reused.

## Values and errors

`set_global()` and script calls convert Godot `Variant` values. `eval()`, `get_global()`, `get_property()`, `call()`, and `call_method()` return `PuertsScriptValue` wrappers.

Use `to_float()` for an ECMAScript Number, `to_int()` for a backend integer (V8 and Node.js require BigInt), `unwrap_native()` for a Godot value, and `to_native()` when the result type varies. `to_native()` does not recursively turn script objects into Godot containers.

Register `set_error_callback()`, `set_warn_callback()`, and `set_info_callback()` for synchronous messages. Script helpers `log_error()`, `log_warn()`, and `log_info()` use those callbacks. A failed evaluation or call returns `null`; a successful script `null` has a valid wrapper whose `is_null()` is true.

See [conventions](conventions.md), [object lifetime](object-allocating.md), and the Godot editor API reference for the remaining methods.