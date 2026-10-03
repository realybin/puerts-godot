# Binding Conventions

The environment provides `load_type()`, `to_callable()`, and logging helpers.

## Types

`load_type(name)` checks static bindings before Godot ClassDB. Cache a type used repeatedly.

```javascript
const Vector2 = load_type("Vector2");
const value = new Vector2(3.0, 4.0);
value.length(); // 5
```

ClassDB fallback constructors are zero-argument. Static bindings may expose other constructors. Script-created `Node` objects remain script-owned after `add_child()`; see [Object Allocation and Lifetime](object-allocating.md).

## Callables and signals

`to_callable(function)` retains the function and closure. Keep the callable to disconnect it:

```javascript
const callback = to_callable(() => log_info("timeout"));
timer.timeout.connect(callback);
timer.timeout.disconnect(callback);
```

Disposal invalidates script callables. A signal connection also retains its callable until disconnected or its emitter is destroyed.

## Enums, constants, and globals

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

See the [operator map](../tools/puerts-godot-operator-model/index.js).