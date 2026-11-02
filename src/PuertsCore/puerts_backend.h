// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_BACKEND_H
#define PUERTS_GODOT_PUERTS_BACKEND_H

#include "pesapi.h"

#include <cstdint>

struct PuertsBackendFunctionTable {
	static constexpr uint32_t VERSION = 1;
	uint32_t version = VERSION;
	int (*get_api_version)() = nullptr;
	pesapi_ffi *(*get_ffi)() = nullptr;
	pesapi_env_ref (*create_env_ref)() = nullptr;
	void (*destroy_env_ref)(pesapi_env_ref p_env_ref) = nullptr;
	void (*tick)(pesapi_env_ref p_env_ref) = nullptr;
	void (*low_memory_notification)(pesapi_env_ref p_env_ref) = nullptr;
	void (*open_debugger)(pesapi_env_ref p_env_ref, int32_t p_port) = nullptr;
	bool (*debugger_tick)(pesapi_env_ref p_env_ref) = nullptr;
	void (*close_debugger)(pesapi_env_ref p_env_ref) = nullptr;
	void (*terminate_execution)(void *p_execution_handle) = nullptr;
	bool supports_dispose = true;
	void *(*get_execution_handle)(pesapi_env_ref p_env_ref) = nullptr;
};

#endif // PUERTS_GODOT_PUERTS_BACKEND_H
