// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_v8_backend.h"

extern "C" {
int GetV8PapiVersion();
pesapi_ffi *GetV8FFIApi();
pesapi_env_ref CreateV8PapiEnvRef();
void DestroyV8PapiEnvRef(pesapi_env_ref p_env_ref);
void *GetV8Isolate(pesapi_env_ref p_env_ref);
void LogicTick(void *p_isolate);
void LowMemoryNotification(void *p_isolate);
void CreateInspector(void *p_isolate, int32_t p_port);
int InspectorTick(void *p_isolate);
void DestroyInspector(void *p_isolate);
void TerminateExecution(void *p_isolate);
}

namespace {

int puerts_v8_get_api_version() {
	return GetV8PapiVersion();
}

pesapi_ffi *puerts_v8_get_ffi() {
	return GetV8FFIApi();
}

pesapi_env_ref puerts_v8_create_env_ref() {
	return CreateV8PapiEnvRef();
}

void puerts_v8_destroy_env_ref(pesapi_env_ref p_env_ref) {
	DestroyV8PapiEnvRef(p_env_ref);
}

void puerts_v8_tick(pesapi_env_ref p_env_ref) {
	LogicTick(GetV8Isolate(p_env_ref));
}

void puerts_v8_low_memory_notification(pesapi_env_ref p_env_ref) {
	LowMemoryNotification(GetV8Isolate(p_env_ref));
}

void puerts_v8_open_debugger(pesapi_env_ref p_env_ref, int32_t p_port) {
	CreateInspector(GetV8Isolate(p_env_ref), p_port);
}

bool puerts_v8_debugger_tick(pesapi_env_ref p_env_ref) {
	return InspectorTick(GetV8Isolate(p_env_ref)) != 0;
}

void puerts_v8_close_debugger(pesapi_env_ref p_env_ref) {
	DestroyInspector(GetV8Isolate(p_env_ref));
}

void puerts_v8_terminate_execution(pesapi_env_ref p_env_ref) {
	TerminateExecution(GetV8Isolate(p_env_ref));
}

const PuertsBackendFunctions puerts_v8_functions = {
	&puerts_v8_get_api_version,
	&puerts_v8_get_ffi,
	&puerts_v8_create_env_ref,
	&puerts_v8_destroy_env_ref,
	&puerts_v8_tick,
	&puerts_v8_low_memory_notification,
	&puerts_v8_open_debugger,
	&puerts_v8_debugger_tick,
	&puerts_v8_close_debugger,
	&puerts_v8_terminate_execution,
};

const PuertsBackendDescriptor puerts_v8_descriptor = {
	"v8",
	"V8",
	"ecmascript",
	&puerts_v8_functions,
};

} // namespace

using namespace godot;

void PuertsV8Backend::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_backend_id"), &PuertsV8Backend::get_backend_id);
	ClassDB::bind_method(D_METHOD("get_backend_name"), &PuertsV8Backend::get_backend_name);
	ClassDB::bind_method(D_METHOD("get_language_id"), &PuertsV8Backend::get_language_id);
	ClassDB::bind_method(D_METHOD("_puerts_get_functions_ptr"), &PuertsV8Backend::_puerts_get_functions_ptr);
}

godot::StringName PuertsV8Backend::get_backend_id() const {
	return puerts_backend_resource::get_backend_id(puerts_v8_descriptor);
}

godot::String PuertsV8Backend::get_backend_name() const {
	return puerts_backend_resource::get_backend_name(puerts_v8_descriptor);
}

godot::StringName PuertsV8Backend::get_language_id() const {
	return puerts_backend_resource::get_language_id(puerts_v8_descriptor);
}

uint64_t PuertsV8Backend::_puerts_get_functions_ptr() const {
	return puerts_backend_resource::get_functions_ptr(puerts_v8_descriptor);
}
