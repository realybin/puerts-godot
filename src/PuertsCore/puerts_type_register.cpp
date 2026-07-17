// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_type_register.h"

#include "puerts_bridge_registry.h"
#include "puerts_environment.h"
#include "puerts_static_binding.h"

#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/version.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/templates/hashfuncs.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/signal.hpp>

using namespace godot;

extern "C" void *GetRegisterApi();

namespace {

using TypeInfo = PuertsTypeRegister::TypeInfo;
using RuntimeArgumentValues = puerts_eastl::fixed_vector<Variant, puerts::internal::INLINE_ARGUMENT_COUNT>;
using RuntimeArgumentPointers = puerts_eastl::fixed_vector<const Variant *, puerts::internal::INLINE_ARGUMENT_COUNT>;

String read_string_arg(pesapi_ffi *apis, pesapi_env env, pesapi_value value) {
	size_t size = 0;
	const char *inline_text = apis->get_value_string_utf8(env, value, nullptr, &size);
	if (inline_text != nullptr) {
		return String::utf8(inline_text, static_cast<int>(size));
	}

	char *buffer = memnew_arr(char, size + 1);
	apis->get_value_string_utf8(env, value, buffer, &size);
	buffer[size] = 0;
	String result = String::utf8(buffer, static_cast<int>(size));
	memdelete_arr(buffer);
	return result;
}

bool should_skip_reflected_property(const Dictionary &p_property_dict) {
	const uint32_t usage = p_property_dict["usage"];
	constexpr uint32_t ignored_usage = PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP;
	return (usage & ignored_usage) != 0;
}

bool is_read_only_reflected_property(const Dictionary &p_property_dict) {
	return (static_cast<uint32_t>(p_property_dict["usage"]) & PROPERTY_USAGE_READ_ONLY) != 0;
}

bool should_index_builtin_variant(const TypeInfo *p_type_info) {
	return p_type_info != nullptr && p_type_info->kind == TypeInfo::Kind::STATIC_BOUND &&
			p_type_info->variant_type != Variant::NIL && p_type_info->variant_type != Variant::OBJECT;
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

Object *instantiate_reflected_object(const TypeInfo *p_type_info, String &r_error) {
#if GODOT_VERSION_MAJOR > 4 || (GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR >= 4)
	GDExtensionObjectPtr native_object = godot::gdextension_interface::classdb_construct_object2(
			static_cast<GDExtensionConstStringNamePtr>(p_type_info->class_name._native_ptr()));
#else
	GDExtensionObjectPtr native_object = godot::gdextension_interface::classdb_construct_object(
			static_cast<GDExtensionConstStringNamePtr>(p_type_info->class_name._native_ptr()));
#endif
	Object *object = native_object != nullptr ? internal::get_object_instance_binding(native_object) : nullptr;
	if (object == nullptr) {
		r_error = "Failed to instantiate " + String(p_type_info->class_name);
	}
	return object;
}

void fill_runtime_arguments(
		RuntimeArgumentValues &r_values,
		RuntimeArgumentPointers &r_arg_ptrs,
		puerts::internal::callback_context &p_context) {
	const int argc = p_context.arg_count;
	r_values.resize(argc);
	r_arg_ptrs.resize(argc);
	for (int i = 0; i < argc; ++i) {
		r_values[i] = puerts::script_to_variant(
				p_context.environment,
				p_context.env,
				p_context.get_arg(i));
		r_arg_ptrs[i] = &r_values[i];
	}
}

String format_reflected_call_error(const String &p_target_name, const GDExtensionCallError &p_call_error) {
	switch (p_call_error.error) {
		case GDEXTENSION_CALL_OK:
			return {};
		case GDEXTENSION_CALL_ERROR_INVALID_METHOD:
			return "Method not found: " + p_target_name;
		case GDEXTENSION_CALL_ERROR_INVALID_ARGUMENT: {
			String expected_type = "unknown";
			if (p_call_error.expected >= 0 && p_call_error.expected < Variant::VARIANT_MAX) {
				expected_type = Variant::get_type_name(static_cast<Variant::Type>(p_call_error.expected));
			}
			return vformat("Invalid argument %d when calling %s. Expected %s.", p_call_error.argument + 1, p_target_name, expected_type);
		}
		case GDEXTENSION_CALL_ERROR_TOO_MANY_ARGUMENTS:
			return vformat("Too many arguments when calling %s. Expected %d.", p_target_name, p_call_error.expected);
		case GDEXTENSION_CALL_ERROR_TOO_FEW_ARGUMENTS:
			return vformat("Too few arguments when calling %s. Expected %d.", p_target_name, p_call_error.expected);
		case GDEXTENSION_CALL_ERROR_INSTANCE_IS_NULL:
			return "Instance is null when calling " + p_target_name;
		case GDEXTENSION_CALL_ERROR_METHOD_NOT_CONST:
			return "Method is not const: " + p_target_name;
		default:
			return "Call failed: " + p_target_name;
	}
}

Object *resolve_holder_object(puerts::internal::callback_context &p_context) {
	void *holder = p_context.get_holder_ptr();
	if (holder == nullptr) {
		return nullptr;
	}
	Object *object = nullptr;
	if (p_context.env_private->bridge->get_object(holder, object)) {
		return object;
	}
	return PuertsBridgeRegistry::is_handle(holder) ? nullptr : static_cast<Object *>(holder);
}

bool call_reflected_method(Object *p_object, TypeInfo::MethodData *p_method, puerts::internal::callback_context &p_context, Variant &r_result, String &r_error) {
	if (p_method->method_bind == nullptr) {
		p_method->method_bind = godot::gdextension_interface::classdb_get_method_bind(
				static_cast<GDExtensionConstStringNamePtr>(p_method->owner_class_name._native_ptr()),
				static_cast<GDExtensionConstStringNamePtr>(p_method->name._native_ptr()),
				p_method->compatibility_hash);
		if (p_method->method_bind == nullptr) {
			r_error = "MethodBind not found: " + String(p_method->owner_class_name) + "." + String(p_method->name);
			return false;
		}
	}

	RuntimeArgumentValues args;
	RuntimeArgumentPointers arg_ptrs;
	fill_runtime_arguments(args, arg_ptrs, p_context);
	GDExtensionCallError call_error{ GDEXTENSION_CALL_OK, 0, 0 };
	godot::gdextension_interface::object_method_bind_call(
			p_method->method_bind,
			p_object != nullptr ? p_object->_owner : nullptr,
			reinterpret_cast<const GDExtensionConstVariantPtr *>(arg_ptrs.data()),
			arg_ptrs.size(),
			r_result._native_ptr(),
			&call_error);
	if (call_error.error != GDEXTENSION_CALL_OK) {
		r_error = format_reflected_call_error(String(p_method->owner_class_name) + "." + String(p_method->name), call_error);
		return false;
	}
	return true;
}

} // namespace

namespace puerts_type_register_internal {

using TypeInfo = PuertsTypeRegister::TypeInfo;

class TypeInfoFactory {
public:
	static TypeInfo *build_object_type(PuertsTypeRegister &p_registry, const StringName &p_name) {
		if (!ClassDB::class_exists(p_name)) {
			return nullptr;
		}

		TypeInfo *type_info = memnew(TypeInfo);
		type_info->type_id = type_info;
		type_info->kind = TypeInfo::Kind::OBJECT_CLASS;
		type_info->class_name = p_name;
		type_info->variant_type = Variant::OBJECT;
		type_info->can_instantiate = ClassDB::can_instantiate(p_name);

		const StringName parent_name = ClassDB::get_parent_class(p_name);
		if (parent_name != StringName()) {
			type_info->base_type = p_registry.find_or_add_object_type(parent_name);
		}

		append_reflected_methods(type_info, ClassDB::class_get_method_list(p_name, true));
		append_reflected_properties(type_info, ClassDB::class_get_property_list(p_name, true));
		append_reflected_signals(type_info, p_name);
		const PackedStringArray integer_constants = ClassDB::class_get_integer_constant_list(p_name, true);
		append_reflected_enum_groups(p_registry, type_info, p_name);
		append_reflected_integer_constants(type_info, p_name, integer_constants);
		return type_info;
	}

	static TypeInfo *build_static_type(const puerts::StaticTypeDefinition &p_definition) {
		TypeInfo *type_info = memnew(TypeInfo);
		type_info->type_id = p_definition.type_id;
		type_info->kind = TypeInfo::Kind::STATIC_BOUND;
		type_info->class_name = p_definition.class_name;
		type_info->variant_type = p_definition.variant_type;
		type_info->base_type_id = p_definition.base_type_id;
		type_info->base_class_name = p_definition.base_class_name;
		type_info->constructor = p_definition.constructor;
		type_info->finalize = p_definition.finalize;
		type_info->native_to_variant = p_definition.native_to_variant;
		type_info->static_methods.reserve(p_definition.static_methods.size());
		type_info->instance_methods.reserve(p_definition.instance_methods.size());
		type_info->instance_properties.reserve(p_definition.instance_properties.size());
		type_info->static_properties.reserve(p_definition.static_properties.size());

		for (const puerts::FunctionBinding &binding : p_definition.static_methods) {
			TypeInfo::MethodData method;
			method.name = binding.name;
			method.callback = binding.callback;
			method.userdata = binding.userdata;
			type_info->static_methods.push_back(method);
		}

		for (const puerts::FunctionBinding &binding : p_definition.instance_methods) {
			TypeInfo::MethodData method;
			method.name = binding.name;
			method.callback = binding.callback;
			method.userdata = binding.userdata;
			type_info->instance_methods.push_back(method);
		}

		for (const puerts::PropertyBinding &binding : p_definition.instance_properties) {
			TypeInfo::PropertyData property{};
			property.name = binding.name;
			property.getter = binding.getter;
			property.setter = binding.setter;
			property.getter_userdata = binding.getter_userdata;
			property.setter_userdata = binding.setter_userdata;
			type_info->instance_properties.push_back(property);
		}

		for (const puerts::PropertyBinding &binding : p_definition.static_properties) {
			TypeInfo::PropertyData property{};
			property.name = binding.name;
			property.getter = binding.getter;
			property.setter = binding.setter;
			property.getter_userdata = binding.getter_userdata;
			property.setter_userdata = binding.setter_userdata;
			type_info->static_properties.push_back(property);
		}

		return type_info;
	}

private:
	static void append_reflected_methods(TypeInfo *p_type_info, const TypedArray<Dictionary> &p_method_list) {
		p_type_info->instance_methods.reserve(p_method_list.size());
		p_type_info->static_methods.reserve(p_method_list.size());
		for (const auto &i : p_method_list) {
			const Dictionary method_dict = i;
			const MethodInfo method_info = MethodInfo::from_dict(method_dict);
			TypeInfo::MethodData method;
			method.name = method_dict["name"];
			method.owner_class_name = p_type_info->class_name;
			method.argument_count = method_info.arguments.size();
			method.compatibility_hash = get_method_compatibility_hash(method_info);
			const bool is_static = (static_cast<uint32_t>(method_dict["flags"]) & METHOD_FLAG_STATIC) != 0;
			method.callback = is_static ? &PuertsTypeRegister::object_static_method_callback : &PuertsTypeRegister::object_method_callback;

			puerts_eastl::vector<TypeInfo::MethodData> &target_methods = is_static ? p_type_info->static_methods : p_type_info->instance_methods;
			target_methods.push_back(method);
		}
	}

	static TypeInfo::MethodData *find_instance_method(TypeInfo *p_type_info, const StringName &p_name) {
		while (p_type_info != nullptr) {
			for (TypeInfo::MethodData &method : p_type_info->instance_methods) {
				if (method.name == p_name) {
					return &method;
				}
			}
			p_type_info = p_type_info->base_type;
		}
		return nullptr;
	}

	static void append_reflected_properties(TypeInfo *p_type_info, const TypedArray<Dictionary> &p_property_list) {
		p_type_info->instance_properties.reserve(p_property_list.size());

		for (const auto &i : p_property_list) {
			const Dictionary property_dict = i;
			if (should_skip_reflected_property(property_dict)) {
				continue;
			}

			TypeInfo::PropertyData property;
			property.name = property_dict["name"];
			property.getter_method = find_instance_method(
					p_type_info,
					ClassDB::class_get_property_getter(p_type_info->class_name, property.name));
			property.indexed = property.getter_method->argument_count != 0;
			property.getter = &PuertsTypeRegister::object_property_getter_callback;
			if (!is_read_only_reflected_property(property_dict)) {
				if (!property.indexed) {
					property.setter_method = find_instance_method(
							p_type_info,
							ClassDB::class_get_property_setter(p_type_info->class_name, property.name));
				}
				property.setter = &PuertsTypeRegister::object_property_setter_callback;
			}
			p_type_info->instance_properties.push_back(property);
		}
	}

	static void append_reflected_signals(TypeInfo *p_type_info, const StringName &p_name) {
		const TypedArray<Dictionary> signal_list = ClassDB::class_get_signal_list(p_name, true);
		p_type_info->instance_properties.reserve(p_type_info->instance_properties.size() + signal_list.size());
		for (const auto &entry : signal_list) {
			const Dictionary signal_dict = entry;
			TypeInfo::PropertyData signal_property;
			signal_property.name = signal_dict["name"];
			signal_property.getter = &PuertsTypeRegister::object_signal_getter_callback;
			p_type_info->instance_properties.push_back(signal_property);
		}
	}

	static void append_reflected_enum_groups(PuertsTypeRegister &p_registry, TypeInfo *p_type_info, const StringName &p_name) {
		const PackedStringArray enum_names = ClassDB::class_get_enum_list(p_name, true);
		p_type_info->static_properties.reserve(p_type_info->static_properties.size() + enum_names.size());
		for (const auto &enum_name_value : enum_names) {
			const StringName enum_name = enum_name_value;
			TypeInfo *enum_type = memnew(TypeInfo);
			enum_type->type_id = enum_type;
			enum_type->kind = TypeInfo::Kind::STATIC_BOUND;
			enum_type->class_name = StringName(String(p_name) + "." + String(enum_name));

			const PackedStringArray enum_constants = ClassDB::class_get_enum_constants(p_name, enum_name, true);
			enum_type->static_properties.reserve(enum_constants.size());
			for (const auto &enum_constant_value : enum_constants) {
				const StringName enum_constant_name = enum_constant_value;
				TypeInfo::PropertyData constant;
				constant.name = enum_constant_name;
				constant.int_constant = ClassDB::class_get_integer_constant(p_name, enum_constant_name);
				constant.getter = &PuertsTypeRegister::integer_constant_getter_callback;
				enum_type->static_properties.push_back(constant);
			}
			p_registry.store_type(enum_type);

			TypeInfo::PropertyData enum_group_property;
			enum_group_property.name = enum_name;
			enum_group_property.getter = &PuertsTypeRegister::enum_group_getter_callback;
			enum_group_property.getter_userdata = enum_type;
			p_type_info->static_properties.push_back(enum_group_property);
		}
	}

	static void append_reflected_integer_constants(TypeInfo *p_type_info, const StringName &p_name, const PackedStringArray &p_integer_constants) {
		p_type_info->static_properties.reserve(p_type_info->static_properties.size() + p_integer_constants.size());

		for (const auto &integer_constant : p_integer_constants) {
			const StringName constant_name = integer_constant;
			const StringName enum_name = ClassDB::class_get_integer_constant_enum(p_name, constant_name, true);
			if (enum_name != StringName()) {
				continue;
			}

			TypeInfo::PropertyData constant;
			constant.name = constant_name;
			constant.int_constant = ClassDB::class_get_integer_constant(p_name, constant_name);
			constant.getter = &PuertsTypeRegister::integer_constant_getter_callback;
			p_type_info->static_properties.push_back(constant);
		}
	}
};

} // namespace puerts_type_register_internal

PuertsTypeRegister &PuertsTypeRegister::get_singleton() {
	static PuertsTypeRegister singleton;
	return singleton;
}

PuertsTypeRegister::PuertsTypeRegister() {
	reg_api_ = *static_cast<pesapi_registry_api *>(GetRegisterApi());
	registry_ = reg_api_.create_registry();
	reg_api_.on_class_not_found(registry_, [](const void *type_id) -> int {
		PuertsTypeRegister &type_register = PuertsTypeRegister::get_singleton();
		TypeInfo *type_info = type_register.get_type_by_id(type_id);
		return type_info != nullptr && type_register.ensure_registered(type_info);
	});
}

PuertsTypeRegister::~PuertsTypeRegister() {
	for (TypeInfo *type_info : owned_types_) {
		memdelete(type_info);
	}
}

pesapi_registry PuertsTypeRegister::get_registry() const {
	return registry_;
}

TypeInfo *PuertsTypeRegister::get_type_by_id(const void *p_type_id) {
	auto found = types_by_id_.find(p_type_id);
	return found == types_by_id_.end() ? nullptr : found->second;
}

TypeInfo *PuertsTypeRegister::get_builtin_type(Variant::Type p_type) {
	return builtin_types_by_variant_[static_cast<int>(p_type)];
}

void PuertsTypeRegister::register_static_type(const puerts::StaticTypeDefinition &p_definition) {
	TypeInfo *type_info = puerts_type_register_internal::TypeInfoFactory::build_static_type(p_definition);
	if (type_info->base_type_id != nullptr) {
		type_info->base_type = get_type_by_id(type_info->base_type_id);
	}
	store_type(type_info);
}

TypeInfo *PuertsTypeRegister::find_or_add_object_type(const StringName &p_name) {
	auto found = object_types_by_name_.find(p_name);
	if (found != object_types_by_name_.end()) {
		return found->second;
	}

	TypeInfo *type_info = puerts_type_register_internal::TypeInfoFactory::build_object_type(*this, p_name);
	if (type_info == nullptr) {
		return nullptr;
	}

	store_type(type_info);
	return type_info;
}

TypeInfo *PuertsTypeRegister::find_type_by_name(const StringName &p_name) {
	auto found = types_by_name_.find(p_name);
	if (found != types_by_name_.end()) {
		return found->second;
	}

	const String requested_name = p_name;
	const int separator = requested_name.find(".");
	if (separator > 0) {
		const StringName owner_type_name = requested_name.substr(0, separator);
		find_or_add_object_type(owner_type_name);
		found = types_by_name_.find(p_name);
		if (found != types_by_name_.end()) {
			return found->second;
		}
	}

	return find_or_add_object_type(p_name);
}

bool PuertsTypeRegister::ensure_registered(TypeInfo *p_type_info) {
	if (p_type_info->is_registered) {
		return true;
	}
	if (!resolve_base_type(p_type_info)) {
		return false;
	}
	if (p_type_info->base_type != nullptr && !ensure_registered(p_type_info->base_type)) {
		return false;
	}

	register_type(p_type_info);
	return true;
}

void PuertsTypeRegister::store_type(TypeInfo *p_type_info) {
	owned_types_.push_back(p_type_info);
	types_by_id_[p_type_info->type_id] = p_type_info;
	if (p_type_info->kind == TypeInfo::Kind::OBJECT_CLASS) {
		object_types_by_name_[p_type_info->class_name] = p_type_info;
	}

	auto by_name = types_by_name_.find(p_type_info->class_name);
	const bool keep_existing_static_binding = by_name != types_by_name_.end() &&
			by_name->second->kind == TypeInfo::Kind::STATIC_BOUND &&
			p_type_info->kind != TypeInfo::Kind::STATIC_BOUND;
	if (!keep_existing_static_binding) {
		types_by_name_[p_type_info->class_name] = p_type_info;
	}

	if (should_index_builtin_variant(p_type_info)) {
		builtin_types_by_variant_[static_cast<int>(p_type_info->variant_type)] = p_type_info;
	}
}

bool PuertsTypeRegister::resolve_base_type(TypeInfo *p_type_info) {
	if (p_type_info->base_type != nullptr) {
		return true;
	}

	if (p_type_info->base_type_id != nullptr) {
		p_type_info->base_type = get_type_by_id(p_type_info->base_type_id);
	}
	if (p_type_info->base_type == nullptr && p_type_info->base_class_name != StringName()) {
		p_type_info->base_type = find_type_by_name(p_type_info->base_class_name);
	}
	if (p_type_info->base_type_id == nullptr && p_type_info->base_class_name == StringName()) {
		return true;
	}
	return p_type_info->base_type != nullptr;
}

void PuertsTypeRegister::register_type(TypeInfo *p_type_info) {
	pesapi_constructor constructor = p_type_info->constructor;
	if (constructor == nullptr) {
		constructor = p_type_info->kind == TypeInfo::Kind::OBJECT_CLASS && p_type_info->can_instantiate ? &PuertsTypeRegister::object_default_constructor_callback : &PuertsTypeRegister::no_constructor_callback;
	}

	CharString class_name = String(p_type_info->class_name).utf8();
	reg_api_.define_class(
			registry_,
			p_type_info->type_id,
			p_type_info->base_type != nullptr ? p_type_info->base_type->type_id : nullptr,
			"godot",
			class_name.get_data(),
			constructor,
			p_type_info->finalize,
			p_type_info,
			true);
	reg_api_.set_property_info_size(
			registry_,
			p_type_info->type_id,
			static_cast<int>(p_type_info->instance_methods.size()),
			static_cast<int>(p_type_info->static_methods.size()),
			static_cast<int>(p_type_info->instance_properties.size()),
			static_cast<int>(p_type_info->static_properties.size()));

	auto bind_methods = [this, p_type_info](auto &p_methods, bool p_is_static) {
		for (int i = 0; i < static_cast<int>(p_methods.size()); ++i) {
			auto *method = &p_methods[i];
			CharString method_name = String(method->name).utf8();
			reg_api_.set_method_info(
					registry_, p_type_info->type_id, i, method_name.get_data(), p_is_static,
					method->callback, method->userdata != nullptr ? method->userdata : method, true);
		}
	};
	auto bind_properties = [this, p_type_info](auto &p_properties, bool p_is_static) {
		for (int i = 0; i < static_cast<int>(p_properties.size()); ++i) {
			auto *property = &p_properties[i];
			CharString property_name = String(property->name).utf8();
			reg_api_.set_property_info(
					registry_, p_type_info->type_id, i, property_name.get_data(), p_is_static,
					property->getter,
					property->setter != nullptr ? property->setter : &PuertsTypeRegister::read_only_property_setter_callback,
					property->getter_userdata != nullptr ? property->getter_userdata : property,
					property->setter != nullptr && property->setter_userdata != nullptr ? property->setter_userdata : property,
					true);
		}
	};
	bind_methods(p_type_info->instance_methods, false);
	bind_methods(p_type_info->static_methods, true);
	bind_properties(p_type_info->instance_properties, false);
	bind_properties(p_type_info->static_properties, true);

	reg_api_.trace_native_object_lifecycle(registry_, p_type_info->type_id, nullptr, &PuertsTypeRegister::on_native_binding_exit);

	p_type_info->is_registered = true;
}

void PuertsTypeRegister::on_native_binding_exit(void *ptr, void *class_data, void *env_private, void *userdata) {
	const auto *private_state = static_cast<const PuertsEnvPrivate *>(env_private);
	if (private_state == nullptr || !private_state->alive || private_state->bridge == nullptr) {
		return;
	}
	Ref<PuertsEnvironment> keep_alive(private_state->environment);
	private_state->bridge->release(ptr);
}

void *PuertsTypeRegister::no_constructor_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	const auto *type_info = static_cast<const TypeInfo *>(apis->get_userdata(info));
	CharString message = String("No constructor available for " + String(type_info->class_name)).utf8();
	apis->throw_by_string(info, message.get_data());
	return nullptr;
}

void *PuertsTypeRegister::object_default_constructor_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	puerts::internal::callback_context context(apis, info);
	if (!context.require()) {
		return nullptr;
	}

	const auto *type_info = static_cast<const TypeInfo *>(apis->get_userdata(info));
	if (context.arg_count != 0) {
		apis->throw_by_string(info, "Reflected ClassDB object types only support zero-argument construction.");
		return nullptr;
	}

	String error;
	Object *object = instantiate_reflected_object(type_info, error);
	if (object == nullptr) {
		CharString message = error.utf8();
		apis->throw_by_string(info, message.get_data());
		return nullptr;
	}

	void *handle = context.env_private->bridge->own_object(
			object,
			type_info->type_id);
	if (handle == nullptr) {
		memdelete(object);
		apis->throw_by_string(info, "Failed to create reflected object handle.");
		return nullptr;
	}
	return handle;
}

void PuertsTypeRegister::object_method_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	auto *method = static_cast<TypeInfo::MethodData *>(apis->get_userdata(info));
	puerts::internal::callback_context context(apis, info);
	if (!context.require()) {
		return;
	}

	Object *object = resolve_holder_object(context);
	if (object == nullptr) {
		apis->throw_by_string(info, "Native object is no longer valid.");
		return;
	}

	Variant result;
	String call_error;
	if (!call_reflected_method(object, method, context, result, call_error)) {
		CharString message = call_error.utf8();
		apis->throw_by_string(info, message.get_data());
		return;
	}
	puerts::return_variant(apis, info, context.env, context.environment, result);
}

void PuertsTypeRegister::object_static_method_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	auto *method = static_cast<TypeInfo::MethodData *>(apis->get_userdata(info));
	puerts::internal::callback_context context(apis, info);
	if (!context.require()) {
		return;
	}

	Variant result;
	String call_error;
	if (!call_reflected_method(nullptr, method, context, result, call_error)) {
		CharString message = call_error.utf8();
		apis->throw_by_string(info, message.get_data());
		return;
	}

	puerts::return_variant(apis, info, context.env, context.environment, result);
}

void PuertsTypeRegister::object_property_getter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	auto *property = static_cast<TypeInfo::PropertyData *>(apis->get_userdata(info));
	puerts::internal::callback_context context(apis, info);
	if (!context.require()) {
		return;
	}

	Object *object = resolve_holder_object(context);
	if (object == nullptr) {
		apis->throw_by_string(info, "Native object is no longer valid.");
		return;
	}

	Variant result;
	if (!property->indexed) {
		String call_error;
		if (!call_reflected_method(object, property->getter_method, context, result, call_error)) {
			CharString message = call_error.utf8();
			apis->throw_by_string(info, message.get_data());
			return;
		}
	} else {
		result = ClassDB::class_get_property(object, property->name);
	}
	puerts::return_variant(apis, info, context.env, context.environment, result);
}

void PuertsTypeRegister::object_property_setter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	auto *property = static_cast<TypeInfo::PropertyData *>(apis->get_userdata(info));
	puerts::internal::callback_context context(apis, info);
	if (!context.require()) {
		return;
	}

	Object *object = resolve_holder_object(context);
	if (object == nullptr) {
		apis->throw_by_string(info, "Native object is no longer valid.");
		return;
	}
	if (!property->indexed) {
		Variant result;
		String call_error;
		if (!call_reflected_method(object, property->setter_method, context, result, call_error)) {
			CharString message = call_error.utf8();
			apis->throw_by_string(info, message.get_data());
		}
		return;
	}

	const Variant value = puerts::script_to_variant(context.environment, context.env, context.get_arg(0));
	if (ClassDB::class_set_property(object, property->name, value) != OK) {
		CharString message = String("Failed to set property: " + String(property->name)).utf8();
		apis->throw_by_string(info, message.get_data());
	}
}

void PuertsTypeRegister::object_signal_getter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	auto *property = static_cast<TypeInfo::PropertyData *>(apis->get_userdata(info));
	puerts::internal::callback_context context(apis, info);
	if (!context.require()) {
		return;
	}

	Object *object = resolve_holder_object(context);
	if (object == nullptr) {
		apis->throw_by_string(info, "Native object is no longer valid.");
		return;
	}
	puerts::return_variant(
			apis,
			info,
			context.env,
			context.environment,
			Variant(Signal(object, property->name)));
}

void PuertsTypeRegister::enum_group_getter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	auto *enum_type = static_cast<TypeInfo *>(apis->get_userdata(info));
	PuertsTypeRegister &registry = PuertsTypeRegister::get_singleton();
	if (!registry.ensure_registered(enum_type)) {
		CharString message = String("Failed to register enum type: " + String(enum_type->class_name)).utf8();
		apis->throw_by_string(info, message.get_data());
		return;
	}

	pesapi_env env = apis->get_env(info);
	apis->add_return(info, apis->create_class(env, enum_type->type_id));
}

void PuertsTypeRegister::integer_constant_getter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	auto *property = static_cast<TypeInfo::PropertyData *>(apis->get_userdata(info));
	pesapi_env env = apis->get_env(info);
	if (property->int_constant >= INT32_MIN && property->int_constant <= INT32_MAX) {
		apis->add_return(info, apis->create_int32(env, static_cast<int32_t>(property->int_constant)));
		return;
	}
	apis->add_return(info, apis->create_int64(env, property->int_constant));
}

void PuertsTypeRegister::read_only_property_setter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	const auto *property = static_cast<const TypeInfo::PropertyData *>(apis->get_userdata(info));
	CharString message = String("Property is read-only: " + String(property->name)).utf8();
	apis->throw_by_string(info, message.get_data());
}

void PuertsTypeRegister::load_type_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	pesapi_env env = apis->get_env(info);
	if (apis->get_args_len(info) < 1) {
		apis->add_return(info, apis->create_null(env));
		return;
	}

	const String class_name = read_string_arg(apis, env, apis->get_arg(info, 0));
	PuertsTypeRegister &type_register = PuertsTypeRegister::get_singleton();
	auto *type_info = type_register.find_type_by_name(class_name);
	if (type_info == nullptr || !type_register.ensure_registered(type_info)) {
		CharString message = String("Type not found: " + class_name).utf8();
		apis->throw_by_string(info, message.get_data());
		return;
	}

	apis->add_return(info, apis->create_class(env, type_info->type_id));
}
