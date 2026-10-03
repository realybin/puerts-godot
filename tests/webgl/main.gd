# SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
# SPDX-License-Identifier: BSD-3-Clause

extends Control

var env: Object
var backend: Object
var pool: Object
var errors: Array[String] = []
var passed := 0
var failed := 0
var native_calls := 0
var output: Array[String] = []


func _ready() -> void:
	if not OS.has_feature("web"):
		check(false, "run this project as a Web export with Extensions Support")
		finish()
		return
	for name in ["PuertsEnvironment", "PuertsStringNameCachePool", "PuertsWebglBackend"]:
		if not check(ClassDB.class_exists(name), "extension class loaded: " + name):
			finish()
			return
	pool = ClassDB.instantiate("PuertsStringNameCachePool")
	backend = ClassDB.instantiate("PuertsWebglBackend")
	env = ClassDB.instantiate("PuertsEnvironment")
	env.set_error_callback(func(message: String): errors.append(message))
	if not equal(pool.initialize(), OK, "initialize StringName cache"):
		finish()
		return
	if not equal(env.initialize(backend, pool), OK, "initialize browser environment"):
		finish()
		return
	test_values()
	test_bindings_and_callbacks()
	test_errors()
	test_single_environment_lifetime()
	finish()


func test_values() -> void:
	var number: Variant = evaluate("7.5")
	if number != null:
		equal(number.to_float(), 7.5, "JavaScript Number")
	var bigint: Variant = evaluate("9007199254740993n")
	if bigint != null:
		equal(bigint.to_int(), 9007199254740993, "BigInt beyond Number precision")
	var text: Variant = evaluate("'Puerts-你好-😀'")
	if text != null:
		equal(text.to_string(), "Puerts-你好-😀", "UTF-8 string")
	var changing_text: Variant = evaluate("globalThis.webglCoercions = 0; ({toString() { return ++globalThis.webglCoercions === 1 ? 'snapshot-你好' : 'a much longer replacement'; }})")
	if changing_text != null:
		equal(changing_text.to_string(), "snapshot-你好", "string conversion uses one snapshot")
		var coercions: Variant = evaluate("globalThis.webglCoercions")
		if coercions != null:
			equal(coercions.to_int(), 1, "string conversion invokes toString once")
	var dom: Variant = evaluate("typeof document === 'object'")
	if dom != null:
		equal(dom.to_bool(), true, "browser main thread has DOM access")


func test_bindings_and_callbacks() -> void:
	var vector: Variant = evaluate("new (load_type('Vector2'))(3, 4)")
	if vector != null:
		equal(vector.unwrap_native(), Vector2(3, 4), "Vector2 static binding constructor")
		var length: Variant = vector.call_method("length")
		if check(length != null, "Vector2 method returns a value"):
			equal(length.to_float(), 5.0, "Vector2 static binding method")
	env.set_global("native_callback", Callable(self, "native_add"))
	var callback_result: Variant = evaluate("native_callback.call(19, 23)")
	if callback_result != null:
		equal(callback_result.to_native(), 42, "JavaScript invokes native callback")
	equal(native_calls, 1, "native callback invoked once")
	var function: Variant = evaluate("to_callable((left, right) => left + right)")
	if function != null:
		var script_callable: Variant = function.to_native()
		if check(typeof(script_callable) == TYPE_CALLABLE, "JavaScript function becomes Callable"):
			equal(script_callable.call(20, 22), 42, "native Callable invokes JavaScript")


func test_errors() -> void:
	var count := errors.size()
	equal(env.eval("throw new Error('webgl-smoke-exception')"), null, "exception fails evaluation")
	check(errors.size() > count and "webgl-smoke-exception" in errors[-1], "exception reaches error callback")
	var recovered: Variant = evaluate("6 * 7")
	if recovered != null:
		equal(recovered.to_native(), 42, "evaluation recovers after exception")
	equal(env.eval("globalThis.webglStackReads = 0; throw {get stack() { return ++globalThis.webglStackReads === 1 ? 'webgl-stack-snapshot' : 'a much longer replacement stack'; }}"), null, "exception with changing stack getter fails evaluation")
	check(errors[-1] == "webgl-stack-snapshot", "exception formatting preserves its first stack snapshot")
	var stack_reads: Variant = evaluate("globalThis.webglStackReads")
	if stack_reads != null:
		equal(stack_reads.to_int(), 1, "exception formatting reads stack once")


func test_single_environment_lifetime() -> void:
	var held: Variant = evaluate("({answer: 42})")
	if held == null:
		return
	var function: Variant = evaluate("to_callable(() => 42)")
	var held_callable: Variant = function.to_native() if function != null else Callable()
	var second: Object = ClassDB.instantiate("PuertsEnvironment")
	var second_errors: Array[String] = []
	second.set_error_callback(func(message: String): second_errors.append(message))
	equal(second.initialize(backend, pool), ERR_CANT_CREATE, "second environment initialization fails")
	check(not second.is_alive(), "second environment stays inactive")
	check(not second_errors.is_empty(), "second initialization reports its error")
	check(env.is_alive() and held.is_valid(), "first environment and retained value survive second initialization")
	var count := errors.size()
	env.dispose()
	check(errors.size() > count and "disposal" in errors[-1], "disposal reports unsupported operation")
	check(env.is_alive() and held.is_valid(), "disposal preserves browser environment and retained values")
	var answer: Variant = held.get_property("answer")
	if check(answer != null, "retained object remains accessible after disposal attempt"):
		equal(answer.to_native(), 42, "retained object keeps its value")
	equal(env.initialize(backend, pool), ERR_ALREADY_IN_USE, "same environment reinitialization fails")
	check(env.is_alive() and held.is_valid(), "reinitialization preserves the original environment")
	var final_value: Variant = evaluate("40 + 2")
	if final_value != null:
		equal(final_value.to_native(), 42, "original environment stays usable")
	var reentrant_calls := [0]
	var reentrant_result := [OK]
	env.set_error_callback(func(message: String):
		errors.append(message)
		reentrant_calls[0] += 1
		env.dispose()
		reentrant_result[0] = env.initialize(backend, pool)
	)
	env.dispose()
	env.set_error_callback(func(message: String): errors.append(message))
	equal(reentrant_calls[0], 1, "recursive lifecycle rejection callback runs once")
	equal(reentrant_result[0], ERR_ALREADY_IN_USE, "reinitialization from error callback is rejected")
	check(env.is_alive() and held.is_valid(), "recursive rejection preserves environment and value")
	if check(typeof(held_callable) == TYPE_CALLABLE and held_callable.is_valid(), "retained Callable survives lifecycle rejections"):
		equal(held_callable.call(), 42, "retained Callable stays usable")
	var weak_environment: WeakRef = weakref(env)
	env = null
	check(weak_environment.get_ref() != null, "page lifetime keeps environment alive without user references")
	env = weak_environment.get_ref()
	# The browser host environment must remain alive until the page unloads.


func native_add(left: int, right: int) -> int:
	native_calls += 1
	return left + right


func evaluate(source: String) -> Variant:
	var value: Variant = env.eval(source, "webgl-smoke.js")
	check(value != null and value.is_valid(), "evaluate: " + source)
	return value


func check(condition: bool, message: String) -> bool:
	if condition:
		passed += 1
	else:
		failed += 1
	var line := "[webgl-smoke] %s %s" % ["PASS" if condition else "FAIL", message]
	output.append(line)
	print(line)
	return condition


func equal(actual: Variant, expected: Variant, message: String) -> bool:
	return check(actual == expected, "%s (expected=%s, actual=%s)" % [message, str(expected), str(actual)])


func finish() -> void:
	var summary := "[webgl-smoke] summary passed=%d failed=%d" % [passed, failed]
	output.append(summary)
	print(summary)
	$Results.text = "\n".join(output)
	if OS.has_feature("web"):
		JavaScriptBridge.eval("globalThis.puertsWebglSmoke = %s" % JSON.stringify({
			"complete": true,
			"threads": OS.has_feature("threads"),
			"passed": passed,
			"failed": failed,
			"output": output,
		}), true)

