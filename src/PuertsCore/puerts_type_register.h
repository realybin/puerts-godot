// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_TYPE_REGISTER_H
#define PUERTS_GODOT_PUERTS_TYPE_REGISTER_H

#include "pesapi.h"
#include "puerts_eastl.h"

#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace puerts {
struct TypeDefinition;
}

class PuertsTypeRegister {
public:
	struct TypeRecord;

	static PuertsTypeRegister &get_singleton();

	[[nodiscard]] pesapi_registry get_registry() const;

	void register_static_type(const puerts::TypeDefinition &p_definition);
	[[nodiscard]] const void *find_or_add_object_type(const godot::StringName &p_name);
	[[nodiscard]] const void *get_builtin_type_id(godot::Variant::Type p_type) const;
	[[nodiscard]] bool has_type(const void *p_type_id) const;
	bool ensure_registered(const void *p_type_id);
	[[nodiscard]] bool is_assignable(const void *p_type_id, const void *p_base_type_id) const;
	bool native_to_variant(void *p_pointer, const void *p_type_id, godot::Variant &r_value) const;

	static void on_native_binding_exit(void *ptr, void *class_data, void *env_private, void *userdata);
	static void load_type_callback(struct pesapi_ffi *apis, pesapi_callback_info info);

private:
	class RecordBuilder;

	struct StringNameHash {
		size_t operator()(const godot::StringName &p_name) const {
			return static_cast<size_t>(p_name.hash());
		}
	};

	struct StringNameEqual {
		bool operator()(const godot::StringName &p_left, const godot::StringName &p_right) const {
			return p_left == p_right;
		}
	};

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

	[[nodiscard]] TypeRecord *find_record(const void *p_type_id) const;
	[[nodiscard]] TypeRecord *find_type_by_name(const godot::StringName &p_name);
	[[nodiscard]] TypeRecord *find_or_add_object_record(const godot::StringName &p_name);
	void store_type(TypeRecord *p_type);
	bool resolve_base_type(TypeRecord *p_type);
	void register_type(TypeRecord *p_type);
	bool ensure_registered(TypeRecord *p_type);

	pesapi_registry_api reg_api_{};
	pesapi_registry registry_ = nullptr;
	puerts_eastl::hash_map<godot::StringName, TypeRecord *, StringNameHash, StringNameEqual> types_by_name_;
	puerts_eastl::hash_map<godot::StringName, TypeRecord *, StringNameHash, StringNameEqual> reflected_types_by_name_;
	puerts_eastl::hash_map<const void *, TypeRecord *> types_by_id_;
	TypeRecord *builtin_types_[godot::Variant::VARIANT_MAX] = {};
	puerts_eastl::vector<TypeRecord *> owned_types_;
};

#endif // PUERTS_GODOT_PUERTS_TYPE_REGISTER_H
