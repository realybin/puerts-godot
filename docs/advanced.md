# Advanced Usage

## Performance

> [!NOTE]
> If you hit performance problems caused by limitations in our design, please report them in an Issue.

* Reuse environments, loaded types, and script functions.
* Batch related work into a single script call to reduce conversion and FFI overhead.
* Use static bindings for custom C++ types.
* Use typed reads when the result type is known.
* For containers such as `Array`, use typed constructors with matching argument types whenever possible. For example:
  ```gdscript
  Array(base: Array, type: int, class_name: StringName, script: Variant)
  Dictionary(base: Dictionary, key_type: int, key_class_name: StringName, key_script: Variant, value_type: int, value_class_name: StringName, value_script: Variant)
  ```
    Make sure each argument uses exactly the expected type to avoid unnecessary conversions and dynamic type resolution.

## Reentry and Threads

Use an environment and its values on a single thread; the Web backend requires the browser main thread. For disposal during callbacks, see [object lifetime](lifetime.md#disposal-and-reinitialization).
