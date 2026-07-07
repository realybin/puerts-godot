# SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
# SPDX-License-Identifier: BSD-3-Clause

extends "res://tests/support/backend_test_case.gd"

const DebuggerSuite = preload("res://tests/suites/debugger_suite.gd")

const DEBUGGER_CASES = [
	"open_accepts_tcp_connections",
	"close_stops_accepting_tcp_connections",
]


func test_debugger_suite_cases() -> Array:
	return case_parameters(DEBUGGER_CASES)


func test_debugger_suite(case_data: Dictionary) -> void:
	var entry: Dictionary = case_data["entry"]
	var case_name: String = case_data["case_name"]
	var backend_info: Dictionary = entry["info"]
	var backend: Object = entry["backend"]
	var backend_name: String = backend_info["name"]
	var label = "%s/debugger/%s" % [backend_name, case_name]
	var result: Variant

	match case_name:
		"open_accepts_tcp_connections":
			result = DebuggerSuite.run_debugger_open_suite(backend, backend_info)
		"close_stops_accepting_tcp_connections":
			result = DebuggerSuite.run_debugger_close_suite(backend, backend_info)
		_:
			fail_test("unknown debugger case: %s" % case_name)
			return

	assert_suite_result(label, result)

