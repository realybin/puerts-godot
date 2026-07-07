# SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
# SPDX-License-Identifier: BSD-3-Clause

extends "res://tests/support/backend_test_case.gd"

const LifecycleSuite = preload("res://tests/suites/lifecycle_suite.gd")

const LIFECYCLE_CASES = [
	"dispose_invalidation",
	"environment_release",
	"environment_owned_objects_release",
	"value_release_order",
	"dispose_stress",
	"godot_holds_js_gc",
	"js_refcounted_gc",
	"non_refcounted_unbind",
	"same_backend_multi_env",
]


func test_lifecycle_release_suite_cases() -> Array:
	return case_parameters(LIFECYCLE_CASES)


func test_lifecycle_release_suite(case_data: Dictionary) -> void:
	var entry: Dictionary = case_data["entry"]
	var case_name: String = case_data["case_name"]
	var backend_info: Dictionary = entry["info"]
	var backend: Object = entry["backend"]
	var backend_name: String = backend_info["name"]
	var label = "%s/lifecycle/%s" % [backend_name, case_name]
	var result: Variant

	match case_name:
		"dispose_invalidation":
			result = LifecycleSuite.run_dispose_invalidation_suite(backend, backend_info)
		"environment_release":
			result = LifecycleSuite.run_environment_release_suite(backend_info)
		"environment_owned_objects_release":
			result = LifecycleSuite.run_environment_owned_objects_release_suite(backend_info)
		"value_release_order":
			result = LifecycleSuite.run_value_release_order_suite(backend_info)
		"dispose_stress":
			result = LifecycleSuite.run_dispose_stress_suite(backend_info)
		"godot_holds_js_gc":
			result = LifecycleSuite.run_godot_holds_js_gc_suite(backend, backend_info)
		"js_refcounted_gc":
			result = LifecycleSuite.run_js_refcounted_gc_suite(backend, backend_info)
		"non_refcounted_unbind":
			result = LifecycleSuite.run_non_refcounted_unbind_suite(backend, backend_info)
		"same_backend_multi_env":
			result = LifecycleSuite.run_same_backend_multi_env_suite(backend, backend_info)
		_:
			fail_test("unknown lifecycle case: %s" % case_name)
			return

	assert_suite_result(label, result)
