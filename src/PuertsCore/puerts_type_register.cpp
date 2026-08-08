// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_type_register.h"

#include "puerts_bridge_registry.h"
#include "puerts_environment.h"
#include "puerts_type_record.h"

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/version.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/signal.hpp>

using namespace godot;

extern "C" void *GetRegisterApi();

namespace {

using TypeRecord = PuertsTypeRegister::TypeRecord;

void throw_script_error(pesapi_ffi *p_apis, pesapi_callback_info p_info, const String &p_message) {
	const CharString utf8 = p_message.utf8();
	p_apis->throw_by_string(p_info, utf8.get_data());
}

bool should_index_builtin_variant(const TypeRecord *p_type) {
	return p_type->kind == TypeRecord::Kind::StaticBinding &&
			p_type->variant_type != Variant::NIL && p_type->variant_type != Variant::OBJECT;
}

Object *instantiate_reflected_object(const TypeRecord *p_type, String &r_error) {
#if GODOT_VERSION_MAJOR > 4 || (GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR >= 4)
	GDExtensionObjectPtr native_object = godot::gdextension_interface::classdb_construct_object2(
			static_cast<GDExtensionConstStringNamePtr>(p_type->name._native_ptr()));
#else
	GDExtensionObjectPtr native_object = godot::gdextension_interface::classdb_construct_object(
			static_cast<GDExtensionConstStringNamePtr>(p_type->name._native_ptr()));
#endif
	Object *object = native_object != nullptr ? internal::get_object_instance_binding(native_object) : nullptr;
	if (object == nullptr) {
		r_error = "Failed to instantiate " + String(p_type->name);
	}
	return object;
}

void build_call_arguments(
		puerts::internal::CallArguments &r_arguments,
		puerts::internal::CallbackContext &p_frame) {
	const int argc = p_frame.argument_count();
	r_arguments.resize(argc);
	for (int i = 0; i < argc; ++i) {
		r_arguments.values[i] = puerts::script_to_variant(
				p_frame.puerts_environment(),
				p_frame.script_env(),
				p_frame.argument_value(i));
	}
}

Object *resolve_holder_object(puerts::internal::CallbackContext &p_frame) {
	void *holder = p_frame.holder_ptr();
	if (holder == nullptr) {
		return nullptr;
	}
	Object *object = nullptr;
	if (p_frame.environment_data()->bridge.try_get_object(holder, object)) {
		return object;
	}
	return PuertsBridgeRegistry::is_handle(holder) ? nullptr : static_cast<Object *>(holder);
}

Object *require_holder_object(puerts::internal::CallbackContext &p_frame) {
	Object *object = resolve_holder_object(p_frame);
	if (object == nullptr) {
		p_frame.api()->throw_by_string(p_frame.callback_info(), "Native object is no longer valid.");
	}
	return object;
}

bool call_reflected_method(Object *p_object, TypeRecord::Method *p_method, puerts::internal::CallbackContext &p_frame, Variant &r_result) {
	// MethodBind resolution is intentionally performed once while building the
	// type record. Godot may still reject virtual-only or compatibility entries;
	// keep that boundary explicit instead of forwarding a null bind into the FFI.
	if (p_method == nullptr || p_method->method_bind == nullptr) {
		const String target = p_method != nullptr ? String(p_method->owner_class_name) + "." + String(p_method->name) : "unknown method";
		throw_script_error(p_frame.api(), p_frame.callback_info(), "MethodBind not found: " + target);
		return false;
	}

	puerts::internal::CallArguments args;
	build_call_arguments(args, p_frame);
	GDExtensionCallError call_error{ GDEXTENSION_CALL_OK, 0, 0 };
	godot::gdextension_interface::object_method_bind_call(
			p_method->method_bind,
			p_object != nullptr ? p_object->_owner : nullptr,
			reinterpret_cast<const GDExtensionConstVariantPtr *>(args.pointers.data()),
			args.pointers.size(),
			r_result._native_ptr(),
			&call_error);
	if (call_error.error != GDEXTENSION_CALL_OK) {
		throw_script_error(
				p_frame.api(),
				p_frame.callback_info(),
				puerts::internal::format_call_error(String(p_method->owner_class_name) + "." + String(p_method->name), call_error));
		return false;
	}
	return true;
}

template <typename Callback>
void dispatch_callback(pesapi_ffi *p_apis, pesapi_callback_info p_info, Callback &&p_callback) {
	puerts::internal::CallbackContext context(p_apis, p_info);
	if (context.require()) {
		p_callback(context);
	}
}

void return_reflected_method(Object *p_object, TypeRecord::Method *p_method, puerts::internal::CallbackContext &p_frame) {
	Variant result;
	if (call_reflected_method(p_object, p_method, p_frame, result)) {
		puerts::return_variant(p_frame.api(), p_frame.callback_info(), p_frame.script_env(), p_frame.puerts_environment(), result);
	}
}

} // namespace

PuertsTypeRegister &PuertsTypeRegister::get_singleton() {
	static PuertsTypeRegister singleton;
	return singleton;
}

PuertsTypeRegister::PuertsTypeRegister() {
	reg_api_ = *static_cast<pesapi_registry_api *>(GetRegisterApi());
	registry_ = reg_api_.create_registry();
	reg_api_.on_class_not_found(registry_, [](const void *type_id) -> int {
		PuertsTypeRegister &type_register = PuertsTypeRegister::get_singleton();
		return type_register.ensure_registered(type_id);
	});
}

void PuertsTypeRegister::TypeRecordDeleter::operator()(TypeRecord *p_type) const {
	memdelete(p_type);
}

PuertsTypeRegister::~PuertsTypeRegister() = default;

pesapi_registry PuertsTypeRegister::get_registry() const {
	return registry_;
}

PuertsTypeRegister::TypeRecord *PuertsTypeRegister::find_record(const void *p_type_id) const {
	const auto found = types_by_id_.find(p_type_id);
	return found == types_by_id_.end() ? nullptr : found->second;
}

const void *PuertsTypeRegister::get_builtin_type_id(Variant::Type p_type) const {
	const TypeRecord *type = builtin_types_[static_cast<int>(p_type)];
	return type != nullptr ? type->type_id : nullptr;
}

bool PuertsTypeRegister::has_type(const void *p_type_id) const {
	return find_record(p_type_id) != nullptr;
}

void PuertsTypeRegister::register_static_type(const puerts::TypeDefinition &p_definition) {
	TypeOwner owner = RecordBuilder::build_static_type(p_definition);
	TypeRecord *type = owner.get();
	if (type->base_id != nullptr) {
		type->base = find_record(type->base_id);
	}
	store_type(puerts_eastl::move(owner));
}

PuertsTypeRegister::TypeRecord *PuertsTypeRegister::find_or_add_object_record(const StringName &p_name) {
	auto found = reflected_types_by_name_.find(p_name);
	if (found != reflected_types_by_name_.end()) {
		return found->second;
	}

	TypeOwner owner = RecordBuilder::build_object_type(*this, p_name);
	if (owner == nullptr) {
		return nullptr;
	}

	TypeRecord *type = owner.get();
	store_type(puerts_eastl::move(owner));
	return type;
}

const void *PuertsTypeRegister::find_or_add_object_type(const StringName &p_name) {
	TypeRecord *type = find_or_add_object_record(p_name);
	return type != nullptr ? type->type_id : nullptr;
}

PuertsTypeRegister::TypeRecord *PuertsTypeRegister::find_type_by_name(const StringName &p_name) {
	auto found = types_by_name_.find(p_name);
	if (found != types_by_name_.end()) {
		return found->second;
	}

	const String requested_name = p_name;
	const int separator = requested_name.find(".");
	if (separator > 0) {
		const StringName owner_type_name = requested_name.substr(0, separator);
		(void)find_or_add_object_record(owner_type_name);
		found = types_by_name_.find(p_name);
		if (found != types_by_name_.end()) {
			return found->second;
		}
	}

	return find_or_add_object_record(p_name);
}

bool PuertsTypeRegister::ensure_registered(TypeRecord *p_type) {
	switch (p_type->registration_state) {
		case TypeRecord::RegistrationState::Registered:
			return true;
		case TypeRecord::RegistrationState::Registering:
			// A type cannot depend on itself through its base chain.
			return false;
		case TypeRecord::RegistrationState::Unregistered:
			break;
	}

	p_type->registration_state = TypeRecord::RegistrationState::Registering;
	if (!resolve_base_type(p_type)) {
		p_type->registration_state = TypeRecord::RegistrationState::Unregistered;
		return false;
	}
	if (p_type->base != nullptr && !ensure_registered(p_type->base)) {
		p_type->registration_state = TypeRecord::RegistrationState::Unregistered;
		return false;
	}

	register_type(p_type);
	return true;
}

bool PuertsTypeRegister::ensure_registered(const void *p_type_id) {
	TypeRecord *type = find_record(p_type_id);
	return type != nullptr && ensure_registered(type);
}

bool PuertsTypeRegister::is_assignable(const void *p_type_id, const void *p_base_id) const {
	for (TypeRecord *type = find_record(p_type_id); type != nullptr; type = type->base) {
		if (type->type_id == p_base_id) {
			return true;
		}
	}
	return false;
}

bool PuertsTypeRegister::native_to_variant(void *p_pointer, const void *p_type_id, Variant &r_value) const {
	const TypeRecord *type = find_record(p_type_id);
	if (type == nullptr || type->kind != TypeRecord::Kind::StaticBinding || type->to_variant == nullptr) {
		return false;
	}
	r_value = type->to_variant(p_pointer);
	return true;
}

void PuertsTypeRegister::store_type(TypeOwner p_owner) {
	TypeRecord *type = p_owner.get();
	owned_types_.push_back(puerts_eastl::move(p_owner));
	types_by_id_.insert({ type->type_id, type });
	if (type->kind == TypeRecord::Kind::ReflectedObject) {
		reflected_types_by_name_.insert({ type->name, type });
	}

	auto by_name = types_by_name_.find(type->name);
	const bool keep_existing_static_binding = by_name != types_by_name_.end() &&
			by_name->second->kind == TypeRecord::Kind::StaticBinding &&
			type->kind != TypeRecord::Kind::StaticBinding;
	if (by_name == types_by_name_.end()) {
		types_by_name_.insert({ type->name, type });
	} else if (!keep_existing_static_binding) {
		by_name->second = type;
	}

	if (should_index_builtin_variant(type)) {
		builtin_types_[static_cast<int>(type->variant_type)] = type;
	}
}

bool PuertsTypeRegister::resolve_base_type(TypeRecord *p_type) {
	if (p_type->base != nullptr) {
		return true;
	}
	if (p_type->base_id != nullptr) {
		p_type->base = find_record(p_type->base_id);
	} else if (!p_type->base_name.is_empty()) {
		p_type->base = find_type_by_name(p_type->base_name);
	} else {
		return true;
	}
	return p_type->base != nullptr;
}

void PuertsTypeRegister::register_type(TypeRecord *p_type) {
	pesapi_constructor constructor = p_type->constructor;
	if (constructor == nullptr) {
		constructor = p_type->kind == TypeRecord::Kind::ReflectedObject && p_type->constructible ? &PuertsTypeRegister::object_default_constructor_callback : &PuertsTypeRegister::no_constructor_callback;
	}

	CharString class_name = String(p_type->name).utf8();
	reg_api_.define_class(
			registry_,
			p_type->type_id,
			p_type->base != nullptr ? p_type->base->type_id : nullptr,
			"godot",
			class_name.get_data(),
			constructor,
			p_type->finalize,
			p_type,
			true);
	reg_api_.set_property_info_size(
			registry_,
			p_type->type_id,
			static_cast<int>(p_type->instance_methods.size()),
			static_cast<int>(p_type->static_methods.size()),
			static_cast<int>(p_type->instance_properties.size()),
			static_cast<int>(p_type->static_properties.size()));

	auto bind_methods = [this, p_type](auto &p_methods, bool p_is_static) {
		for (int i = 0; i < static_cast<int>(p_methods.size()); ++i) {
			auto *method = &p_methods[i];
			CharString method_name = String(method->name).utf8();
			reg_api_.set_method_info(
					registry_, p_type->type_id, i, method_name.get_data(), p_is_static,
					method->callback, method->userdata != nullptr ? method->userdata : method, true);
		}
	};
	auto bind_properties = [this, p_type](auto &p_properties, bool p_is_static) {
		for (int i = 0; i < static_cast<int>(p_properties.size()); ++i) {
			auto *property = &p_properties[i];
			CharString property_name = String(property->name).utf8();
			reg_api_.set_property_info(
					registry_, p_type->type_id, i, property_name.get_data(), p_is_static,
					property->getter,
					property->setter != nullptr ? property->setter : &PuertsTypeRegister::read_only_property_setter_callback,
					property->getter_userdata != nullptr ? property->getter_userdata : property,
					property->setter != nullptr && property->setter_userdata != nullptr ? property->setter_userdata : property,
					true);
		}
	};
	bind_methods(p_type->instance_methods, false);
	bind_methods(p_type->static_methods, true);
	bind_properties(p_type->instance_properties, false);
	bind_properties(p_type->static_properties, true);

	reg_api_.trace_native_object_lifecycle(registry_, p_type->type_id, nullptr, &PuertsTypeRegister::on_native_binding_exit);

	p_type->registration_state = TypeRecord::RegistrationState::Registered;
}

void PuertsTypeRegister::on_native_binding_exit(void *ptr, void *class_data, void *environment_data_ptr, void *userdata) {
	auto *environment_data = static_cast<PuertsEnvironmentData *>(environment_data_ptr);
	if (environment_data == nullptr || !environment_data->accepts_calls()) {
		return;
	}
	Ref<PuertsEnvironment> keep_alive(environment_data->environment);
	environment_data->bridge.release(ptr);
}

void *PuertsTypeRegister::no_constructor_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	const auto *type = static_cast<const TypeRecord *>(apis->get_userdata(info));
	throw_script_error(apis, info, "No constructor available for " + String(type->name));
	return nullptr;
}

void *PuertsTypeRegister::object_default_constructor_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	puerts::internal::CallbackContext frame(apis, info);
	if (!frame.require()) {
		return nullptr;
	}

	const auto *type = static_cast<const TypeRecord *>(apis->get_userdata(info));
	if (frame.argument_count() != 0) {
		apis->throw_by_string(info, "Reflected ClassDB object types only support zero-argument construction.");
		return nullptr;
	}

	String error;
	Object *object = instantiate_reflected_object(type, error);
	if (object == nullptr) {
		throw_script_error(apis, info, error);
		return nullptr;
	}

	void *handle = frame.environment_data()->bridge.own_object(
			object,
			type->type_id);
	if (handle == nullptr) {
		memdelete(object);
		apis->throw_by_string(info, "Failed to create reflected object handle.");
		return nullptr;
	}
	return handle;
}

void PuertsTypeRegister::object_method_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	auto *method = static_cast<TypeRecord::Method *>(apis->get_userdata(info));
	dispatch_callback(apis, info, [&](puerts::internal::CallbackContext &context) {
		if (Object *object = require_holder_object(context); object != nullptr) {
			return_reflected_method(object, method, context);
		}
	});
}

void PuertsTypeRegister::object_static_method_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	auto *method = static_cast<TypeRecord::Method *>(apis->get_userdata(info));
	dispatch_callback(apis, info, [&](puerts::internal::CallbackContext &context) {
		return_reflected_method(nullptr, method, context);
	});
}

void PuertsTypeRegister::object_property_getter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	auto *property = static_cast<TypeRecord::Property *>(apis->get_userdata(info));
	dispatch_callback(apis, info, [&](puerts::internal::CallbackContext &context) {
		Object *object = require_holder_object(context);
		if (object == nullptr) {
			return;
		}

		Variant result;
		if (!property->indexed) {
			if (!call_reflected_method(object, property->getter_method, context, result)) {
				return;
			}
		} else {
			result = ClassDB::class_get_property(object, property->name);
		}
		puerts::return_variant(apis, info, context.script_env(), context.puerts_environment(), result);
	});
}

void PuertsTypeRegister::object_property_setter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	auto *property = static_cast<TypeRecord::Property *>(apis->get_userdata(info));
	dispatch_callback(apis, info, [&](puerts::internal::CallbackContext &context) {
		Object *object = require_holder_object(context);
		if (object == nullptr) {
			return;
		}
		if (!property->indexed) {
			Variant result;
			(void)call_reflected_method(object, property->setter_method, context, result);
			return;
		}

		const Variant value = puerts::script_to_variant(context.puerts_environment(), context.script_env(), context.argument_value(0));
		if (ClassDB::class_set_property(object, property->name, value) != OK) {
			throw_script_error(apis, info, "Failed to set property: " + String(property->name));
		}
	});
}

void PuertsTypeRegister::object_signal_getter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	auto *property = static_cast<TypeRecord::Property *>(apis->get_userdata(info));
	dispatch_callback(apis, info, [&](puerts::internal::CallbackContext &context) {
		Object *object = require_holder_object(context);
		if (object == nullptr) {
			return;
		}
		puerts::return_variant(
				apis,
				info,
				context.script_env(),
				context.puerts_environment(),
				Variant(Signal(object, property->name)));
	});
}

void PuertsTypeRegister::enum_group_getter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	auto *enum_type = static_cast<TypeRecord *>(apis->get_userdata(info));
	PuertsTypeRegister &registry = PuertsTypeRegister::get_singleton();
	if (!registry.ensure_registered(enum_type)) {
		throw_script_error(apis, info, "Failed to register enum type: " + String(enum_type->name));
		return;
	}

	pesapi_env env = apis->get_env(info);
	apis->add_return(info, apis->create_class(env, enum_type->type_id));
}

void PuertsTypeRegister::integer_constant_getter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	auto *property = static_cast<TypeRecord::Property *>(apis->get_userdata(info));
	pesapi_env env = apis->get_env(info);
	if (property->int_constant >= INT32_MIN && property->int_constant <= INT32_MAX) {
		apis->add_return(info, apis->create_int32(env, static_cast<int32_t>(property->int_constant)));
		return;
	}
	apis->add_return(info, apis->create_int64(env, property->int_constant));
}

void PuertsTypeRegister::read_only_property_setter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	const auto *property = static_cast<const TypeRecord::Property *>(apis->get_userdata(info));
	throw_script_error(apis, info, "Property is read-only: " + String(property->name));
}

void PuertsTypeRegister::load_type_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	pesapi_env env = apis->get_env(info);
	if (apis->get_args_len(info) < 1) {
		apis->add_return(info, apis->create_null(env));
		return;
	}

	const String class_name = puerts::internal::read_utf8_string(apis, env, apis->get_arg(info, 0));
	PuertsTypeRegister &type_register = PuertsTypeRegister::get_singleton();
	auto *type = type_register.find_type_by_name(class_name);
	if (type == nullptr || !type_register.ensure_registered(type)) {
		throw_script_error(apis, info, "Type not found: " + class_name);
		return;
	}

	apis->add_return(info, apis->create_class(env, type->type_id));
}
