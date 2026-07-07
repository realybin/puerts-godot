// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_TYPE_REGISTER_H
#define PUERTS_GODOT_PUERTS_TYPE_REGISTER_H

#include "pesapi.h"
#include "puerts_eastl.h"

#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace puerts {
struct StaticTypeDefinition;
}

class PuertsEnvironment;

struct PuertsStringNameHash {
	size_t operator()(const godot::StringName &p_name) const {
		return static_cast<size_t>(p_name.hash());
	}
};

struct PuertsStringNameEqual {
	bool operator()(const godot::StringName &p_left, const godot::StringName &p_right) const {
		return p_left == p_right;
	}
};

namespace puerts_type_register_internal {
class TypeInfoFactory;
class RegistryBinder;
} //namespace puerts_type_register_internal

class PuertsTypeRegister {
public:
	struct TypeInfo {
		enum class Kind {
			OBJECT_CLASS,
			STATIC_BOUND,
			ENUM_GROUP,
		};

		struct MethodData {
			godot::StringName name;
			godot::StringName owner_class_name;
			uint32_t compatibility_hash = 0;
			GDExtensionMethodBindPtr method_bind = nullptr;
			pesapi_callback callback = nullptr;
			void *userdata = nullptr;
		};

		struct PropertyData {
			godot::StringName name;
			godot::Variant::Type variant_type = godot::Variant::NIL;
			godot::StringName class_name;
			int64_t int_constant = 0;
			pesapi_callback getter = nullptr;
			pesapi_callback setter = nullptr;
			void *getter_userdata = nullptr;
			void *setter_userdata = nullptr;
		};

		const void *type_id = nullptr;
		Kind kind = Kind::OBJECT_CLASS;
		godot::StringName class_name;
		godot::Variant::Type variant_type = godot::Variant::NIL;
		TypeInfo *base_type = nullptr;
		const void *base_type_id = nullptr;
		godot::StringName base_class_name;
		bool can_instantiate = false;
		bool is_registered = false;
		pesapi_constructor constructor = nullptr;
		pesapi_finalize finalize = nullptr;
		godot::Variant (*native_to_variant)(void *ptr) = nullptr;
		puerts_eastl::vector<MethodData> static_methods;
		puerts_eastl::vector<MethodData> instance_methods;
		puerts_eastl::vector<PropertyData> instance_properties;
		puerts_eastl::vector<PropertyData> static_properties;
	};

	static PuertsTypeRegister &get_singleton();

	[[nodiscard]] pesapi_registry get_registry() const;

	void register_static_type(const puerts::StaticTypeDefinition &p_definition);
	TypeInfo *find_or_add_object_type(const godot::StringName &p_name);
	TypeInfo *find_type_by_name(const godot::StringName &p_name);
	TypeInfo *get_builtin_type(godot::Variant::Type p_type);
	TypeInfo *get_type_by_id(const void *p_type_id);

	bool ensure_registered(TypeInfo *p_type_info);

	static void on_native_binding_exit(void *ptr, void *class_data, void *env_private, void *userdata);
	static void load_type_callback(struct pesapi_ffi *apis, pesapi_callback_info info);

private:
	friend class puerts_type_register_internal::TypeInfoFactory;
	friend class puerts_type_register_internal::RegistryBinder;

	PuertsTypeRegister();
	~PuertsTypeRegister();

	static void *object_default_constructor_callback(struct pesapi_ffi *apis, pesapi_callback_info info);
	static void *no_constructor_callback(struct pesapi_ffi *apis, pesapi_callback_info info);
	static void object_method_callback(struct pesapi_ffi *apis, pesapi_callback_info info);
	static void object_static_method_callback(struct pesapi_ffi *apis, pesapi_callback_info info);
	static void object_property_getter_callback(struct pesapi_ffi *apis, pesapi_callback_info info);
	static void object_property_setter_callback(struct pesapi_ffi *apis, pesapi_callback_info info);
	static void object_signal_getter_callback(struct pesapi_ffi *apis, pesapi_callback_info info);
	static void enum_group_getter_callback(struct pesapi_ffi *apis, pesapi_callback_info info);
	static void integer_constant_getter_callback(struct pesapi_ffi *apis, pesapi_callback_info info);
	static void read_only_property_setter_callback(struct pesapi_ffi *apis, pesapi_callback_info info);

	TypeInfo *build_object_type(const godot::StringName &p_name);
	void store_type(TypeInfo *p_type_info);
	bool resolve_base_type(TypeInfo *p_type_info);
	void register_type(TypeInfo *p_type_info);

	pesapi_registry_api reg_api_{};
	pesapi_registry registry_ = nullptr;
	pesapi_class_not_found_callback on_type_not_found_ = nullptr;
	puerts_eastl::hash_map<godot::StringName, TypeInfo *, PuertsStringNameHash, PuertsStringNameEqual> types_by_name_;
	puerts_eastl::hash_map<godot::StringName, TypeInfo *, PuertsStringNameHash, PuertsStringNameEqual> object_types_by_name_;
	puerts_eastl::hash_map<uint64_t, TypeInfo *> types_by_id_;
	TypeInfo *builtin_types_by_variant_[godot::Variant::VARIANT_MAX] = {};
	puerts_eastl::vector<TypeInfo *> owned_types_;
};

#endif // PUERTS_GODOT_PUERTS_TYPE_REGISTER_H
