# SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
# SPDX-License-Identifier: Apache-2.0

extends "res://tests/support/test_case.gd"

const WORKER_COUNT := 4
const ITERATIONS := 100


class Rendezvous extends RefCounted:
	var mutex := Mutex.new()
	var semaphore := Semaphore.new()
	var arrived := 0
	var count: int

	func _init(participants: int) -> void:
		count = participants

	func enter() -> bool:
		mutex.lock()
		arrived += 1
		if arrived == count:
			for _index in range(count):
				semaphore.post()
		mutex.unlock()
		var deadline := Time.get_ticks_msec() + 3000
		while Time.get_ticks_msec() < deadline:
			if semaphore.try_wait():
				return true
			OS.delay_msec(1)
		return false


func test_independent_environments_execute_in_parallel() -> bool:
	if OS.get_environment("PUERTS_TEST_THREAD_SAFE") == "no":
		return skip("requires thread_safe=yes")
	var rendezvous := Rendezvous.new(WORKER_COUNT)
	var cache: Object = ClassDB.instantiate("PuertsStringNameUtf8Cache")
	var threads: Array[Thread] = []
	for index in range(WORKER_COUNT):
		var worker := Thread.new()
		if equal(worker.start(_independent_worker.bind(backend, backend_name, cache, rendezvous, index)), OK, "start worker"):
			threads.append(worker)
	for worker in threads:
		check(worker.wait_to_finish() == true, "independent environment runs alongside the others")
	return true


static func _independent_worker(engine: Object, name: String, cache: Object, rendezvous: Rendezvous, index: int) -> bool:
	var environment: Object = ClassDB.instantiate("PuertsEnvironment")
	if environment.initialize(engine, cache) != OK:
		return false
	environment.set_global("rendezvous", rendezvous.enter)
	# Waiting inside script execution detects an accidental process-wide execution lock.
	var entered = environment.eval("return rendezvous:call()" if name == "lua" else "rendezvous.call()")
	var success: bool = entered != null and entered.to_bool()
	for _attempt in range(16):
		var loaded = environment.eval("return load_type('AStarGrid2D') ~= nil" if name == "lua" else "load_type('AStarGrid2D') !== null")
		success = success and loaded != null and loaded.to_bool()
		environment.set_global("worker_index", index)
		var value = environment.get_global("worker_index")
		success = success and value != null and value.to_native() == index
	environment.dispose()
	return success


func test_shared_environment_serializes_calls() -> bool:
	if OS.get_environment("PUERTS_TEST_THREAD_SAFE") == "no":
		return skip("requires thread_safe=yes")
	var function = evaluate(
		"thread_counter = 0; return function() thread_counter = thread_counter + 1; return thread_counter end",
		"globalThis.thread_counter = 0; () => ++thread_counter"
	)
	if not check(function != null, "create shared function"):
		return false
	env.set_global("worker_function", function)
	var callback: Callable = native(evaluate("return to_callable(worker_function)", "to_callable(worker_function)"))
	var threads: Array[Thread] = []
	for _index in range(WORKER_COUNT):
		var worker := Thread.new()
		if equal(worker.start(_shared_worker.bind(env, backend_name, function, callback)), OK, "start shared worker"):
			threads.append(worker)
	for worker in threads:
		check(worker.wait_to_finish() == true, "eval, script value and Callable work across threads")
	equal(native(evaluate("return thread_counter", "thread_counter")), threads.size() * ITERATIONS * 3, "no lost increments")
	return true


static func _shared_worker(environment: Object, name: String, function: Variant, callback: Callable) -> bool:
	for index in range(ITERATIONS):
		if name != "lua" and index % 25 == 0:
			environment.low_memory_notification()
			if name in ["v8", "nodejs"]:
				environment.tick()
		var value = environment.eval("return worker_function()" if name == "lua" else "worker_function()")
		if value == null or value.to_native() <= 0:
			return false
		value = function.call()
		if value == null or value.to_native() <= 0 or callback.call() <= 0:
			return false
	return true


func test_wrapper_release_races_with_cache_lookup() -> bool:
	if OS.get_environment("PUERTS_TEST_THREAD_SAFE") == "no":
		return skip("requires thread_safe=yes")
	evaluate("thread_object = {read = function() return 42 end}", "globalThis.thread_object = {read() { return 42; }}")
	var threads: Array[Thread] = []
	for _index in range(WORKER_COUNT):
		var worker := Thread.new()
		if equal(worker.start(_cache_worker.bind(env)), OK, "start cache worker"):
			threads.append(worker)
	for worker in threads:
		check(worker.wait_to_finish() == true, "cache remains valid during cross-thread releases")
	var value = env.get_global("thread_object")
	equal(value, env.get_global("thread_object"), "live wrapper retains its identity")
	return true


static func _cache_worker(environment: Object) -> bool:
	for _index in range(ITERATIONS):
		var value = environment.get_global("thread_object")
		if value == null:
			return false
		var answer = value.call_method("read")
		if answer == null or answer.to_native() != 42:
			return false
		value = null
	return true


func test_dispose_races_with_wrapper_release() -> bool:
	return _release_during_shutdown(false)


func test_owner_destruction_races_with_wrapper_release() -> bool:
	return _release_during_shutdown(true)


func _release_during_shutdown(destroy_owner: bool) -> bool:
	if OS.get_environment("PUERTS_TEST_THREAD_SAFE") == "no":
		return skip("requires thread_safe=yes")
	var owner_reference: WeakRef = weakref(env)
	var values: Array = []
	for index in range(ITERATIONS):
		values.append(env.eval(code("return " + str(index), str(index))))
	var retained = values[0]
	var ready := Semaphore.new()
	var worker := Thread.new()
	if not equal(worker.start(_release_worker.bind(values, ready)), OK, "start release worker"):
		return false
	values = []
	ready.wait()
	if destroy_owner:
		environments.clear()
		env = null
	else:
		env.dispose()
	worker.wait_to_finish()
	check(not retained.is_valid(), "retained value is invalid after concurrent shutdown")
	if destroy_owner:
		equal(owner_reference.get_ref(), null, "wrappers do not retain the environment")
	return true


static func _release_worker(values: Array, ready: Semaphore) -> void:
	ready.post()
	for index in range(values.size()):
		values[index] = null


func test_terminate_execution_from_another_thread() -> bool:
	if OS.get_environment("PUERTS_TEST_THREAD_SAFE") == "no" or backend_name not in ["v8", "nodejs"]:
		return skip("requires a thread-safe V8 backend")
	var entered := Semaphore.new()
	env.set_log_callback(_ignore_log)
	env.set_global("entered", entered.post)
	var worker := Thread.new()
	if not equal(worker.start(_infinite_loop.bind(env)), OK, "start running script"):
		return false
	entered.wait()
	env.terminate_execution()
	check(worker.wait_to_finish() == true, "termination interrupts the active script")
	return true


static func _infinite_loop(environment: Object) -> bool:
	return environment.eval("entered.call(); while (true) {}") == null


static func _ignore_log(_category: int, _level: int, _message: String) -> void:
	pass
