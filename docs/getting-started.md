# Getting Started

* Godot 4.5 or later; the latest version is recommended.

Download core and the desired backend (v8 for the example below) from a successful
[Build GDExtension](https://github.com/realybin/puerts-godot/actions/workflows/make_build.yml) run.
Use packages with matching project versions, Godot API versions, and precision.
Extract both ZIP files into your project root, preserving the `addons/puerts-godot/` directory.
You can also [build from source](build.md).

```gdscript
extends Node

var _env: PuertsEnvironment

func _ready() -> void:
	var pool := PuertsStringNameUtf8Cache.new()
	_env = PuertsEnvironment.new()
	_env.set_log_callback(func(_category: int, _level: int, message: String): push_error(message))
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

```gdscript
_env.set_log_callback(func(category: int, level: int, message: String):
    print("[%d][%d] %s" % [category, level, message])
)
_env.set_log_level(PuertsEnvironment.LOG_LEVEL_INFO)
_env.set_category_log_level(1, PuertsEnvironment.LOG_LEVEL_DEBUG)
_env.clear_category_log_level(1)
```

```javascript
PLogger.trace(1, "Sending packet");
PLogger.debug(1, "Connected");
PLogger.info(1, "Player joined");
PLogger.warning(1, "Connection is slow");
PLogger.error(1, "Connection failed");
```

- Category IDs are positive 32-bit integers. `0` is reserved for Puerts internal errors and script exceptions.
- By default, logs at `INFO` and above are emitted. You can override the minimum level for individual categories.
- The callback runs synchronously on the thread that produces the log. Nested logs produced inside the callback are discarded.
- Without a valid callback, logs are sent to Godot. Pass `Callable()` to restore default output.
- Calling `dispose()` and then `initialize()` on the same `PuertsEnvironment` preserves its logging settings; no reconfiguration is needed.

## ESM

See [README.md](../src/PuertsRuntime/ECMAScript/README.md)
