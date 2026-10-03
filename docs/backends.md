# Backends

Pass a backend resource to `PuertsEnvironment.initialize()`. Resources can be shared.

| Backend | Language | `tick` | Inspector | Low memory | Terminate |
|---------|----------|--------|-----------|------------|-----------|
| `PuertsV8Backend` | ECMAScript | Yes | Yes | Yes | Yes |
| `PuertsNodejsBackend` | ECMAScript | Yes | Yes | Yes | Yes |
| `PuertsQuickjsBackend` | ECMAScript | No | No | Yes | No |
| `PuertsLuaBackend` | Lua | No | No | No | No |
| `PuertsWebglBackend` | ECMAScript | No | No | No | No |

Unsupported hooks report through the error callback. Choose V8 or Node.js for a process; loading both may conflict, especially on Linux. QuickJS, Lua, and the browser backend support Web builds.

Each backend exposes `get_backend_id()`, `get_backend_name()`, and `get_language_id()`. `_puerts_get_functions_ptr()` is an internal core hook.

## Browser JavaScript (WebGL)

`PuertsWebglBackend` runs in the browser's JavaScript engine and shares `globalThis` with the page, including DOM access. Calls must run on the browser main thread. Requires `WeakRef` and `FinalizationRegistry`; a page CSP must permit `unsafe-eval`. Use browser DevTools for debugging.

One environment per page, retained until page unload. Initializing a second environment returns `ERR_CANT_CREATE`; reinitializing the first returns `ERR_ALREADY_IN_USE`. `dispose()` reports an error without invalidating values or callables. Reload the page to restart.

After installing the [Web extensions](build.md#web-builds):

```gdscript
extends Node

func _ready() -> void:
	var pool = ClassDB.instantiate("PuertsStringNameCachePool")
	if pool.initialize() != OK:
		return
	var env = ClassDB.instantiate("PuertsEnvironment")
	if env.initialize(ClassDB.instantiate("PuertsWebglBackend"), pool) != OK:
		return
	var title = env.eval("document.title")
	if title != null:
		print(title.to_native())
```

Use `ClassDB.instantiate()` because the Web-only extension classes are unavailable in the desktop editor. Run this code in the Web export.
