// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_WEB_FFI_H
#define PUERTS_GODOT_PUERTS_WEB_FFI_H

#include "pesapi.h"

#include <godot_cpp/core/defs.hpp>

GDE_EXPORT int puerts_web_get_api_version();
GDE_EXPORT pesapi_ffi *puerts_web_get_ffi();
GDE_EXPORT pesapi_env_ref puerts_web_create_env_ref();
GDE_EXPORT void puerts_web_destroy_env_ref(pesapi_env_ref p_env_ref);

#endif // PUERTS_GODOT_PUERTS_WEB_FFI_H
