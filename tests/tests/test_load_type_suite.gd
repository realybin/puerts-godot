# SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
# SPDX-License-Identifier: BSD-3-Clause

extends "res://tests/support/backend_test_case.gd"

const TestSupport = preload("res://tests/support/puerts_support.gd")
const LoadTypeSuite = preload("res://tests/suites/load_type_suite.gd")

const LOAD_TYPE_CASES = [
	"exists",
	"backend_constructor",
	"global_scope",
	"reflected_objects",
	"error_paths",
	"builtin_static_binding",
]


func test_load_type_suite_cases() -> Array:
	return case_parameters(LOAD_TYPE_CASES)


func test_load_type_suite(case_data: Dictionary) -> void:
	var entry: Dictionary = case_data["entry"]
	var case_name: String = case_data["case_name"]
	var backend_info: Dictionary = entry["info"]
	var backend: Object = entry["backend"]
	var backend_name: String = backend_info["name"]
	var label = "%s/load_type/%s" % [backend_name, case_name]

	var created = TestSupport.create_environment(backend, backend_name)
	assert_true(bool(created["ok"]), "%s/load_type environment initialize" % backend_name)
	if not bool(created["ok"]):
		return

	var env: Object = created["env"]
	env.set_global("backend_object", backend)
	var result: Variant
	match case_name:
		"exists":
			result = LoadTypeSuite.run_load_type_existence_suite(env, backend, backend_info)
		"backend_constructor":
			result = LoadTypeSuite.run_load_type_backend_constructor_suite(env, backend, backend_info)
		"global_scope":
			result = LoadTypeSuite.run_global_scope_suite(env, backend, backend_info)
		"reflected_objects":
			result = LoadTypeSuite.run_reflected_object_binding_suite(env, backend, backend_info)
		"error_paths":
			result = LoadTypeSuite.run_reflected_error_suite(env, backend, backend_info)
		"builtin_static_binding":
			result = LoadTypeSuite.run_builtin_static_binding_suite(env, backend, backend_info)
		_:
			result = "unknown load_type case: %s" % case_name

	assert_suite_result(label, result)

	if env.is_alive():
		env.dispose()
