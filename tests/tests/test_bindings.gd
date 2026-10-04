# SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
# SPDX-License-Identifier: BSD-3-Clause

extends "res://tests/support/test_case.gd"


func start(name: String, names: Array[String]) -> bool:
	if not super.start(name, names):
		return false
	env.set_global("backend_object", backend)
	env.set_global("backend_class_name", backend_class(name))
	var path := "res://tests/runtime_cases/bindings." + ("lua" if name == "lua" else "js")
	if not check(FileAccess.file_exists(path), "binding test script exists: " + path):
		return false
	var count := errors.size()
	env.eval(FileAccess.get_file_as_string(path), path)
	return check(errors.size() == count, "load binding test script: %s" % [errors])


func run_binding_case(name: String) -> bool:
	var count := errors.size()
	var value = evaluate(
		"binding_cases.%s(); return true" % name,
		"binding_cases.%s(); true" % name
	)
	return check(errors.size() == count, "binding assertions: %s" % [errors]) and equal(native(value), true, name)


func test_type_loading() -> bool:
	return run_binding_case("exists") and run_binding_case("backend_constructor")


func test_global_scope() -> bool:
	return run_binding_case("global_scope")


func test_reflected_objects() -> bool:
	return run_binding_case("reflected_objects")


func test_binding_errors() -> bool:
	return run_binding_case("error_paths")


func test_builtin_bindings() -> bool:
	return run_binding_case("builtin_static_binding")


func test_static_argument_conversion() -> bool:
	env.set_global("boxed_vector", Vector2(3, 4))
	env.set_global("boxed_int_vector", Vector2i(3, 4))
	return run_binding_case("static_argument_conversion")
