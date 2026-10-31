# SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
# SPDX-License-Identifier: BSD-3-Clause

extends "res://tests/support/test_case.gd"


func test_levels_and_category_overrides() -> bool:
	var logs: Array = []
	env.set_log_callback(func(category: int, level: int, message: String):
		logs.append([category, level, message])
	)
	evaluate("PLogger.debug(1, 'hidden'); PLogger.info(1, 'visible')", "PLogger.debug(1, 'hidden'); PLogger.info(1, 'visible')")
	equal(logs, [[1, 2, "visible"]], "default minimum is INFO")
	logs.clear()
	env.set_category_log_level(1, 0)
	evaluate("PLogger.trace(1, 'trace'); PLogger.debug(2, 'hidden')", "PLogger.trace(1, 'trace'); PLogger.debug(2, 'hidden')")
	equal(logs, [[1, 0, "trace"]], "category override is isolated")
	logs.clear()
	env.clear_category_log_level(1)
	env.set_log_level(3)
	evaluate("PLogger.info(1, 'hidden'); PLogger.warning(1, 'warning'); PLogger.error(2, 'error')", "PLogger.info(1, 'hidden'); PLogger.warning(1, 'warning'); PLogger.error(2, 'error')")
	equal(logs, [[1, 3, "warning"], [2, 4, "error"]], "clear restores default threshold")
	return true


func test_reserved_category_and_argument_validation() -> bool:
	for call in ["PLogger.info(0, 'reserved')", "PLogger.info(-1, 'negative')", "PLogger.info(1.5, 'fraction')", "PLogger.info(1)", "PLogger.info(1, 42)"]:
		var count := errors.size()
		evaluate(call, call)
		check(errors.size() > count, "invalid log call raises an exception: " + call)
	return true


func test_internal_category() -> bool:
	var logs: Array = []
	env.set_log_callback(func(category: int, level: int, message: String):
		logs.append([category, level, message])
	)
	evaluate("error('failure')", "throw new Error('failure')")
	check(logs.size() == 1, "script exception reaches callback")
	if logs.size() == 1:
		equal(logs[0][0], 0, "internal category is reserved zero")
		equal(logs[0][1], 4, "exceptions use ERROR")
	return true


func test_configuration_survives_reinitialization() -> bool:
	var logs: Array = []
	env.set_log_callback(func(category: int, level: int, message: String):
		logs.append([category, level, message])
	)
	env.set_log_level(4)
	env.set_category_log_level(1, 1)
	var pool: Object = ClassDB.instantiate("PuertsStringNameUtf8Cache")
	equal(env.initialize(backend, pool), OK, "reinitialize environment")
	evaluate("PLogger.debug(1, 'debug'); PLogger.warning(2, 'hidden')", "PLogger.debug(1, 'debug'); PLogger.warning(2, 'hidden')")
	equal(logs, [[1, 1, "debug"]], "callback and thresholds survive reinitialization")
	return true


func test_nested_logs_do_not_reenter_callback() -> bool:
	var calls := [0]
	env.set_log_callback(func(_category: int, _level: int, _message: String):
		calls[0] += 1
		evaluate("PLogger.info(1, 'nested')", "PLogger.info(1, 'nested')")
	)
	evaluate("PLogger.info(1, 'outer')", "PLogger.info(1, 'outer')")
	equal(calls[0], 1, "nested logs cannot recurse indefinitely")
	return true
