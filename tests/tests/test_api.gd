# SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
# SPDX-License-Identifier: BSD-3-Clause

extends "res://tests/support/test_case.gd"


func test_values_and_binary() -> bool:
	var integer = evaluate("return 7", "7")
	check(integer.is_int(), "integer classification")
	equal(native(integer), 7, "integer conversion")
	equal(integer.to_int() if backend_name == "lua" else env.eval("7n").to_int(), 7, "64-bit integer conversion")
	var decimal = evaluate("return 7.5", "7.5")
	check(decimal.is_float() and not decimal.is_int(), "float classification")
	equal(decimal.to_float(), 7.5, "float conversion")
	var text = evaluate("return 'hello'", "'hello'")
	check(text.is_string(), "string classification")
	equal(text.to_string(), "hello", "string conversion")
	equal(evaluate("return true", "true").to_bool(), true, "boolean conversion")
	equal(text.unwrap_native(), null, "primitive is not a native wrapper")
	var bytes := PackedByteArray([1, 2, 3, 4])
	env.set_global("bytes", bytes)
	var value = env.get_global("bytes")
	equal(value.to_binary() if value.is_binary() else value.unwrap_native(), bytes, "binary roundtrip")
	return true


func test_globals_and_error_recovery() -> bool:
	equal(evaluate("error('boom')", "throw new Error('boom')"), null, "failed eval result")
	error_contains("boom")
	env.set_global("number", 21)
	equal(native(env.get_global("number")), 21, "get_global after exception")
	equal(native(evaluate("return number * 2", "number * 2")), 42, "eval after exception")
	var text := "puerts-你好-" + "x".repeat(600)
	env.set_global("text", text)
	equal(native(env.get_global("text")), text, "UTF-8 string beyond inline storage")
	return true


func test_global_setter_exception() -> bool:
	if backend_name not in ["v8", "nodejs"]:
		return skip("backend does not catch global setter exceptions")
	env.eval("Object.defineProperty(globalThis, 'rejected', {set() { throw new Error('setter failed'); }});")
	env.set_global("rejected", 1)
	error_contains("setter failed")
	env.set_global("accepted", 42)
	equal(native(env.get_global("accepted")), 42, "set_global recovers")
	return true


func test_native_roundtrip() -> bool:
	env.set_global("vector", Vector2(3, 4))
	equal(native(evaluate("return vector", "vector")), Vector2(3, 4), "builtin roundtrip")
	equal(native(evaluate("return vector:length()", "vector.length()")), 5.0, "builtin method")
	equal(native(evaluate("return vector.x", "vector.x")), 3.0, "builtin property")
	env.set_global("backend", backend)
	equal(env.get_global("backend").unwrap_native(), backend, "host object unwrap")
	equal(native(evaluate("return backend:get_backend_id()", "backend.get_backend_id()")), backend_name, "host method")
	equal(evaluate("return load_type('Vector2')(2, 5)", "new (load_type('Vector2'))(2, 5)").unwrap_native(),
		Vector2(2, 5), "static builtin unwrap")
	return true


func test_functions_and_properties() -> bool:
	var object = evaluate(
		"return {add = function(a, b) return a + b end, nested = {value = 7}}",
		"({add(a, b) { return a + b; }, nested: {value: 7}})"
	)
	equal(native(object.call_method("add", [4, 5])), 9, "method call")
	equal(native(object.get_property("add").call([7, 8])), 15, "function call")
	var nested = object.get_property("nested")
	equal(nested, object.get_property("nested"), "property wrapper identity")
	env.set_global("root", object)
	equal(native(evaluate("return root.nested.value", "root.nested.value")), 7, "nested property")
	if backend_name != "lua":
		equal(native(nested.get_property("value")), 7, "script value property getter")
	nested.set_property("value", 9)
	equal(native(evaluate("return root.nested.value", "root.nested.value")), 9, "set_property")
	equal(object.call_method("nested"), null, "non-callable method rejected")
	error_contains("not callable: nested")
	equal(nested.call(), null, "non-function rejected")
	error_contains("not a function")
	equal(native(object.call_method("add", [6, 7])), 13, "call recovers")
	var sum = evaluate(
		"return function(...) local n = 0; for i = 1, select('#', ...) do n = n + select(i, ...) end; return n end",
		"(...args) => args.reduce((sum, value) => sum + value, 0)"
	)
	equal(native(sum.call([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])), 55, "arguments exceed inline capacity")
	return true


func test_wrapper_cache_rebuild() -> bool:
	var value = evaluate("cached = {value = 9}; return cached", "globalThis.cached = {value: 9}; cached")
	equal(value, env.get_global("cached"), "global wrapper identity")
	value = null
	var rebuilt = env.get_global("cached")
	equal(rebuilt, env.get_global("cached"), "identity after wrapper release")
	if backend_name != "lua":
		env.eval("Object.freeze(cached)")
		rebuilt = null
		var unrelated = env.eval("({unrelated: true})")
		equal(native(env.get_global("cached").get_property("value")), 9, "frozen object cache rebuild")
		check(unrelated.is_valid(), "unrelated wrapper remains valid")
	return true


func test_freed_host_object() -> bool:
	var node := Node.new()
	env.set_global("node", node)
	var value = env.get_global("node")
	equal(value.unwrap_native(), node, "borrowed node unwrap")
	node.free()
	equal(value.unwrap_native(), null, "freed node unwrap")
	var count := errors.size()
	value.get_property("name")
	check(errors.size() > count, "freed node access logs an error")
	error_contains("Native object is no longer valid")
	return true


func test_environment_isolation_and_foreign_values() -> bool:
	var other := new_environment(backend)
	if other == null:
		return false
	env.set_global("number", 11)
	other.set_global("number", 22)
	equal(native(env.eval(code("return number", "number"))), 11, "first environment global")
	equal(native(other.eval(code("return number", "number"))), 22, "second environment global")

	var local_value = other.eval(code("return {value = 9}", "({value: 9})"))
	other.set_global("local_value", local_value)
	equal(native(other.eval(code("return local_value.value", "local_value.value"))), 9, "same environment wrapper accepted")
	var foreign = evaluate("return {value = 7}", "({value: 7})")
	other.set_global("foreign", foreign)
	error_contains("another PuertsEnvironment")
	equal(native(other.get_global("foreign")), null, "foreign global rejected")
	var echo = other.eval(code("return function(value) return value end", "(value) => value"))
	equal(echo.call([foreign]), null, "foreign function argument rejected")
	error_contains("another PuertsEnvironment")
	other.dispose()
	equal(native(evaluate("return number", "number")), 11, "first environment survives second disposal")
	return true


func test_callable_arguments_and_result_conversion() -> bool:
	var sum: Callable = native(evaluate(
		"return to_callable(function(...) local n = 0; for i = 1, select('#', ...) do n = n + select(i, ...) end; return n end)",
		"to_callable((...args) => args.reduce((sum, value) => sum + value, 0))"
	))
	equal(sum.callv([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]), 55, "callable arguments exceed inline capacity")
	var empty: Callable = native(evaluate("return to_callable(function() end)", "to_callable(() => {})"))
	equal(empty.call(), null, "callable with no result succeeds")
	var factory: Callable = native(evaluate("return to_callable(function() return {answer = 42} end)", "to_callable(() => ({answer: 42}))"))
	var result = factory.call()
	check(result != null and result.is_valid(), "callable retains script object result")
	env.set_global("result", result)
	equal(native(evaluate("return result.answer", "result.answer")), 42, "callable script object roundtrip")
	var vector: Callable = native(evaluate("return to_callable(function() return load_type('Vector2')(3, 4) end)", "to_callable(() => new (load_type('Vector2'))(3, 4))"))
	equal(vector.call(), Vector2(3, 4), "callable unwraps native result")
	return true
