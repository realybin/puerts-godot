# SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
# SPDX-License-Identifier: BSD-3-Clause

extends "res://tests/support/test_case.gd"


func per_backend() -> bool:
	return false


func test_different_backends() -> bool:
	if backend_names.size() < 2:
		return skip("requires two backends")
	var other_name := backend_names[-1]
	var other_backend: Object = ClassDB.instantiate(backend_class(other_name))
	var other := new_environment(other_backend)
	if other == null:
		return false
	env.set_global("owner", backend_name)
	other.set_global("owner", other_name)
	equal(native(evaluate("return owner", "owner")), backend_name, "first backend global")
	equal(native(other.eval("return owner" if other_name == "lua" else "owner")), other_name, "second backend global")
	other.dispose()
	equal(native(evaluate("return owner", "owner")), backend_name, "first backend survives second disposal")
	return true
