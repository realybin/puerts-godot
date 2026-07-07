# SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
# SPDX-License-Identifier: BSD-3-Clause

extends RefCounted

var _case_failed = false
var _case_skipped = false
var _failure_messages: Array[String] = []
var _skip_message = ""
var _notes: Array[String] = []


func _mt_begin_case() -> void:
	_case_failed = false
	_case_skipped = false
	_failure_messages.clear()
	_skip_message = ""
	_notes.clear()


func _mt_end_case() -> Dictionary:
	return {
		"failed": _case_failed,
		"skipped": _case_skipped,
		"failures": _failure_messages.duplicate(),
		"skip_message": _skip_message,
		"notes": _notes.duplicate(),
	}


func fail_test(message: String) -> void:
	_case_failed = true
	_failure_messages.append(message)


func pass_test(message: String = "") -> void:
	if not message.is_empty():
		_notes.append(message)


func pending(reason: String = "pending") -> void:
	_case_skipped = true
	_skip_message = reason


func assert_true(value: Variant, label: String = "") -> void:
	if not bool(value):
		fail_test("%s expected=true actual=%s" % [_label_prefix(label), str(value)])


func assert_false(value: Variant, label: String = "") -> void:
	if bool(value):
		fail_test("%s expected=false actual=%s" % [_label_prefix(label), str(value)])


func assert_eq(actual: Variant, expected: Variant, label: String = "") -> void:
	if actual != expected:
		fail_test("%s expected=%s actual=%s" % [_label_prefix(label), str(expected), str(actual)])


func assert_string_contains(text: String, expected_fragment: String, label: String = "") -> void:
	if text.find(expected_fragment) == -1:
		fail_test(
			"%s expected to contain=%s actual=%s"
			% [_label_prefix(label), expected_fragment, text]
		)


func _label_prefix(label: String) -> String:
	if label.is_empty():
		return "assertion"
	return label
