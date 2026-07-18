// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_TYPE_RECORD_H
#define PUERTS_GODOT_PUERTS_TYPE_RECORD_H

#include "puerts_type_register.h"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/typed_array.hpp>

struct PuertsTypeRegister::TypeRecord {
	enum class Kind {
		REFLECTED_OBJECT,
		STATIC_BINDING,
	};

	struct Method {
		godot::StringName name;
		godot::StringName owner_class_name;
		int argument_count = 0;
		GDExtensionMethodBindPtr method_bind = nullptr;
		pesapi_callback callback = nullptr;
		void *userdata = nullptr;
	};

	struct Property {
		godot::StringName name;
		Method *getter_method = nullptr;
		Method *setter_method = nullptr;
		bool indexed = false;
		int64_t int_constant = 0;
		pesapi_callback getter = nullptr;
		pesapi_callback setter = nullptr;
		void *getter_userdata = nullptr;
		void *setter_userdata = nullptr;
	};

	const void *type_id = nullptr;
	Kind kind = Kind::REFLECTED_OBJECT;
	godot::StringName name;
	godot::Variant::Type variant_type = godot::Variant::NIL;
	TypeRecord *base = nullptr;
	const void *base_id = nullptr;
	godot::StringName base_name;
	bool constructible = false;
	bool registered = false;
	pesapi_constructor constructor = nullptr;
	pesapi_finalize finalize = nullptr;
	godot::Variant (*to_variant)(void *ptr) = nullptr;
	puerts_eastl::vector<Method> static_methods;
	puerts_eastl::vector<Method> instance_methods;
	puerts_eastl::vector<Property> instance_properties;
	puerts_eastl::vector<Property> static_properties;
};

class PuertsTypeRegister::RecordBuilder {
public:
	static TypeRecord *build_object_type(PuertsTypeRegister &p_registry, const godot::StringName &p_name);
	static TypeRecord *build_static_type(const puerts::TypeDefinition &p_definition);

private:
	static void append_reflected_methods(TypeRecord *p_type, const godot::TypedArray<godot::Dictionary> &p_method_list);
	static void append_reflected_properties(TypeRecord *p_type, const godot::TypedArray<godot::Dictionary> &p_property_list);
	static void append_reflected_signals(TypeRecord *p_type, const godot::StringName &p_name);
	static void append_reflected_enum_groups(PuertsTypeRegister &p_registry, TypeRecord *p_type, const godot::StringName &p_name);
	static void append_reflected_integer_constants(TypeRecord *p_type, const godot::StringName &p_name, const godot::PackedStringArray &p_integer_constants);
};

#endif // PUERTS_GODOT_PUERTS_TYPE_RECORD_H
