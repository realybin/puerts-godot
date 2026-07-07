# SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
# SPDX-License-Identifier: BSD-3-Clause

extends RefCounted

const TestSupport = preload("res://tests/support/puerts_support.gd")

static func ensure_runtime_bundle_loaded(env: Object, backend_info: Dictionary, bundle_name: String) -> String:
	return TestSupport.ensure_runtime_bundle_loaded(env, backend_info, bundle_name)

static func script_for(backend_info: Dictionary, lua_code: String, ecmascript_code: String) -> String:
	return TestSupport.script_for(backend_info, lua_code, ecmascript_code)

static func unwrap_script_value(value: Variant) -> Variant:
	return TestSupport.unwrap_script_value(value)
const _RUNTIME_BUNDLE_NAME = "load_type_bundle"

static func _run_runtime_case(env: Object, backend: Object, backend_info: Dictionary, case_name: String) -> String:
	var backend_name: String = backend_info["name"]
	env.set_global("backend_object", backend)
	env.set_global("backend_class_name", backend_info["class_name"])

	var load_error = ensure_runtime_bundle_loaded(env, backend_info, _RUNTIME_BUNDLE_NAME)
	if not load_error.is_empty():
		return "%s/load_type/%s runtime bundle load failed: %s" % [backend_name, case_name, load_error]

	var case_eval = env.eval(
		script_for(
			backend_info,
			"return __puerts_cases.load_type.%s()" % case_name,
			"__puerts_cases.load_type.%s()" % case_name
		)
	)
	if case_eval == null and not TestSupport.env_last_error(env).is_empty():
		return "%s/load_type/%s runtime case eval failed: %s" % [backend_name, case_name, TestSupport.env_last_error(env)]

	var resolved = unwrap_script_value(case_eval)
	if typeof(resolved) != TYPE_STRING:
		return "%s/load_type/%s runtime case returned invalid type=%s value=%s" % [
			backend_name,
			case_name,
			type_string(typeof(resolved)),
			str(resolved)
		]

	return String(resolved)


static func run_load_type_existence_suite(env: Object, backend: Object, backend_info: Dictionary) -> String:
	return _run_runtime_case(env, backend, backend_info, "exists")


static func run_load_type_backend_constructor_suite(env: Object, backend: Object, backend_info: Dictionary) -> String:
	return _run_runtime_case(env, backend, backend_info, "backend_constructor")


static func run_global_scope_suite(env: Object, backend: Object, backend_info: Dictionary) -> String:
	return _run_runtime_case(env, backend, backend_info, "global_scope")


static func run_reflected_object_binding_suite(env: Object, backend: Object, backend_info: Dictionary) -> String:
	return _run_runtime_case(env, backend, backend_info, "reflected_objects")


static func run_reflected_error_suite(env: Object, backend: Object, backend_info: Dictionary) -> String:
	return _run_runtime_case(env, backend, backend_info, "error_paths")


static func run_builtin_static_binding_suite(env: Object, backend: Object, backend_info: Dictionary) -> String:
	return _run_runtime_case(env, backend, backend_info, "builtin_static_binding")
