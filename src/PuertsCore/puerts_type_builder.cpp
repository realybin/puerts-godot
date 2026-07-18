// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_binding_types.h"
#include "puerts_type_record.h"

#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/templates/hashfuncs.hpp>

using namespace godot;

namespace {

using TypeRecord = PuertsTypeRegister::TypeRecord;

struct StringNameHash {
	size_t operator()(const StringName &p_name) const {
		return static_cast<size_t>(p_name.hash());
	}
};

struct StringNameEqual {
	bool operator()(const StringName &p_left, const StringName &p_right) const {
		return p_left == p_right;
	}
};

using ReflectedMethodIndex = puerts_eastl::hash_map<StringName, TypeRecord::Method *, StringNameHash, StringNameEqual>;

bool should_skip_reflected_property(const Dictionary &p_property_dict) {
	const uint32_t usage = p_property_dict["usage"];
	constexpr uint32_t ignored_usage = PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP;
	return (usage & ignored_usage) != 0;
}

bool is_read_only_reflected_property(const Dictionary &p_property_dict) {
	return (static_cast<uint32_t>(p_property_dict["usage"]) & PROPERTY_USAGE_READ_ONLY) != 0;
}

uint32_t get_method_compatibility_hash(const MethodInfo &p_method_info) {
	const bool has_return = (p_method_info.return_val.type != Variant::NIL) || (p_method_info.return_val.usage & PROPERTY_USAGE_NIL_IS_VARIANT);

	uint32_t hash = hash_murmur3_one_32(has_return);
	hash = hash_murmur3_one_32(p_method_info.arguments.size(), hash);

	if (has_return) {
		hash = hash_murmur3_one_32(p_method_info.return_val.type, hash);
		if (p_method_info.return_val.class_name != StringName()) {
			hash = hash_murmur3_one_32(p_method_info.return_val.class_name.hash(), hash);
		}
	}

	for (const PropertyInfo &arg : p_method_info.arguments) {
		hash = hash_murmur3_one_32(arg.type, hash);
		if (arg.class_name != StringName()) {
			hash = hash_murmur3_one_32(arg.class_name.hash(), hash);
		}
	}

	hash = hash_murmur3_one_32(p_method_info.default_arguments.size(), hash);
	for (const Variant &default_arg : p_method_info.default_arguments) {
		hash = hash_murmur3_one_32(default_arg.hash(), hash);
	}

	hash = hash_murmur3_one_32((p_method_info.flags & METHOD_FLAG_CONST) ? 1 : 0, hash);
	hash = hash_murmur3_one_32((p_method_info.flags & METHOD_FLAG_VARARG) ? 1 : 0, hash);
	return hash_fmix32(hash);
}

void copy_methods(puerts_eastl::vector<TypeRecord::Method> &r_methods, const puerts_eastl::vector<puerts::MethodBinding> &p_bindings) {
	r_methods.reserve(p_bindings.size());
	for (const puerts::MethodBinding &binding : p_bindings) {
		r_methods.push_back({});
		TypeRecord::Method &method = r_methods.back();
		method.name = binding.name;
		method.callback = binding.callback;
		method.userdata = binding.userdata;
	}
}

void copy_properties(puerts_eastl::vector<TypeRecord::Property> &r_properties, const puerts_eastl::vector<puerts::PropertyBinding> &p_bindings) {
	r_properties.reserve(p_bindings.size());
	for (const puerts::PropertyBinding &binding : p_bindings) {
		r_properties.push_back({});
		TypeRecord::Property &property = r_properties.back();
		property.name = binding.name;
		property.getter = binding.getter;
		property.setter = binding.setter;
		property.getter_userdata = binding.getter_userdata;
		property.setter_userdata = binding.setter_userdata;
	}
}

} // namespace

PuertsTypeRegister::TypeRecord *PuertsTypeRegister::RecordBuilder::build_object_type(PuertsTypeRegister &p_registry, const StringName &p_name) {
	if (!ClassDB::class_exists(p_name)) {
		return nullptr;
	}

	TypeRecord *type = memnew(TypeRecord);
	type->type_id = type;
	type->kind = TypeRecord::Kind::REFLECTED_OBJECT;
	type->name = p_name;
	type->variant_type = Variant::OBJECT;
	type->constructible = ClassDB::can_instantiate(p_name);

	const StringName parent_name = ClassDB::get_parent_class(p_name);
	if (parent_name != StringName()) {
		type->base = p_registry.find_or_add_object_record(parent_name);
	}

	append_reflected_methods(type, ClassDB::class_get_method_list(p_name, true));
	append_reflected_properties(type, ClassDB::class_get_property_list(p_name, true));
	append_reflected_signals(type, p_name);
	const PackedStringArray integer_constants = ClassDB::class_get_integer_constant_list(p_name, true);
	append_reflected_enum_groups(p_registry, type, p_name);
	append_reflected_integer_constants(type, p_name, integer_constants);
	return type;
}

PuertsTypeRegister::TypeRecord *PuertsTypeRegister::RecordBuilder::build_static_type(const puerts::TypeDefinition &p_definition) {
	TypeRecord *type = memnew(TypeRecord);
	type->type_id = p_definition.type_id;
	type->kind = TypeRecord::Kind::STATIC_BINDING;
	type->name = p_definition.name;
	type->variant_type = p_definition.variant_type;
	type->base_id = p_definition.base_id;
	type->base_name = p_definition.base_name;
	type->constructor = p_definition.constructor;
	type->finalize = p_definition.finalize;
	type->to_variant = p_definition.to_variant;
	copy_methods(type->static_methods, p_definition.static_methods);
	copy_methods(type->instance_methods, p_definition.instance_methods);
	copy_properties(type->instance_properties, p_definition.instance_properties);
	copy_properties(type->static_properties, p_definition.static_properties);

	return type;
}

void PuertsTypeRegister::RecordBuilder::append_reflected_methods(TypeRecord *p_type, const TypedArray<Dictionary> &p_method_list) {
	size_t static_method_count = 0;
	for (const auto &entry : p_method_list) {
		const Dictionary method_dict = entry;
		static_method_count += (static_cast<uint32_t>(method_dict["flags"]) & METHOD_FLAG_STATIC) != 0;
	}
	p_type->static_methods.reserve(static_method_count);
	p_type->instance_methods.reserve(p_method_list.size() - static_method_count);
	for (const auto &i : p_method_list) {
		const Dictionary method_dict = i;
		const MethodInfo method_info = MethodInfo::from_dict(method_dict);
		const uint32_t method_flags = static_cast<uint32_t>(method_dict["flags"]);
		TypeRecord::Method method;
		method.name = method_dict["name"];
		method.owner_class_name = p_type->name;
		method.argument_count = method_info.arguments.size();
		if ((method_flags & METHOD_FLAG_VIRTUAL) == 0 && ClassDB::class_has_method(method.owner_class_name, method.name, true)) {
			// Resolve once while building the immutable type record. Reflected calls
			// then enter Godot without a lazy lookup branch or shared-state write.
			// The existence check also rejects pseudo-virtual entries such as
			// Object.free, which intentionally has no MethodBind despite its flags.
			method.method_bind = godot::gdextension_interface::classdb_get_method_bind(
					static_cast<GDExtensionConstStringNamePtr>(method.owner_class_name._native_ptr()),
					static_cast<GDExtensionConstStringNamePtr>(method.name._native_ptr()),
					get_method_compatibility_hash(method_info));
		}
		const bool is_static = (method_flags & METHOD_FLAG_STATIC) != 0;
		method.callback = is_static ? &PuertsTypeRegister::object_static_method_callback : &PuertsTypeRegister::object_method_callback;

		puerts_eastl::vector<TypeRecord::Method> &target_methods = is_static ? p_type->static_methods : p_type->instance_methods;
		target_methods.push_back(method);
	}
}

static ReflectedMethodIndex build_instance_method_index(TypeRecord *p_type) {
	size_t method_count = 0;
	for (TypeRecord *type = p_type; type != nullptr; type = type->base) {
		method_count += type->instance_methods.size();
	}

	// A transient index avoids the former property_count * method_count scan,
	// while keeping the steady-state TypeRecord objects free of redundant maps.
	ReflectedMethodIndex methods;
	methods.reserve(method_count);
	while (p_type != nullptr) {
		for (TypeRecord::Method &method : p_type->instance_methods) {
			methods.insert({ method.name, &method });
		}
		p_type = p_type->base;
	}
	return methods;
}

static TypeRecord::Method *find_reflected_method(const ReflectedMethodIndex &p_methods, const StringName &p_name) {
	const auto found = p_methods.find(p_name);
	return found == p_methods.end() ? nullptr : found->second;
}

void PuertsTypeRegister::RecordBuilder::append_reflected_properties(TypeRecord *p_type, const TypedArray<Dictionary> &p_property_list) {
	p_type->instance_properties.reserve(p_property_list.size());
	const ReflectedMethodIndex methods = build_instance_method_index(p_type);

	for (const auto &i : p_property_list) {
		const Dictionary property_dict = i;
		if (should_skip_reflected_property(property_dict)) {
			continue;
		}

		TypeRecord::Property property;
		property.name = property_dict["name"];
		property.getter_method = find_reflected_method(
				methods,
				ClassDB::class_get_property_getter(p_type->name, property.name));
		property.indexed = property.getter_method->argument_count != 0;
		property.getter = &PuertsTypeRegister::object_property_getter_callback;
		if (!is_read_only_reflected_property(property_dict)) {
			if (!property.indexed) {
				property.setter_method = find_reflected_method(
						methods,
						ClassDB::class_get_property_setter(p_type->name, property.name));
			}
			property.setter = &PuertsTypeRegister::object_property_setter_callback;
		}
		p_type->instance_properties.push_back(property);
	}
}

void PuertsTypeRegister::RecordBuilder::append_reflected_signals(TypeRecord *p_type, const StringName &p_name) {
	const TypedArray<Dictionary> signal_list = ClassDB::class_get_signal_list(p_name, true);
	p_type->instance_properties.reserve(p_type->instance_properties.size() + signal_list.size());
	for (const auto &entry : signal_list) {
		const Dictionary signal_dict = entry;
		TypeRecord::Property signal_property;
		signal_property.name = signal_dict["name"];
		signal_property.getter = &PuertsTypeRegister::object_signal_getter_callback;
		p_type->instance_properties.push_back(signal_property);
	}
}

void PuertsTypeRegister::RecordBuilder::append_reflected_enum_groups(PuertsTypeRegister &p_registry, TypeRecord *p_type, const StringName &p_name) {
	const PackedStringArray enum_names = ClassDB::class_get_enum_list(p_name, true);
	p_type->static_properties.reserve(p_type->static_properties.size() + enum_names.size());
	for (const auto &enum_name_value : enum_names) {
		const StringName enum_name = enum_name_value;
		TypeRecord *enum_type = memnew(TypeRecord);
		enum_type->type_id = enum_type;
		enum_type->kind = TypeRecord::Kind::STATIC_BINDING;
		enum_type->name = StringName(String(p_name) + "." + String(enum_name));

		const PackedStringArray enum_constants = ClassDB::class_get_enum_constants(p_name, enum_name, true);
		enum_type->static_properties.reserve(enum_constants.size());
		for (const auto &enum_constant_value : enum_constants) {
			const StringName enum_constant_name = enum_constant_value;
			TypeRecord::Property constant;
			constant.name = enum_constant_name;
			constant.int_constant = ClassDB::class_get_integer_constant(p_name, enum_constant_name);
			constant.getter = &PuertsTypeRegister::integer_constant_getter_callback;
			enum_type->static_properties.push_back(constant);
		}
		p_registry.store_type(enum_type);

		TypeRecord::Property enum_group_property;
		enum_group_property.name = enum_name;
		enum_group_property.getter = &PuertsTypeRegister::enum_group_getter_callback;
		enum_group_property.getter_userdata = enum_type;
		p_type->static_properties.push_back(enum_group_property);
	}
}

void PuertsTypeRegister::RecordBuilder::append_reflected_integer_constants(TypeRecord *p_type, const StringName &p_name, const PackedStringArray &p_integer_constants) {
	p_type->static_properties.reserve(p_type->static_properties.size() + p_integer_constants.size());

	for (const auto &integer_constant : p_integer_constants) {
		const StringName constant_name = integer_constant;
		const StringName enum_name = ClassDB::class_get_integer_constant_enum(p_name, constant_name, true);
		if (enum_name != StringName()) {
			continue;
		}

		TypeRecord::Property constant;
		constant.name = constant_name;
		constant.int_constant = ClassDB::class_get_integer_constant(p_name, constant_name);
		constant.getter = &PuertsTypeRegister::integer_constant_getter_callback;
		p_type->static_properties.push_back(constant);
	}
}
