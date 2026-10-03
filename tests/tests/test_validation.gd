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
	var pool: Object = ClassDB.instantiate("PuertsStringNameCachePool")
	pool.initialize(0)
	var node := Node.new()
	var uninitialized: Object = ClassDB.instantiate("PuertsStringNameCachePool")
	for arguments in [[null, pool], [node, pool], [backend, null], [backend, uninitialized]]:
		equal(env.initialize(arguments[0], arguments[1]), ERR_INVALID_PARAMETER, "invalid initialize arguments")
		check(not env.is_alive(), "failed initialize leaves environment dead")
	node.free()
	check(not errors.is_empty(), "invalid initialize reports errors")
	return true


func test_cache_pool_policies() -> bool:
	var pool: Object = ClassDB.instantiate("PuertsStringNameCachePool")
	for policy in [0, 1, 2]:
		equal(pool.initialize(policy, 2), OK, "initialize cache policy")
		equal(env.initialize(backend, pool), OK, "initialize with cache policy")
		for index in range(4):
			env.set_global("value_%d" % index, index)
			equal(native(env.get_global("value_%d" % index)), index, "cache-backed lookup")
		pool.clear()
		equal(native(env.get_global("value_0")), 0, "lookup after cache clear")
	equal(pool.initialize(99, 8), ERR_INVALID_PARAMETER, "invalid cache policy")
	equal(pool.get_policy(), 2, "rejected policy preserves configuration")
	return true


func test_reentrant_initialization() -> bool:
	var pool: Object = ClassDB.instantiate("PuertsStringNameCachePool")
	pool.initialize()
	var proxy := ReentrantBackend.new()
	proxy.environment = env
	proxy.delegate = backend
	proxy.pool = pool
	equal(env.initialize(proxy, pool), OK, "outer initialization")
	equal(proxy.nested_result, ERR_BUSY, "nested initialization is rejected")
	check(env.is_alive(), "outer initialization remains usable")
	proxy.environment = null
	return true
