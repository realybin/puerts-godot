# SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
# SPDX-License-Identifier: BSD-3-Clause

extends "res://tests/support/backend_test_case.gd"

const TestSupport = preload("res://tests/support/puerts_support.gd")


func test_cross_backend_isolated_environments() -> void:
	if _available_backends.size() < 2:
		pending("cross backend requires at least two available backends")
		return

	var entry_a: Dictionary = _available_backends[0]
	var entry_b: Dictionary = _available_backends[1]
	var backend_info_a: Dictionary = entry_a["info"]
	var backend_info_b: Dictionary = entry_b["info"]
	var backend_a: Object = entry_a["backend"]
	var backend_b: Object = entry_b["backend"]

	assert_true(ClassDB.class_exists("PuertsEnvironment"), "PuertsEnvironment class exists")
	var env_a: Variant = ClassDB.instantiate("PuertsEnvironment")
	var env_b: Variant = ClassDB.instantiate("PuertsEnvironment")
	if env_a == null or env_b == null:
		fail_test("cross backend instantiate expected two PuertsEnvironment instances")
		return
	var init_error: int = int(env_a.initialize(backend_a, TestSupport.create_cache_pool()))
	assert_eq(init_error, OK, "cross backend env_a initialize")
	if init_error != OK:
		return

	init_error = env_b.initialize(backend_b, TestSupport.create_cache_pool())
	assert_eq(init_error, OK, "cross backend env_b initialize")
	if init_error != OK:
		env_a.dispose()
		return

	env_a.set_global("backend_name", backend_info_a["name"])
	env_b.set_global("backend_name", backend_info_b["name"])

	var eval_a := "backend_name"
	var eval_b := "backend_name"
	if backend_info_a["language"] == "lua":
		eval_a = "return backend_name"
	if backend_info_b["language"] == "lua":
		eval_b = "return backend_name"

	assert_eq(TestSupport.unwrap_script_value(env_a.eval(eval_a)), backend_info_a["name"], "cross backend isolated globals env_a")
	assert_eq(TestSupport.unwrap_script_value(env_b.eval(eval_b)), backend_info_b["name"], "cross backend isolated globals env_b")
	assert_eq(TestSupport.unwrap_script_value(env_a.eval("return 1 + 2" if backend_info_a["language"] == "lua" else "1 + 2")), 3, "cross backend eval env_a")
	assert_eq(TestSupport.unwrap_script_value(env_b.eval("return 4 + 5" if backend_info_b["language"] == "lua" else "4 + 5")), 9, "cross backend eval env_b")

	env_a.dispose()
	env_b.dispose()
