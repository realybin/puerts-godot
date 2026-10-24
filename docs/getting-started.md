# Getting Started

`puerts-godot` needs Godot 4.5 or later, the core extension, and one backend extension. Use binaries built for the same Godot version, platform, architecture, and precision. See the [build guide](build.md) or [GitHub Actions](https://github.com/realybin/puerts-godot/actions/workflows/make_build.yml).

Put the extensions and libraries in the project:

```text
res://bin/
res://puerts_core.gdextension
res://puerts_v8.gdextension
```

Other [backends](backends.md) use their matching extension. Each `.gdextension` must list its libraries and native dependencies. For browser JavaScript, follow [Web builds](build.md#web-builds).

## First runtime

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
		_env.tick() # V8 and Node.js's logic tick

func _exit_tree() -> void:
	if _env != null:
		_env.dispose()
```

Share an environment across scenes and keep it alive while using its values. This example uses V8; WebGL has different [initialization and lifetime rules](backends.md#browser-javascript-webgl).

## Values

* `set_global()` `get_global()`
* `eval()`
* `get_property()` `call()` `call_method()` `PuertsScriptValue`
* `to_float()` `to_int()` `unwrap_native()` `to_native()`

See [conventions](conventions.md), [object lifetime](object-allocating.md), and the Godot editor API reference for the remaining methods.

## Errors

> planning

We provide these to godot:

```gdscript
_env.set_error_callback(func(message: String): ...(message))
_env.set_warn_callback(func(message: String): ...(message))
_env.set_info_callback(func(message: String): ...(message))
```

And you can use

```javascript
log_error("..")
log_warn("...")
log_info("...")
```

to trigger the callback.

It is not necessary to set the callback.
