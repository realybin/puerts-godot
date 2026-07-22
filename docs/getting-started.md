# Getting Started

Simple guide to get started with `puerts-godot`.

You can refer [puerts-godot-demo](https://github.com/realybin/puerts-godot-demo) for more information.

## Requirements

- Godot `4.5+`
- Built `puerts-core` and `puerts-v8` (or other backends) GDExtension modules, see [build.md](./build.md), you can use our prebuilt binaries at [Actions](https://github.com/realybin/puerts-godot/actions/workflows/make_build.yml)

## Project setup

> It depends on your project structure

```text
res://bin/
res://puerts_core.gdextension
res://puerts_v8.gdextension
# Or puerts_nodejs.gdextension / puerts_quickjs.gdextension / puerts_lua.gdextension
```

`puerts_core.gdextension` for `PuertsCore`

```ini
[configuration]
entry_symbol = "puerts_core_library_init"
compatibility_minimum = "4.5"
reloadable = true

[libraries]
windows.debug.x86_64 = "res://bin/PuertsCore.windows.template_debug.x86_64.dll"
windows.editor.x86_64 = "res://bin/PuertsCore.windows.template_debug.x86_64.dll"
[dependencies]
windows.debug.arm64 = {
                      "res://bin/PuertsCore.dll": ""
}
```

`puerts_v8.gdextension` for `PuertsV8Backend`

```ini
[configuration]
entry_symbol = "puerts_v8_library_init"
compatibility_minimum = "4.5"
reloadable = true

[libraries]
windows.debug.x86_64 = "res://bin/PuertsV8.windows.template_debug.x86_64.dll"
windows.editor.x86_64 = "res://bin/PuertsV8.windows.template_debug.x86_64.dll"
[dependencies]
windows.debug.arm64 = {
                      "res://bin/PapiV8.dll": ""
}
```

> You need to add a dependencies section if the backend has dependencies; otherwise, the GDExtension may fail to load,
> especially if you encounter an error like `Could not find type "PuertsEnvironment" in the current scope`.

More backends see [build.md](./build.md)

## Example

- CreateInstance `PuertsV8Backend`
- Initialize `PuertsStringNameCachePool`
- Execute `eval` and get a result
- Tick in `_process` when using V8/Node.js debugger integration
- Dispose in `_exit_tree`

```gdscript
extends Node

var _env: PuertsEnvironment

func _ready() -> void:
	var backend := PuertsV8Backend.new()

	var pool := PuertsStringNameCachePool.new()
	var pool_err := pool.initialize(PuertsStringNameCachePool.POLICY_HASH_MAP, 512)
	if pool_err != OK:
		push_error("StringName cache pool initialize failed: %d" % pool_err)
		return

	_env = PuertsEnvironment.new()
	_env.set_error_callback(func(message: String):
		push_error("Puerts error: %s" % message)
	)
	var init_err := _env.initialize(backend, pool)
	if init_err != OK:
		push_error("PuertsEnvironment initialize failed: %d" % init_err)
		return

	var result := _env.eval("globalThis.answer = 40 + 2; answer;", "bootstrap.js")
	if result == null:
		push_error("Eval failed")
		return

	var answer := _env.get_global("answer").to_int()
	print("Puerts answer = ", answer) # 42

func _process(_delta: float) -> void:
	_env.debugger_tick() # V8/Node.js debugger tick

func _exit_tree() -> void:
	if _env != null:
		_env.dispose()
```

## Convention

### load_type

`load_type` is a function provided by `puerts-godot` to load a [static-binding](static-binding.md) type or a dynamic type from [ClassDB](https://docs.godotengine.org/en/stable/classes/class_classdb.html).

```javascript
const Vector2 = load_type("Vector2");
const v = new Vector2();
v.x = 8.0;
v.y = 6.0;
v.length() === 10.0 // true

const GlobalScope = load_type("GlobalScope");
GlobalScope.sin(1)
```

### to_callable

`to_callable` converts a script function to a Godot `Callable`.

```javascript
const callback = to_callable((message) => {
	log_info(message);
});

callback.call("hello");
```

### Enum / Signal

We treat enum as a static nested class and signal as a read-only property of the class.

```javascript
Vector2 = load_type("Vector2");
Vector2.Axis && Vector2.Axis.AXIS_X === 0 && Vector2.Axis.AXIS_Y === 1;
```

```javascript
const Timer = load_type("Timer");
const timer = new Timer();
timer.timeout.connect(to_callable(() => {
	log_info("timeout");
}));
// Note: You may need save the function reference to reuse it.
timer.start(1.0);
```

### Global Scope

We treat global scope as a static class with static methods, properties, and enums as static members.

```javascript
const GlobalScope = load_type("GlobalScope");
GlobalScope.sin(1)
```

### Operator Overloading

JavaScript does not support operator overloading, so we treat operator overloads as normal methods with a special name. e.g. `op_Addition`

See [index.js](../tools/puerts-godot-operator-model/index.js) for the operator name mapping.

```javascript
const Vector2 = load_type("Vector2");
const sum = Vector2.op_Addition(new Vector2(1.0, 2.0), new Vector2(3.0, 4.0));
sum.x === 4.0 && sum.y === 6.0;
```

### Constant

We treat constant as a static read-only property of the class. While get the value, we will create a new instance representing the constant value.

```javascript
const Vector2 = load_type("Vector2");
const v = Vector2.ZERO;
v.x === 0.0 && v.y === 0.0;
```

## API Reference

See api reference in godot editor.

## Supported backends and capabilities

We support `PuertsV8Backend`, `PuertsNodejsBackend`, `PuertsQuickjsBackend`, and `PuertsLuaBackend`.

Capability checks are based on the backend function table exposed to `PuertsEnvironment`; unsupported environment calls emit an error through the configured error callback.

See [backends.md](backends.md) for more details.

## Interop between Godot and Puerts

### PuertsEnvironment

We suggest you create a few instances of `PuertsEnvironment` in your project and share them between different scenes for performance.

Most projects will create only 1 `PuertsEnvironment` instance, but you can create multiple instances if you want to use different backends or separate script environments for different purposes.

Remember to call `dispose` when the environment is no longer needed.

### ScriptBackend

We recommend `PuertsV8Backend` or `PuertsNodejsBackend` for most projects. V8 has better performance in most cases.

> Use one of them. Do not use V8 and Nodejs in the same process, especially on Linux. It may cause issues.

And `ECMAScript` has a good ecosystem and more mature tools, so it is easier to use and maintain.

QuickJS is a good choice if you want a smaller binary size and do not need the advanced features of V8 or Nodejs, or when running on platforms where V8 or Nodejs is not supported, such as WASM.

### Godot to Puerts

We marshal Godot values to script values when passing them to script side, for example in `set_global` or as arguments of `call` and `call_method`.

| Godot                          | Way                           | Puerts             |
|--------------------------------|-------------------------------|--------------------|
| `Variant::NIL`                 | direct                        | `null`             |
| `Variant::BOOL`                | direct                        | `bool`             |
| `Variant::INT`                 | direct                        | `int32` or `int64` |
| `Variant::FLOAT`               | direct                        | `double`           |
| `Variant::STRING`              | direct                        | `string`           |
| `Variant::STRING_NAME`         | direct                        | `string`           |
| `Variant::PACKED_BYTE_ARRAY`   | direct binary cast if enabled | `binary`           |
| `Variant::OBJECT`              | direct or native binding      | `object`           |
| other built-in `Variant` types | boxed native binding          | native object      |

See [object-allocating.md](object-allocating.md) for more details.

Notes:

- `Variant::PACKED_BYTE_ARRAY -> script binary` is enabled by `VARIANT_TO_SCRIPT_PACKED_BYTE_ARRAY_CAST`.
- `script binary -> PackedByteArray` is enabled by `SCRIPT_TO_VARIANT_PACKED_BYTE_ARRAY_CAST`.
- Other built-in `Variant` types, such as `Vector2`, `Color`, `Array`, `Dictionary`, and the other `Packed*Array` types, are boxed and exposed through Puerts' native binding layer.

### Eval

`eval` executes script code and returns a `PuertsScriptValue` which holds a reference to the script value in the script engine.

The signature of `eval` is:

```gdscript
func eval(code: String, chunk_name: StringName = &"chunk") -> PuertsScriptValue
```

- `code`: the script code to execute
- `chunk_name`: an optional name for the code chunk, used for error reporting and debugging

Note that if you are using a backend that evaluates code in global scope, you can wrap the code in an IIFE to create a local scope, for example:

```js
(function() {
  // your code here
})();
```

### Puerts to Godot

`PuertsScriptValue` is created by `eval`, `get_global`, or other APIs. It holds a reference to a script value in the script engine and provides methods to convert it to Godot values.

- `is_valid`: checks its lifetime. It becomes false if the environment is disposed or the internal `value_ref` is released
- `is_null`: checks whether the script value is `null`
- `is_undefined`: checks whether the script value is `undefined`
- `is_bool`, `is_int`, `is_float`, `is_string`, `is_binary`, `is_object`: check the type on the script side
- `to_bool`, `to_int`, `to_float`, `to_string`, `to_binary`: convert using direct typed access. If the wrapper is invalid they return the default value for that type: `false`, `0`, `0.0`, `""`, or an empty `PackedByteArray`
- `unwrap_native`: unwraps a native pointer if the script value is a wrapped native Godot object or built-in value. Returns `null` if it is not a wrapped native value or the type does not match
- `to_native`: converts to the closest Godot `Variant` representation. It returns direct Godot values for primitive values, strings, binary values, and wrapped native Godot values. If the value cannot be represented directly, Puerts keeps it wrapped and returns `Ref<PuertsScriptValue>`
- `get_property`, `set_property`: get or set properties of an object
- `call`: call it as a function
- `call_method`: call a method of a script object

> We recommend `to_int`/`to_float`/`to_string`/`to_binary`/`unwrap_native` when you know the expected type, and `to_native` when you want to support multiple types or get the value generically.
>
> `to_native` is a bit slower than `to_int`/`to_float`/`to_string`/`to_binary`/`unwrap_native`, because it needs to check the type and do more work, and lack of type information can cause maintenance issues.

If you want better performance, you can use [static-binding](static-binding.md) to register your C++ types to script engine.

### PuertsStringNameCachePool

`PuertsStringNameCachePool` is a utility class to cache C strings in Godot, which can improve performance when calling script functions or methods frequently.

- `POLICY_HASH_MAP`: use a hash map to cache C strings, more memory usage
- `POLICY_FIXED_HASH_MAP`: use a fixed-size hash map to cache C strings, less memory usage
- `POLICY_NO_CACHE`: no cache, always create a new C string, less memory usage but lower performance if duplicate strings are used frequently

We recommend `POLICY_HASH_MAP` or `POLICY_FIXED_HASH_MAP` for better performance when you marshal many repeated strings.

Use `POLICY_NO_CACHE` if most strings are unique.

### Error reporting

- `set_error_callback(callback)`
- `set_warn_callback(callback)`
- `set_info_callback(callback)`

We call the callback when internal error, warning, or info messages occur, and pass the message to the callback.

And you can use `log_error`, `log_warn`, `log_info` to call the callback manually in your script code.

### Rich editor support

You can refer [puerts-godot-demo](https://github.com/realybin/puerts-godot-demo) for more information.

## Next

- Building: [build.md](./build.md)
- Backends: [backends.md](./backends.md)
- V8 Inspector: [v8-inspector.md](./v8-inspector.md)
- Static binding: [static-binding.md](./static-binding.md)
- Object allocation and lifetime: [object-allocating.md](./object-allocating.md)
- Advanced: [advanced.md](./advanced.md)
