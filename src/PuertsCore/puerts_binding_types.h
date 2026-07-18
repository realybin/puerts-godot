// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_BINDING_TYPES_H
#define PUERTS_GODOT_PUERTS_BINDING_TYPES_H

#include "pesapi.h"
#include "puerts_eastl.h"

#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace puerts {

template <typename T, typename = void>
struct script_type_name;

template <typename T>
struct script_type_name<T, eastl::void_t<decltype(T::get_class_static())>> {
	static godot::StringName value() {
		return T::get_class_static();
	}
};

template <typename T>
struct static_type_id {
	static const void *get() {
		static int type_tag = 0;
		return &type_tag;
	}
};

struct MethodBinding {
	godot::StringName name;
	pesapi_callback callback = nullptr;
	void *userdata = nullptr;
};

struct PropertyBinding {
	godot::StringName name;
	pesapi_callback getter = nullptr;
	pesapi_callback setter = nullptr;
	void *getter_userdata = nullptr;
	void *setter_userdata = nullptr;
};

struct TypeDefinition {
	const void *type_id = nullptr;
	godot::StringName name;
	godot::Variant::Type variant_type = godot::Variant::NIL;
	const void *base_id = nullptr;
	godot::StringName base_name;
	pesapi_constructor constructor = nullptr;
	pesapi_finalize finalize = nullptr;
	godot::Variant (*to_variant)(void *ptr) = nullptr;
	puerts_eastl::vector<MethodBinding> static_methods;
	puerts_eastl::vector<MethodBinding> instance_methods;
	puerts_eastl::vector<PropertyBinding> instance_properties;
	puerts_eastl::vector<PropertyBinding> static_properties;
};

struct MethodSpec {
	pesapi_callback callback = nullptr;
	void *userdata = nullptr;
};

struct PropertySpec {
	pesapi_callback getter = nullptr;
	pesapi_callback setter = nullptr;
	void *getter_userdata = nullptr;
	void *setter_userdata = nullptr;
};

struct ConstructorSpec {
	pesapi_constructor constructor = nullptr;
	pesapi_finalize finalize = nullptr;
};

} // namespace puerts

#endif // PUERTS_GODOT_PUERTS_BINDING_TYPES_H
