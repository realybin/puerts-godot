// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_nodejs_backend.h"

extern "C" {
int GetNodejsPapiVersion();
pesapi_ffi *GetNodejsFFIApi();
pesapi_env_ref CreateNodejsPapiEnvRef();
void DestroyNodejsPapiEnvRef(pesapi_env_ref p_env_ref);
void *GetNodejsIsolate(pesapi_env_ref p_env_ref);
void NodejsLogicTick(void *p_isolate);
void NodejsLowMemoryNotification(void *p_isolate);
void NodejsCreateInspector(void *p_isolate, int32_t p_port);
int NodejsInspectorTick(void *p_isolate);
void NodejsDestroyInspector(void *p_isolate);
void NodejsTerminateExecution(void *p_isolate);
}

namespace {

int puerts_nodejs_get_api_version() {
	return GetNodejsPapiVersion();
}

pesapi_ffi *puerts_nodejs_get_ffi() {
	return GetNodejsFFIApi();
}

pesapi_env_ref puerts_nodejs_create_env_ref() {
	return CreateNodejsPapiEnvRef();
}

void puerts_nodejs_destroy_env_ref(pesapi_env_ref p_env_ref) {
	DestroyNodejsPapiEnvRef(p_env_ref);
}

void puerts_nodejs_tick(pesapi_env_ref p_env_ref) {
	NodejsLogicTick(GetNodejsIsolate(p_env_ref));
}

void puerts_nodejs_low_memory_notification(pesapi_env_ref p_env_ref) {
	NodejsLowMemoryNotification(GetNodejsIsolate(p_env_ref));
}

void puerts_nodejs_open_debugger(pesapi_env_ref p_env_ref, int32_t p_port) {
	NodejsCreateInspector(GetNodejsIsolate(p_env_ref), p_port);
}

bool puerts_nodejs_debugger_tick(pesapi_env_ref p_env_ref) {
	return NodejsInspectorTick(GetNodejsIsolate(p_env_ref)) != 0;
}

void puerts_nodejs_close_debugger(pesapi_env_ref p_env_ref) {
	NodejsDestroyInspector(GetNodejsIsolate(p_env_ref));
}

void puerts_nodejs_terminate_execution(pesapi_env_ref p_env_ref) {
	NodejsTerminateExecution(GetNodejsIsolate(p_env_ref));
}

const PuertsBackendFunctions puerts_nodejs_functions = {
	&puerts_nodejs_get_api_version,
	&puerts_nodejs_get_ffi,
	&puerts_nodejs_create_env_ref,
	&puerts_nodejs_destroy_env_ref,
	&puerts_nodejs_tick,
	&puerts_nodejs_low_memory_notification,
	&puerts_nodejs_open_debugger,
	&puerts_nodejs_debugger_tick,
	&puerts_nodejs_close_debugger,
	&puerts_nodejs_terminate_execution,
};

const PuertsBackendDescriptor puerts_nodejs_descriptor = {
	"nodejs",
	"Node.js",
	"ecmascript",
	&puerts_nodejs_functions,
};

} // namespace

using namespace godot;

void PuertsNodejsBackend::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_backend_id"), &PuertsNodejsBackend::get_backend_id);
	ClassDB::bind_method(D_METHOD("get_backend_name"), &PuertsNodejsBackend::get_backend_name);
	ClassDB::bind_method(D_METHOD("get_language_id"), &PuertsNodejsBackend::get_language_id);
	ClassDB::bind_method(D_METHOD("_puerts_get_functions_ptr"), &PuertsNodejsBackend::_puerts_get_functions_ptr);
}

godot::StringName PuertsNodejsBackend::get_backend_id() const {
	return puerts_backend_resource::get_backend_id(puerts_nodejs_descriptor);
}

godot::String PuertsNodejsBackend::get_backend_name() const {
	return puerts_backend_resource::get_backend_name(puerts_nodejs_descriptor);
}

godot::StringName PuertsNodejsBackend::get_language_id() const {
	return puerts_backend_resource::get_language_id(puerts_nodejs_descriptor);
}

uint64_t PuertsNodejsBackend::_puerts_get_functions_ptr() const {
	return puerts_backend_resource::get_functions_ptr(puerts_nodejs_descriptor);
}
