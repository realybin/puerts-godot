# V8 Inspector

Works on V8 and Nodejs

```gdscript
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

	var v = Vector2i(1,1)
	env.set_global("v", v)

	env.set_global("p", print_func)
	var ret = env.eval("""(function () {
	const Vector2 = new load_type("Vector2")
	const v = new Vector2(3.0, 4.0).length()
	return v; })()
	"""
	, "chunk4.js") # returns 5

	print(ret.to_int())

func _process(delta: float) -> void:
	env.debugger_tick()
```

Then open devtools://devtools/bundled/inspector.html?v8only=true&ws=127.0.0.1:9229

Note that you may need call `env.debugger_tick()` to make the inspector work.
