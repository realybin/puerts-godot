# SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
# SPDX-License-Identifier: Apache-2.0

extends Control

var env: Object
var backend: Object
var pool: Object
var errors: Array[String] = []
var passed := 0
var failed := 0
var output: Array[String] = []


func _ready() -> void:
	if not OS.has_feature("web"):
		check(false, "run this project as a Web export with Extensions Support")
		finish()
		return
	for name in ["PuertsEnvironment", "PuertsStringNameUtf8Cache", "PuertsWebBackend"]:
		if not check(ClassDB.class_exists(name), "extension class loaded: " + name):
			finish()
			return
	pool = ClassDB.instantiate("PuertsStringNameUtf8Cache")
	backend = ClassDB.instantiate("PuertsWebBackend")
	env = ClassDB.instantiate("PuertsEnvironment")
	env.set_log_callback(func(_category: int, _level: int, message: String): errors.append(message))
	if not equal(env.initialize(backend, pool), OK, "initialize browser environment"):
		finish()
		return
	test_browser_calls()
	test_single_environment_lifetime()
	finish()


func test_browser_calls() -> void:
	var dom: Variant = evaluate("typeof document === 'object'")
	if dom != null:
		equal(dom.to_bool(), true, "browser main thread has DOM access")
	env.set_global("native_callback", Callable(self, "native_length"))
	var callback_result: Variant = evaluate("native_callback.call(new (load_type('Vector2'))(3, 4))")
	if callback_result != null:
		equal(callback_result.to_native(), 5.0, "JavaScript passes a bound Vector2 to Godot")
	var function: Variant = evaluate("to_callable((left, right) => left + right)")
	if function != null:
		var script_callable: Variant = function.to_native()
		if check(typeof(script_callable) == TYPE_CALLABLE, "JavaScript function becomes Callable"):
			equal(script_callable.call(20, 22), 42, "native Callable invokes JavaScript")


func test_single_environment_lifetime() -> void:
	var held: Variant = evaluate("({answer: 42})")
	if held == null:
		return
	var function: Variant = evaluate("to_callable(() => 42)")
	var held_callable: Variant = function.to_native() if function != null else Callable()
	var second: Object = ClassDB.instantiate("PuertsEnvironment")
	var second_errors: Array[String] = []
	second.set_log_callback(func(_category: int, _level: int, message: String): second_errors.append(message))
	equal(second.initialize(backend, pool), ERR_CANT_CREATE, "second environment initialization fails")
	check(not second.is_ready(), "second environment stays inactive")
	check(not second_errors.is_empty(), "second initialization reports its error")
	check(env.is_ready() and held.is_valid(), "first environment and retained value survive second initialization")
	var count := errors.size()
	env.dispose()
	check(errors.size() > count and "disposal" in errors[-1], "disposal reports unsupported operation")
	equal(env.initialize(backend, pool), ERR_ALREADY_IN_USE, "same environment reinitialization fails")
	var reentrant_calls := [0]
	var reentrant_result := [OK]
	env.set_log_callback(func(_category: int, _level: int, _message: String):
		reentrant_calls[0] += 1
		env.dispose()
		reentrant_result[0] = env.initialize(backend, pool)
	)
	env.dispose()
	env.set_log_callback(func(_category: int, _level: int, message: String): errors.append(message))
	equal(reentrant_calls[0], 1, "recursive lifecycle rejection callback runs once")
	equal(reentrant_result[0], ERR_ALREADY_IN_USE, "reinitialization from error callback is rejected")
	check(env.is_ready() and held.is_valid(), "recursive rejection preserves environment and value")
	var answer: Variant = held.get_property("answer")
	if check(answer != null, "retained object remains accessible"):
		equal(answer.to_native(), 42, "retained object keeps its value")
	if check(typeof(held_callable) == TYPE_CALLABLE and held_callable.is_valid(), "retained Callable survives lifecycle rejections"):
		equal(held_callable.call(), 42, "retained Callable stays usable")
	var weak_environment: WeakRef = weakref(env)
	env = null
	check(weak_environment.get_ref() != null, "page lifetime keeps environment alive without user references")
	env = weak_environment.get_ref()
	# The browser host environment must remain alive until the page unloads.


func native_length(vector: Vector2) -> float:
	return vector.length()


func evaluate(source: String) -> Variant:
	var value: Variant = env.eval(source, "web-smoke.js")
	check(value != null and value.is_valid(), "evaluate: " + source)
	return value


func check(condition: bool, message: String) -> bool:
	if condition:
		passed += 1
	else:
		failed += 1
	var line := "[web-smoke] %s %s" % ["PASS" if condition else "FAIL", message]
	output.append(line)
	print(line)
	return condition


func equal(actual: Variant, expected: Variant, message: String) -> bool:
	return check(actual == expected, "%s (expected=%s, actual=%s)" % [message, str(expected), str(actual)])


func finish() -> void:
	var summary := "[web-smoke] summary passed=%d failed=%d" % [passed, failed]
	output.append(summary)
	print(summary)
	$Results.text = "\n".join(output)
	if OS.has_feature("web"):
		JavaScriptBridge.eval("globalThis.puertsWebSmoke = %s" % JSON.stringify({
			"complete": true,
			"threads": OS.has_feature("threads"),
			"passed": passed,
			"failed": failed,
			"output": output,
		}), true)

