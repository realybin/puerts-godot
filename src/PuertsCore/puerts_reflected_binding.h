// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_REFLECTED_BINDING_H
#define PUERTS_GODOT_PUERTS_REFLECTED_BINDING_H

#include "puerts_type_record.h"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/typed_array.hpp>

struct PuertsReflectedMethod {
	const PuertsTypeRegistry::TypeRecord *owner_type = nullptr;
	godot::StringName name;
	GDExtensionMethodBindPtr method_bind = nullptr;
	bool has_arguments = false;
	bool use_no_args_ptrcall = false;
};

struct PuertsReflectedProperty {
	godot::StringName name;
	const PuertsReflectedMethod *getter_method = nullptr;
	const PuertsReflectedMethod *setter_method = nullptr;
	bool indexed = false;
};

struct PuertsReflectedType {
	puerts_eastl::vector<PuertsReflectedMethod> methods;
	puerts_eastl::vector<PuertsReflectedProperty> properties;
};

class PuertsTypeRegistry::TypeRecordBuilder {
public:
	static TypeRecordOwner build_object_type(PuertsTypeRegistry &p_registry, const godot::StringName &p_name);
	static bool complete_object_type(PuertsTypeRegistry &p_registry, TypeRecord *p_type);

private:
	static void append_reflected_methods(TypeRecord *p_type, const godot::TypedArray<godot::Dictionary> &p_method_list);
	static void append_reflected_properties(TypeRecord *p_type, const godot::TypedArray<godot::Dictionary> &p_property_list);
	static void append_reflected_signals(TypeRecord *p_type, const godot::StringName &p_name);
	static void append_reflected_enum_groups(PuertsTypeRegistry &p_registry, TypeRecord *p_type, const godot::StringName &p_name);
	static void append_reflected_integer_constants(TypeRecord *p_type, const godot::StringName &p_name, const godot::PackedStringArray &p_integer_constants);
};

#endif // PUERTS_GODOT_PUERTS_REFLECTED_BINDING_H
