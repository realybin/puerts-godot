// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_webgl_backend.h"
#include "puerts_webgl_ffi.h"

namespace {

const PuertsBackendFunctions puerts_webgl_functions = {
	&puerts_webgl_get_api_version,
	&puerts_webgl_get_ffi,
	&puerts_webgl_create_env_ref,
	&puerts_webgl_destroy_env_ref,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	false,
};

const PuertsBackendDescriptor puerts_webgl_descriptor = {
	"webgl",
	"Browser JavaScript",
	"ecmascript",
	&puerts_webgl_functions,
};

} // namespace

using namespace godot;

void PuertsWebglBackend::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_backend_id"), &PuertsWebglBackend::get_backend_id);
	ClassDB::bind_method(D_METHOD("get_backend_name"), &PuertsWebglBackend::get_backend_name);
	ClassDB::bind_method(D_METHOD("get_language_id"), &PuertsWebglBackend::get_language_id);
	ClassDB::bind_method(D_METHOD("_puerts_get_functions_ptr"), &PuertsWebglBackend::_puerts_get_functions_ptr);
}

godot::StringName PuertsWebglBackend::get_backend_id() const {
	return puerts_backend_resource::get_backend_id(puerts_webgl_descriptor);
}

godot::String PuertsWebglBackend::get_backend_name() const {
	return puerts_backend_resource::get_backend_name(puerts_webgl_descriptor);
}

godot::StringName PuertsWebglBackend::get_language_id() const {
	return puerts_backend_resource::get_language_id(puerts_webgl_descriptor);
}

uint64_t PuertsWebglBackend::_puerts_get_functions_ptr() const {
	return puerts_backend_resource::get_functions_ptr(puerts_webgl_descriptor);
}
