# SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
# SPDX-License-Identifier: BSD-3-Clause

extends "res://tests/support/test_case.gd"


func root_value() -> Variant:
	return evaluate(
		"return {nested = {value = 7}, add = function(a, b) return a + b end}",
		"({nested: {value: 7}, add(a, b) { return a + b; }})"
	)


func test_reentrant_dispose() -> bool:
	var calls := [0]
	env.set_error_callback(func(_message: String):
		calls[0] += 1
		env.dispose()
	)
	var value = evaluate("error('dispose')", "throw new Error('dispose')")
	env.set_error_callback(Callable())
	equal(value, null, "failed eval")
	equal(calls[0], 1, "one error callback")
	check(not env.is_alive(), "dispose is deferred until active eval finishes")
	return true


func test_dispose_during_coercion() -> bool:
	if backend_name == "lua":
		return skip("requires JavaScript coercion")
	env.set_global("owner", env)
	var value = env.eval("({toString() { owner.dispose(); return 'coerced'; }})")
	equal(value.to_string(), "coerced", "coercion completes")
	check(not env.is_alive() and not value.is_valid(), "coercion disposal invalidates the environment and source")
	return true


func test_dispose_invalidates_values_and_callable() -> bool:
	var root = root_value()
	var nested = root.get_property("nested")
	var callable_value = evaluate("return to_callable(function() return 7 end)", "to_callable(() => 7)")
	var callback: Callable = callable_value.to_native()
	equal(callback.call(), 7, "callable before dispose")
	env.set_global("number", 21)
	var values := [root, nested, env.get_global("number"), root.call_method("add", [2, 3]), callable_value]
	if backend_name != "lua":
		values.append(nested.get_property("value"))
	for value in values:
		if not check(value != null and value.is_valid(), "retained value is valid before dispose"):
			return false
	env.dispose()
	for value in values:
		check(not value.is_valid(), "retained value invalidated")
	check(not callback.is_valid(), "native callable invalidated")
	return true


func test_owner_release_with_retained_wrappers() -> bool:
	var weak_env: WeakRef = weakref(env)
	var weak_backend: WeakRef = weakref(backend)
	var root = root_value()
	var nested = root.get_property("nested")
	var callback: Callable = native(evaluate("return to_callable(function() return 1 end)", "to_callable(() => 1)"))
	env.dispose()
	environments.clear()
	env = null
	backend = null
	equal(weak_env.get_ref(), null, "wrappers do not retain disposed environment")
	equal(weak_backend.get_ref(), null, "wrappers do not retain backend")
	check(not root.is_valid() and not nested.is_valid() and not callback.is_valid(), "retained handles are invalid")
	return true


func test_values_released_before_environment() -> bool:
	var root = root_value()
	var nested = root.get_property("nested")
	root = null
	nested = null
	var weak_env: WeakRef = weakref(env)
	var weak_backend: WeakRef = weakref(backend)
	environments.clear()
	env = null
	backend = null
	equal(weak_env.get_ref(), null, "environment destructor")
	equal(weak_backend.get_ref(), null, "backend release after values")
	return true


func test_environment_releases_script_owned_objects() -> bool:
	var ref_id = evaluate(
		"held_ref = load_type('RefCounted')(); return held_ref:get_instance_id()",
		"globalThis.held_ref = new (load_type('RefCounted'))(); held_ref.get_instance_id()"
	).to_int()
	var node_id = evaluate(
		"held_node = load_type('Node')(); return held_node:get_instance_id()",
		"globalThis.held_node = new (load_type('Node'))(); held_node.get_instance_id()"
	).to_int()
	check(is_instance_id_valid(ref_id) and is_instance_id_valid(node_id), "script-owned objects alive")
	env.dispose()
	check(not is_instance_id_valid(ref_id) and not is_instance_id_valid(node_id), "dispose frees script-owned objects")
	return true


func test_registry_removal_and_bulk_invalidation() -> bool:
	var factory = evaluate("return function(value) return {value = value} end", "(value) => ({value})")
	var values := []
	for index in range(16):
		values.append(factory.call([index]))
	for index in range(1, values.size(), 2):
		values[index] = null
	env.dispose()
	check(not factory.is_valid(), "factory invalidated")
	for index in range(0, values.size(), 2):
		check(not values[index].is_valid(), "remaining wrapper invalidated")
	return true


func test_retained_script_value_survives_gc() -> bool:
	if backend_name == "lua":
		return skip("requires JavaScript object retention")
	var held = env.eval(
		"(() => { const ref = new (load_type('RefCounted'))(); return {ref, id: ref.get_instance_id()}; })()"
	)
	var id = held.get_property("id").to_int()
	var reference: WeakRef = weakref(instance_from_id(id))
	collect_garbage()
	check(reference.get_ref() != null, "script value retains native object through GC")
	held = null
	check(released(reference), "native object freed after script value release")
	return true


func test_shared_callable_owns_closure() -> bool:
	var id = evaluate(
		"local ref = load_type('RefCounted')(); local fn = function() return ref:get_instance_id() end; first = to_callable(fn); second = to_callable(fn); return ref:get_instance_id()",
		"(() => { const ref = new (load_type('RefCounted'))(); const fn = () => ref.get_instance_id(); globalThis.first = to_callable(fn); globalThis.second = to_callable(fn); return ref.get_instance_id(); })()"
	).to_int()
	var reference: WeakRef = weakref(instance_from_id(id))
	var first: Callable = native(env.get_global("first"))
	var second: Callable = native(env.get_global("second"))
	evaluate("first = nil; second = nil", "globalThis.first = undefined; globalThis.second = undefined")
	collect_garbage()
	equal(first.call(), id, "first callable survives GC")
	equal(second.call(), id, "second callable survives GC")
	first = Callable()
	collect_garbage()
	check(reference.get_ref() != null, "second callable retains shared closure")
	second = Callable()
	check(released(reference), "last callable releases captured object")
	return true


func test_signal_owns_callable_until_disconnect() -> bool:
	var id = evaluate(
		"local ref = load_type('RefCounted')(); callback = to_callable(function() ref:set_meta('called', true) end); return ref:get_instance_id()",
		"(() => { const ref = new (load_type('RefCounted'))(); globalThis.callback = to_callable(() => ref.set_meta('called', true)); return ref.get_instance_id(); })()"
	).to_int()
	var reference: WeakRef = weakref(instance_from_id(id))
	var callback: Callable = native(env.get_global("callback"))
	env.set_global("callback", null)
	var emitter := RefCounted.new()
	var signal_value: Signal = emitter.script_changed
	equal(signal_value.connect(callback), OK, "connect script callable")
	callback = Callable()
	collect_garbage()
	check(reference.get_ref() != null, "signal retains callback closure")
	signal_value.emit()
	var object = reference.get_ref()
	if not check(object != null, "captured object still alive"):
		return false
	equal(object.get_meta("called", false), true, "signal invokes callback")
	object = null
	var connections := signal_value.get_connections()
	signal_value.disconnect(connections[0].callable)
	connections.clear()
	check(released(reference), "disconnect releases closure")
	return true


func test_gc_respects_native_ownership() -> bool:
	if backend_name == "lua":
		return skip("backend has no low_memory_notification")
	var borrowed := Node.new()
	env.set_global("borrowed", borrowed)
	var id = env.eval(
		"globalThis.owned = [new (load_type('RefCounted'))(), new (load_type('Node'))()]; owned[0].get_instance_id()"
	).to_int()
	var node_id = env.eval("owned[1].get_instance_id()").to_int()
	collect_garbage()
	check(is_instance_id_valid(id) and is_instance_id_valid(node_id), "script references keep owned objects alive")
	env.eval("globalThis.owned = undefined")
	env.set_global("borrowed", null)
	var reference: WeakRef = weakref(instance_from_id(id))
	check(released(reference), "GC releases script-owned RefCounted")
	check(not is_instance_id_valid(node_id), "GC frees script-owned Node")
	check(is_instance_valid(borrowed), "GC preserves borrowed Node")
	borrowed.free()
	return true


func test_rewrapped_script_owned_object_keeps_one_lifetime() -> bool:
	var id = evaluate(
		"held = load_type('Node')(); return held:get_instance_id()",
		"globalThis.held = new (load_type('Node'))(); held.get_instance_id()"
	).to_int()
	var reference: WeakRef = weakref(instance_from_id(id))
	var native_object: Object = instance_from_id(id)
	env.set_global("alias", native_object)
	equal(evaluate("return alias:get_instance_id()", "alias.get_instance_id()").to_int(), id, "rewrapped object identity")
	native_object = null
	evaluate("held = nil", "globalThis.held = undefined")
	collect_garbage()
	check(reference.get_ref() != null, "alias retains rewrapped Node")
	evaluate("alias = nil", "globalThis.alias = undefined")
	check(released(reference), "last script reference frees rewrapped Node")
	return true


func test_dispose_script_owned_parent_and_child() -> bool:
	var parent_id = evaluate(
		"parent = load_type('Node')(); child = load_type('Node')(); parent:add_child(child); return parent:get_instance_id()",
		"globalThis.parent = new (load_type('Node'))(); globalThis.child = new (load_type('Node'))(); parent.add_child(child); parent.get_instance_id()"
	).to_int()
	var child_id = evaluate("return child:get_instance_id()", "child.get_instance_id()").to_int()
	check(is_instance_id_valid(parent_id) and is_instance_id_valid(child_id), "script-owned node tree alive")
	env.dispose()
	check(not is_instance_id_valid(parent_id) and not is_instance_id_valid(child_id), "node tree released once")
	return true


func test_dispose_releases_values_owned_by_other_values() -> bool:
	# Finalizing the container can destroy another wrapper during registry cleanup.
	var held = evaluate("return {callback = to_callable(function() return 7 end)}", "({callback: to_callable(() => 7)})")
	env.dispose()
	check(not held.is_valid(), "container invalidated during nested releases")
	return true


func test_dispose_during_callable() -> bool:
	env.set_global("owner", env)
	var callback: Callable = native(evaluate("return to_callable(function() owner:dispose(); return 42 end)", "to_callable(() => { owner.dispose(); return 42; })"))
	equal(callback.call(), 42, "active callable completes before disposal")
	check(not env.is_alive() and not callback.is_valid(), "callback and environment invalidated")
	return true


func test_log_callback_can_replace_itself() -> bool:
	var calls := [0]
	env.set_info_callback(func(_message: String):
		calls[0] += 1
		env.set_info_callback(Callable())
		env.dispose()
	)
	evaluate("log_info('done'); return 42", "log_info('done'); 42")
	equal(calls[0], 1, "log callback completes after clearing itself")
	check(not env.is_alive(), "logging defers disposal")
	return true


func test_reinitialization_keeps_old_handles_invalid() -> bool:
	var old_value = root_value()
	var old_callback: Callable = native(evaluate("return to_callable(function() return 1 end)", "to_callable(() => 1)"))
	var pool: Object = ClassDB.instantiate("PuertsStringNameCachePool")
	pool.initialize()
	equal(env.initialize(backend, pool), OK, "replace live runtime")
	check(not old_value.is_valid() and not old_callback.is_valid(), "old generation stays invalid")
	var fresh = root_value()
	check(fresh.is_valid(), "new generation is usable")
	equal(native(fresh.call_method("add", [2, 3])), 5, "new generation executes")
	env.dispose()
	env.dispose()
	check(not fresh.is_valid(), "repeated disposal is harmless")
	return true


func test_signal_callback_can_disconnect_itself() -> bool:
	var callback: Callable = native(evaluate(
		"local ref = load_type('RefCounted')(); captured_id = ref:get_instance_id(); return to_callable(function() log_info('disconnect'); ref:set_meta('called', true) end)",
		"(() => { const ref = new (load_type('RefCounted'))(); globalThis.captured_id = ref.get_instance_id(); return to_callable(() => { log_info('disconnect'); ref.set_meta('called', true); }); })()"
	))
	var id = env.get_global("captured_id").to_int()
	var reference: WeakRef = weakref(instance_from_id(id))
	var emitter := RefCounted.new()
	var signal_value: Signal = emitter.script_changed
	signal_value.connect(callback)
	callback = Callable()
	collect_garbage()
	env.set_info_callback(func(_message: String):
		signal_value.disconnect(signal_value.get_connections()[0].callable)
	)
	signal_value.emit()
	check(signal_value.get_connections().is_empty(), "callback disconnects itself")
	check(released(reference), "completed callback releases its closure")
	env.set_info_callback(Callable())
	return true
