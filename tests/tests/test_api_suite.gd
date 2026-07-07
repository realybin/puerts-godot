# SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
# SPDX-License-Identifier: BSD-3-Clause

extends "res://tests/support/backend_test_case.gd"

const ApiSuite = preload("res://tests/suites/api_suite.gd")

const API_CASES = [
	"eval_basics",
	"bridge_roundtrip",
	"script_value_api",
	"foreign_script_value",
	"object_graph",
]


func test_api_suite_cases() -> Array:
	return case_parameters(API_CASES)


func test_api_suite(case_data: Dictionary) -> void:
	var entry: Dictionary = case_data["entry"]
	var case_name: String = case_data["case_name"]
	var backend_info: Dictionary = entry["info"]
	var backend: Object = entry["backend"]
	var backend_name: String = backend_info["name"]
	var label = "%s/api/%s" % [backend_name, case_name]
	var result: Variant

	match case_name:
		"eval_basics":
			result = ApiSuite.run_eval_basics_suite(backend, backend_info)
		"bridge_roundtrip":
			result = ApiSuite.run_bridge_suite(backend, backend_info)
		"script_value_api":
			result = ApiSuite.run_script_value_suite(backend, backend_info)
		"foreign_script_value":
			result = ApiSuite.run_foreign_script_value_suite(backend, backend_info)
		"object_graph":
			result = ApiSuite.run_object_graph_suite(backend, backend_info)
		_:
			fail_test("unknown api case: %s" % case_name)
			return

	assert_suite_result(label, result)

