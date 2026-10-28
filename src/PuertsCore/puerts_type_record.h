// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_TYPE_RECORD_H
#define PUERTS_GODOT_PUERTS_TYPE_RECORD_H

#include "puerts_type_registry.h"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace puerts::internal {
class CallbackContext;
}

struct PuertsTypeRegistry::TypeRecord {
	enum class Kind {
		ReflectedObject,
		StaticBinding,
	};

	enum class RegistrationState {
		Unregistered,
		Registering,
		Registered,
	};

	struct MethodOverload {
		int min_arity = 0;
		int max_arity = 0;
		bool (*invoke)(pesapi_ffi *apis, pesapi_callback_info info, puerts::internal::CallbackContext &frame) = nullptr;
	};

	struct ConstructorOverload {
		int arity = 0;
		bool (*invoke)(pesapi_ffi *apis, puerts::internal::CallbackContext &frame, void *&r_instance) = nullptr;
	};

	struct Method {
		const TypeRecord *owner_type = nullptr;
		godot::StringName name;
		bool has_arguments = false;
		bool use_no_args_ptrcall = false;
		GDExtensionMethodBindPtr method_bind = nullptr;
		GDExtensionPtrUtilityFunction utility_function = nullptr;
		int64_t utility_hash = -1;
		pesapi_callback callback = nullptr;
		puerts_eastl::vector<MethodOverload> overloads;
	};

	struct Property {
		godot::StringName name;
		const Method *getter_method = nullptr;
		const Method *setter_method = nullptr;
		bool indexed = false;
		int64_t int_constant = 0;
		pesapi_callback getter = nullptr;
		pesapi_callback setter = nullptr;
		void *getter_userdata = nullptr;
	};

	const void *type_id = nullptr;
	Kind kind = Kind::ReflectedObject;
	godot::StringName name;
	godot::Variant::Type variant_type = godot::Variant::NIL;
	TypeRecord *base = nullptr;
	const void *base_id = nullptr;
	godot::StringName base_name;
	bool constructible = false;
	RegistrationState registration_state = RegistrationState::Unregistered;
	puerts_eastl::vector<ConstructorOverload> constructor_overloads;
	pesapi_finalize finalize = nullptr;
	godot::Variant (*to_variant)(void *ptr) = nullptr;
	puerts_eastl::vector<Method> static_methods;
	puerts_eastl::vector<Method> instance_methods;
	puerts_eastl::vector<Property> instance_properties;
	puerts_eastl::vector<Property> static_properties;
};

class PuertsTypeRegistry::TypeRecordBuilder {
public:
	static TypeRecordOwner build_object_type(PuertsTypeRegistry &p_registry, const godot::StringName &p_name);

private:
	static void append_reflected_methods(TypeRecord *p_type, const godot::TypedArray<godot::Dictionary> &p_method_list);
	static void append_reflected_properties(TypeRecord *p_type, const godot::TypedArray<godot::Dictionary> &p_property_list);
	static void append_reflected_signals(TypeRecord *p_type, const godot::StringName &p_name);
	static void append_reflected_enum_groups(PuertsTypeRegistry &p_registry, TypeRecord *p_type, const godot::StringName &p_name);
	static void append_reflected_integer_constants(TypeRecord *p_type, const godot::StringName &p_name, const godot::PackedStringArray &p_integer_constants);
};

#endif // PUERTS_GODOT_PUERTS_TYPE_RECORD_H
