# SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
# SPDX-License-Identifier: BSD-3-Clause

extends "res://tests/support/backend_test_case.gd"

const TestSupport = preload("res://tests/support/puerts_support.gd")


class InvalidBackend:
	extends Node


func _invalid_environment_parameters() -> Array:
	return [
		{
			"label": "null backend",
			"backend": null,
			"cache_pool_factory": TestSupport.create_cache_pool,
			"expected_error": ERR_INVALID_PARAMETER,
			"expected_message": "Puerts backend is null.",
		},
		{
			"label": "invalid backend type",
			"backend": InvalidBackend.new(),
			"cache_pool_factory": TestSupport.create_cache_pool,
			"expected_error": ERR_INVALID_PARAMETER,
			"expected_message": "Puerts backend must inherit RefCounted.",
		},
		{
			"label": "null cache pool",
			"backend": TestSupport.create_backend("PuertsQuickjsBackend"),
			"cache_pool_factory": func():
				return null,
			"expected_error": ERR_INVALID_PARAMETER,
			"expected_message": "Puerts StringName cache pool is null.",
		},
		{
			"label": "uninitialized cache pool",
			"backend": TestSupport.create_backend("PuertsQuickjsBackend"),
			"cache_pool_factory": func():
				if not ClassDB.class_exists("PuertsStringNameCachePool"):
					return null
				return ClassDB.instantiate("PuertsStringNameCachePool"),
			"expected_error": ERR_INVALID_PARAMETER,
			"expected_message": "Puerts StringName cache pool is not initialized.",
		},
	]


func test_environment_validation_cases() -> Array:
	return _invalid_environment_parameters()


func test_environment_validation(case_data: Dictionary) -> void:
	var env: Variant = ClassDB.instantiate("PuertsEnvironment")
	if env == null:
		fail_test("PuertsEnvironment instantiate")
		return

	var sink := {"error": [], "warn": [], "info": []}
	env.set_error_callback(func(message: String):
		sink["error"].append(message)
	)
	env.set_warn_callback(func(message: String):
		sink["warn"].append(message)
	)
	env.set_info_callback(func(message: String):
		sink["info"].append(message)
	)

	var backend: Object = case_data["backend"]
	var cache_pool = case_data["cache_pool_factory"].call()
	var init_error: int = int(env.initialize(backend, cache_pool))
	assert_eq(init_error, case_data["expected_error"], "%s error code" % case_data["label"])

	var last_error := ""
	var errors: Array = sink["error"]
	if not errors.is_empty():
		last_error = str(errors[errors.size() - 1])
	assert_string_contains(last_error, case_data["expected_message"])
	assert_false(env.is_alive(), "%s environment alive after failure" % case_data["label"])
	env.dispose()


func test_backend_metadata_cases() -> Array:
	return backend_parameters()


func test_backend_metadata(entry: Dictionary) -> void:
	var backend_info: Dictionary = entry["info"]
	var backend = entry["backend"]
	var backend_name: String = backend_info["name"]

	assert_eq(backend.get_backend_id(), backend_name, "%s backend id" % backend_name)
	assert_eq(backend.get_language_id(), backend_info["language"], "%s language id" % backend_name)

	var created := TestSupport.create_environment(backend, backend_name)
	assert_true(bool(created["ok"]), "%s environment initialize" % backend_name)
	if bool(created["ok"]):
		var env: Object = created["env"]
		assert_true(env.is_alive(), "%s environment alive after initialize" % backend_name)

		env.tick()
		if not backend.supports_tick():
			assert_string_contains(TestSupport.env_last_error(env), "does not support tick.")

		env.low_memory_notification()
		if not backend.supports_low_memory_notification():
			assert_string_contains(TestSupport.env_last_error(env), "does not support low memory notification.")

		env.terminate_execution()
		if not backend.supports_terminate_execution():
			assert_string_contains(TestSupport.env_last_error(env), "does not support terminate execution.")

		env.dispose()
		assert_false(env.is_alive(), "%s environment dispose" % backend_name)
