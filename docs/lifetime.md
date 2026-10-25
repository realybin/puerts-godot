# Lifetime

Learning Rust's ownership concepts helps when reading this document.

## Godot values in script

| Native value                                            | Ownership while wrapped                                            |
|---------------------------------------------------------|--------------------------------------------------------------------|
| `RefCounted`                                            | One strong reference; released with the wrapper.                   |
| Non-`RefCounted` created by script                      | Script-owned; deleted when its bridge record is released if alive. |
| Non-`RefCounted` passed from Godot                      | Borrowed; the wrapper neither keeps it alive nor deletes it.       |
| Boxed built-in (`Vector2`, `Array`, etc.)               | A copied `Variant`; released with the wrapper.                     |

The same object reuses one bridge record in an environment. Borrowed wrappers resolve by object ID: after Godot frees the object, `unwrap_native()` returns `null` and native access reports an invalid object. The script-side wrapper may remain valid.

## Script values in Godot

`eval()`, `get_global()`, property reads, and calls return `PuertsScriptValue`. Holding the wrapper keeps its script value reachable. `to_callable()` retains a function and closure. Neither keeps the environment alive, and values cannot cross environments.

## Disposal and reinitialization

> The following applies to V8, Node.js, QuickJS, and Lua. [WebGL](backends.md#browser-javascript-webgl) keeps its environment until page unload, i.e. it is a singleton.

`dispose()` immediately makes `is_ready()` false and invalidates all values and callables. Cleanup releases script references, strong `RefCounted` references, boxed values, and script-owned non-`RefCounted` objects. Borrowed objects remain Godot-owned.

If disposal is requested during a script operation or callback, new operations are rejected and cleanup waits for the outer operation. `initialize()` returns `ERR_BUSY` during initialization, an active operation, or disposal. Use one thread for an environment, its values, and its UTF-8 name cache.

Calling `initialize()` on an idle environment first disposes the previous runtime, even if new setup fails. A successful call starts a new generation; old values and callables stay invalid and must be reacquired. Log callbacks remain configured.

```gdscript
var old_value := env.eval("({ answer: 42 })")
env.dispose()
assert(not old_value.is_valid())
if env.initialize(backend, pool) == OK:
	var new_value := env.eval("({ answer: 43 })")
	assert(new_value.is_valid())
```

See [Getting Started](getting-started.md) for initialization and [test_lifecycle.gd](../tests/tests/test_lifecycle.gd) for ownership checks.
