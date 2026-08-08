// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_environment.h"

#include "puerts_runtime.h"
#include "puerts_script_callable.h"
#include "puerts_script_value.h"
#include "puerts_type_register.h"

#include <godot_cpp/core/object.hpp>

using namespace godot;

extern "C" int GetPapiVersion();

namespace {

constexpr char load_type_property_name[] = "load_type";
constexpr char to_callable_property_name[] = "to_callable";
constexpr char log_error_property_name[] = "log_error";
constexpr char log_warn_property_name[] = "log_warn";
constexpr char log_info_property_name[] = "log_info";

} // namespace

PuertsEnvironment::OperationScope::OperationScope(PuertsEnvironment *p_environment) {
	if (p_environment == nullptr || !p_environment->is_alive()) {
		return;
	}
	environment_ = p_environment;
	keep_alive_ = Ref<PuertsEnvironment>(p_environment);
	++environment_->active_operations_;
}

PuertsEnvironment::OperationScope::~OperationScope() {
	if (environment_ != nullptr) {
		environment_->end_operation();
	}
}

void PuertsEnvironment::_bind_methods() {
	ClassDB::bind_method(D_METHOD("initialize", "backend", "string_name_cache_pool"), &PuertsEnvironment::initialize);
	ClassDB::bind_method(D_METHOD("dispose"), &PuertsEnvironment::dispose);
	ClassDB::bind_method(D_METHOD("is_alive"), &PuertsEnvironment::is_alive);
	ClassDB::bind_method(D_METHOD("get_backend"), &PuertsEnvironment::get_backend);
	ClassDB::bind_method(D_METHOD("set_error_callback", "callback"), &PuertsEnvironment::set_error_callback);
	ClassDB::bind_method(D_METHOD("set_warn_callback", "callback"), &PuertsEnvironment::set_warn_callback);
	ClassDB::bind_method(D_METHOD("set_info_callback", "callback"), &PuertsEnvironment::set_info_callback);
	ClassDB::bind_method(D_METHOD("eval", "code", "chunk_name"), &PuertsEnvironment::eval, DEFVAL(StringName("chunk")));
	ClassDB::bind_method(D_METHOD("set_global", "name", "value"), &PuertsEnvironment::set_global);
	ClassDB::bind_method(D_METHOD("get_global", "name"), &PuertsEnvironment::get_global);
	ClassDB::bind_method(D_METHOD("tick"), &PuertsEnvironment::tick);
	ClassDB::bind_method(D_METHOD("low_memory_notification"), &PuertsEnvironment::low_memory_notification);
	ClassDB::bind_method(D_METHOD("open_debugger", "port"), &PuertsEnvironment::open_debugger);
	ClassDB::bind_method(D_METHOD("debugger_tick"), &PuertsEnvironment::debugger_tick);
	ClassDB::bind_method(D_METHOD("close_debugger"), &PuertsEnvironment::close_debugger);
	ClassDB::bind_method(D_METHOD("terminate_execution"), &PuertsEnvironment::terminate_execution);
}

PuertsEnvironment::~PuertsEnvironment() {
	dispose_internal();
}

Error PuertsEnvironment::initialize(Object *p_backend, const Ref<PuertsStringNameCachePool> &p_string_name_cache_pool) {
	if (environment_data_.state == PuertsEnvironmentState::Disposing || active_operations_ != 0) {
		log_error("Puerts environment is being disposed.");
		return ERR_BUSY;
	}
	dispose();

	if (p_backend == nullptr) {
		log_error("Puerts backend is null.");
		return ERR_INVALID_PARAMETER;
	}
	if (p_string_name_cache_pool.is_null()) {
		log_error("Puerts StringName cache pool is null.");
		return ERR_INVALID_PARAMETER;
	}
	if (!p_string_name_cache_pool->is_initialized()) {
		log_error("Puerts StringName cache pool is not initialized.");
		return ERR_INVALID_PARAMETER;
	}
	auto *backend_refcounted = Object::cast_to<RefCounted>(p_backend);
	if (backend_refcounted == nullptr) {
		log_error("Puerts backend must inherit RefCounted.");
		return ERR_INVALID_PARAMETER;
	}

	const StringName backend_functions_ptr_method("_puerts_get_functions_ptr");
	const Variant functions_ptr_variant = p_backend->call(backend_functions_ptr_method);
	if (functions_ptr_variant.get_type() != Variant::INT) {
		log_error("Puerts backend _puerts_get_functions_ptr() must return an integer pointer value.");
		return ERR_UNCONFIGURED;
	}

	const int64_t functions_address = functions_ptr_variant;
	if (functions_address <= 0) {
		log_error("Puerts backend does not expose its function table.");
		return ERR_UNCONFIGURED;
	}
	if (static_cast<uint64_t>(functions_address) > static_cast<uint64_t>(UINTPTR_MAX)) {
		log_error("Puerts backend function table address does not fit this architecture.");
		return ERR_UNCONFIGURED;
	}

	const auto *functions = reinterpret_cast<const PuertsBackendFunctions *>(static_cast<uintptr_t>(functions_address));
	if (functions->get_api_version == nullptr || functions->get_ffi == nullptr || functions->create_env_ref == nullptr || functions->destroy_env_ref == nullptr) {
		log_error("Puerts backend function table is incomplete.");
		return ERR_UNCONFIGURED;
	}

	const int backend_api_version = functions->get_api_version();
	const int core_api_version = GetPapiVersion();
	if (backend_api_version != core_api_version) {
		log_error(vformat("Puerts backend API version mismatch. Core=%d Backend=%d.", core_api_version, backend_api_version));
		return ERR_CANT_CREATE;
	}

	ffi_ = functions->get_ffi();
	if (ffi_ == nullptr) {
		log_error("Puerts backend returned a null pesapi ffi.");
		return ERR_CANT_CREATE;
	}

	env_ref_ = functions->create_env_ref();
	if (env_ref_ == nullptr) {
		ffi_ = nullptr;
		log_error("Puerts backend failed to create an environment.");
		return ERR_CANT_CREATE;
	}

	puerts::internal::EnvironmentHandleScope scope(ffi_, env_ref_);
	pesapi_env env = scope.env();
	PuertsTypeRegister &type_register = PuertsTypeRegister::get_singleton();
	ffi_->set_registry(env, type_register.get_registry());

	environment_data_.state = PuertsEnvironmentState::Ready;
	environment_data_.environment = this;
	ffi_->set_env_private(env, &environment_data_);

	pesapi_value load_type = ffi_->create_function(env, &PuertsTypeRegister::load_type_callback, nullptr, nullptr);
	ffi_->set_property(env, ffi_->global(env), load_type_property_name, load_type);
	pesapi_value to_callable = ffi_->create_function(env, &PuertsEnvironment::script_to_callable_callback, nullptr, nullptr);
	ffi_->set_property(env, ffi_->global(env), to_callable_property_name, to_callable);
	pesapi_value log_error = ffi_->create_function(env, &PuertsEnvironment::script_log_error_callback, nullptr, nullptr);
	ffi_->set_property(env, ffi_->global(env), log_error_property_name, log_error);
	pesapi_value log_warn = ffi_->create_function(env, &PuertsEnvironment::script_log_warn_callback, nullptr, nullptr);
	ffi_->set_property(env, ffi_->global(env), log_warn_property_name, log_warn);
	pesapi_value log_info = ffi_->create_function(env, &PuertsEnvironment::script_log_info_callback, nullptr, nullptr);
	ffi_->set_property(env, ffi_->global(env), log_info_property_name, log_info);

	backend_functions_ = functions;
	backend_ref_ = Ref(backend_refcounted);
	string_name_cache_pool_ = p_string_name_cache_pool;
	return OK;
}

void PuertsEnvironment::dispose() {
	if (environment_data_.state != PuertsEnvironmentState::Ready) {
		return;
	}
	if (active_operations_ != 0) {
		environment_data_.state = PuertsEnvironmentState::DisposePending;
		return;
	}
	Ref<PuertsEnvironment> keep_alive(this);
	dispose_internal();
}

void PuertsEnvironment::end_operation() {
	--active_operations_;
	if (active_operations_ == 0 && environment_data_.state == PuertsEnvironmentState::DisposePending) {
		dispose_internal();
	}
}

void PuertsEnvironment::dispose_internal() {
	if (environment_data_.state != PuertsEnvironmentState::Ready && environment_data_.state != PuertsEnvironmentState::DisposePending) {
		return;
	}
	environment_data_.state = PuertsEnvironmentState::Disposing;
	invalidate_script_values();

	if (env_ref_ != nullptr) {
		{
			puerts::internal::EnvironmentHandleScope scope(ffi_, env_ref_);
			ffi_->set_env_private(scope.env(), nullptr);
		}

		environment_data_.bridge.clear();
	}
	environment_data_.environment = nullptr;

	if (env_ref_ != nullptr) {
		backend_functions_->destroy_env_ref(env_ref_);
	}

	env_ref_ = nullptr;
	ffi_ = nullptr;
	backend_functions_ = nullptr;
	backend_ref_.unref();
	string_name_cache_pool_.unref();
	environment_data_.state = PuertsEnvironmentState::Uninitialized;
}

bool PuertsEnvironment::is_alive() const {
	return environment_data_.state == PuertsEnvironmentState::Ready;
}

Object *PuertsEnvironment::get_backend() const {
	return backend_ref_.ptr();
}

void PuertsEnvironment::set_error_callback(const Callable &p_callback) {
	error_callback_ = p_callback;
}

void PuertsEnvironment::set_warn_callback(const Callable &p_callback) {
	warn_callback_ = p_callback;
}

void PuertsEnvironment::set_info_callback(const Callable &p_callback) {
	info_callback_ = p_callback;
}

void PuertsEnvironment::tick() {
	call_backend(
			backend_functions_ != nullptr ? backend_functions_->tick : nullptr,
			"Puerts backend does not support tick.");
}

void PuertsEnvironment::low_memory_notification() {
	call_backend(
			backend_functions_ != nullptr ? backend_functions_->low_memory_notification : nullptr,
			"Puerts backend does not support low memory notification.");
}

void PuertsEnvironment::open_debugger(int32_t p_port) {
	call_backend(
			backend_functions_ != nullptr ? backend_functions_->open_debugger : nullptr,
			"Puerts backend does not support remote debugging.",
			p_port);
}

bool PuertsEnvironment::debugger_tick() {
	return call_backend(
			backend_functions_ != nullptr ? backend_functions_->debugger_tick : nullptr,
			"Puerts backend does not support debugger tick.");
}

void PuertsEnvironment::close_debugger() {
	call_backend(
			backend_functions_ != nullptr ? backend_functions_->close_debugger : nullptr,
			"Puerts backend does not support remote debugging.");
}

void PuertsEnvironment::terminate_execution() {
	call_backend(
			backend_functions_ != nullptr ? backend_functions_->terminate_execution : nullptr,
			"Puerts backend does not support terminate execution.");
}

void PuertsEnvironment::log_error(const String &p_message) {
	emit_log(error_callback_, p_message);
}

void PuertsEnvironment::log_warn(const String &p_message) {
	emit_log(warn_callback_, p_message);
}

void PuertsEnvironment::log_info(const String &p_message) {
	emit_log(info_callback_, p_message);
}

void PuertsEnvironment::emit_log(const Callable &p_callback, const String &p_message) {
	if (!p_callback.is_valid()) {
		return;
	}
	p_callback.call(p_message);
}

static String read_log_message_arg(pesapi_ffi *p_apis, pesapi_env p_env, pesapi_callback_info p_info) {
	return puerts::internal::read_utf8_string(p_apis, p_env, p_apis->get_arg(p_info, 0));
}

template <void (PuertsEnvironment::*LogFunction)(const String &)>
void PuertsEnvironment::dispatch_log_callback(pesapi_ffi *p_apis, pesapi_callback_info p_info) {
	const pesapi_env env = p_apis->get_env(p_info);
	const auto *environment_data = static_cast<const PuertsEnvironmentData *>(p_apis->get_env_private(env));
	if (environment_data == nullptr || environment_data->environment == nullptr) {
		return;
	}
	(environment_data->environment->*LogFunction)(read_log_message_arg(p_apis, env, p_info));
}

void PuertsEnvironment::script_log_error_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	PuertsEnvironment::dispatch_log_callback<&PuertsEnvironment::log_error>(apis, info);
}

void PuertsEnvironment::script_log_warn_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	PuertsEnvironment::dispatch_log_callback<&PuertsEnvironment::log_warn>(apis, info);
}

void PuertsEnvironment::script_log_info_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	PuertsEnvironment::dispatch_log_callback<&PuertsEnvironment::log_info>(apis, info);
}

void PuertsEnvironment::script_to_callable_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	puerts::internal::CallbackContext frame(apis, info);
	if (!frame.require()) {
		return;
	}
	if (frame.argument_count() != 1) {
		apis->throw_by_string(info, "to_callable expects exactly one argument.");
		return;
	}

	pesapi_value function = frame.argument_value(0);
	if (!apis->is_function(frame.script_env(), function)) {
		apis->throw_by_string(info, "to_callable expects a script function.");
		return;
	}

	Ref<PuertsScriptValue> script_function = frame.puerts_environment()->create_script_value(frame.script_env(), function);
	if (script_function.is_null()) {
		apis->throw_by_string(info, "Failed to retain script function.");
		return;
	}

	puerts::return_variant(
			apis,
			info,
			frame.script_env(),
			frame.puerts_environment(),
			puerts::internal::make_script_callable(script_function));
}

const CharString &PuertsEnvironment::get_cached_utf8(const StringName &p_name) {
	return string_name_cache_pool_->get_cached_utf8(p_name);
}

void PuertsEnvironment::register_script_value(PuertsScriptValue *p_value) {
	p_value->previous_ = nullptr;
	p_value->next_ = script_values_head_;
	if (script_values_head_ != nullptr) {
		script_values_head_->previous_ = p_value;
	}
	script_values_head_ = p_value;
}

void PuertsEnvironment::unregister_script_value(PuertsScriptValue *p_value) {
	if (p_value->previous_ != nullptr) {
		p_value->previous_->next_ = p_value->next_;
	} else {
		script_values_head_ = p_value->next_;
	}
	if (p_value->next_ != nullptr) {
		p_value->next_->previous_ = p_value->previous_;
	}
	p_value->previous_ = nullptr;
	p_value->next_ = nullptr;
}

void PuertsEnvironment::invalidate_script_values() {
	PuertsScriptValue *value = script_values_head_;
	script_values_head_ = nullptr;
	while (value != nullptr) {
		PuertsScriptValue *next = value->next_;
		value->previous_ = nullptr;
		value->next_ = nullptr;
		value->cache_entry_ = nullptr;
		Ref<PuertsScriptValue> keep_alive(value);
		value->release_value_ref();
		value = next;
	}
	script_value_cache_.clear();
}
