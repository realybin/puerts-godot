# SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
# SPDX-License-Identifier: BSD-3-Clause

extends RefCounted
static var _runtime_script_cache: Dictionary = {}

const BACKENDS := [
	{
		"name": "quickjs",
		"class_name": "PuertsQuickjsBackend",
		"language": "ecmascript",
	},
	{
		"name": "v8",
		"class_name": "PuertsV8Backend",
		"language": "ecmascript",
	},
	{
		"name": "nodejs",
		"class_name": "PuertsNodejsBackend",
		"language": "ecmascript",
	},
	{
		"name": "lua",
		"class_name": "PuertsLuaBackend",
		"language": "lua",
	},
]

const SUPPORTED_BACKENDS_BY_PLATFORM := {
	"windows": ["quickjs", "v8", "nodejs", "lua"],
	"macos": ["quickjs", "v8", "nodejs", "lua"],
	"linux": ["quickjs", "v8", "nodejs", "lua"],
	"android": ["quickjs", "v8", "nodejs", "lua"],
	"ios": ["quickjs", "v8", "nodejs", "lua"],
	"web": ["quickjs", "lua"],
}


static func _create_log_sink() -> Dictionary:
	return {
		"error": [],
		"warn": [],
		"info": [],
	}


static func _attach_log_sink(env: Object, sink: Dictionary) -> void:
	env.set_error_callback(func(message: String):
		sink["error"].append(message)
	)
	env.set_warn_callback(func(message: String):
		sink["warn"].append(message)
	)
	env.set_info_callback(func(message: String):
		sink["info"].append(message)
	)


static func env_log_sink(env: Object) -> Dictionary:
	if env == null or not env.has_meta("__puerts_log_sink"):
		return {}
	return env.get_meta("__puerts_log_sink")


static func env_last_error(env: Object) -> String:
	var sink := env_log_sink(env)
	if sink.is_empty():
		return ""
	var errors: Array = sink.get("error", [])
	if errors.is_empty():
		return ""
	return str(errors[errors.size() - 1])


static func env_error_count(env: Object) -> int:
	var sink := env_log_sink(env)
	if sink.is_empty():
		return 0
	return int((sink.get("error", []) as Array).size())


static func normalized_backend_name(name: String) -> String:
	return name.strip_edges().to_lower()


static func requested_backend_names() -> Dictionary:
	var raw = OS.get_environment("PUERTS_TEST_BACKENDS")
	if raw == null:
		raw = ""
	var text: String = str(raw).strip_edges()
	if text.is_empty():
		return {}

	var allowed := {}
	for token in text.split(",", false):
		var key = normalized_backend_name(token)
		if not key.is_empty():
			allowed[key] = true
	return allowed


static func iter_backends() -> Array:
	var requested = requested_backend_names()
	if requested.is_empty():
		return BACKENDS

	var filtered: Array = []
	for backend_info in BACKENDS:
		var backend_name = normalized_backend_name(str(backend_info.get("name", "")))
		if requested.has(backend_name):
			filtered.append(backend_info)
	return filtered


static func current_platform_name() -> String:
	match OS.get_name():
		"Windows":
			return "windows"
		"macOS":
			return "macos"
		"Linux":
			return "linux"
		"Android":
			return "android"
		"iOS":
			return "ios"
		"Web":
			return "web"
		_:
			return ""


static func backend_supported_on_current_platform(backend_name: String) -> bool:
	backend_name = normalized_backend_name(backend_name)
	var platform_name := current_platform_name()
	if platform_name.is_empty():
		return true
	if not SUPPORTED_BACKENDS_BY_PLATFORM.has(platform_name):
		return true
	return backend_name in SUPPORTED_BACKENDS_BY_PLATFORM[platform_name]


static func script_for(backend_info: Dictionary, lua_code: String, ecmascript_code: String) -> String:
	if backend_info["language"] == "lua":
		return lua_code
	return ecmascript_code


static func runtime_language_extension(backend_info: Dictionary) -> String:
	if backend_info["language"] == "lua":
		return "lua"
	return "js"


static func runtime_bundle_path(backend_info: Dictionary, bundle_name: String) -> String:
	return "res://tests/runtime_cases/%s.%s" % [bundle_name, runtime_language_extension(backend_info)]


static func _read_runtime_script(script_path: String) -> Dictionary:
	if _runtime_script_cache.has(script_path):
		return {"ok": true, "text": _runtime_script_cache[script_path]}
	if not FileAccess.file_exists(script_path):
		return {"ok": false, "error": "runtime script not found: %s" % script_path}

	var script_file := FileAccess.open(script_path, FileAccess.READ)
	if script_file == null:
		return {"ok": false, "error": "failed to open runtime script: %s" % script_path}

	var script_text := script_file.get_as_text()
	_runtime_script_cache[script_path] = script_text
	return {"ok": true, "text": script_text}


static func ensure_runtime_bundle_loaded(env: Object, backend_info: Dictionary, bundle_name: String) -> String:
	var script_path := runtime_bundle_path(backend_info, bundle_name)
	var loaded_meta_key := "__runtime_bundle_loaded__%d" % script_path.hash()
	if env.has_meta(loaded_meta_key):
		return ""

	var read_result := _read_runtime_script(script_path)
	if not bool(read_result["ok"]):
		return str(read_result["error"])

	var eval_result = env.eval(str(read_result["text"]))
	if eval_result == null:
		var last_error := env_last_error(env)
		if not last_error.is_empty():
			return "runtime bundle eval failed path=%s error=%s" % [script_path, last_error]

	env.set_meta(loaded_meta_key, true)
	return ""

static func create_cache_pool():
	if not ClassDB.class_exists("PuertsStringNameCachePool"):
		return null
	var pool: Variant = ClassDB.instantiate("PuertsStringNameCachePool")
	if pool == null:
		return null
	# POLICY_HASH_MAP enum value.
	pool.initialize(0, 512)
	return pool


static func create_environment(backend: Object, backend_name: String) -> Dictionary:
	if not ClassDB.class_exists("PuertsEnvironment"):
		return {
			"ok": false,
			"error": "%s environment class not found" % backend_name,
		}
	var env: Variant = ClassDB.instantiate("PuertsEnvironment")
	if env == null:
		return {
			"ok": false,
			"error": "%s environment instantiate failed" % backend_name,
		}
	var sink := _create_log_sink()
	_attach_log_sink(env, sink)
	env.set_meta("__puerts_log_sink", sink)
	var init_error: int = int(env.initialize(backend, create_cache_pool()))
	if init_error != OK:
		return {
			"ok": false,
			"error": "%s initialize failed: %s" % [backend_name, env_last_error(env)],
		}
	return {
		"ok": true,
		"env": env,
	}


static func skip(reason: String) -> Dictionary:
	return {
		"skip": true,
		"message": reason,
	}


static func unwrap_script_value(value):
	if value != null and value is Object and value.has_method("to_native"):
		return value.to_native()
	return value


static func create_backend(backend_class: String) -> Object:
	if not ClassDB.class_exists(backend_class):
		return null
	return ClassDB.instantiate(backend_class)


static func create_lifecycle_value(env: Object, language: String):
	if language == "lua":
		return env.eval("return { marker = 'alive', nested = { value = 7 }, add = function(a, b) return a + b end }")
	return env.eval("({ marker: 'alive', nested: { value: 7 }, add(a, b) { return a + b; } })")
