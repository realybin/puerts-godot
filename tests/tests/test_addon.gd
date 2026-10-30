# SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
# SPDX-License-Identifier: BSD-3-Clause

extends "res://tests/support/test_case.gd"


func test_shared_addon() -> bool:
	var path := OS.get_environment("PUERTS_TEST_ADDON_PATH")
	if path.is_empty():
		return skip("set PUERTS_TEST_ADDON_PATH to the built PuertsTestAddon library")
	if not equal(ClassDB.class_call_static("PuertsAddon", "load", path, "PuertsTestAddon"), OK, "load addon"):
		return true
	equal(ClassDB.class_call_static("PuertsAddon", "load", path, "PuertsTestAddon"), OK, "load addon again")
	equal(native(evaluate(
		"local Counter = load_addon_type('Example.Counter'); counter = Counter(); return counter:increment()",
		"var Counter = load_addon_type('Example.Counter'); globalThis.counter = new Counter(); counter.increment()"
	)), 1, "namespaced addon constructor and method")
	equal(native(evaluate("return counter:increment()", "counter.increment()")), 2, "addon instance state")
	equal(native(evaluate(
		"local Counter = load_addon_type('PlainCounter'); return Counter():increment()",
		"new (load_addon_type('PlainCounter'))().increment()"
	)), 1, "unnamespaced addon type")
	var other_env := new_environment(backend)
	if other_env == null:
		return true
	env.dispose()
	equal(native(other_env.eval(code(
		"return load_addon_type('Example.Counter').live_count()",
		"load_addon_type('Example.Counter').live_count()"
	))), 0, "addon instances finalized on environment disposal")
	equal(native(other_env.eval(code(
		"local Counter = load_addon_type('Example.Counter'); return Counter():increment()",
		"new (load_addon_type('Example.Counter'))().increment()"
	))), 1, "addon survives disposal of another environment")
	return true


func test_invalid_addon_type() -> bool:
	equal(evaluate("return load_addon_type('Missing.AddonType')", "load_addon_type('Missing.AddonType')"), null, "missing addon type")
	error_contains("Addon type not found")
	equal(evaluate("return load_addon_type(42)", "load_addon_type(42)"), null, "invalid addon type argument")
	error_contains("expects exactly one string")
	return true
