# SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
# SPDX-License-Identifier: BSD-3-Clause

extends Control

const TEST_ROOT := "res://tests"
const TEST_PREFIX := "test_"
const TEST_SUFFIX := ".gd"
const REQUIRED_TYPES := [
	"PuertsEnvironment",
	"PuertsStringNameCachePool",
]
const EXTENSION_CONFIGS := [
	"res://puerts_core.gdextension",
	"res://puerts_quickjs.gdextension",
	"res://puerts_v8.gdextension",
	"res://puerts_nodejs.gdextension",
	"res://puerts_lua.gdextension",
]
const EXTENSION_CONFIGS_BY_BACKEND := {
	"quickjs": "res://puerts_quickjs.gdextension",
	"v8": "res://puerts_v8.gdextension",
	"nodejs": "res://puerts_nodejs.gdextension",
	"lua": "res://puerts_lua.gdextension",
}
const BACKEND_ALIAS := {
	"nodejs": "v8",
}
const TYPE_WAIT_TIMEOUT_MS := 5000
const TYPE_WAIT_POLL_MS := 50
const PREFLIGHT_COMPILE_RETRY_COUNT := 30
const PREFLIGHT_COMPILE_RETRY_MS := 100

const BaseCase = preload("res://tests/support/minitest_case.gd")


func _ready() -> void:
	var ready := await _ensure_runtime_types_ready()
	if not ready:
		print("[mini-test] FAIL bootstrap :: required extension types were not registered in time")
		get_tree().quit(1)
		return
	if not await _preflight_compile():
		get_tree().quit(1)
		return

	var script_paths: Array[String] = []
	_collect_test_scripts(TEST_ROOT, script_paths)
	script_paths.sort()

	var total_cases := 0
	var passed_cases := 0
	var failed_cases := 0
	var skipped_cases := 0

	print("[mini-test] discovered scripts=%d" % script_paths.size())

	for script_path in script_paths:
		var script_resource = load(script_path)
		if script_resource == null:
			failed_cases += 1
			total_cases += 1
			print("[mini-test] FAIL %s :: load failed" % script_path)
			continue
		if script_resource is Script and not script_resource.can_instantiate():
			failed_cases += 1
			total_cases += 1
			print("[mini-test] FAIL %s :: script loaded but cannot instantiate (compile failed)" % script_path)
			continue

		var test_object: Variant = script_resource.new()
		if not (test_object is BaseCase):
			failed_cases += 1
			total_cases += 1
			print("[mini-test] FAIL %s :: must extend minitest_case.gd" % script_path)
			continue

		var test_case: BaseCase = test_object
		if test_case.has_method("before_all"):
			test_case.call("before_all")

		var test_methods := _collect_test_methods(test_case)
		for method_name in test_methods:
			var case_entries := _resolve_case_entries(test_case, method_name)
			for case_index in range(case_entries.size()):
				var case_payload = case_entries[case_index]
				var case_label := _format_case_label(method_name, case_payload, case_index)

				if test_case.has_method("before_each"):
					test_case.call("before_each")

				test_case._mt_begin_case()
				_invoke_test_method(test_case, method_name, case_payload)
				var case_result := test_case._mt_end_case()

				total_cases += 1
				if bool(case_result["skipped"]):
					skipped_cases += 1
					print("[mini-test] SKIP %s :: %s" % [script_path, str(case_result["skip_message"])])
				elif bool(case_result["failed"]):
					failed_cases += 1
					var failures: Array = case_result["failures"]
					if failures.is_empty():
						print("[mini-test] FAIL %s :: %s" % [script_path, case_label])
					else:
						print("[mini-test] FAIL %s :: %s -> %s" % [script_path, case_label, str(failures[0])])
				else:
					passed_cases += 1
					print("[mini-test] PASS %s :: %s" % [script_path, case_label])

				if test_case.has_method("after_each"):
					test_case.call("after_each")

		if test_case.has_method("after_all"):
			test_case.call("after_all")

	print("[mini-test] summary total=%d passed=%d failed=%d skipped=%d" % [
		total_cases,
		passed_cases,
		failed_cases,
		skipped_cases,
	])

	get_tree().quit(1 if failed_cases > 0 else 0)


func _ensure_runtime_types_ready() -> bool:
	print("[mini-test] bootstrap :: initial type snapshot => %s" % _type_snapshot_text())
	if _has_required_types():
		return true

	if Engine.has_singleton("GDExtensionManager"):
		var extension_manager = Engine.get_singleton("GDExtensionManager")
		for extension_path in _extension_configs_for_run():
			if not FileAccess.file_exists(extension_path):
				print("[mini-test] bootstrap :: extension missing path=%s" % extension_path)
				continue
			var load_error: int = extension_manager.load_extension(extension_path)
			print("[mini-test] bootstrap :: load_extension path=%s result=%d (%s)" % [
				extension_path,
				load_error,
				_error_code_text(load_error),
			])
			print("[mini-test] bootstrap :: post-load type snapshot => %s" % _type_snapshot_text())
	else:
		print("[mini-test] WARN bootstrap :: GDExtensionManager singleton not available")

	var deadline := Time.get_ticks_msec() + TYPE_WAIT_TIMEOUT_MS
	while Time.get_ticks_msec() <= deadline:
		if _has_required_types():
			print("[mini-test] bootstrap :: required types ready => %s" % _type_snapshot_text())
			return true
		await get_tree().create_timer(float(TYPE_WAIT_POLL_MS) / 1000.0).timeout
	print("[mini-test] bootstrap :: timeout type snapshot => %s" % _type_snapshot_text())
	return _has_required_types()


func _extension_configs_for_run() -> Array[String]:
	var raw = OS.get_environment("PUERTS_TEST_BACKENDS")
	var requested_text: String = str(raw).strip_edges().to_lower()
	if requested_text.is_empty():
		return EXTENSION_CONFIGS

	var selected: Array[String] = ["res://puerts_core.gdextension"]
	var requested := {}
	for token in requested_text.split(",", false):
		var key = token.strip_edges().to_lower()
		if not key.is_empty():
			if BACKEND_ALIAS.has(key):
				key = String(BACKEND_ALIAS[key])
			requested[key] = true
	for backend_name in requested.keys():
		if EXTENSION_CONFIGS_BY_BACKEND.has(backend_name):
			selected.append(String(EXTENSION_CONFIGS_BY_BACKEND[backend_name]))
	return selected


func _has_required_types() -> bool:
	for required_type in REQUIRED_TYPES:
		if not ClassDB.class_exists(required_type):
			return false
	return true


func _type_snapshot_text() -> String:
	var items: Array[String] = []
	for required_type in REQUIRED_TYPES:
		items.append("%s=%s" % [required_type, str(ClassDB.class_exists(required_type))])
	return ", ".join(items)


func _preflight_compile() -> bool:
	for attempt in range(PREFLIGHT_COMPILE_RETRY_COUNT):
		var support_script = load("res://tests/support/puerts_support.gd")
		if support_script != null and (not (support_script is Script) or support_script.can_instantiate()):
			print("[mini-test] preflight :: support script load ok (attempt %d/%d)" % [attempt + 1, PREFLIGHT_COMPILE_RETRY_COUNT])
			return true

		if attempt == PREFLIGHT_COMPILE_RETRY_COUNT - 1:
			break

		print("[mini-test] WARN preflight :: support script compile not ready (attempt %d/%d), retrying..." % [attempt + 1, PREFLIGHT_COMPILE_RETRY_COUNT])
		await get_tree().create_timer(float(PREFLIGHT_COMPILE_RETRY_MS) / 1000.0).timeout

	print("[mini-test] FAIL preflight :: failed to compile/load res://tests/support/puerts_support.gd after %d attempts" % PREFLIGHT_COMPILE_RETRY_COUNT)
	return false


func _error_code_text(code: int) -> String:
	match code:
		OK:
			return "OK"
		ERR_ALREADY_EXISTS:
			return "ERR_ALREADY_EXISTS"
		ERR_CANT_OPEN:
			return "ERR_CANT_OPEN"
		ERR_CANT_CREATE:
			return "ERR_CANT_CREATE"
		ERR_FILE_NOT_FOUND:
			return "ERR_FILE_NOT_FOUND"
		ERR_UNAVAILABLE:
			return "ERR_UNAVAILABLE"
		ERR_PARSE_ERROR:
			return "ERR_PARSE_ERROR"
		_:
			return "UNKNOWN"


func _collect_test_scripts(dir_path: String, output: Array[String]) -> void:
	var directory := DirAccess.open(dir_path)
	if directory == null:
		return

	directory.list_dir_begin()
	while true:
		var entry_name := directory.get_next()
		if entry_name.is_empty():
			break
		if entry_name == "." or entry_name == "..":
			continue

		var entry_path := "%s/%s" % [dir_path, entry_name]
		if directory.current_is_dir():
			_collect_test_scripts(entry_path, output)
			continue

		if entry_name.begins_with(TEST_PREFIX) and entry_name.ends_with(TEST_SUFFIX):
			output.append(entry_path)
	directory.list_dir_end()


func _collect_test_methods(test_case: BaseCase) -> Array[String]:
	var method_names: Array[String] = []
	for method_info in test_case.get_method_list():
		var method_name: String = method_info["name"]
		if method_name.begins_with(TEST_PREFIX) and not method_name.ends_with("_cases"):
			method_names.append(method_name)
	method_names.sort()
	return method_names


func _resolve_case_entries(test_case: BaseCase, method_name: String) -> Array:
	var cases_method := "%s_cases" % method_name
	if test_case.has_method(cases_method):
		var resolved = test_case.call(cases_method)
		if resolved is Array:
			return resolved
		return [resolved]
	return [null]


func _invoke_test_method(test_case: BaseCase, method_name: String, case_payload: Variant) -> void:
	if case_payload == null:
		test_case.call(method_name)
		return
	test_case.call(method_name, case_payload)


func _format_case_label(method_name: String, case_payload: Variant, index: int) -> String:
	if case_payload is Dictionary:
		var payload_dict: Dictionary = case_payload
		if payload_dict.has("label"):
			return "%s[%s]" % [method_name, str(payload_dict["label"])]
		if payload_dict.has("case_name"):
			return "%s[%s]" % [method_name, str(payload_dict["case_name"])]
	return "%s[#%d]" % [method_name, index]
