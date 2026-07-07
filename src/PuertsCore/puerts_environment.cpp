// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_environment.h"

#include "puerts_bridge_registry.h"
#include "puerts_runtime.h"
#include "puerts_script_value.h"
#include "puerts_type_register.h"

#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>

using namespace godot;

extern "C" int GetPapiVersion();

namespace {

constexpr char load_type_property_name[] = "load_type";
constexpr char log_error_property_name[] = "log_error";
constexpr char log_warn_property_name[] = "log_warn";
constexpr char log_info_property_name[] = "log_info";

} // namespace

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
	dispose();
}

Error PuertsEnvironment::initialize(Object *p_backend, const Ref<PuertsStringNameCachePool> &p_string_name_cache_pool) {
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

	const uint64_t functions_ptr = functions_ptr_variant;
	if (functions_ptr == 0) {
		log_error("Puerts backend does not expose its function table.");
		return ERR_UNCONFIGURED;
	}

	const auto *functions = reinterpret_cast<const PuertsBackendFunctions *>(functions_ptr);
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

	pesapi_scope scope = ffi_->open_scope(env_ref_);
	pesapi_env env = ffi_->get_env_from_ref(env_ref_);
	PuertsTypeRegister &type_register = PuertsTypeRegister::get_singleton();
	ffi_->set_registry(env, type_register.get_registry());

	env_private_ = memnew(PuertsEnvPrivate);
	env_private_->alive = true;
	env_private_->environment = this;
	env_private_->bridge = memnew(PuertsBridgeRegistry);
	ffi_->set_env_private(env, env_private_);

	pesapi_value load_type = ffi_->create_function(env, &PuertsTypeRegister::load_type_callback, nullptr, nullptr);
	ffi_->set_property(env, ffi_->global(env), load_type_property_name, load_type);
	pesapi_value log_error = ffi_->create_function(env, &PuertsEnvironment::script_log_error_callback, nullptr, nullptr);
	ffi_->set_property(env, ffi_->global(env), log_error_property_name, log_error);
	pesapi_value log_warn = ffi_->create_function(env, &PuertsEnvironment::script_log_warn_callback, nullptr, nullptr);
	ffi_->set_property(env, ffi_->global(env), log_warn_property_name, log_warn);
	pesapi_value log_info = ffi_->create_function(env, &PuertsEnvironment::script_log_info_callback, nullptr, nullptr);
	ffi_->set_property(env, ffi_->global(env), log_info_property_name, log_info);
	ffi_->close_scope(scope);

	backend_functions_ = functions;
	backend_ref_ = Ref(backend_refcounted);
	string_name_cache_pool_ = p_string_name_cache_pool;
	return OK;
}

void PuertsEnvironment::dispose() {
	invalidate_script_values();

	if (env_ref_ != nullptr && ffi_ != nullptr && env_private_ != nullptr) {
		env_private_->alive = false;
		pesapi_scope scope = ffi_->open_scope(env_ref_);
		pesapi_env env = ffi_->get_env_from_ref(env_ref_);
		ffi_->set_env_private(env, nullptr);
		ffi_->close_scope(scope);

		env_private_->bridge->clear();
		memdelete(env_private_->bridge);
		env_private_->bridge = nullptr;
		memdelete(env_private_);
		env_private_ = nullptr;
	}

	if (env_ref_ != nullptr && backend_functions_ != nullptr && backend_functions_->destroy_env_ref != nullptr) {
		backend_functions_->destroy_env_ref(env_ref_);
	}

	env_ref_ = nullptr;
	ffi_ = nullptr;
	backend_functions_ = nullptr;
	backend_ref_.unref();
	string_name_cache_pool_.unref();
}

bool PuertsEnvironment::is_alive() const {
	return ffi_ != nullptr && env_ref_ != nullptr && backend_ref_.is_valid() && backend_functions_ != nullptr &&
			env_private_ != nullptr && env_private_->alive && env_private_->bridge != nullptr &&
			string_name_cache_pool_.is_valid();
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
	if (!can_use_backend_function(backend_functions_ != nullptr ? reinterpret_cast<const void *>(backend_functions_->tick) : nullptr, "Puerts backend does not support tick.")) {
		return;
	}
	backend_functions_->tick(env_ref_);
}

void PuertsEnvironment::low_memory_notification() {
	if (!can_use_backend_function(backend_functions_ != nullptr ? reinterpret_cast<const void *>(backend_functions_->low_memory_notification) : nullptr, "Puerts backend does not support low memory notification.")) {
		return;
	}
	backend_functions_->low_memory_notification(env_ref_);
}

void PuertsEnvironment::open_debugger(int32_t p_port) {
	if (!can_use_backend_function(backend_functions_ != nullptr ? reinterpret_cast<const void *>(backend_functions_->open_debugger) : nullptr, "Puerts backend does not support remote debugging.")) {
		return;
	}
	backend_functions_->open_debugger(env_ref_, p_port);
}

bool PuertsEnvironment::debugger_tick() {
	if (!can_use_backend_function(backend_functions_ != nullptr ? reinterpret_cast<const void *>(backend_functions_->debugger_tick) : nullptr, "Puerts backend does not support debugger tick.")) {
		return false;
	}
	return backend_functions_->debugger_tick(env_ref_);
}

void PuertsEnvironment::close_debugger() {
	if (!can_use_backend_function(backend_functions_ != nullptr ? reinterpret_cast<const void *>(backend_functions_->close_debugger) : nullptr, "Puerts backend does not support remote debugging.")) {
		return;
	}
	backend_functions_->close_debugger(env_ref_);
}

void PuertsEnvironment::terminate_execution() {
	if (!can_use_backend_function(backend_functions_ != nullptr ? reinterpret_cast<const void *>(backend_functions_->terminate_execution) : nullptr, "Puerts backend does not support terminate execution.")) {
		return;
	}
	backend_functions_->terminate_execution(env_ref_);
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

static String _read_log_message_arg(pesapi_ffi *p_apis, pesapi_env p_env, pesapi_callback_info p_info) {
	pesapi_value arg = p_apis->get_arg(p_info, 0);
	size_t size = 0;
	const char *inline_text = p_apis->get_value_string_utf8(p_env, arg, nullptr, &size);
	if (inline_text != nullptr) {
		return String::utf8(inline_text, static_cast<int>(size));
	}

	char *buffer = memnew_arr(char, size + 1);
	p_apis->get_value_string_utf8(p_env, arg, buffer, &size);
	buffer[size] = 0;
	String result = String::utf8(buffer, static_cast<int>(size));
	memdelete_arr(buffer);
	return result;
}

void PuertsEnvironment::script_log_error_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	pesapi_env env = apis->get_env(info);
	const auto *env_private = static_cast<const PuertsEnvPrivate *>(apis->get_env_private(env));
	env_private->environment->log_error(_read_log_message_arg(apis, env, info));
}

void PuertsEnvironment::script_log_warn_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	pesapi_env env = apis->get_env(info);
	const auto *env_private = static_cast<const PuertsEnvPrivate *>(apis->get_env_private(env));
	env_private->environment->log_warn(_read_log_message_arg(apis, env, info));
}

void PuertsEnvironment::script_log_info_callback(struct pesapi_ffi *apis, pesapi_callback_info info) {
	pesapi_env env = apis->get_env(info);
	const auto *env_private = static_cast<const PuertsEnvPrivate *>(apis->get_env_private(env));
	env_private->environment->log_info(_read_log_message_arg(apis, env, info));
}

const CharString &PuertsEnvironment::get_cached_utf8(const StringName &p_name) {
	static const CharString empty_utf8;
	if (p_name.is_empty() || string_name_cache_pool_.is_null()) {
		return empty_utf8;
	}
	return string_name_cache_pool_->get_cached_utf8(p_name);
}

bool PuertsEnvironment::can_use_backend_function(const void *p_function, const String &p_error_message) {
	if (!is_alive()) {
		log_error("Puerts environment is not initialized.");
		return false;
	}
	if (p_function == nullptr) {
		log_error(p_error_message);
		return false;
	}
	return true;
}

void PuertsEnvironment::register_script_value(ObjectID p_id) {
	if (p_id.is_valid()) {
		script_value_ids_.insert(p_id);
	}
}

void PuertsEnvironment::unregister_script_value(ObjectID p_id) {
	if (p_id.is_valid()) {
		script_value_ids_.erase(p_id);
	}
}

void PuertsEnvironment::invalidate_script_values() {
	if (script_value_ids_.empty()) {
		return;
	}

	puerts_eastl::vector<uint64_t> ids;
	ids.reserve(script_value_ids_.size());
	for (unsigned long long script_value_id : script_value_ids_) {
		ids.push_back(script_value_id);
	}
	script_value_ids_.clear();
	for (const uint64_t raw_id : ids) {
		Object *object = ObjectDB::get_instance(ObjectID(raw_id));
		auto *script_value = Object::cast_to<PuertsScriptValue>(object);
		if (script_value != nullptr) {
			script_value->release_value_ref();
		}
	}
}
