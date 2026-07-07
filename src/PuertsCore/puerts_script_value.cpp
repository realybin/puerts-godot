// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_script_value.h"

#include "puerts_bridge_registry.h"
#include "puerts_environment.h"
#include "puerts_runtime.h"
#include "puerts_script_value_cache.h"
#include "puerts_type_register.h"

#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/templates/local_vector.hpp>

using namespace godot;

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
	Ref<PuertsEnvironment> environment = get_environment_ref();
	if (environment.is_valid()) {
		environment->unregister_script_value(ObjectID(get_instance_id()));
	}
	release_value_ref();
}

bool PuertsScriptValue::is_valid() const {
	PuertsEnvironment *environment = get_environment();
	return environment != nullptr && environment->is_alive() && ffi_ != nullptr && value_ref_ != nullptr;
}

bool PuertsScriptValue::is_null() const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value value;
	if (!resolve(scope, env, value)) {
		return true;
	}
	bool result = ffi_->is_null(env, value);
	ffi_->close_scope(scope);
	return result;
}

bool PuertsScriptValue::is_undefined() const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value value;
	if (!resolve(scope, env, value)) {
		return true;
	}
	bool result = ffi_->is_undefined(env, value);
	ffi_->close_scope(scope);
	return result;
}

bool PuertsScriptValue::is_bool() const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value value;
	if (!resolve(scope, env, value)) {
		return false;
	}
	bool result = ffi_->is_boolean(env, value);
	ffi_->close_scope(scope);
	return result;
}

bool PuertsScriptValue::is_int() const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value value;
	if (!resolve(scope, env, value)) {
		return false;
	}
	bool result = ffi_->is_int32(env, value) || ffi_->is_uint32(env, value) || ffi_->is_int64(env, value) || ffi_->is_uint64(env, value);
	ffi_->close_scope(scope);
	return result;
}

bool PuertsScriptValue::is_float() const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value value;
	if (!resolve(scope, env, value)) {
		return false;
	}
	bool result = ffi_->is_double(env, value);
	ffi_->close_scope(scope);
	return result;
}

bool PuertsScriptValue::is_string() const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value value;
	if (!resolve(scope, env, value)) {
		return false;
	}
	bool result = ffi_->is_string(env, value);
	ffi_->close_scope(scope);
	return result;
}

bool PuertsScriptValue::is_binary() const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value value;
	if (!resolve(scope, env, value)) {
		return false;
	}
	bool result = ffi_->is_binary(env, value);
	ffi_->close_scope(scope);
	return result;
}

bool PuertsScriptValue::is_object() const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value value;
	if (!resolve(scope, env, value)) {
		return false;
	}
	bool result = ffi_->is_object(env, value);
	ffi_->close_scope(scope);
	return result;
}

bool PuertsScriptValue::is_function() const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value value;
	if (!resolve(scope, env, value)) {
		return false;
	}
	bool result = ffi_->is_function(env, value);
	ffi_->close_scope(scope);
	return result;
}

bool PuertsScriptValue::to_bool() const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value value;
	if (!resolve(scope, env, value)) {
		return false;
	}
	bool result = ffi_->get_value_bool(env, value) != 0;
	ffi_->close_scope(scope);
	return result;
}

int64_t PuertsScriptValue::to_int() const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value value;
	if (!resolve(scope, env, value)) {
		return 0;
	}
	int64_t result = ffi_->get_value_int64(env, value);
	ffi_->close_scope(scope);
	return result;
}

double PuertsScriptValue::to_float() const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value value;
	if (!resolve(scope, env, value)) {
		return 0.0;
	}
	double result = ffi_->get_value_double(env, value);
	ffi_->close_scope(scope);
	return result;
}

String PuertsScriptValue::to_string() const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value value;
	if (!resolve(scope, env, value)) {
		return {};
	}

	size_t size = 0;
	const char *inline_text = ffi_->get_value_string_utf8(env, value, nullptr, &size);
	String result;
	if (inline_text != nullptr) {
		result = String::utf8(inline_text, static_cast<int>(size));
	} else {
		char *buffer = memnew_arr(char, size + 1);
		ffi_->get_value_string_utf8(env, value, buffer, &size);
		buffer[size] = 0;
		result = String::utf8(buffer, static_cast<int>(size));
		memdelete_arr(buffer);
	}
	ffi_->close_scope(scope);
	return result;
}

PackedByteArray PuertsScriptValue::to_binary() const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value value;
	if (!resolve(scope, env, value)) {
		return {};
	}

	size_t size = 0;
	void *buffer = ffi_->get_value_binary(env, value, &size);
	PackedByteArray result;
	result.resize(static_cast<int>(size));
	if (size > 0 && buffer != nullptr) {
		memcpy(result.ptrw(), buffer, size);
	}
	ffi_->close_scope(scope);
	return result;
}

Variant PuertsScriptValue::to_native() const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value value;
	if (!resolve(scope, env, value)) {
		return {};
	}
	Ref<PuertsEnvironment> environment = get_environment_ref();
	Variant result = environment->script_to_variant(env, value);
	ffi_->close_scope(scope);
	return result;
}

Variant PuertsScriptValue::unwrap_native() const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value value;
	if (!resolve(scope, env, value)) {
		return {};
	}
	Ref<PuertsEnvironment> environment = get_environment_ref();
	Variant result;
	PuertsBridgeRegistry *bridge = environment->env_private_->bridge;
	PuertsTypeRegister &type_register = PuertsTypeRegister::get_singleton();
	if (void *handle = ffi_->get_native_object_ptr(env, value); handle != nullptr) {
		if (const void *type_id = ffi_->get_native_object_typeid(env, value); type_id != nullptr) {
			Variant native_value;
			if (bridge->get_variant(handle, type_id, native_value)) {
				result = native_value;
			} else {
				auto *type_info = type_register.get_type_by_id(type_id);
				if (type_info != nullptr &&
						type_info->kind == PuertsTypeRegister::TypeInfo::Kind::STATIC_BOUND &&
						type_info->native_to_variant != nullptr) {
					result = type_info->native_to_variant(handle);
				}
			}
		}
	}
	ffi_->close_scope(scope);
	return result;
}

Ref<PuertsScriptValue> PuertsScriptValue::get_property(const StringName &p_name) const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value value;
	if (!resolve(scope, env, value)) {
		return {};
	}
	Ref<PuertsEnvironment> environment = get_environment_ref();
	if (!ensure_live_native_object_receiver(environment, env, value)) {
		ffi_->close_scope(scope);
		return {};
	}

	const CharString &name_utf8 = environment->get_cached_utf8(p_name);
	pesapi_value property = ffi_->get_property(env, value, name_utf8.get_data());
	if (ffi_->has_caught(scope)) {
		environment->log_error(environment->read_exception(scope));
		ffi_->close_scope(scope);
		return {};
	}

	Ref<PuertsScriptValue> result = environment->create_script_value(env, property);
	ffi_->close_scope(scope);
	return result;
}

void PuertsScriptValue::set_property(const StringName &p_name, const Variant &p_value) {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value value;
	if (!resolve(scope, env, value)) {
		return;
	}
	Ref<PuertsEnvironment> environment = get_environment_ref();
	if (!ensure_live_native_object_receiver(environment, env, value)) {
		ffi_->close_scope(scope);
		return;
	}

	const CharString &name_utf8 = environment->get_cached_utf8(p_name);
	bool converted = false;
	pesapi_value script_value = environment->variant_to_script(env, p_value, &converted, nullptr);
	if (!converted) {
		ffi_->close_scope(scope);
		return;
	}
	ffi_->set_property(env, value, name_utf8.get_data(), script_value);
	if (ffi_->has_caught(scope)) {
		environment->log_error(environment->read_exception(scope));
		ffi_->close_scope(scope);
		return;
	}
	ffi_->close_scope(scope);
}

Ref<PuertsScriptValue> PuertsScriptValue::call(const Array &p_args) const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value function_value;
	if (!resolve(scope, env, function_value)) {
		return {};
	}
	Ref<PuertsEnvironment> environment = get_environment_ref();
	if (!ffi_->is_function(env, function_value)) {
		environment->log_error("Puerts script value is not a function.");
		ffi_->close_scope(scope);
		return {};
	}

	const int32_t arg_count = p_args.size();
	LocalVector<pesapi_value, int32_t, true> argv;
	argv.resize(arg_count);
	for (int32_t i = 0; i < arg_count; i++) {
		bool converted = false;
		argv[i] = environment->variant_to_script(env, p_args[i], &converted, nullptr);
		if (!converted) {
			ffi_->close_scope(scope);
			return {};
		}
	}

	pesapi_value result = ffi_->call_function(env, function_value, ffi_->global(env), static_cast<int>(arg_count), argv.ptr());
	if (ffi_->has_caught(scope)) {
		environment->log_error(environment->read_exception(scope));
		ffi_->close_scope(scope);
		return {};
	}

	Ref<PuertsScriptValue> script_value = environment->create_script_value(env, result);
	ffi_->close_scope(scope);
	return script_value;
}

Ref<PuertsScriptValue> PuertsScriptValue::call_method(const StringName &p_name, const Array &p_args) const {
	pesapi_scope scope;
	pesapi_env env;
	pesapi_value object_value;
	if (!resolve(scope, env, object_value)) {
		return {};
	}
	Ref<PuertsEnvironment> environment = get_environment_ref();
	if (!ensure_live_native_object_receiver(environment, env, object_value)) {
		ffi_->close_scope(scope);
		return {};
	}

	const CharString &name_utf8 = environment->get_cached_utf8(p_name);
	pesapi_value function_value = ffi_->get_property(env, object_value, name_utf8.get_data());
	if (ffi_->has_caught(scope)) {
		environment->log_error(environment->read_exception(scope));
		ffi_->close_scope(scope);
		return {};
	}
	if (function_value == nullptr || !ffi_->is_function(env, function_value)) {
		environment->log_error("Puerts script value property is not callable: " + String(p_name));
		ffi_->close_scope(scope);
		return {};
	}

	const int32_t arg_count = p_args.size();
	LocalVector<pesapi_value, int32_t, true> argv;
	argv.resize(arg_count);
	for (int32_t i = 0; i < arg_count; i++) {
		bool converted = false;
		argv[i] = environment->variant_to_script(env, p_args[i], &converted, nullptr);
		if (!converted) {
			ffi_->close_scope(scope);
			return {};
		}
	}

	pesapi_value result = ffi_->call_function(env, function_value, object_value, static_cast<int>(arg_count), argv.ptr());
	if (ffi_->has_caught(scope)) {
		environment->log_error(environment->read_exception(scope));
		ffi_->close_scope(scope);
		return {};
	}

	Ref<PuertsScriptValue> script_value = environment->create_script_value(env, result);
	ffi_->close_scope(scope);
	return script_value;
}

void PuertsScriptValue::initialize(const Ref<PuertsEnvironment> &p_environment, pesapi_ffi *p_ffi, pesapi_value_ref p_value_ref) {
	environment_id_ = p_environment.is_valid() ? ObjectID(p_environment->get_instance_id()) : ObjectID();
	ffi_ = p_ffi;
	value_ref_ = p_value_ref;
}

void PuertsScriptValue::release_value_ref() {
	if (ffi_ != nullptr && value_ref_ != nullptr) {
		if (auto *cache_entry = puerts::script_value_cache::entry_from_value_ref(ffi_, value_ref_); cache_entry != nullptr && cache_entry->value_ref == value_ref_) {
			Ref<PuertsEnvironment> environment = get_environment_ref();
			if (environment.is_valid() && environment->is_alive()) {
				pesapi_scope scope = ffi_->open_scope(environment->env_ref_);
				pesapi_env env = ffi_->get_env_from_ref(environment->env_ref_);
				pesapi_value value = ffi_->get_value_from_ref(env, value_ref_);
				void *private_ptr = nullptr;
				if (ffi_->get_private(env, value, &private_ptr) && private_ptr == cache_entry) {
					ffi_->set_private(env, value, nullptr);
				}
				ffi_->close_scope(scope);
			}

			puerts::script_value_cache::clear_value_ref_link(ffi_, value_ref_);
			puerts::script_value_cache::unregister_entry(cache_entry);
			memdelete(cache_entry);
		}

		ffi_->release_value_ref(value_ref_);
	}
	value_ref_ = nullptr;
	ffi_ = nullptr;
	environment_id_ = ObjectID();
}

bool PuertsScriptValue::resolve(pesapi_scope &r_scope, pesapi_env &r_env, pesapi_value &r_value) const {
	PuertsEnvironment *environment = get_environment();
	if (environment == nullptr || !environment->is_alive() || value_ref_ == nullptr) {
		if (environment != nullptr) {
			environment->log_error("Puerts script value is no longer valid.");
		}
		return false;
	}

	pesapi_env_ref env_ref = environment->env_ref_;
	r_scope = ffi_->open_scope(env_ref);
	r_env = ffi_->get_env_from_ref(env_ref);
	r_value = ffi_->get_value_from_ref(r_env, value_ref_);
	return true;
}

bool PuertsScriptValue::ensure_live_native_object_receiver(const Ref<PuertsEnvironment> &p_environment, pesapi_env p_env, pesapi_value p_value) const {
	if (!p_environment.is_valid() || p_environment->env_private_ == nullptr || p_environment->env_private_->bridge == nullptr) {
		return true;
	}

	void *handle = ffi_->get_native_object_ptr(p_env, p_value);
	if (handle == nullptr) {
		return true;
	}

	PuertsBridgeRegistry *bridge = p_environment->env_private_->bridge;
	if (!bridge->is_object(handle)) {
		return true;
	}

	if (bridge->get_object(handle) != nullptr) {
		return true;
	}

	p_environment->log_error("Native object is no longer valid.");
	return false;
}

PuertsEnvironment *PuertsScriptValue::get_environment() const {
	if (!environment_id_.is_valid()) {
		return nullptr;
	}
	return godot::Object::cast_to<PuertsEnvironment>(godot::ObjectDB::get_instance(environment_id_));
}

Ref<PuertsEnvironment> PuertsScriptValue::get_environment_ref() const {
	PuertsEnvironment *environment = get_environment();
	return environment != nullptr ? Ref<PuertsEnvironment>(environment) : Ref<PuertsEnvironment>();
}
