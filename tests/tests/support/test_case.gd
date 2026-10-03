# SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
# SPDX-License-Identifier: BSD-3-Clause

extends RefCounted

var backend_name: String
var backend: Object
var env: Object
var backend_names: Array[String]
var failures: Array[String] = []
var skip_reason := ""
var errors: Array[String] = []
var environments: Array[Object] = []


func per_backend() -> bool:
	return true


func start(name: String, names: Array[String]) -> bool:
	backend_name = name
	backend_names = names
	backend = ClassDB.instantiate(backend_class(name))
	env = new_environment(backend)
	return env != null


func finish() -> void:
	for environment in environments:
		environment.dispose()
	environments.clear()
	env = null
	backend = null


static func backend_class(name: String) -> String:
	return {
		"quickjs": "PuertsQuickjsBackend",
		"v8": "PuertsV8Backend",
		"nodejs": "PuertsNodejsBackend",
		"lua": "PuertsLuaBackend",
	}.get(name, "")


func new_environment(engine: Object) -> Object:
	var environment: Object = ClassDB.instantiate("PuertsEnvironment")
	environment.set_error_callback(func(message: String): errors.append(message))
	var pool: Object = ClassDB.instantiate("PuertsStringNameCachePool")
	pool.initialize(0, 512)
	if not equal(environment.initialize(engine, pool), OK, "initialize environment"):
		return null
	environments.append(environment)
	return environment


func code(lua: String, javascript: String) -> String:
	return lua if backend_name == "lua" else javascript


func evaluate(lua: String, javascript: String) -> Variant:
	return env.eval(code(lua, javascript))


func native(value: Variant) -> Variant:
	return null if value == null else value.to_native()


func check(condition: bool, message: String) -> bool:
	if not condition:
		failures.append(message)
	return condition


func equal(actual: Variant, expected: Variant, message: String) -> bool:
	return check(actual == expected, "%s: expected=%s actual=%s" % [message, str(expected), str(actual)])


func error_contains(text: String) -> bool:
	return check(not errors.is_empty() and text in errors[-1], "expected error containing '%s': %s" % [text, errors])


func skip(reason: String) -> bool:
	skip_reason = reason
	return true


func collect_garbage() -> void:
	for _attempt in range(8):
		if backend_name == "lua":
			env.eval("collectgarbage('collect')")
		else:
			env.low_memory_notification()
			if backend_name in ["v8", "nodejs"]:
				env.tick()


func released(reference: WeakRef) -> bool:
	for _attempt in range(16):
		collect_garbage()
		if reference.get_ref() == null:
			return true
	return false
