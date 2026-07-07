# Static Binding

> Note: This may outdate if we have no time to update the document.

This document explains static binding in `puerts-godot`.

## What static binding is?

In general, we use reflection-based paths based on [ClassDB](https://docs.godotengine.org/en/stable/classes/class_classdb.html), but reflection may be slow.
Static binding may be faster.

Static binding maps C++ types and members to script-visible types at compile time.
You register constructors, methods, and properties in C++.
At runtime, scripts load the type by name with `load_type("TypeName")`.

We will find the type in a static registry first, if not found, fallback to reflection-based paths.

Use static binding when you need predictable behavior and lower runtime overhead than reflection-based paths.
So you can integrate with existing C++ libraries and expose their APIs to scripts.

More details in
- [puerts_type_register.cpp](../src/PuertsCore/puerts_type_register.cpp)
- [register_types.cpp](../src/PuertsCore/register_types.cpp)
- [puerts_builtin_binding.cpp](../src/PuertsCore/puerts_builtin_binding.cpp)
- [puerts_builtin_binding.h](../src/PuertsCore/puerts_builtin_binding.h)
- [puerts_builtin_bindings.generated.inc](../src/PuertsCore/puerts_builtin_bindings.generated.inc)

## Minimal example

```cpp
#include "puerts_static_binding.h"
#include <godot_cpp/variant/vector2.hpp>

PUERTS_SCRIPT_TYPE(godot::Vector2, "Vector2")

void register_my_bindings() {
	puerts::define_class<godot::Vector2>()
			.constructor(puerts::combine_constructors(
					puerts::make_constructor_overload<godot::Vector2>(),
					puerts::make_constructor_overload<godot::Vector2, float, float>()))
			.method("length", puerts::make_method<&godot::Vector2::length>())
			.property("x", puerts::make_property<&godot::Vector2::x>())
			.property("y", puerts::make_property<&godot::Vector2::y>())
			.register_type();
}
```

## Inheritance example (`extends`)

```cpp
class MyBase {
public:
	int id = 0;
	int get_id() const { return id; }
};

class MyDerived : public MyBase {
public:
	void set_id(int p_id) { id = p_id; }
};

PUERTS_SCRIPT_TYPE(MyBase, "MyBase")
PUERTS_SCRIPT_TYPE(MyDerived, "MyDerived")

void register_inheritance_bindings() {
	puerts::define_class<MyBase>()
			.constructor<>()
			.method("get_id", puerts::make_method<&MyBase::get_id>())
			.register_type();

	puerts::define_class<MyDerived>()
			.extends<MyBase>()
			.constructor<>()
			.method("set_id", puerts::make_method<&MyDerived::set_id>())
			.register_type();
}
```

```js
const Vector2 = load_type("Vector2");
const v = new Vector2(3.0, 4.0);
console.log(v.length()); // 5
```

## Register at initialization

Register static types during module initialization, before script code uses `load_type`.
For example, this project registers built-in static bindings in `initialize_puerts_core_module()`:

- [register_types.cpp](../src/PuertsCore/register_types.cpp)
- [puerts_builtin_binding.cpp](../src/PuertsCore/puerts_builtin_binding.cpp)

## Auto generate static binding code

[puerts-godot-binding-gen](../tools/puerts-godot-binding-gen)

## Troubleshooting

- `Type not found`: confirm `PUERTS_SCRIPT_TYPE` name matches the `load_type` name.
- `No constructor overload matches`: constructor signature or argument types do not match.
- `No overload matches`: method overload resolution failed for the provided arguments.
- `Property type does not match`: assignment value cannot convert to the property type.
