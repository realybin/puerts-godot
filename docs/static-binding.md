# Static Binding

Static bindings connect script calls to C++ constructors, methods, and properties through template callbacks. `load_type(name)` queries the static registry first, then Godot ClassDB.

## Example

This example binds a plain C++ type without redeclaring a Godot built-in:

```cpp
#include "PuertsCore/puerts_static_binding.h"

class ExampleCounter {
public:
	double value = 0.0;
	ExampleCounter() = default;
	explicit ExampleCounter(double p_value) : value(p_value) {}
	double add(double p_amount) { value += p_amount; return value; }
};

PUERTS_SCRIPT_TYPE(ExampleCounter, "ExampleCounter")

void register_example_bindings() {
	puerts::define_type<ExampleCounter>()
			.constructor<>()
			.constructor<double>()
			.method<&ExampleCounter::add>("add")
			.property<&ExampleCounter::value>("value")
			.register_type();
}
```

Call `constructor<Args...>()` repeatedly to register constructor overloads. Register once during scene initialization, before scripts call `load_type`:

```javascript
const Counter = load_type("ExampleCounter");
const counter = new Counter(10.0);
counter.add(2.0); // 12
counter.value = 5.0;
```

## Supported builders

| Member | Builder |
|--------|---------|
| Constructor | `constructor<Args...>()` |
| Instance method | `method<&T::method>("name")` |
| Method overloads | `method<&overload1, &overload2>("name")` |
| Static function | `static_method<&function>("name")` |
| Member property | `property<&T::member>("name")` |
| Accessor property | `property<&getter, &setter>("name")` |
| Constant | `constant<VALUE>("name")` or `constant<&getter>("name")` |
| Enum group | `enum_type<EnumTag>("name")` |
| Signal | `signal<SignalName>("name")` |
| Finalizer | `finalizer(&fn)` / `no_finalize()` |

Use explicit member-pointer casts for overloaded methods. Instance methods can also be free functions whose first parameter is `T &` or `const T &`; this is useful for default-argument adapters and custom property accessors.

## Inheritance and lifetime

Register the base first, then use `.extends<Base>()` on the derived type. Each type needs its own `PUERTS_SCRIPT_TYPE` and constructor. The registry models one simple base relationship; multiple or virtual inheritance is not adjusted.

Plain C++ values use the generated finalizer and are deleted when their script wrapper is collected or the environment is disposed. Use `.finalizer(&fn)` to replace it, or `.no_finalize()` when the instance is owned elsewhere. Never set a finalizer on a Godot `Object` subclass. Godot `Object` subclasses follow the bridge ownership rules in [Object Allocation and Lifetime](lifetime.md). Plain C++ types do not gain a Godot `Variant` representation automatically.

The builder and macro are declared in [puerts_static_binding.h](../src/PuertsCore/puerts_static_binding.h). Generated bindings are registered by [puerts_builtin_binding.cpp](../src/PuertsCore/puerts_builtin_binding.cpp).
