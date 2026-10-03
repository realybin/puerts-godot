# Advanced Usage

Reuse environments, loaded types, and script functions. Batch related work in one script call to reduce conversion and FFI overhead. Use static bindings for custom C++ types and typed reads when the result type is known.

For V8 and Node.js, `to_int()` reads BigInt; ordinary ECMAScript Number values should use `to_float()` or `to_native()`.

> [!NOTE]
> When constructing containers such as `Array`, use typed constructors with matching typed arguments whenever possible. For example:
> ```gdscript
> Array(base: Array, type: int, class_name: StringName, script: Variant)
> Dictionary(base: Dictionary, key_type: int, key_class_name: StringName, key_script: Variant, value_type: int, value_class_name: StringName, value_script: Variant)
> ```
> Ensure each argument uses the exact expected type to avoid unnecessary conversions and dynamic type resolution.

## StringName cache

`PuertsStringNameCachePool` caches UTF-8 names used for globals, properties, methods, and chunk names.

| Policy | Behavior |
|--------|----------|
| `POLICY_HASH_MAP` | Caches up to `capacity` entries (default 512). |
| `POLICY_FIXED_HASH_MAP` | Uses fixed storage for 1024 entries; ignores `capacity`. |
| `POLICY_NO_CACHE` | Converts each name on demand. |

When a cache reaches capacity, it clears all entries; it is not an LRU. Share a pool only between environments used on the same thread. `clear()` and reinitialization do not invalidate script values.

## Reentry and threads

Use an environment and its values on one thread; WebGL requires the browser main thread. Synchronous callbacks may reenter it. See [object lifetime](object-allocating.md#disposal-and-reinitialization) for disposal during callbacks.

## Runtime hooks

Call `tick()` for V8 and Node.js. Call `debugger_tick()` while their inspector is open. `low_memory_notification()` is available for V8, Node.js, and QuickJS; it may trigger collection but does not guarantee immediate finalization. See [Backends](backends.md).
