// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_web_backend.h"
#include "puerts_web_ffi.h"

namespace {

const PuertsBackendFunctionTable puerts_web_functions = {
	&puerts_web_get_api_version,
	&puerts_web_get_ffi,
	&puerts_web_create_env_ref,
	&puerts_web_destroy_env_ref,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	false,
};

const PuertsBackendDescriptor puerts_web_descriptor = {
	"web",
	"Browser JavaScript",
	"ecmascript",
	&puerts_web_functions,
};

} // namespace

using namespace godot;

void PuertsWebBackend::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_backend_id"), &PuertsWebBackend::get_backend_id);
	ClassDB::bind_method(D_METHOD("get_backend_name"), &PuertsWebBackend::get_backend_name);
	ClassDB::bind_method(D_METHOD("get_language_id"), &PuertsWebBackend::get_language_id);
	ClassDB::bind_method(D_METHOD("_puerts_get_functions_ptr"), &PuertsWebBackend::_puerts_get_functions_ptr);
}

godot::StringName PuertsWebBackend::get_backend_id() const {
	return puerts_backend_resource::get_backend_id(puerts_web_descriptor);
}

godot::String PuertsWebBackend::get_backend_name() const {
	return puerts_backend_resource::get_backend_name(puerts_web_descriptor);
}

godot::StringName PuertsWebBackend::get_language_id() const {
	return puerts_backend_resource::get_language_id(puerts_web_descriptor);
}

uint64_t PuertsWebBackend::_puerts_get_functions_ptr() const {
	return puerts_backend_resource::get_functions_ptr(puerts_web_descriptor);
}
