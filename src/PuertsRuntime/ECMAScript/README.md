# ECMAScript Runtime Assets

## Project Setup

See [puerts-godot-demo](https://github.com/realybin/puerts-godot-demo) for a complete example.

## ESM

ES modules are loaded through [puerts_native_esm_bootstrap.js](puerts_native_esm_bootstrap.js) and [puerts_native_esm_loader.gd](puerts_native_esm_loader.gd).

See [puerts_native_esm_bootstrap.js](puerts_native_esm_bootstrap.js) and [BackendEnv.cpp](https://github.com/Tencent/puerts/blob/master/unity/native/papi-v8/source/BackendEnv.cpp) for more details.

Usage

```gdscript
const PUERTS_MODULE_ROOT := "res://JavaScript"

var env: PuertsEnvironment
var game_module: PuertsScriptValue

func _ready():
	if not _setup_puerts():
		push_error("Failed to setup Puerts")
		return

	game_module = env.eval("__puertsExecuteModule('%s')" % PUERTS_ENTRY_MODULE)
	if game_module == null:
		push_error("ExecuteModule failed")
		return

	game_module.call_method("foo")

func _setup_puerts() -> bool:
	env = PuertsEnvironment.new()
	var backend := PuertsV8Backend.new()
	var pool := PuertsStringNameCachePool.new()
	var pool_err := pool.initialize(PuertsStringNameCachePool.POLICY_HASH_MAP)
	if pool_err != OK:
		push_error("StringName cache pool initialize failed: %d" % pool_err)
		env = null
		return false

	env.set_error_callback(func(message: String):
		push_error("[E][puerts] %s" % message)
	)
	env.set_warn_callback(func(message: String):
		push_warning("[W][puerts] %s" % message)
	)
	env.set_info_callback(func(message: String):
		print("[I][puerts] %s" % message)
	)

	var init_err := env.initialize(backend, pool)
	if init_err != OK:
		push_error("Puerts initialize failed: %d" % init_err)
		env = null
		return false

	env.open_debugger(9229)

	var loader := PuertsNativeEsmLoader.new()
	loader.root_prefix = PUERTS_MODULE_ROOT
	env.set_global("__puer_module_loader__", loader)
	env.eval(
		FileAccess.get_file_as_string("res://puerts_native_esm_bootstrap.js"),
		"puerts_native_esm_bootstrap.js"
	)

	return true

```

## `load_type` transform

The local Babel plugin transforms imports from `"godot"` into `load_type` calls. See its [README](babel-plugin-godot-import-rewrite/README.md).

## V8 Inspector

While the game is running, attach through the VS Code configuration.

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Attach V8 (9229)",
      "type": "node",
      "request": "attach",
      "address": "127.0.0.1",
      "port": 9229,
      "restart": true,
      "timeout": 60000,
      "cwd": "${workspaceFolder}",
      "sourceMaps": true,
      "outFiles": [
        "${workspaceFolder}/../JavaScript/**/*.mjs"
      ],
      "resolveSourceMapLocations": [
        "${workspaceFolder}/**",
        "${workspaceFolder}/../JavaScript/**",
        "!**/node_modules/**"
      ]
    }
  ]
}
```

See [V8 Inspector](https://github.com/realybin/puerts-godot/blob/main/docs/v8-inspector.md) for details.

## Exporting

Add JavaScript files to the export [resource options](https://docs.godotengine.org/en/latest/tutorials/export/exporting_projects.html#resource-options).
