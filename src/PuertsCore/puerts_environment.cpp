// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_environment.h"

#include "puerts_runtime.h"
#include "puerts_script_callable.h"
#include "puerts_script_value.h"
#include "puerts_type_registry.h"

#include <godot_cpp/core/object.hpp>

using namespace godot;

extern "C" int GetPapiVersion();

namespace {

constexpr char kLoadTypePropertyName[] = "load_type";
constexpr char kLoadAddonTypePropertyName[] = "load_addon_type";
constexpr char kToCallablePropertyName[] = "to_callable";
constexpr char kLogErrorPropertyName[] = "log_error";
constexpr char kLogWarnPropertyName[] = "log_warn";
constexpr char kLogInfoPropertyName[] = "log_info";

} // namespace

PuertsEnvironment::OperationScope::OperationScope(PuertsEnvironment *p_environment) {
	if (p_environment == nullptr || !p_environment->is_ready()) {
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
	ClassDB::bind_method(D_METHOD("initialize", "backend", "string_name_utf8_cache"), &PuertsEnvironment::initialize);
	ClassDB::bind_method(D_METHOD("dispose"), &PuertsEnvironment::dispose);
	ClassDB::bind_method(D_METHOD("is_ready"), &PuertsEnvironment::is_ready);
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

Error PuertsEnvironment::initialize(Object *p_backend, const Ref<PuertsStringNameUtf8Cache> &p_string_name_utf8_cache) {
	if (environment_context_.state == PuertsEnvironmentState::Ready && backend_functions_ != nullptr && !backend_functions_->supports_dispose) {
		log_lifecycle_rejection("This Puerts backend does not support environment reinitialization.");
		return ERR_ALREADY_IN_USE;
	}
	if (environment_context_.state == PuertsEnvironmentState::Initializing || environment_context_.state == PuertsEnvironmentState::Disposing || active_operations_ != 0) {
		log_error("Puerts environment is busy.");
		return ERR_BUSY;
	}
	Ref<PuertsEnvironment> keep_alive(this);
	Ref<RefCounted> backend(Object::cast_to<RefCounted>(p_backend));
	dispose();
	environment_context_.state = PuertsEnvironmentState::Initializing;
	struct InitializationGuard {
		PuertsEnvironmentContext &data;
		~InitializationGuard() {
			if (data.state == PuertsEnvironmentState::Initializing) {
				data.state = PuertsEnvironmentState::Uninitialized;
			}
		}
	} initialization_guard{ environment_context_ };

	if (p_backend == nullptr) {
		log_error("Puerts backend is null.");
		return ERR_INVALID_PARAMETER;
	}
	if (p_string_name_utf8_cache.is_null()) {
		log_error("Puerts StringName UTF-8 cache is null.");
		return ERR_INVALID_PARAMETER;
	}
	if (backend.is_null()) {
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

	const auto *functions = reinterpret_cast<const PuertsBackendFunctionTable *>(static_cast<uintptr_t>(functions_address));
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

	api_ = functions->get_ffi();
	if (api_ == nullptr) {
		log_error("Puerts backend returned a null pesapi ffi.");
		return ERR_CANT_CREATE;
	}

	env_ref_ = functions->create_env_ref();
	if (env_ref_ == nullptr) {
		api_ = nullptr;
		log_error("Puerts backend failed to create an environment.");
		return ERR_CANT_CREATE;
	}

	backend_functions_ = functions;
	backend_ref_ = backend;
	string_name_utf8_cache_ = p_string_name_utf8_cache;
	environment_context_.state = PuertsEnvironmentState::Ready;
	environment_context_.environment = this;
	OperationScope operation(this);
	puerts::internal::EnvironmentHandleScope scope(api_, env_ref_);
	pesapi_env env = scope.env();
	PuertsTypeRegistry &type_registry = PuertsTypeRegistry::get_singleton();
	api_->set_registry(env, type_registry.get_registry());
	api_->set_env_private(env, &environment_context_);

	pesapi_value load_type = api_->create_function(env, &PuertsTypeRegistry::load_type_callback, nullptr, nullptr);
	api_->set_property(env, api_->global(env), kLoadTypePropertyName, load_type);
	pesapi_value load_addon_type = api_->create_function(env, &PuertsTypeRegistry::load_addon_type_callback, nullptr, nullptr);
	api_->set_property(env, api_->global(env), kLoadAddonTypePropertyName, load_addon_type);
	pesapi_value to_callable = api_->create_function(env, &PuertsEnvironment::script_to_callable_callback, nullptr, nullptr);
	api_->set_property(env, api_->global(env), kToCallablePropertyName, to_callable);
	pesapi_value log_error = api_->create_function(env, &PuertsEnvironment::script_log_error_callback, nullptr, nullptr);
	api_->set_property(env, api_->global(env), kLogErrorPropertyName, log_error);
	pesapi_value log_warn = api_->create_function(env, &PuertsEnvironment::script_log_warn_callback, nullptr, nullptr);
	api_->set_property(env, api_->global(env), kLogWarnPropertyName, log_warn);
	pesapi_value log_info = api_->create_function(env, &PuertsEnvironment::script_log_info_callback, nullptr, nullptr);
	api_->set_property(env, api_->global(env), kLogInfoPropertyName, log_info);

	if (!backend_functions_->supports_dispose) {
		page_lifetime_ref_ = keep_alive;
	}
	return OK;
}

void PuertsEnvironment::dispose() {
	if (environment_context_.state != PuertsEnvironmentState::Ready) {
		return;
	}
	if (backend_functions_ != nullptr && !backend_functions_->supports_dispose) {
		log_lifecycle_rejection("This Puerts backend does not support environment disposal.");
		return;
	}
	if (active_operations_ != 0) {
		environment_context_.state = PuertsEnvironmentState::DisposePending;
		return;
	}
	Ref<PuertsEnvironment> keep_alive(this);
	dispose_internal();
}

void PuertsEnvironment::end_operation() {
	--active_operations_;
	if (active_operations_ == 0 && environment_context_.state == PuertsEnvironmentState::DisposePending) {
		dispose_internal();
	}
}

void PuertsEnvironment::dispose_internal() {
	if (environment_context_.state != PuertsEnvironmentState::Ready && environment_context_.state != PuertsEnvironmentState::DisposePending) {
		return;
	}
	environment_context_.state = PuertsEnvironmentState::Disposing;
	invalidate_script_values();

	if (env_ref_ != nullptr) {
		{
			puerts::internal::EnvironmentHandleScope scope(api_, env_ref_);
			api_->set_env_private(scope.env(), nullptr);
		}

		environment_context_.bridge.clear();
	}
	environment_context_.environment = nullptr;

	if (env_ref_ != nullptr) {
		backend_functions_->destroy_env_ref(env_ref_);
	}

	env_ref_ = nullptr;
	api_ = nullptr;
	backend_functions_ = nullptr;
	backend_ref_.unref();
	string_name_utf8_cache_.unref();
	environment_context_.state = PuertsEnvironmentState::Uninitialized;
}

bool PuertsEnvironment::is_ready() const {
	return environment_context_.state == PuertsEnvironmentState::Ready;
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

void PuertsEnvironment::log_lifecycle_rejection(const String &p_message) {
	if (reporting_lifecycle_rejection_) {
		return;
	}
	struct ReportingGuard {
		bool &reporting;
		explicit ReportingGuard(bool &p_reporting) : reporting(p_reporting) {
			reporting = true;
		}
		~ReportingGuard() {
			reporting = false;
		}
	} reporting_guard(reporting_lifecycle_rejection_);
	log_error(p_message);
}

void PuertsEnvironment::log_warn(const String &p_message) {
	emit_log(warn_callback_, p_message);
}

void PuertsEnvironment::log_info(const String &p_message) {
	emit_log(info_callback_, p_message);
}

void PuertsEnvironment::emit_log(const Callable &p_callback, const String &p_message) {
	const Callable callback = p_callback;
	if (!callback.is_valid()) {
		return;
	}
	callback.call(p_message);
}

static String read_log_message_arg(pesapi_ffi *p_api, pesapi_env p_env, pesapi_callback_info p_info) {
	return puerts::internal::read_utf8_string(p_api, p_env, p_api->get_arg(p_info, 0));
}

template <void (PuertsEnvironment::*LogFunction)(const String &)>
void PuertsEnvironment::dispatch_log_callback(pesapi_ffi *p_api, pesapi_callback_info p_info) {
	const pesapi_env env = p_api->get_env(p_info);
	const auto *environment_context = static_cast<const PuertsEnvironmentContext *>(p_api->get_env_private(env));
	if (environment_context == nullptr || environment_context->environment == nullptr) {
		return;
	}
	(environment_context->environment->*LogFunction)(read_log_message_arg(p_api, env, p_info));
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

	puerts::return_variant_to_script(
			apis,
			info,
			frame.script_env(),
			frame.puerts_environment(),
			puerts::internal::make_script_callable(script_function));
}

CharString PuertsEnvironment::get_utf8(const StringName &p_name) {
	return string_name_utf8_cache_->get_utf8(p_name);
}

void PuertsEnvironment::register_script_value(PuertsScriptValue *p_value) {
	p_value->registry_index_ = script_values_.size();
	script_values_.push_back(p_value);
}

void PuertsEnvironment::unregister_script_value(PuertsScriptValue *p_value) {
	PuertsScriptValue *last = script_values_.back();
	script_values_[p_value->registry_index_] = last;
	last->registry_index_ = p_value->registry_index_;
	script_values_.pop_back();
}

void PuertsEnvironment::invalidate_script_values() {
	while (!script_values_.empty()) {
		Ref<PuertsScriptValue> value(script_values_.back());
		// A backend release can synchronously destroy other registered values.
		unregister_script_value(value.ptr());
		value->release_value_ref();
	}
	script_value_cache_.clear();
}
