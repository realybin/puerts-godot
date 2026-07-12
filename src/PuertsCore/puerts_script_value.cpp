// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_script_value.h"

#include "puerts_bridge_registry.h"
#include "puerts_environment.h"
#include "puerts_runtime.h"

#include <godot_cpp/core/object.hpp>

using namespace godot;

template <typename Result, typename Function>
Result PuertsScriptValue::with_value(Result p_fallback, Function &&p_function, bool p_may_reenter) const {
	PuertsEnvironment *environment = get_environment();
	if (environment == nullptr || !environment->is_alive() || ffi_ == nullptr || value_ref_ == nullptr) {
		if (environment != nullptr) {
			environment->log_error("Puerts script value is no longer valid.");
		}
		return p_fallback;
	}

	PuertsEnvironment::operation_scope operation(p_may_reenter ? environment : nullptr);
	puerts::internal::env_scope scope(ffi_, environment->env_ref_);
	pesapi_env env = scope.get_env();
	pesapi_value value = ffi_->get_value_from_ref(env, value_ref_);
	return p_function(environment, scope.get_scope(), env, value);
}

void PuertsScriptValue::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_valid"), &PuertsScriptValue::is_valid);
	ClassDB::bind_method(D_METHOD("is_null"), &PuertsScriptValue::is_null);
	ClassDB::bind_method(D_METHOD("is_undefined"), &PuertsScriptValue::is_undefined);
	ClassDB::bind_method(D_METHOD("is_bool"), &PuertsScriptValue::is_bool);
	ClassDB::bind_method(D_METHOD("is_int"), &PuertsScriptValue::is_int);
	ClassDB::bind_method(D_METHOD("is_float"), &PuertsScriptValue::is_float);
	ClassDB::bind_method(D_METHOD("is_string"), &PuertsScriptValue::is_string);
	ClassDB::bind_method(D_METHOD("is_binary"), &PuertsScriptValue::is_binary);
	ClassDB::bind_method(D_METHOD("is_object"), &PuertsScriptValue::is_object);
	ClassDB::bind_method(D_METHOD("is_function"), &PuertsScriptValue::is_function);
	ClassDB::bind_method(D_METHOD("to_bool"), &PuertsScriptValue::to_bool);
	ClassDB::bind_method(D_METHOD("to_int"), &PuertsScriptValue::to_int);
	ClassDB::bind_method(D_METHOD("to_float"), &PuertsScriptValue::to_float);
	ClassDB::bind_method(D_METHOD("to_string"), &PuertsScriptValue::to_string);
	ClassDB::bind_method(D_METHOD("to_binary"), &PuertsScriptValue::to_binary);
	ClassDB::bind_method(D_METHOD("to_native"), &PuertsScriptValue::to_native);
	ClassDB::bind_method(D_METHOD("unwrap_native"), &PuertsScriptValue::unwrap_native);
	ClassDB::bind_method(D_METHOD("get_property", "name"), &PuertsScriptValue::get_property);
	ClassDB::bind_method(D_METHOD("set_property", "name", "value"), &PuertsScriptValue::set_property);
	ClassDB::bind_method(D_METHOD("call", "args"), &PuertsScriptValue::call, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("call_method", "name", "args"), &PuertsScriptValue::call_method, DEFVAL(Array()));
}

PuertsScriptValue::~PuertsScriptValue() {
	PuertsEnvironment *environment = get_environment();
	if (environment != nullptr) {
		environment->unregister_script_value(this);
	}
	release_value_ref();
}

bool PuertsScriptValue::is_valid() const {
	PuertsEnvironment *environment = get_environment();
	return environment != nullptr && environment->is_alive() && ffi_ != nullptr && value_ref_ != nullptr;
}

bool PuertsScriptValue::is_null() const {
	return with_value(true, [this](const auto &, pesapi_scope, pesapi_env p_env, pesapi_value p_value) {
		return ffi_->is_null(p_env, p_value);
	});
}

bool PuertsScriptValue::is_undefined() const {
	return with_value(true, [this](const auto &, pesapi_scope, pesapi_env p_env, pesapi_value p_value) {
		return ffi_->is_undefined(p_env, p_value);
	});
}

bool PuertsScriptValue::is_bool() const {
	return with_value(false, [this](const auto &, pesapi_scope, pesapi_env p_env, pesapi_value p_value) {
		return ffi_->is_boolean(p_env, p_value);
	});
}

bool PuertsScriptValue::is_int() const {
	return with_value(false, [this](const auto &, pesapi_scope, pesapi_env p_env, pesapi_value p_value) {
		return ffi_->is_int32(p_env, p_value) || ffi_->is_uint32(p_env, p_value) || ffi_->is_int64(p_env, p_value) || ffi_->is_uint64(p_env, p_value);
	});
}

bool PuertsScriptValue::is_float() const {
	return with_value(false, [this](const auto &, pesapi_scope, pesapi_env p_env, pesapi_value p_value) {
		return ffi_->is_double(p_env, p_value);
	});
}

bool PuertsScriptValue::is_string() const {
	return with_value(false, [this](const auto &, pesapi_scope, pesapi_env p_env, pesapi_value p_value) {
		return ffi_->is_string(p_env, p_value);
	});
}

bool PuertsScriptValue::is_binary() const {
	return with_value(false, [this](const auto &, pesapi_scope, pesapi_env p_env, pesapi_value p_value) {
		return ffi_->is_binary(p_env, p_value);
	});
}

bool PuertsScriptValue::is_object() const {
	return with_value(false, [this](const auto &, pesapi_scope, pesapi_env p_env, pesapi_value p_value) {
		return ffi_->is_object(p_env, p_value);
	});
}

bool PuertsScriptValue::is_function() const {
	return with_value(false, [this](const auto &, pesapi_scope, pesapi_env p_env, pesapi_value p_value) {
		return ffi_->is_function(p_env, p_value);
	});
}

bool PuertsScriptValue::to_bool() const {
	return with_value(false, [this](const auto &, pesapi_scope, pesapi_env p_env, pesapi_value p_value) {
		return ffi_->get_value_bool(p_env, p_value) != 0;
	});
}

int64_t PuertsScriptValue::to_int() const {
	return with_value<int64_t>(0, [this](const auto &, pesapi_scope, pesapi_env p_env, pesapi_value p_value) { return ffi_->get_value_int64(p_env, p_value); }, true);
}

double PuertsScriptValue::to_float() const {
	return with_value(0.0, [this](const auto &, pesapi_scope, pesapi_env p_env, pesapi_value p_value) { return ffi_->get_value_double(p_env, p_value); }, true);
}

String PuertsScriptValue::to_string() const {
	return with_value(String(), [](PuertsEnvironment *p_environment, pesapi_scope, pesapi_env p_env, pesapi_value p_value) { return p_environment->read_string(p_env, p_value); }, true);
}

PackedByteArray PuertsScriptValue::to_binary() const {
	return with_value(PackedByteArray(), [](PuertsEnvironment *p_environment, pesapi_scope, pesapi_env p_env, pesapi_value p_value) {
		return p_environment->read_binary(p_env, p_value);
	});
}

Variant PuertsScriptValue::to_native() const {
	return with_value(Variant(), [](PuertsEnvironment *p_environment, pesapi_scope, pesapi_env p_env, pesapi_value p_value) { return p_environment->script_to_variant(p_env, p_value); }, true);
}

Variant PuertsScriptValue::unwrap_native() const {
	return with_value(Variant(), [this](PuertsEnvironment *p_environment, pesapi_scope, pesapi_env p_env, pesapi_value p_value) {
		if (void *handle = ffi_->get_native_object_ptr(p_env, p_value); handle != nullptr) {
			const void *type_id = ffi_->get_native_object_typeid(p_env, p_value);
			if (type_id == nullptr) {
				return Variant();
			}
			Variant native_value;
			if (p_environment->native_to_variant(handle, type_id, native_value)) {
				return native_value;
			}
		}
		return Variant();
	});
}

Ref<PuertsScriptValue> PuertsScriptValue::get_property(const StringName &p_name) const {
	return with_value(Ref<PuertsScriptValue>(), [this, &p_name](PuertsEnvironment *p_environment, pesapi_scope p_scope, pesapi_env p_env, pesapi_value p_value) {
		if (!ensure_live_native_object_receiver(p_environment, p_env, p_value)) {
			return Ref<PuertsScriptValue>();
		}
		const CharString &name_utf8 = p_environment->get_cached_utf8(p_name);
		pesapi_value property = ffi_->get_property(p_env, p_value, name_utf8.get_data());
		if (ffi_->has_caught(p_scope)) {
			p_environment->log_error(p_environment->read_exception(p_scope));
			return Ref<PuertsScriptValue>();
		}
		return p_environment->create_script_value(p_env, property); }, true);
}

void PuertsScriptValue::set_property(const StringName &p_name, const Variant &p_value) {
	with_value(false, [this, &p_name, &p_value](PuertsEnvironment *p_environment, pesapi_scope p_scope, pesapi_env p_env, pesapi_value p_receiver) {
		if (!ensure_live_native_object_receiver(p_environment, p_env, p_receiver)) {
			return false;
		}
		bool converted = false;
		pesapi_value script_value = p_environment->variant_to_script(p_env, p_value, &converted, nullptr);
		if (!converted) {
			return false;
		}
		const CharString &name_utf8 = p_environment->get_cached_utf8(p_name);
		ffi_->set_property(p_env, p_receiver, name_utf8.get_data(), script_value);
		if (ffi_->has_caught(p_scope)) {
			p_environment->log_error(p_environment->read_exception(p_scope));
			return false;
		}
		return true; }, true);
}

Ref<PuertsScriptValue> PuertsScriptValue::call(const Array &p_args) const {
	return with_value(Ref<PuertsScriptValue>(), [this, &p_args](PuertsEnvironment *p_environment, pesapi_scope p_scope, pesapi_env p_env, pesapi_value p_function) {
		if (!ffi_->is_function(p_env, p_function)) {
			p_environment->log_error("Puerts script value is not a function.");
			return Ref<PuertsScriptValue>();
		}
		return call_script_function(p_environment, p_scope, p_env, p_function, ffi_->global(p_env), p_args); }, true);
}

Ref<PuertsScriptValue> PuertsScriptValue::call_method(const StringName &p_name, const Array &p_args) const {
	return with_value(Ref<PuertsScriptValue>(), [this, &p_name, &p_args](PuertsEnvironment *p_environment, pesapi_scope p_scope, pesapi_env p_env, pesapi_value p_receiver) {
		if (!ensure_live_native_object_receiver(p_environment, p_env, p_receiver)) {
			return Ref<PuertsScriptValue>();
		}
		const CharString &name_utf8 = p_environment->get_cached_utf8(p_name);
		pesapi_value function = ffi_->get_property(p_env, p_receiver, name_utf8.get_data());
		if (ffi_->has_caught(p_scope)) {
			p_environment->log_error(p_environment->read_exception(p_scope));
			return Ref<PuertsScriptValue>();
		}
		if (function == nullptr || !ffi_->is_function(p_env, function)) {
			p_environment->log_error("Puerts script value property is not callable: " + String(p_name));
			return Ref<PuertsScriptValue>();
		}
		return call_script_function(p_environment, p_scope, p_env, function, p_receiver, p_args); }, true);
}

Ref<PuertsScriptValue> PuertsScriptValue::call_script_function(
		PuertsEnvironment *p_environment,
		pesapi_scope p_scope,
		pesapi_env p_env,
		pesapi_value p_function,
		pesapi_value p_receiver,
		const Array &p_args) const {
	const int32_t arg_count = p_args.size();
	puerts_eastl::fixed_vector<pesapi_value, puerts::internal::INLINE_ARGUMENT_COUNT> argv;
	argv.resize(arg_count);
	for (int32_t i = 0; i < arg_count; i++) {
		bool converted = false;
		argv[i] = p_environment->variant_to_script(p_env, p_args[i], &converted, nullptr);
		if (!converted) {
			return {};
		}
	}

	pesapi_value result = ffi_->call_function(p_env, p_function, p_receiver, static_cast<int>(arg_count), argv.data());
	if (ffi_->has_caught(p_scope)) {
		p_environment->log_error(p_environment->read_exception(p_scope));
		return {};
	}
	return p_environment->create_script_value(p_env, result);
}

void PuertsScriptValue::initialize(PuertsEnvironment *p_environment, pesapi_ffi *p_ffi, pesapi_value_ref p_value_ref) {
	environment_ = p_environment;
	ffi_ = p_ffi;
	value_ref_ = p_value_ref;
}

void PuertsScriptValue::release_value_ref() {
	PuertsEnvironment *environment = get_environment();
	PuertsEnvironment::operation_scope operation(environment);
	if (cache_token_ != nullptr && environment != nullptr) {
		environment->cached_script_values_.erase(cache_token_);
		if (ffi_ != nullptr && value_ref_ != nullptr && environment->is_alive()) {
			puerts::internal::env_scope scope(ffi_, environment->env_ref_);
			pesapi_env env = scope.get_env();
			pesapi_value value = ffi_->get_value_from_ref(env, value_ref_);
			void *private_ptr = nullptr;
			if (ffi_->get_private(env, value, &private_ptr) && private_ptr == cache_token_) {
				ffi_->set_private(env, value, nullptr);
			}
		}
	}
	cache_token_ = nullptr;

	if (ffi_ != nullptr && value_ref_ != nullptr) {
		ffi_->release_value_ref(value_ref_);
	}
	value_ref_ = nullptr;
	ffi_ = nullptr;
	environment_ = nullptr;
}

bool PuertsScriptValue::ensure_live_native_object_receiver(PuertsEnvironment *p_environment, pesapi_env p_env, pesapi_value p_value) const {
	void *handle = ffi_->get_native_object_ptr(p_env, p_value);
	if (handle == nullptr) {
		return true;
	}

	PuertsBridgeRegistry *bridge = p_environment->env_private_->bridge;
	Object *object = nullptr;
	if (!bridge->get_object(handle, object)) {
		if (!PuertsBridgeRegistry::is_handle(handle)) {
			return true;
		}
		p_environment->log_error("Native object is no longer valid.");
		return false;
	}

	if (object != nullptr) {
		return true;
	}

	p_environment->log_error("Native object is no longer valid.");
	return false;
}

PuertsEnvironment *PuertsScriptValue::get_environment() const {
	return environment_;
}
