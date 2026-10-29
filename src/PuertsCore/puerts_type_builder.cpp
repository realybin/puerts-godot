// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_reflected_binding.h"

#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/templates/hashfuncs.hpp>

using namespace godot;

namespace {

using TypeRecord = PuertsTypeRegistry::TypeRecord;

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

using ReflectedMethodIndex = puerts_eastl::hash_map<StringName, const PuertsReflectedMethod *, StringNameHash, StringNameEqual>;

bool should_skip_reflected_property(const Dictionary &p_property_dict) {
	const uint32_t usage = p_property_dict["usage"];
	constexpr uint32_t kIgnoredPropertyUsage = PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP;
	return (usage & kIgnoredPropertyUsage) != 0;
}

bool is_read_only_reflected_property(const Dictionary &p_property_dict) {
	return (static_cast<uint32_t>(p_property_dict["usage"]) & PROPERTY_USAGE_READ_ONLY) != 0;
}

bool method_has_return(const MethodInfo &p_method_info) {
	return p_method_info.return_val.type != Variant::NIL || (p_method_info.return_val.usage & PROPERTY_USAGE_NIL_IS_VARIANT);
}

uint32_t get_method_compatibility_hash(const MethodInfo &p_method_info) {
	const bool has_return = method_has_return(p_method_info);

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

template <typename Member>
Member *find_member(puerts_eastl::vector<Member> &members, const StringName &name) {
	for (auto &member : members) {
		if (member.name == name) {
			return &member;
		}
	}
	return nullptr;
}

} // namespace

PuertsTypeRegistry::TypeRecordOwner PuertsTypeRegistry::TypeRecordBuilder::build_object_type(PuertsTypeRegistry &p_registry, const StringName &p_name) {
	if (!ClassDB::class_exists(p_name)) {
		return {};
	}

	TypeRecordOwner owner(memnew(TypeRecord));
	TypeRecord *type = owner.get();
	type->type_id = type;
	type->kind = TypeRecord::Kind::ReflectedObject;
	type->name = p_name;
	type->variant_type = Variant::OBJECT;
	if (!complete_object_type(p_registry, type)) {
		return {};
	}
	return owner;
}

bool PuertsTypeRegistry::TypeRecordBuilder::complete_object_type(PuertsTypeRegistry &p_registry, TypeRecord *p_type) {
	const StringName &name = p_type->name;
	ERR_FAIL_COND_V_MSG(!ClassDB::class_exists(name), false, "The bound Godot class must exist in ClassDB.");
	p_type->constructible = ClassDB::can_instantiate(name);
	p_type->reflection = p_type->make_callback_data(PuertsReflectedType{});
	const StringName parent_name = ClassDB::get_parent_class(name);
	if (!parent_name.is_empty()) {
		p_type->base = p_registry.find_or_add_object_record(parent_name);
		ERR_FAIL_NULL_V(p_type->base, false);
	}
	append_reflected_methods(p_type, ClassDB::class_get_method_list(name, true));
	append_reflected_properties(p_type, ClassDB::class_get_property_list(name, true));
	append_reflected_signals(p_type, name);
	append_reflected_enum_groups(p_registry, p_type, name);
	append_reflected_integer_constants(p_type, name, ClassDB::class_get_integer_constant_list(name, true));
	return true;
}

void PuertsTypeRegistry::TypeRecordBuilder::append_reflected_methods(TypeRecord *p_type, const TypedArray<Dictionary> &p_method_list) {
	size_t static_method_count = 0;
	for (const auto &entry : p_method_list) {
		const Dictionary method_dict = entry;
		static_method_count += (static_cast<uint32_t>(method_dict["flags"]) & METHOD_FLAG_STATIC) != 0;
	}
	p_type->static_methods.reserve(p_type->static_methods.size() + static_method_count);
	p_type->instance_methods.reserve(p_type->instance_methods.size() + p_method_list.size() - static_method_count);
	p_type->reflection->methods.reserve(p_method_list.size());
	for (const auto &i : p_method_list) {
		const Dictionary method_dict = i;
		const MethodInfo method_info = MethodInfo::from_dict(method_dict);
		const uint32_t method_flags = method_info.flags;
		PuertsReflectedMethod method;
		method.owner_type = p_type;
		method.name = method_info.name;
		method.has_arguments = !method_info.arguments.is_empty();
		method.use_no_args_ptrcall = !method.has_arguments && !method_has_return(method_info) && (method_flags & METHOD_FLAG_VARARG) == 0;
		if ((method_flags & METHOD_FLAG_VIRTUAL) == 0 && ClassDB::class_has_method(p_type->name, method.name, true)) {
			// Resolve once while building the immutable type record. Reflected calls
			// then enter Godot without a lazy lookup branch or shared-state write.
			// The existence check also rejects pseudo-virtual entries such as
			// Object.free, which intentionally has no MethodBind despite its flags.
			method.method_bind = godot::gdextension_interface::classdb_get_method_bind(
					static_cast<GDExtensionConstStringNamePtr>(p_type->name._native_ptr()),
					static_cast<GDExtensionConstStringNamePtr>(method.name._native_ptr()),
					get_method_compatibility_hash(method_info));
		}
		const bool is_static = (method_flags & METHOD_FLAG_STATIC) != 0;
		p_type->reflection->methods.push_back(method);
		puerts_eastl::vector<TypeRecord::Method> &target_methods = is_static ? p_type->static_methods : p_type->instance_methods;
		if (find_member(target_methods, method.name) == nullptr) {
			TypeRecord::Method binding;
			binding.name = method.name;
			binding.callback = is_static ? &PuertsTypeRegistry::object_static_method_callback : &PuertsTypeRegistry::object_method_callback;
			binding.callback_data = &p_type->reflection->methods.back();
			target_methods.push_back(binding);
		}
	}
}

static ReflectedMethodIndex build_instance_method_index(const TypeRecord *p_type) {
	size_t method_count = 0;
	for (const TypeRecord *type = p_type; type != nullptr; type = type->base) {
		method_count += type->reflection->methods.size();
	}

	// A transient index avoids the former property_count * method_count scan,
	// while keeping the steady-state TypeRecord objects free of redundant maps.
	ReflectedMethodIndex methods;
	methods.reserve(method_count);
	while (p_type != nullptr) {
		for (const PuertsReflectedMethod &method : p_type->reflection->methods) {
			methods.insert({ method.name, &method });
		}
		p_type = p_type->base;
	}
	return methods;
}

static const PuertsReflectedMethod *find_reflected_method(const ReflectedMethodIndex &p_methods, const StringName &p_name) {
	const auto found = p_methods.find(p_name);
	return found == p_methods.end() ? nullptr : found->second;
}

void PuertsTypeRegistry::TypeRecordBuilder::append_reflected_properties(TypeRecord *p_type, const TypedArray<Dictionary> &p_property_list) {
	p_type->instance_properties.reserve(p_type->instance_properties.size() + p_property_list.size());
	p_type->reflection->properties.reserve(p_property_list.size());
	const ReflectedMethodIndex methods = build_instance_method_index(p_type);

	for (const auto &i : p_property_list) {
		const Dictionary property_dict = i;
		if (should_skip_reflected_property(property_dict)) {
			continue;
		}

		PuertsReflectedProperty property;
		property.name = property_dict["name"];
		if (find_member(p_type->instance_properties, property.name) != nullptr) {
			continue;
		}
		property.getter_method = find_reflected_method(
				methods,
				ClassDB::class_get_property_getter(p_type->name, property.name));
		if (property.getter_method == nullptr) {
			continue;
		}
		property.indexed = property.getter_method->has_arguments;
		if (!is_read_only_reflected_property(property_dict)) {
			if (!property.indexed) {
				property.setter_method = find_reflected_method(
						methods,
						ClassDB::class_get_property_setter(p_type->name, property.name));
			}
		}
		p_type->reflection->properties.push_back(property);
		TypeRecord::Property binding;
		binding.name = property.name;
		binding.getter = &PuertsTypeRegistry::object_property_getter_callback;
		binding.getter_data = &p_type->reflection->properties.back();
		if (!is_read_only_reflected_property(property_dict) && (property.indexed || property.setter_method != nullptr)) {
			binding.setter = &PuertsTypeRegistry::object_property_setter_callback;
			binding.setter_data = binding.getter_data;
		}
		p_type->instance_properties.push_back(binding);
	}
}

void PuertsTypeRegistry::TypeRecordBuilder::append_reflected_signals(TypeRecord *p_type, const StringName &p_name) {
	const TypedArray<Dictionary> signal_list = ClassDB::class_get_signal_list(p_name, true);
	p_type->instance_properties.reserve(p_type->instance_properties.size() + signal_list.size());
	for (const auto &entry : signal_list) {
		const Dictionary signal_dict = entry;
		TypeRecord::Property signal_property;
		signal_property.name = signal_dict["name"];
		if (find_member(p_type->instance_properties, signal_property.name) != nullptr) {
			continue;
		}
		signal_property.getter = &PuertsTypeRegistry::signal_getter_callback;
		p_type->instance_properties.push_back(signal_property);
	}
}

void PuertsTypeRegistry::TypeRecordBuilder::append_reflected_enum_groups(PuertsTypeRegistry &p_registry, TypeRecord *p_type, const StringName &p_name) {
	const PackedStringArray enum_names = ClassDB::class_get_enum_list(p_name, true);
	p_type->static_properties.reserve(p_type->static_properties.size() + enum_names.size());
	for (const auto &enum_name_value : enum_names) {
		const StringName enum_name = enum_name_value;
		if (find_member(p_type->static_properties, enum_name) != nullptr) {
			continue;
		}
		const StringName qualified_name = String(p_name) + "." + String(enum_name);
		if (p_registry.types_by_name_.find(qualified_name) == p_registry.types_by_name_.end()) {
			TypeRecordOwner enum_owner(memnew(TypeRecord));
			TypeRecord *nested_type = enum_owner.get();
			nested_type->type_id = nested_type;
			nested_type->kind = TypeRecord::Kind::StaticBinding;
			nested_type->name = qualified_name;
			const PackedStringArray enum_constants = ClassDB::class_get_enum_constants(p_name, enum_name, true);
			for (const auto &constant_name : enum_constants) {
				TypeRecord::Property constant;
				constant.name = constant_name;
				constant.getter_data = nested_type->make_callback_data(static_cast<int64_t>(ClassDB::class_get_integer_constant(p_name, constant_name)));
				constant.getter = &PuertsTypeRegistry::integer_constant_getter_callback;
				nested_type->static_properties.push_back(constant);
			}
			p_registry.store_type(puerts_eastl::move(enum_owner));
		}

		TypeRecord::Property enum_group_property;
		enum_group_property.name = enum_name;
		enum_group_property.getter = &PuertsTypeRegistry::nested_type_getter_callback;
		enum_group_property.getter_data = p_type->make_callback_data(qualified_name);
		p_type->static_properties.push_back(enum_group_property);
	}
}

void PuertsTypeRegistry::TypeRecordBuilder::append_reflected_integer_constants(TypeRecord *p_type, const StringName &p_name, const PackedStringArray &p_integer_constants) {
	p_type->static_properties.reserve(p_type->static_properties.size() + p_integer_constants.size());

	for (const auto &integer_constant : p_integer_constants) {
		const StringName constant_name = integer_constant;
		const StringName enum_name = ClassDB::class_get_integer_constant_enum(p_name, constant_name, true);
		if (enum_name != StringName()) {
			continue;
		}

		if (find_member(p_type->static_properties, constant_name) != nullptr) {
			continue;
		}
		TypeRecord::Property constant;
		constant.name = constant_name;
		constant.getter_data = p_type->make_callback_data(static_cast<int64_t>(ClassDB::class_get_integer_constant(p_name, constant_name)));
		constant.getter = &PuertsTypeRegistry::integer_constant_getter_callback;
		p_type->static_properties.push_back(constant);
	}
}
