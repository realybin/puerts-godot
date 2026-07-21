# Advanced

## Performance Notes

* cache string name
* static binding
* method bind
* avoid reflection-based paths as possible
* Minimize implicit conversions, as they may introduce significant overhead. Use the most precise types available.
  > When constructing containers such as `Array`, use typed constructors with matching typed arguments whenever possible. For example:
  > ```gdscript
  > Array(base: Array, type: int, class_name: StringName, script: Variant)
  > Dictionary(base: Dictionary, key_type: int, key_class_name: StringName, key_script: Variant, value_type: int, value_class_name: StringName, value_script: Variant)
  > ```
  Ensure each argument uses the exact expected type to avoid unnecessary conversions and dynamic type resolution.
* Calling scripts from `_process` or other hot paths may incur significant FFI overhead. Make sure you understand the performance implications.

## V8

### Bytecode Cache
TBD

### Protecting your code
TBD
