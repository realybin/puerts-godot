// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_BACKEND_RESOURCE_H
#define PUERTS_GODOT_PUERTS_BACKEND_RESOURCE_H

#include "puerts_backend.h"

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

struct PuertsBackendDescriptor {
	const char *backend_id = "";
	const char *backend_name = "";
	const char *language_id = "";
	const PuertsBackendFunctions *functions = nullptr;
};

namespace puerts_backend_resource {

inline godot::StringName get_backend_id(const PuertsBackendDescriptor &p_descriptor) {
	return { p_descriptor.backend_id };
}

inline godot::String get_backend_name(const PuertsBackendDescriptor &p_descriptor) {
	return p_descriptor.backend_name;
}

inline godot::StringName get_language_id(const PuertsBackendDescriptor &p_descriptor) {
	return { p_descriptor.language_id };
}

inline uint64_t get_functions_ptr(const PuertsBackendDescriptor &p_descriptor) {
	return reinterpret_cast<uintptr_t>(p_descriptor.functions);
}

} // namespace puerts_backend_resource

#endif // PUERTS_GODOT_PUERTS_BACKEND_RESOURCE_H
