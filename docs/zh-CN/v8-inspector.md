# V8 Inspector

```gdscript
_env.open_debugger(9229)

func _process(_delta: float) -> void:
	if _env != null and _env.is_ready():
		_env.tick()
		_env.debugger_tick()
```

在 Chrome DevTools 中打开
 * devtools://devtools/bundled/inspector.html?v8only=true&ws=127.0.0.1:9229
