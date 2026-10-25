# Conventions

We provide some helper functions.

## Loading types

`load_type(name)` queries static bindings first, then Godot ClassDB. Cache a type that is used repeatedly.

```javascript
const Vector2 = load_type("Vector2");
const value = new Vector2(3.0, 4.0);
value.length(); // 5
```

> [!NOTE]
> Types obtained from ClassDB only support zero-argument construction. Static bindings may expose other constructors.

### Using import

For convenience, we also provide a Babel plugin based on the rewritten output, so you can use `import` to load types.

That is, you can write:

```javascript
import Godot, { Node as GodotNode } from "godot";
```

See [README.md](../src/PuertsRuntime/ECMAScript/babel-plugin-godot-import-rewrite/README.md) for details.

## Type mapping

Default mapping for Godot `Variant` values passed to script:

| Godot                                                             | Script           |
|-------------------------------------------------------------------|------------------|
| `null`, `bool`, `int`, `float`, `String`, `StringName`            | Cast             |
| `Object`                                                          | Wrapper          |
| Built-in types (such as `Vector2`, `Array`, `Dictionary`, etc.)   | Wrapper          |
| `PuertsScriptValue`                                               | Raw Script Value |

Casts for `PackedByteArray`:

- `VARIANT_TO_SCRIPT_PACKED_BYTE_ARRAY_CAST`: copies `PackedByteArray` into the script's ByteArray type.
- `SCRIPT_TO_VARIANT_PACKED_BYTE_ARRAY_CAST`: copies the script binary into `PackedByteArray` when converting to `Variant`, including `to_native()`.

This is not recommended, however, as it incurs extra overhead; so the default is wrapping.

## Callable & Signal

`to_callable(function)` retains the function and its closure. Keep the callable to disconnect it:

```javascript
const callback = to_callable(() => log_info("timeout"));
timer.timeout.connect(callback);
timer.timeout.disconnect(callback);
```

Disposing the environment invalidates script callables. A signal connection also retains its callable until it is disconnected or its emitter is destroyed.

## Enums, Constants, and GlobalScope

Enums are nested static members. Constants are read-only static properties. Global functions and constants are members of `GlobalScope`.

```javascript
const Vector2 = load_type("Vector2");
Vector2.Axis.AXIS_X; // 0
Vector2.ZERO;        // value constant

const GlobalScope = load_type("GlobalScope");
GlobalScope.sin(1.0);
```

## Operators

Operator overloads are static methods named by the operator map:

```javascript
const sum = Vector2.op_Addition(new Vector2(1, 2), new Vector2(3, 4));
```

> [!NOTE]
> We do it this way because JavaScript does not support operator overloading.

See the [operator map](../tools/puerts-godot-operator-model/index.js) for more mappings.
