# SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
# SPDX-License-Identifier: BSD-3-Clause

extends Control

const TestCase = preload("res://tests/support/test_case.gd")

var passed := 0
var failed := 0
var skipped := 0


func _ready() -> void:
	if not ClassDB.class_exists("PuertsEnvironment") or not ClassDB.class_exists("PuertsStringNameCachePool"):
		record_failure("bootstrap", "PuertsCore extension is not loaded")
	else:
		run_tests()
	print("[mini-test] summary total=%d passed=%d failed=%d skipped=%d" % [
		passed + failed + skipped, passed, failed, skipped,
	])
	get_tree().quit(1 if failed > 0 else 0)


func run_tests() -> void:
	var names: Array[String] = []
	var requested := OS.get_environment("PUERTS_TEST_BACKENDS")
	for name in (requested.split(",", false) if not requested.is_empty() else ["quickjs", "v8", "lua"]):
		if ClassDB.class_exists(TestCase.backend_class(name)):
			names.append(name)
		elif not requested.is_empty():
			record_failure("bootstrap", "requested backend is not loaded: " + name)
	if names.is_empty():
		record_failure("bootstrap", "no available backends")
		return

	var files := DirAccess.get_files_at("res://tests")
	files.sort()
	for file in files:
		if not file.begins_with("test_") or not file.ends_with(".gd"):
			continue
		var suite = load("res://tests/" + file)
		if suite == null or not suite.can_instantiate():
			record_failure(file, "test script cannot be instantiated")
			continue
		var prototype: Variant = suite.new()
		if not prototype is TestCase:
			record_failure(file, "test script must extend test_case.gd")
			continue
		var methods: Array[String] = []
		for method in prototype.get_method_list():
			if String(method.name).begins_with("test_"):
				methods.append(method.name)
		methods.sort()
		if methods.is_empty():
			record_failure(file, "no test methods")
		for name in (names if prototype.per_backend() else [names[0]]):
			for method in methods:
				var test: TestCase = suite.new()
				var label := "%s/%s/%s" % [name if prototype.per_backend() else "common", file, method]
				var completed: Variant = false
				if test.start(name, names):
					completed = test.call(method)
				test.finish()
				if completed != true or not test.failures.is_empty():
					record_failure(label, "; ".join(test.failures) if not test.failures.is_empty() else "test did not complete")
				elif not test.skip_reason.is_empty():
					skipped += 1
					print("[mini-test] SKIP %s: %s" % [label, test.skip_reason])
				else:
					passed += 1
					print("[mini-test] PASS " + label)


func record_failure(label: String, message: String) -> void:
	failed += 1
	print("[mini-test] FAIL %s: %s" % [label, message])
