// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_type_registry.h"

#include "puerts_bridge_registry.h"
#include "puerts_environment.h"
#include "puerts_reflected_binding.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/object.hpp>

#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/version.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/signal.hpp>

#include <climits>
#include <cstring>

using namespace godot;

extern "C" void *GetRegisterApi();

namespace {

using TypeRecord = PuertsTypeRegistry::TypeRecord;

void throw_script_error(pesapi_ffi *p_api, pesapi_callback_info p_info, const String &p_message) {
	const CharString utf8 = p_message.utf8();
	p_api->throw_by_string(p_info, utf8.get_data());
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
		r_arguments.values[i] = puerts::script_value_to_variant(
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
	if (p_frame.environment_context()->bridge.try_get_object(holder, object)) {
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

String get_method_target_name(const PuertsReflectedMethod *p_method) {
	if (p_method == nullptr || p_method->owner_type == nullptr) {
		return "unknown method";
	}
	return String(p_method->owner_type->name) + "." + String(p_method->name);
}

bool call_reflected_method(
		Object *p_object,
		const PuertsReflectedMethod *p_method,
		puerts::internal::CallbackContext &p_frame,
		Variant &r_result) {
	// MethodBind resolution is intentionally performed once while building the
	// type record. Godot may still reject virtual-only or compatibility entries;
	// keep that boundary explicit instead of forwarding a null bind into the FFI.
	if (p_method == nullptr || p_method->method_bind == nullptr) {
		throw_script_error(p_frame.api(), p_frame.callback_info(), "MethodBind not found: " + get_method_target_name(p_method));
		return false;
	}

	const int argument_count = p_frame.argument_count();
	const GDExtensionObjectPtr object = p_object != nullptr ? p_object->_owner : nullptr;

	// ptrcall cannot report argument errors. Calls with extra arguments must
	// use the checked Variant path.
	if (p_method->use_no_args_ptrcall && argument_count == 0) {
		godot::gdextension_interface::object_method_bind_ptrcall(
				p_method->method_bind,
				object,
				nullptr,
				nullptr);
		return true;
	}

	puerts::internal::CallArguments args;
	build_call_arguments(args, p_frame);
	GDExtensionCallError call_error{ GDEXTENSION_CALL_OK, 0, 0 };
	godot::gdextension_interface::object_method_bind_call(
			p_method->method_bind,
			object,
			reinterpret_cast<const GDExtensionConstVariantPtr *>(args.pointers.data()),
			argument_count,
			r_result._native_ptr(),
			&call_error);
	if (call_error.error != GDEXTENSION_CALL_OK) {
		throw_script_error(
				p_frame.api(),
				p_frame.callback_info(),
				puerts::internal::format_call_error(get_method_target_name(p_method), call_error));
		return false;
	}
	return true;
}

template <typename Callback>
void dispatch_callback(pesapi_ffi *p_api, pesapi_callback_info p_info, Callback &&p_callback) {
	puerts::internal::CallbackContext context(p_api, p_info);
	if (context.require()) {
		p_callback(context);
	}
}

void return_reflected_method(Object *p_object, const PuertsReflectedMethod *p_method, puerts::internal::CallbackContext &p_frame) {
	Variant result;
	if (call_reflected_method(p_object, p_method, p_frame, result)) {
		puerts::return_variant_to_script(p_frame.api(), p_frame.callback_info(), p_frame.script_env(), p_frame.puerts_environment(), result);
	}
}

} // namespace

PuertsTypeRegistry &PuertsTypeRegistry::get_singleton() {
	static PuertsTypeRegistry singleton;
	return singleton;
}

PuertsTypeRegistry::PuertsTypeRegistry() {
	reg_api_ = *static_cast<pesapi_registry_api *>(GetRegisterApi());
	registry_ = reg_api_.create_registry();
	reg_api_.on_class_not_found(registry_, [](const void *type_id) -> int {
		PuertsTypeRegistry &type_registry = PuertsTypeRegistry::get_singleton();
		return type_registry.ensure_registered(type_id);
	});
}

void PuertsTypeRegistry::TypeRecordDeleter::operator()(TypeRecord *p_type) const {
	memdelete(p_type);
}

PuertsTypeRegistry::~PuertsTypeRegistry() = default;

pesapi_registry PuertsTypeRegistry::get_registry() const {
	return registry_;
}

PuertsTypeRegistry::TypeRecord *PuertsTypeRegistry::find_record(const void *p_type_id) const {
	const auto found = types_by_id_.find(p_type_id);
	return found == types_by_id_.end() ? nullptr : found->second;
}

const void *PuertsTypeRegistry::get_builtin_type_id(Variant::Type p_type) const {
	const TypeRecord *type = builtin_types_[static_cast<int>(p_type)];
	return type != nullptr ? type->type_id : nullptr;
}

bool PuertsTypeRegistry::has_type(const void *p_type_id) const {
	return find_record(p_type_id) != nullptr;
}

bool PuertsTypeRegistry::register_static_type(TypeRecord &&p_record) {
	ERR_FAIL_COND_V_MSG(p_record.name.is_empty(), false, "A bound type requires a name.");
	ERR_FAIL_COND_V_MSG(types_by_name_.find(p_record.name) != types_by_name_.end(), false, "A type with this name is already registered.");
	ERR_FAIL_COND_V_MSG(p_record.type_id != nullptr && has_type(p_record.type_id), false, "This C++ type is already registered.");
	ERR_FAIL_COND_V_MSG(p_record.variant_type == Variant::NIL && !p_record.constructor_overloads.empty() && p_record.finalize == nullptr,
			false, "Native constructors require a finalizer.");
	auto has_unique_members = [](const auto &methods, const auto &properties) {
		puerts_eastl::hash_set<StringName, StringNameHash, StringNameEqual> names;
		auto add_names = [&](const auto &members) {
			for (const auto &member : members) {
				if (member.name.is_empty() || !names.insert(member.name).second) {
					return false;
				}
			}
			return true;
		};
		return add_names(methods) && add_names(properties);
	};
	ERR_FAIL_COND_V_MSG(!has_unique_members(p_record.instance_methods, p_record.instance_properties) ||
					!has_unique_members(p_record.static_methods, p_record.static_properties),
			false, "Bound member names must be nonempty and unique within each scope.");
	ERR_FAIL_COND_V_MSG(p_record.variant_type != Variant::OBJECT && ClassDB::class_exists(p_record.name),
			false, "Native types and namespaces cannot replace Godot classes.");
	TypeRecordOwner owner(memnew(TypeRecord(puerts_eastl::move(p_record))));
	TypeRecord *type = owner.get();
	if (type->type_id == nullptr) {
		type->type_id = type;
	}
	if (type->base_id != nullptr && type->variant_type != Variant::OBJECT) {
		type->base = find_record(type->base_id);
		ERR_FAIL_NULL_V_MSG(type->base, false, "Register a native base type before its derived types.");
	}
	if (should_index_builtin_variant(type)) {
		ERR_FAIL_COND_V_MSG(builtin_types_[static_cast<int>(type->variant_type)] != nullptr, false, "This builtin Variant type is already registered.");
	}
	if (type->variant_type == Variant::OBJECT) {
		if (!TypeRecordBuilder::complete_object_type(*this, type)) {
			return false;
		}
	}
	return store_type(puerts_eastl::move(owner));
}

PuertsTypeRegistry::TypeRecord *PuertsTypeRegistry::find_or_add_object_record(const StringName &p_name) {
	auto found = types_by_name_.find(p_name);
	if (found != types_by_name_.end()) {
		return found->second->variant_type == Variant::OBJECT ? found->second : nullptr;
	}

	TypeRecordOwner owner = TypeRecordBuilder::build_object_type(*this, p_name);
	if (owner == nullptr) {
		return nullptr;
	}

	TypeRecord *type = owner.get();
	return store_type(puerts_eastl::move(owner)) ? type : nullptr;
}

const void *PuertsTypeRegistry::find_or_add_object_type(const StringName &p_name) {
	TypeRecord *type = find_or_add_object_record(p_name);
	return type != nullptr ? type->type_id : nullptr;
}

PuertsTypeRegistry::TypeRecord *PuertsTypeRegistry::find_type_by_name(const StringName &p_name) {
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

bool PuertsTypeRegistry::ensure_registered(TypeRecord *p_type) {
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
	if (p_type->base != nullptr && !ensure_registered(p_type->base)) {
		p_type->registration_state = TypeRecord::RegistrationState::Unregistered;
		return false;
	}

	register_type(p_type);
	return true;
}

bool PuertsTypeRegistry::ensure_registered(const void *p_type_id) {
	TypeRecord *type = find_record(p_type_id);
	return type != nullptr && ensure_registered(type);
}

void *PuertsTypeRegistry::cast_native(void *p_pointer, const void *p_type_id, const void *p_base_type_id) const {
	for (TypeRecord *type = find_record(p_type_id); type != nullptr; type = type->base) {
		if (type->type_id == p_base_type_id) {
			return p_pointer;
		}
		if (type->upcast == nullptr) {
			return nullptr;
		}
		p_pointer = type->upcast(p_pointer);
	}
	return nullptr;
}

bool PuertsTypeRegistry::can_own_native(const void *p_type_id) const {
	const TypeRecord *type = find_record(p_type_id);
	return type != nullptr && type->finalize != nullptr;
}

bool PuertsTypeRegistry::store_type(TypeRecordOwner p_owner) {
	TypeRecord *type = p_owner.get();
	ERR_FAIL_COND_V_MSG(has_type(type->type_id) || types_by_name_.find(type->name) != types_by_name_.end(), false, "Duplicate type registration.");
	types_by_id_.insert({ type->type_id, type });
	types_by_name_.insert({ type->name, type });
	if (should_index_builtin_variant(type)) {
		builtin_types_[static_cast<int>(type->variant_type)] = type;
	}
	owned_types_.push_back(puerts_eastl::move(p_owner));
	return true;
}

void PuertsTypeRegistry::register_type(TypeRecord *p_type) {
	pesapi_constructor constructor = &PuertsTypeRegistry::no_constructor_callback;
	if (p_type->kind == TypeRecord::Kind::StaticBinding && !p_type->constructor_overloads.empty()) {
		constructor = &PuertsTypeRegistry::bound_constructor_callback;
	} else if (p_type->kind == TypeRecord::Kind::ReflectedObject && p_type->constructible) {
		constructor = &PuertsTypeRegistry::object_default_constructor_callback;
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
					method->callback, method->callback_data != nullptr ? method->callback_data : method, true);
		}
	};
	auto bind_properties = [this, p_type](auto &p_properties, bool p_is_static) {
		for (int i = 0; i < static_cast<int>(p_properties.size()); ++i) {
			auto *property = &p_properties[i];
			CharString property_name = String(property->name).utf8();
			reg_api_.set_property_info(
					registry_, p_type->type_id, i, property_name.get_data(), p_is_static,
					property->getter,
					property->setter != nullptr ? property->setter : &PuertsTypeRegistry::read_only_property_setter_callback,
					property->getter_data != nullptr ? property->getter_data : property,
					property->setter != nullptr && property->setter_data != nullptr ? property->setter_data : property,
					true);
		}
	};
	bind_methods(p_type->instance_methods, false);
	bind_methods(p_type->static_methods, true);
	bind_properties(p_type->instance_properties, false);
	bind_properties(p_type->static_properties, true);

	reg_api_.trace_native_object_lifecycle(registry_, p_type->type_id, nullptr, &PuertsTypeRegistry::on_native_binding_exit);

	p_type->registration_state = TypeRecord::RegistrationState::Registered;
}

void PuertsTypeRegistry::on_native_binding_exit(void *ptr, void *class_data, void *environment_context_ptr, void *userdata) {
	const auto *type = static_cast<const TypeRecord *>(class_data);
	if (type == nullptr || type->variant_type == Variant::NIL) {
		return;
	}
	auto *environment_context = static_cast<PuertsEnvironmentContext *>(environment_context_ptr);
	if (environment_context == nullptr || !environment_context->accepts_callbacks()) {
		return;
	}
	Ref<PuertsEnvironment> keep_alive(environment_context->environment);
	environment_context->bridge.release_handle(ptr);
}

void *PuertsTypeRegistry::bound_constructor_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	const auto *type = static_cast<const TypeRecord *>(apis->get_userdata(info));
	puerts::internal::CallbackContext frame(apis, info);
	if (!frame.require()) {
		return nullptr;
	}

	if (type->constructor_overloads.size() == 1) {
		const auto &constructor = type->constructor_overloads[0];
		return frame.require_argument_count(constructor.arity) ? constructor.invoke(frame) : nullptr;
	}
	const TypeRecord::ConstructorOverload *selected = nullptr;
	int best_cost = INT_MAX;
	bool ambiguous = false;
	for (const auto &constructor : type->constructor_overloads) {
		if (constructor.arity != frame.argument_count()) {
			continue;
		}
		const int cost = constructor.match(frame);
		if (cost >= 0 && cost < best_cost) {
			selected = &constructor;
			best_cost = cost;
			ambiguous = false;
		} else if (cost >= 0 && cost == best_cost) {
			ambiguous = true;
		}
	}
	if (selected == nullptr || ambiguous) {
		(void)frame.reject(ambiguous ? "Ambiguous constructor overload." : "No constructor overload matches the provided arguments.");
		return nullptr;
	}
	return selected->invoke(frame);
}

void *PuertsTypeRegistry::no_constructor_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	const auto *type = static_cast<const TypeRecord *>(apis->get_userdata(info));
	throw_script_error(apis, info, "No constructor available for " + String(type->name));
	return nullptr;
}

void *PuertsTypeRegistry::object_default_constructor_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
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

	void *handle = frame.environment_context()->bridge.adopt_object(
			object,
			type->type_id);
	if (handle == nullptr) {
		memdelete(object);
		apis->throw_by_string(info, "Failed to create reflected object handle.");
		return nullptr;
	}
	return handle;
}

void PuertsTypeRegistry::object_method_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	const auto *method = static_cast<const PuertsReflectedMethod *>(apis->get_userdata(info));
	dispatch_callback(apis, info, [&](puerts::internal::CallbackContext &context) {
		if (Object *object = require_holder_object(context); object != nullptr) {
			return_reflected_method(object, method, context);
		}
	});
}

void PuertsTypeRegistry::object_static_method_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	const auto *method = static_cast<const PuertsReflectedMethod *>(apis->get_userdata(info));
	dispatch_callback(apis, info, [&](puerts::internal::CallbackContext &context) {
		return_reflected_method(nullptr, method, context);
	});
}

void PuertsTypeRegistry::object_property_getter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	auto *property = static_cast<const PuertsReflectedProperty *>(apis->get_userdata(info));
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
		puerts::return_variant_to_script(apis, info, context.script_env(), context.puerts_environment(), result);
	});
}

void PuertsTypeRegistry::object_property_setter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	auto *property = static_cast<const PuertsReflectedProperty *>(apis->get_userdata(info));
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

		const Variant value = puerts::script_value_to_variant(context.puerts_environment(), context.script_env(), context.argument_value(0));
		if (ClassDB::class_set_property(object, property->name, value) != OK) {
			throw_script_error(apis, info, "Failed to set property: " + String(property->name));
		}
	});
}

void PuertsTypeRegistry::signal_getter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	auto *property = static_cast<TypeRecord::Property *>(apis->get_userdata(info));
	dispatch_callback(apis, info, [&](puerts::internal::CallbackContext &context) {
		Object *object = require_holder_object(context);
		if (object == nullptr) {
			return;
		}
		if (!object->has_signal(property->name)) {
			throw_script_error(apis, info, "Signal not found: " + String(property->name));
			return;
		}
		puerts::return_variant_to_script(
				apis,
				info,
				context.script_env(),
				context.puerts_environment(),
				Variant(Signal(object, property->name)));
	});
}

void PuertsTypeRegistry::nested_type_getter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	const auto &type_name = *static_cast<const StringName *>(apis->get_userdata(info));
	auto &registry = get_singleton();
	TypeRecord *type = registry.find_type_by_name(type_name);
	if (type == nullptr || !registry.ensure_registered(type)) {
		throw_script_error(apis, info, "Type is not registered: " + String(type_name));
		return;
	}
	apis->add_return(info, apis->create_class(apis->get_env(info), type->type_id));
}

void PuertsTypeRegistry::singleton_getter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	const auto *property = static_cast<const TypeRecord::Property *>(apis->get_userdata(info));
	dispatch_callback(apis, info, [&](puerts::internal::CallbackContext &context) {
		Engine *engine = Engine::get_singleton();
		if (engine == nullptr || !engine->has_singleton(property->name)) {
			(void)context.reject("Requested singleton is unavailable.");
			return;
		}
		puerts::return_variant_to_script(apis, info, context.script_env(), context.puerts_environment(), Variant(engine->get_singleton(property->name)));
	});
}

void PuertsTypeRegistry::integer_constant_getter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	const int64_t value = *static_cast<const int64_t *>(apis->get_userdata(info));
	pesapi_env env = apis->get_env(info);
	if (value >= INT32_MIN && value <= INT32_MAX) {
		apis->add_return(info, apis->create_int32(env, static_cast<int32_t>(value)));
		return;
	}
	apis->add_return(info, apis->create_int64(env, value));
}

void PuertsTypeRegistry::read_only_property_setter_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	const auto *property = static_cast<const TypeRecord::Property *>(apis->get_userdata(info));
	throw_script_error(apis, info, "Property is read-only: " + String(property->name));
}

void PuertsTypeRegistry::register_addon(AddonRegisterFunction p_register) {
	p_register(&reg_api_, registry_);
}

void PuertsTypeRegistry::load_addon_type_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	const pesapi_env env = apis->get_env(info);
	const auto *context = static_cast<const PuertsEnvironmentContext *>(apis->get_env_private(env));
	if (context == nullptr || !context->accepts_callbacks()) {
		apis->throw_by_string(info, "Puerts environment is not available.");
		return;
	}
	if (apis->get_args_len(info) != 1 || !apis->is_string(env, apis->get_arg(info, 0))) {
		apis->throw_by_string(info, "load_addon_type expects exactly one string argument.");
		return;
	}
	const String name = puerts::internal::read_utf8_string(apis, env, apis->get_arg(info, 0));
	const CharString utf8 = name.utf8();
	if (utf8.length() == 0 || std::memchr(utf8.get_data(), '\0', utf8.length()) != nullptr) {
		apis->throw_by_string(info, "Addon type name must be nonempty and contain no NUL characters.");
		return;
	}
	PuertsTypeRegistry &registry = get_singleton();
	const void *type_id = registry.reg_api_.find_type_id(registry.registry_, nullptr, utf8.get_data());
	if (type_id == nullptr) {
		throw_script_error(apis, info, "Addon type not found: " + name);
		return;
	}
	apis->add_return(info, apis->create_class(env, type_id));
}

void PuertsTypeRegistry::load_type_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	pesapi_env env = apis->get_env(info);
	if (apis->get_args_len(info) < 1) {
		apis->add_return(info, apis->create_null(env));
		return;
	}

	const String class_name = puerts::internal::read_utf8_string(apis, env, apis->get_arg(info, 0));
	PuertsTypeRegistry &type_registry = PuertsTypeRegistry::get_singleton();
	auto *type = type_registry.find_type_by_name(class_name);
	if (type == nullptr || !type_registry.ensure_registered(type)) {
		throw_script_error(apis, info, "Type not found: " + class_name);
		return;
	}

	apis->add_return(info, apis->create_class(env, type->type_id));
}
