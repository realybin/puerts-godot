# Advanced

## Enum / Signal

We treat enum as a static nested class and signal as a read-only property of the class.

## Global Scope

We treat global scope as a static class with static methods, properties, and enums as static members.

## Operator Overloading

JavaScript does not support operator overloading, so we treat operator overloads as normal methods with a special name.

See [index.js](../tools/puerts-godot-operator-model/index.js) for the operator name mapping.

## Constant

We treat constant as a static read-only property of the class. While get the value, we will create a new instance representing the constant value.

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
