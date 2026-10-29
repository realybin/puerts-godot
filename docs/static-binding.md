# Static Binding

Static bindings expose C++ types and members to scripts. Static Godot bindings and ClassDB reflection share one type identity; reflection supplies members without static bindings.

## Example

Register once during scene initialization, before scripts call `load_type`:

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

```javascript
const Counter = load_type("ExampleCounter");
const counter = new Counter(10);
counter.add(2); // 12
counter.value = 5;
```

## Registering members

| Member | API |
|--------|-----|
| Constructor | `constructor<Args...>()` |
| Instance method | `method<&T::method>("name")` |
| Static function | `static_method<&function>("name")` |
| Member property | `property<&T::member>("name")` |
| Accessor property | `property<&getter, &setter>("name")`; omit the setter for read-only access |
| Constant | `constant("name", value)` or `constant<&getter>("name")` |
| Nested type or enum group | `nested_type("Mode", "Example.Mode")` |
| Signal | `signal("name")`, for Godot Object types only |

Register namespaces and enum groups with `define_namespace`:

```cpp
puerts::define_namespace("Example.Mode")
		.constant("FAST", 1)
		.register_type();
```

## Overloads and inheritance

Repeat `constructor<Args...>()` for constructor overloads. Bind method overloads with `method<&overload1, &overload2>("name")`, using explicit member-pointer casts where needed. Instance methods also accept free functions whose first parameter is `T &` or `const T &`.

Overloads prefer exact types, then compatible conversions, with `Variant` parameters as a fallback. Equally ranked matches report an ambiguity. Duplicate types and member names within the same scope are rejected.

Use `.extends<Base>()` for plain C++ inheritance and register the base first. Each type exposes one base to scripts, with the required pointer adjustment. Godot classes use ClassDB inheritance.

## Ownership

Conversion rules for plain C++ types:

| Parameter or return value | Behavior |
|---------------------------|----------|
| `const T &` parameter | Borrows the original object |
| Value return | The script owns the returned value |
| Reference or `const T *` return | Creates a script-owned copy; the type must be copyable |
| `T *` return | Borrows the original object, which must remain valid while scripts use it |
| Null pointer | Maps to script null |

Non-const reference parameters and pointers to Godot builtin values are unsupported. Plain C++ types do not automatically convert to Variant.

Plain C++ instances are deleted when their wrapper is collected or the environment is disposed. Use `.finalizer(&fn)` for a custom finalizer. `.no_finalize()` is only for externally owned instances and cannot be combined with constructors or returns that create owned copies.

Godot types reject custom finalizers; Object types follow the [object lifetime rules](lifetime.md). Builtin values use Variant storage, and instance mutations write back to that storage.

See [puerts_static_binding.h](../src/PuertsCore/puerts_static_binding.h) for API definitions and the [generator guide](../tools/puerts-godot-binding-gen/README.md) for builtin binding generation.
