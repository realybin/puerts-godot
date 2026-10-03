# V8 Inspector

V8 and Node.js expose a remote inspector. After initialization, open a port and tick both the runtime and debugger:

```gdscript
_env.open_debugger(9229)

func _process(_delta: float) -> void:
	if _env != null and _env.is_alive():
		_env.tick()
		_env.debugger_tick()
```

Open `devtools://devtools/bundled/inspector.html?v8only=true&ws=127.0.0.1:9229` in Chrome DevTools. Use a different port for each environment and call `close_debugger()` when finished. See [Getting Started](getting-started.md) for initialization.