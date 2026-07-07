# SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
# SPDX-License-Identifier: BSD-3-Clause

extends "res://tests/support/minitest_case.gd"

const BackendCatalog = preload("res://tests/support/backend_catalog.gd")

var _available_backends: Array = []

func before_all() -> void:
	_available_backends = BackendCatalog.collect_available_backends()

func before_each() -> void:
	if _available_backends.is_empty():
		_available_backends = BackendCatalog.collect_available_backends()

func after_all() -> void:
	_available_backends.clear()

func backend_parameters() -> Array:
	if _available_backends.is_empty():
		_available_backends = BackendCatalog.collect_available_backends()
	return _available_backends

func case_parameters(case_names: Array) -> Array:
	var parameters: Array = []
	for entry in backend_parameters():
		for case_name in case_names:
			parameters.append({
				"entry": entry,
				"case_name": case_name,
			})
	return parameters

func assert_suite_result(label: String, result: Variant) -> void:
	if result is Dictionary and bool(result.get("skip", false)):
		pending("%s -> skipped: %s" % [label, str(result.get("message", ""))])
	elif typeof(result) == TYPE_STRING:
		assert_eq(String(result), "", label)
	else:
		fail_test("%s returned invalid type=%s value=%s" % [label, type_string(typeof(result)), str(result)])
