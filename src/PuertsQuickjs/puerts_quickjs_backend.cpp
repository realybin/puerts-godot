// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_quickjs_backend.h"

extern "C" {
int GetQjsPapiVersion();
pesapi_ffi *GetQjsFFIApi();
pesapi_env_ref CreateQjsPapiEnvRef();
void DestroyQjsPapiEnvRef(pesapi_env_ref p_env_ref);
void RunGC(pesapi_env_ref p_env_ref);
}

namespace {

int puerts_quickjs_get_api_version() {
	return GetQjsPapiVersion();
}

pesapi_ffi *puerts_quickjs_get_ffi() {
	return GetQjsFFIApi();
}

pesapi_env_ref puerts_quickjs_create_env_ref() {
	return CreateQjsPapiEnvRef();
}

void puerts_quickjs_destroy_env_ref(pesapi_env_ref p_env_ref) {
	DestroyQjsPapiEnvRef(p_env_ref);
}

void puerts_quickjs_low_memory_notification(pesapi_env_ref p_env_ref) {
	RunGC(p_env_ref);
}

const PuertsBackendFunctions puerts_quickjs_functions = {
	&puerts_quickjs_get_api_version,
	&puerts_quickjs_get_ffi,
	&puerts_quickjs_create_env_ref,
	&puerts_quickjs_destroy_env_ref,
	nullptr,
	&puerts_quickjs_low_memory_notification,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};

const PuertsBackendDescriptor puerts_quickjs_descriptor = {
	"quickjs",
	"QuickJS",
	"ecmascript",
	&puerts_quickjs_functions,
};

} // namespace

using namespace godot;

void PuertsQuickjsBackend::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_available"), &PuertsQuickjsBackend::is_available);
	ClassDB::bind_method(D_METHOD("get_backend_id"), &PuertsQuickjsBackend::get_backend_id);
	ClassDB::bind_method(D_METHOD("get_backend_name"), &PuertsQuickjsBackend::get_backend_name);
	ClassDB::bind_method(D_METHOD("get_language_id"), &PuertsQuickjsBackend::get_language_id);
	ClassDB::bind_method(D_METHOD("supports_tick"), &PuertsQuickjsBackend::supports_tick);
	ClassDB::bind_method(D_METHOD("supports_debugger"), &PuertsQuickjsBackend::supports_debugger);
	ClassDB::bind_method(D_METHOD("supports_low_memory_notification"), &PuertsQuickjsBackend::supports_low_memory_notification);
	ClassDB::bind_method(D_METHOD("supports_terminate_execution"), &PuertsQuickjsBackend::supports_terminate_execution);
	ClassDB::bind_method(D_METHOD("_puerts_get_functions_ptr"), &PuertsQuickjsBackend::_puerts_get_functions_ptr);
}

bool PuertsQuickjsBackend::is_available() const {
	return puerts_backend_resource::is_available(puerts_quickjs_descriptor);
}

godot::StringName PuertsQuickjsBackend::get_backend_id() const {
	return puerts_backend_resource::get_backend_id(puerts_quickjs_descriptor);
}

godot::String PuertsQuickjsBackend::get_backend_name() const {
	return puerts_backend_resource::get_backend_name(puerts_quickjs_descriptor);
}

godot::StringName PuertsQuickjsBackend::get_language_id() const {
	return puerts_backend_resource::get_language_id(puerts_quickjs_descriptor);
}

bool PuertsQuickjsBackend::supports_tick() const {
	return puerts_backend_resource::supports_tick(puerts_quickjs_descriptor);
}

bool PuertsQuickjsBackend::supports_debugger() const {
	return puerts_backend_resource::supports_debugger(puerts_quickjs_descriptor);
}

bool PuertsQuickjsBackend::supports_low_memory_notification() const {
	return puerts_backend_resource::supports_low_memory_notification(puerts_quickjs_descriptor);
}

bool PuertsQuickjsBackend::supports_terminate_execution() const {
	return puerts_backend_resource::supports_terminate_execution(puerts_quickjs_descriptor);
}

uint64_t PuertsQuickjsBackend::_puerts_get_functions_ptr() const {
	return puerts_backend_resource::get_functions_ptr(puerts_quickjs_descriptor);
}
