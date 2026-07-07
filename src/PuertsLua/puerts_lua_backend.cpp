// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_lua_backend.h"

extern "C" {
int GetLuaPapiVersion();
pesapi_ffi *GetLuaFFIApi();
pesapi_env_ref CreateLuaPapiEnvRef();
void DestroyLuaPapiEnvRef(pesapi_env_ref p_env_ref);
}

namespace {

int puerts_lua_get_api_version() {
	return GetLuaPapiVersion();
}

pesapi_ffi *puerts_lua_get_ffi() {
	return GetLuaFFIApi();
}

pesapi_env_ref puerts_lua_create_env_ref() {
	return CreateLuaPapiEnvRef();
}

void puerts_lua_destroy_env_ref(pesapi_env_ref p_env_ref) {
	DestroyLuaPapiEnvRef(p_env_ref);
}

constexpr PuertsBackendFunctions puerts_lua_functions = {
	&puerts_lua_get_api_version,
	&puerts_lua_get_ffi,
	&puerts_lua_create_env_ref,
	&puerts_lua_destroy_env_ref,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};

const PuertsBackendDescriptor puerts_lua_descriptor = {
	"lua",
	"Lua",
	"lua",
	&puerts_lua_functions,
};

} // namespace

using namespace godot;

void PuertsLuaBackend::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_available"), &PuertsLuaBackend::is_available);
	ClassDB::bind_method(D_METHOD("get_backend_id"), &PuertsLuaBackend::get_backend_id);
	ClassDB::bind_method(D_METHOD("get_backend_name"), &PuertsLuaBackend::get_backend_name);
	ClassDB::bind_method(D_METHOD("get_language_id"), &PuertsLuaBackend::get_language_id);
	ClassDB::bind_method(D_METHOD("supports_tick"), &PuertsLuaBackend::supports_tick);
	ClassDB::bind_method(D_METHOD("supports_debugger"), &PuertsLuaBackend::supports_debugger);
	ClassDB::bind_method(D_METHOD("supports_low_memory_notification"), &PuertsLuaBackend::supports_low_memory_notification);
	ClassDB::bind_method(D_METHOD("supports_terminate_execution"), &PuertsLuaBackend::supports_terminate_execution);
	ClassDB::bind_method(D_METHOD("_puerts_get_functions_ptr"), &PuertsLuaBackend::_puerts_get_functions_ptr);
}

bool PuertsLuaBackend::is_available() const {
	return puerts_backend_resource::is_available(puerts_lua_descriptor);
}

godot::StringName PuertsLuaBackend::get_backend_id() const {
	return puerts_backend_resource::get_backend_id(puerts_lua_descriptor);
}

godot::String PuertsLuaBackend::get_backend_name() const {
	return puerts_backend_resource::get_backend_name(puerts_lua_descriptor);
}

godot::StringName PuertsLuaBackend::get_language_id() const {
	return puerts_backend_resource::get_language_id(puerts_lua_descriptor);
}

bool PuertsLuaBackend::supports_tick() const {
	return puerts_backend_resource::supports_tick(puerts_lua_descriptor);
}

bool PuertsLuaBackend::supports_debugger() const {
	return puerts_backend_resource::supports_debugger(puerts_lua_descriptor);
}

bool PuertsLuaBackend::supports_low_memory_notification() const {
	return puerts_backend_resource::supports_low_memory_notification(puerts_lua_descriptor);
}

bool PuertsLuaBackend::supports_terminate_execution() const {
	return puerts_backend_resource::supports_terminate_execution(puerts_lua_descriptor);
}

uint64_t PuertsLuaBackend::_puerts_get_functions_ptr() const {
	return puerts_backend_resource::get_functions_ptr(puerts_lua_descriptor);
}
