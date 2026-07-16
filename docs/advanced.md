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

## V8

### Bytecode Cache
TBD

### Protecting your code
TBD
