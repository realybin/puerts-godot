# SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
# SPDX-License-Identifier: BSD-3-Clause

extends "res://tests/support/test_case.gd"


class ReentrantBackend extends RefCounted:
	var environment: Object
	var delegate: Object
	var pool: Object
	var nested_result: int

	func _puerts_get_functions_ptr() -> int:
		nested_result = environment.initialize(delegate, pool)
		return delegate._puerts_get_functions_ptr()


func per_backend() -> bool:
	return false


func test_invalid_initialization() -> bool:
	var pool: Object = ClassDB.instantiate("PuertsStringNameUtf8Cache")
	var node := Node.new()
	for arguments in [[null, pool], [node, pool], [backend, null]]:
		equal(env.initialize(arguments[0], arguments[1]), ERR_INVALID_PARAMETER, "invalid initialize arguments")
		check(not env.is_ready(), "failed initialize leaves environment dead")
	node.free()
	check(not errors.is_empty(), "invalid initialize reports errors")
	return true


func test_utf8_cache() -> bool:
	var pool: Object = ClassDB.instantiate("PuertsStringNameUtf8Cache")
	equal(env.initialize(backend, pool), OK, "initialize with default-constructed cache")
	for index in range(4):
		env.set_global("value_%d" % index, index)
		equal(native(env.get_global("value_%d" % index)), index, "cache-backed lookup")
	pool.clear()
	equal(native(env.get_global("value_0")), 0, "lookup after cache clear")
	return true


func test_reentrant_initialization() -> bool:
	var pool: Object = ClassDB.instantiate("PuertsStringNameUtf8Cache")
	var proxy := ReentrantBackend.new()
	proxy.environment = env
	proxy.delegate = backend
	proxy.pool = pool
	equal(env.initialize(proxy, pool), OK, "outer initialization")
	equal(proxy.nested_result, ERR_BUSY, "nested initialization is rejected")
	check(env.is_ready(), "outer initialization remains usable")
	proxy.environment = null
	return true
