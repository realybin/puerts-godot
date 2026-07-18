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


func test_string_name_cache_pool_reconfiguration() -> void:
	var pool: Variant = ClassDB.instantiate("PuertsStringNameCachePool")
	var env: Variant = ClassDB.instantiate("PuertsEnvironment")
	var backend := TestSupport.create_backend("PuertsQuickjsBackend")
	if pool == null or env == null or backend == null:
		fail_test("cache pool reconfiguration prerequisites")
		return

	var policies := [
		{"policy": 0, "capacity": 2, "expected_capacity": 2},
		{"policy": 1, "capacity": 2, "expected_capacity": 1024},
		{"policy": 2, "capacity": 2, "expected_capacity": 2},
	]
	for index in policies.size():
		var settings: Dictionary = policies[index]
		assert_eq(pool.initialize(settings["policy"], settings["capacity"]), OK, "cache policy initialize")
		assert_true(pool.is_initialized(), "cache pool initialized")
		assert_eq(pool.get_policy(), settings["policy"], "cache policy")
		assert_eq(pool.get_capacity(), settings["expected_capacity"], "cache capacity")

		assert_eq(env.initialize(backend, pool), OK, "environment initialize with cache pool")
		var property_name := "cache_policy_%d" % index
		env.set_global(property_name, index + 1)
		var value: Variant = env.get_global(property_name)
		var actual: int = -1 if value == null else int(value.to_native())
		assert_eq(actual, index + 1, "cache-backed property lookup for policy %d" % settings["policy"])
		pool.clear()

	assert_eq(pool.initialize(99, 8), ERR_INVALID_PARAMETER, "invalid cache policy rejected")
	assert_eq(pool.get_policy(), 2, "invalid cache policy preserves prior policy")
	assert_eq(pool.get_capacity(), 2, "invalid cache policy preserves prior capacity")

	env.dispose()
