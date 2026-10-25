# Getting Started

* Godot 4.5 or later; the latest version is recommended.

Build it yourself or get prebuilt binaries from [GitHub Actions](https://github.com/realybin/puerts-godot/actions/workflows/make_build.yml).

Put the extensions and libraries in the project:

```text
res://bin/
res://puerts_core.gdextension
res://puerts_v8.gdextension
```


```gdscript
extends Node

var _env: PuertsEnvironment

func _ready() -> void:
	var pool := PuertsStringNameUtf8Cache.new()
	_env = PuertsEnvironment.new()
	_env.set_error_callback(func(message: String): push_error(message))
	if _env.initialize(PuertsV8Backend.new(), pool) != OK:
		return
	var value := _env.eval("40 + 2", &"bootstrap.js")
	if value != null:
		print(value.to_native()) # 42

func _process(_delta: float) -> void:
	if _env != null and _env.is_ready():
		_env.tick() # Logic tick, not available on all backends

func _game_exiting() -> void:
	if _env != null:
		_env.dispose() # Dispose the environment at the right time, e.g. when the game exits
```

## Values

We roughly provide the following API:

* `set_global()` `get_global()`
* `eval()`
* `get_property()` `call()` `call_method()` `PuertsScriptValue`
* `to_float()` `to_int()`... `unwrap_native()` `to_native()`

`PuertsScriptValue` wraps a value in the script engine:
* It is a reference to a script value. Destroying the wrapper decrements the reference; when the reference count reaches zero (depending on the situation), the script engine reclaims the script value.
* It must not be used across script instances.

> [!WARNING]
> You can read the documentation of these APIs in the editor, and we recommend reading all of them carefully.

See [conventions](conventions.md) and [object lifetime](lifetime.md) for the remaining methods.

## Logging

> [!WARNING]
> Breaking changes are planned to add a Category.

We provide the following interfaces to Godot:

```gdscript
_env.set_error_callback(func(message: String): ...(message))
_env.set_warn_callback(func(message: String): ...(message))
_env.set_info_callback(func(message: String): ...(message))
```

That is, you can use these methods in the script engine to handle these cases:

```javascript
log_error("..")
log_warn("...")
log_info("...")
```

The callbacks are optional.

## ESM

See [README.md](../src/PuertsRuntime/ECMAScript/README.md)
