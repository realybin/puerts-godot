# Static Binding

Static bindings connect script calls to C++ constructors, methods, and properties through template callbacks. `load_type(name)` checks the static registry before Godot ClassDB.

## Minimal binding

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
			.constructor(puerts::combine_constructors(
					puerts::make_constructor_overload<ExampleCounter>(),
					puerts::make_constructor_overload<ExampleCounter, double>()))
			.method("add", puerts::make_method<&ExampleCounter::add>())
			.property("value", puerts::make_property<&ExampleCounter::value>())
			.register_type();
}
```

Register once during scene initialization, before scripts call `load_type`:

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
| Constructor overloads | `combine_constructors(make_constructor_overload<T, Args...>(), ...)` |
| Instance method | `method("name", make_method<&T::method>())` |
| Static function | `static_method("name", make_function<&function>())` |
| Property | `property("name", make_property<&T::member>())` |

Use an explicit member-pointer cast for overloaded methods. `combine_overloads()` combines method signatures.

## Inheritance and lifetime

Register the base first, then use `.extends<Base>()` on the derived type. Each type needs its own `PUERTS_SCRIPT_TYPE` and constructor. The registry models one simple base relationship; multiple or virtual inheritance is not adjusted.

Plain C++ values use the generated finalizer and are deleted when their script wrapper is collected or the environment is disposed. Godot `Object` subclasses follow the bridge ownership rules in [Object Allocation and Lifetime](object-allocating.md). Plain C++ types do not gain a Godot `Variant` representation automatically.

The builder and macro are declared in [puerts_static_binding.h](../src/PuertsCore/puerts_static_binding.h). Generated bindings are registered by [puerts_builtin_binding.cpp](../src/PuertsCore/puerts_builtin_binding.cpp).