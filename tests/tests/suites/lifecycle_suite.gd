# SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
# SPDX-License-Identifier: BSD-3-Clause

extends RefCounted

const TestSupport = preload("res://tests/support/puerts_support.gd")

static func create_cache_pool():
	return TestSupport.create_cache_pool()

static func create_environment(backend: Object, backend_name: String) -> Dictionary:
	return TestSupport.create_environment(backend, backend_name)

static func create_backend(backend_class: String) -> Object:
	return TestSupport.create_backend(backend_class)

static func create_lifecycle_value(env: Object, language: String):
	return TestSupport.create_lifecycle_value(env, language)

static func script_for(backend_info: Dictionary, lua_code: String, ecmascript_code: String) -> String:
	return TestSupport.script_for(backend_info, lua_code, ecmascript_code)

static func unwrap_script_value(value: Variant) -> Variant:
	return TestSupport.unwrap_script_value(value)

static func skip(reason: String) -> Dictionary:
	return TestSupport.skip(reason)
class WeakObjectTracker:
	extends RefCounted

	var _weak_refs: Array[WeakRef] = []

	func remember(object: Object) -> void:
		_weak_refs.append(weakref(object))

	func total_count() -> int:
		return _weak_refs.size()

	func living_count() -> int:
		var count = 0
		for weak in _weak_refs:
			if weak.get_ref() != null:
				count += 1
		return count


static func _dispose_env_if_alive(env: Object) -> void:
	if env != null and env.is_alive():
		env.dispose()


static func _cleanup_dual_envs_and_return(error: String, env_a: Object, env_b: Object) -> String:
	_dispose_env_if_alive(env_a)
	_dispose_env_if_alive(env_b)
	return error


static func _backend_has_low_memory_notification(backend_name: String) -> bool:
	return backend_name == "quickjs" or backend_name == "v8" or backend_name == "nodejs"


static func _extract_script_id_array(env: Object, backend_name: String, ids_value: Variant, label: String):
	var ids_native = ids_value.to_native()
	if ids_native is Array:
		return ids_native
	if not (ids_native is Object and ids_native.has_method("get_property")):
		return "%s %s ids to_native expected Array or PuertsScriptValue actual type=%s value=%s last_error=%s" % [
			backend_name,
			label,
			type_string(typeof(ids_native)),
			str(ids_native),
			TestSupport.env_last_error(env)
		]

	var ids_script: Object = ids_native
	var length_variant = ids_script.get_property("length").to_int()
	if length_variant == null:
		return "%s %s ids length conversion failed last_error=%s" % [backend_name, label, TestSupport.env_last_error(env)]

	var length = int(length_variant)
	var result: Array = []
	result.resize(length)
	for i in range(length):
		var id_variant = ids_script.get_property(StringName(str(i))).to_int()
		if id_variant == null:
			return "%s %s ids[%d] conversion failed last_error=%s" % [backend_name, label, i, TestSupport.env_last_error(env)]
		result[i] = int(id_variant)
	return result


static func _pump_backend_gc(env: Object, backend: Object, attempts: int = 8) -> void:
	var backend_id := ""
	if backend != null:
		backend_id = str(backend.get_backend_id())
	for _i in range(attempts):
		if backend_id == "lua":
			env.eval("collectgarbage('collect')")
			continue
		if backend_id == "v8" or backend_id == "nodejs":
			env.eval("(function () { if (typeof gc === 'function') { gc(); } return 0; })()")
		env.low_memory_notification()
		if backend_id == "v8" or backend_id == "nodejs":
			env.tick()


static func _create_script_callable_pair(env: Object, backend_info: Dictionary, tracker: WeakObjectTracker) -> Dictionary:
	var backend_name: String = backend_info["name"]
	var id_value = env.eval(script_for(
		backend_info,
		"local RefCounted = load_type('RefCounted'); local captured = RefCounted(); local callback = function() return captured:get_instance_id() end; _G.__callable_gc_first = to_callable(callback); _G.__callable_gc_second = to_callable(callback); return captured:get_instance_id()",
		"(function () { const RefCounted = load_type('RefCounted'); const captured = new RefCounted(); const callback = () => captured.get_instance_id(); globalThis.__callable_gc_first = to_callable(callback); globalThis.__callable_gc_second = to_callable(callback); return captured.get_instance_id(); })()"
	))
	if id_value == null or not id_value.is_valid():
		return { "ok": false, "error": "%s script_callable_gc setup failed: %s" % [backend_name, TestSupport.env_last_error(env)] }

	var first_value = env.get_global("__callable_gc_first")
	var second_value = env.get_global("__callable_gc_second")
	if first_value == null or second_value == null:
		return { "ok": false, "error": "%s script_callable_gc global read failed: %s" % [backend_name, TestSupport.env_last_error(env)] }
	env.eval(script_for(
		backend_info,
		"_G.__callable_gc_first = nil; _G.__callable_gc_second = nil",
		"globalThis.__callable_gc_first = undefined; globalThis.__callable_gc_second = undefined"
	))

	var object_id = int(id_value.to_int())
	var captured = instance_from_id(object_id)
	var first = first_value.to_native()
	var second = second_value.to_native()
	if captured == null or typeof(first) != TYPE_CALLABLE or typeof(second) != TYPE_CALLABLE:
		return { "ok": false, "error": "%s script_callable_gc native conversion failed" % backend_name }
	tracker.remember(captured)
	return { "ok": true, "id": object_id, "first": first, "second": second }


static func run_reentrant_dispose_suite(backend: Object, backend_info: Dictionary) -> String:
	var backend_name: String = backend_info["name"]
	var created: Dictionary = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])
	var env: Object = created["env"]
	var callback_state := {"count": 0}
	env.set_error_callback(func(_message: String):
		callback_state["count"] += 1
		env.dispose()
	)

	var value = env.eval(script_for(backend_info, "error('reentrant dispose')", "throw new Error('reentrant dispose')"))
	env.set_error_callback(Callable())
	if value != null:
		return "%s reentrant dispose expected null eval result" % backend_name
	if env.is_alive():
		return "%s reentrant dispose expected environment dead" % backend_name
	if callback_state["count"] != 1:
		return "%s reentrant dispose expected one error callback actual=%d" % [backend_name, callback_state["count"]]
	return ""


static func run_coercion_dispose_suite(backend: Object, backend_info: Dictionary):
	var backend_name: String = backend_info["name"]
	if backend_info["language"] != "ecmascript":
		return skip("%s coercion dispose requires an ECMAScript backend" % backend_name)
	var created: Dictionary = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])
	var env: Object = created["env"]
	env.set_global("dispose_environment", env)
	var value = env.eval("({ toString() { dispose_environment.dispose(); return 'coerced'; } })")
	if value == null or not value.is_valid():
		_dispose_env_if_alive(env)
		return "%s coercion dispose setup failed" % backend_name

	var text: String = value.to_string()
	if text != "coerced":
		return "%s coercion dispose expected=coerced actual=%s" % [backend_name, text]
	if env.is_alive():
		return "%s coercion dispose expected environment dead" % backend_name
	if value.is_valid():
		return "%s coercion dispose expected source value invalid" % backend_name
	return ""


static func run_dispose_invalidation_suite(backend: Object, backend_info: Dictionary) -> String:
	var backend_name: String = backend_info["name"]
	var is_lua: bool = backend_info["language"] == "lua"
	var created: Dictionary = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])
	var env: Object = created["env"]
	created = {}

	env.set_global("holder", 21)
	var root_value = create_lifecycle_value(env, backend_info["language"])
	var nested_value = root_value.get_property("nested")
	var leaf_value = nested_value.get_property("value")
	var call_value = root_value.call_method("add", [2, 3])
	var global_value = env.get_global("holder")
	var callable_value = env.eval(script_for(backend_info, "return to_callable(function() return 7 end)", "to_callable(() => 7)"))
	if callable_value == null:
		return "%s invalidation callable setup failed (last_error=%s)" % [backend_name, TestSupport.env_last_error(env)]
	var script_callable = callable_value.to_native()
	if typeof(script_callable) != TYPE_CALLABLE or not script_callable.is_valid() or script_callable.call() != 7:
		return "%s invalidation callable mismatch before dispose" % backend_name
	var derived_values: Array = [root_value, nested_value, call_value, global_value, callable_value]
	if leaf_value != null:
		derived_values.insert(2, leaf_value)
	elif not is_lua:
		return "%s invalidation leaf value is null before dispose (last_error=%s)" % [backend_name, TestSupport.env_last_error(env)]

	for index in range(derived_values.size()):
		var value = derived_values[index]
		if value == null:
			return "%s invalidation derived[%d] is null before dispose (last_error=%s)" % [backend_name, index, TestSupport.env_last_error(env)]
		if not value.is_valid():
			return "%s invalidation derived[%d] unexpectedly invalid before dispose (last_error=%s)" % [backend_name, index, TestSupport.env_last_error(env)]

	env.dispose()
	if env.is_alive():
		return "%s invalidation dispose expected env dead" % backend_name

	for index in range(derived_values.size()):
		if derived_values[index] == null:
			continue
		if derived_values[index].is_valid():
			return "%s invalidation derived[%d] expected invalid after dispose" % [backend_name, index]
	if script_callable.is_valid():
		return "%s invalidation callable expected invalid after dispose" % backend_name

	return ""


static func run_environment_release_suite(backend_info: Dictionary) -> String:
	var backend_name: String = backend_info["name"]
	var backend = create_backend(backend_info["class_name"])
	if backend == null:
		return "%s release backend class not found" % backend_name

	var created: Dictionary = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])
	var env: Object = created["env"]

	env.set_global("holder", 34)
	var retained_values: Array = []
	retained_values.append(create_lifecycle_value(env, backend_info["language"]))
	retained_values.append(retained_values[0].get_property("nested"))
	retained_values.append(retained_values[0].call_method("add", [8, 9]))
	retained_values.append(env.get_global("holder"))

	for index in range(retained_values.size()):
		var value = retained_values[index]
		if value == null:
			return "%s release retained[%d] is null before dispose (last_error=%s)" % [backend_name, index, TestSupport.env_last_error(env)]
		if not value.is_valid():
			return "%s release retained[%d] unexpectedly invalid before dispose (last_error=%s)" % [backend_name, index, TestSupport.env_last_error(env)]

	env.dispose()

	for value in retained_values:
		if value == null:
			continue
		if value.is_valid():
			return "%s release kept derived value valid after owner release" % backend_name

	retained_values.clear()
	env = null
	backend = null

	return ""


static func run_callable_environment_release_suite(backend_info: Dictionary) -> String:
	var backend_name: String = backend_info["name"]
	var backend = create_backend(backend_info["class_name"])
	if backend == null:
		return "%s callable environment release backend class not found" % backend_name

	var created: Dictionary = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])
	var env: Object = created["env"]
	created = {}

	var callable_value = env.eval(script_for(
		backend_info,
		"return to_callable(function() return 1 end)",
		"to_callable(() => 1)"
	))
	if callable_value == null or not callable_value.is_valid():
		_dispose_env_if_alive(env)
		return "%s callable environment release setup failed: %s" % [backend_name, TestSupport.env_last_error(env)]
	var script_callable = callable_value.to_native()
	if typeof(script_callable) != TYPE_CALLABLE or not script_callable.is_valid():
		_dispose_env_if_alive(env)
		return "%s callable environment release native conversion failed" % backend_name
	callable_value = null

	var weak_env: WeakRef = weakref(env)
	var weak_backend: WeakRef = weakref(backend)
	env.dispose()
	if script_callable.is_valid():
		return "%s callable environment release expected invalid Callable after dispose" % backend_name
	env = null
	backend = null

	if weak_env.get_ref() != null:
		return "%s callable environment release Callable retained disposed environment" % backend_name
	if weak_backend.get_ref() != null:
		return "%s callable environment release Callable retained backend" % backend_name
	script_callable = Callable()
	return ""


static func run_environment_owned_objects_release_suite(backend_info: Dictionary) -> String:
	var backend_name: String = backend_info["name"]
	var backend = create_backend(backend_info["class_name"])
	if backend == null:
		return "%s env-owned release backend class not found" % backend_name

	var created: Dictionary = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])
	var env: Object = created["env"]
	var tracker: WeakObjectTracker = WeakObjectTracker.new()

	var ref_id_value = env.eval(script_for(
		backend_info,
		"local RefCounted = load_type('RefCounted'); _G.__env_release_ref = RefCounted(); return _G.__env_release_ref:get_instance_id()",
		"(function () { const RefCounted = load_type('RefCounted'); globalThis.__env_release_ref = new RefCounted(); return globalThis.__env_release_ref.get_instance_id(); })()"
	))
	if ref_id_value == null or not ref_id_value.is_valid():
		_dispose_env_if_alive(env)
		return "%s env-owned RefCounted setup failed: %s" % [backend_name, TestSupport.env_last_error(env)]

	var node_id_value = env.eval(script_for(
		backend_info,
		"local Node = load_type('Node'); _G.__env_release_node = Node(); return _G.__env_release_node:get_instance_id()",
		"(function () { const Node = load_type('Node'); globalThis.__env_release_node = new Node(); return globalThis.__env_release_node.get_instance_id(); })()"
	))
	if node_id_value == null or not node_id_value.is_valid():
		_dispose_env_if_alive(env)
		return "%s env-owned Node setup failed: %s" % [backend_name, TestSupport.env_last_error(env)]

	var ref_object = instance_from_id(int(ref_id_value.to_int()))
	if ref_object == null:
		_dispose_env_if_alive(env)
		return "%s env-owned RefCounted instance missing before dispose" % backend_name
	tracker.remember(ref_object)

	var node_object = instance_from_id(int(node_id_value.to_int()))
	if node_object == null:
		_dispose_env_if_alive(env)
		return "%s env-owned Node instance missing before dispose" % backend_name
	tracker.remember(node_object)

	ref_object = null
	node_object = null
	ref_id_value = null
	node_id_value = null

	if tracker.living_count() != 2:
		_dispose_env_if_alive(env)
		return "%s env-owned objects expected alive before dispose actual=%d" % [backend_name, tracker.living_count()]

	env.dispose()
	env = null
	backend = null

	if tracker.living_count() != 0:
		return "%s env-owned objects expected released after dispose actual=%d" % [backend_name, tracker.living_count()]
	return ""


static func run_value_release_order_suite(backend_info: Dictionary) -> String:
	var backend_name: String = backend_info["name"]
	var backend = create_backend(backend_info["class_name"])
	if backend == null:
		return "%s release-order backend class not found" % backend_name

	var created: Dictionary = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])
	var env: Object = created["env"]

	var root_value = create_lifecycle_value(env, backend_info["language"])
	var nested_value = root_value.get_property("nested")
	var result_value = root_value.call_method("add", [10, 11])
	if root_value == null or nested_value == null or result_value == null:
		return "%s release-order created null value before release (last_error=%s)" % [backend_name, TestSupport.env_last_error(env)]
	if not root_value.is_valid() or not nested_value.is_valid() or not result_value.is_valid():
		return "%s release-order created invalid value before release" % backend_name

	root_value = null
	nested_value = null
	result_value = null

	var weak_backend: WeakRef = weakref(backend)
	env.dispose()
	env = null
	backend = null

	if weak_backend.get_ref() != null:
		return "%s release-order backend still retained after all values released" % backend_name
	return ""


static func run_script_value_registry_churn_suite(backend: Object, backend_info: Dictionary) -> String:
	var backend_name: String = backend_info["name"]
	var created: Dictionary = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])
	var env: Object = created["env"]

	var factory = env.eval(script_for(
		backend_info,
		"return function(value) return { value = value } end",
		"(value) => ({ value })"
	))
	if factory == null or not factory.is_valid():
		_dispose_env_if_alive(env)
		return "%s registry churn factory is invalid: %s" % [backend_name, TestSupport.env_last_error(env)]

	var values: Array = []
	for i in range(256):
		var value = factory.call([i])
		if value == null or not value.is_valid():
			_dispose_env_if_alive(env)
			return "%s registry churn value %d is invalid: %s" % [backend_name, i, TestSupport.env_last_error(env)]
		values.append(value)

	# Remove alternating interior nodes before disposal; the remaining wrappers
	# exercise bulk invalidation of a non-contiguous intrusive list.
	for i in range(1, values.size(), 2):
		values[i] = null

	env.dispose()
	for i in range(0, values.size(), 2):
		if values[i].is_valid():
			return "%s registry churn value %d remained valid after dispose" % [backend_name, i]
	return ""


static func run_dispose_stress_suite(backend_info: Dictionary) -> String:
	var backend_name: String = backend_info["name"]
	var retained_values: Array = []
	var weak_envs: Array[WeakRef] = []
	var weak_backends: Array[WeakRef] = []

	for i in range(256):
		var backend = create_backend(backend_info["class_name"])
		if backend == null:
			return "%s stress backend class not found at %d" % [backend_name, i]

		var created: Dictionary = create_environment(backend, backend_name)
		if not bool(created["ok"]):
			return "%s stress initialize failed at %d: %s" % [backend_name, i, str(created["error"])]
		var env: Object = created["env"]

		var root_value = create_lifecycle_value(env, backend_info["language"])
		var nested_value = root_value.get_property("nested")
		var result_value = root_value.call_method("add", [i, 1])
		if root_value == null or nested_value == null or result_value == null:
			return "%s stress derived value null at %d (last_error=%s)" % [backend_name, i, TestSupport.env_last_error(env)]
		if not root_value.is_valid() or not nested_value.is_valid() or not result_value.is_valid():
			return "%s stress derived value invalid at %d" % [backend_name, i]

		weak_envs.append(weakref(env))
		weak_backends.append(weakref(backend))
		retained_values.append(root_value)
		retained_values.append(nested_value)
		retained_values.append(result_value)

		env.dispose()
		env = null
		backend = null

	if retained_values.is_empty():
		return "%s stress produced no retained values" % backend_name

	var retained_env_count = 0
	for weak_env in weak_envs:
		if weak_env.get_ref() != null:
			retained_env_count += 1

	var retained_backend_count = 0
	for weak_backend in weak_backends:
		if weak_backend.get_ref() != null:
			retained_backend_count += 1

	retained_values.clear()

	if retained_env_count != 0:
		return "%s stress retained %d disposed environments" % [backend_name, retained_env_count]
	if retained_backend_count != 0:
		return "%s stress retained %d backends through disposed environments" % [backend_name, retained_backend_count]
	return ""


static func run_godot_holds_js_gc_suite(backend: Object, backend_info: Dictionary):
	var backend_name: String = backend_info["name"]
	if backend_info["language"] != "ecmascript":
		return skip("%s godot_holds_js_gc requires an ECMAScript backend" % backend_name)
	if not _backend_has_low_memory_notification(backend_name):
		return skip("%s godot_holds_js_gc requires low_memory_notification support" % backend_name)

	var created = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])

	var env: Object = created["env"]
	var tracker: WeakObjectTracker = WeakObjectTracker.new()

	var held_value = env.eval(
		"(function () { const RefCounted = load_type('RefCounted'); const ref = new RefCounted(); return { id: ref.get_instance_id(), ref }; })()"
	)
	if held_value == null or not held_value.is_valid():
		_dispose_env_if_alive(env)
		return "%s godot_holds_js_gc setup failed: %s" % [backend_name, TestSupport.env_last_error(env)]

	var id_value = held_value.get_property("id")
	if id_value == null or not id_value.is_valid():
		_dispose_env_if_alive(env)
		return "%s godot_holds_js_gc id read failed: %s" % [backend_name, TestSupport.env_last_error(env)]

	var native_object = instance_from_id(int(id_value.to_int()))
	if native_object == null:
		_dispose_env_if_alive(env)
		return "%s godot_holds_js_gc captured RefCounted missing before gc" % backend_name
	tracker.remember(native_object)
	native_object = null
	id_value = null

	_pump_backend_gc(env, backend)
	if tracker.living_count() != 1:
		_dispose_env_if_alive(env)
		return "%s godot_holds_js_gc expected JS object alive while PuertsScriptValue is retained" % backend_name

	held_value = null
	var released = false
	for _i in range(16):
		_pump_backend_gc(env, backend, 2)
		if tracker.living_count() == 0:
			released = true
			break

	if not released:
		_dispose_env_if_alive(env)
		return "%s godot_holds_js_gc expected JS object released after PuertsScriptValue is dropped" % backend_name

	if env.is_alive():
		env.dispose()
	return ""


static func run_script_callable_gc_suite(backend: Object, backend_info: Dictionary) -> String:
	var backend_name: String = backend_info["name"]
	var created = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])

	var env: Object = created["env"]
	var tracker: WeakObjectTracker = WeakObjectTracker.new()
	var callables: Array[Callable] = []
	var object_ids: Array[int] = []

	for i in range(64):
		var pair = _create_script_callable_pair(env, backend_info, tracker)
		if not bool(pair["ok"]):
			_dispose_env_if_alive(env)
			return "%s at %d" % [str(pair["error"]), i]
		object_ids.append(int(pair["id"]))
		callables.append(pair["first"])
		callables.append(pair["second"])
		pair = {}

	_pump_backend_gc(env, backend)
	if tracker.living_count() != object_ids.size():
		_dispose_env_if_alive(env)
		return "%s script_callable_gc retained count expected=%d actual=%d" % [backend_name, object_ids.size(), tracker.living_count()]

	for i in range(object_ids.size()):
		callables[i * 2] = Callable()

	_pump_backend_gc(env, backend)
	if tracker.living_count() != object_ids.size():
		_dispose_env_if_alive(env)
		return "%s script_callable_gc first release dropped shared function expected=%d actual=%d" % [backend_name, object_ids.size(), tracker.living_count()]

	callables.clear()
	var released = false
	for _i in range(16):
		_pump_backend_gc(env, backend, 2)
		if tracker.living_count() == 0:
			released = true
			break
	if not released:
		var leaked_count = tracker.living_count()
		var last_error = TestSupport.env_last_error(env)
		_dispose_env_if_alive(env)
		return "%s script_callable_gc leaked %d captured objects after Callable release (last_error=%s)" % [backend_name, leaked_count, last_error]

	env.dispose()
	return ""


static func run_signal_callable_gc_suite(backend: Object, backend_info: Dictionary) -> String:
	var backend_name: String = backend_info["name"]
	var created = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])

	var env: Object = created["env"]
	var id_value = env.eval(script_for(
		backend_info,
		"local RefCounted = load_type('RefCounted'); local captured = RefCounted(); local callback = function() captured:set_meta('called', true) end; _G.__signal_callable_gc = to_callable(callback); return captured:get_instance_id()",
		"(function () { const RefCounted = load_type('RefCounted'); const captured = new RefCounted(); const callback = () => captured.set_meta('called', true); globalThis.__signal_callable_gc = to_callable(callback); return captured.get_instance_id(); })()"
	))
	if id_value == null or not id_value.is_valid():
		_dispose_env_if_alive(env)
		return "%s signal_callable_gc setup failed: %s" % [backend_name, TestSupport.env_last_error(env)]

	var callable_value = env.get_global("__signal_callable_gc")
	if callable_value == null:
		_dispose_env_if_alive(env)
		return "%s signal_callable_gc global read failed: %s" % [backend_name, TestSupport.env_last_error(env)]
	env.eval(script_for(backend_info, "_G.__signal_callable_gc = nil", "globalThis.__signal_callable_gc = undefined"))

	var captured = instance_from_id(int(id_value.to_int()))
	var script_callable = callable_value.to_native()
	if captured == null or typeof(script_callable) != TYPE_CALLABLE:
		_dispose_env_if_alive(env)
		return "%s signal_callable_gc native conversion failed" % backend_name
	var weak_captured: WeakRef = weakref(captured)
	var emitter := RefCounted.new()
	var signal_value: Signal = emitter.script_changed
	if signal_value.connect(script_callable) != OK:
		_dispose_env_if_alive(env)
		return "%s signal_callable_gc connect failed" % backend_name

	id_value = null
	callable_value = null
	script_callable = Callable()
	captured = null
	_pump_backend_gc(env, backend)
	if weak_captured.get_ref() == null:
		_dispose_env_if_alive(env)
		return "%s signal_callable_gc Signal did not retain callback closure" % backend_name

	signal_value.emit()
	var retained = weak_captured.get_ref()
	if retained == null or not bool(retained.get_meta("called", false)):
		_dispose_env_if_alive(env)
		return "%s signal_callable_gc retained callback was not invocable" % backend_name
	retained = null

	var connections: Array = signal_value.get_connections()
	if connections.size() != 1:
		_dispose_env_if_alive(env)
		return "%s signal_callable_gc expected one connection actual=%d" % [backend_name, connections.size()]
	var connected_callable: Callable = connections[0]["callable"]
	signal_value.disconnect(connected_callable)
	connected_callable = Callable()
	connections.clear()

	var released = false
	for _i in range(16):
		_pump_backend_gc(env, backend, 2)
		if weak_captured.get_ref() == null:
			released = true
			break
	if not released:
		var last_error = TestSupport.env_last_error(env)
		_dispose_env_if_alive(env)
		return "%s signal_callable_gc captured object survived disconnect (last_error=%s)" % [backend_name, last_error]

	env.dispose()
	return ""


static func run_js_refcounted_gc_suite(backend: Object, backend_info: Dictionary):
	var backend_name: String = backend_info["name"]
	if backend_info["language"] != "ecmascript":
		return skip("%s js_gc requires an ECMAScript backend" % backend_name)
	if not _backend_has_low_memory_notification(backend_name):
		return skip("%s js_gc requires low_memory_notification support" % backend_name)

	var created = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])

	var env: Object = created["env"]
	var result = ""
	var tracker: WeakObjectTracker = WeakObjectTracker.new()

	var ids_value = env.eval(
		"(function () { const RefCounted = load_type('RefCounted'); const ObjectType = load_type('Object'); const objectCount = 37; globalThis.__gc_test_objs = []; const ids = []; for (let i = 0; i < objectCount; i++) { const obj = new RefCounted(); globalThis.__gc_test_objs.push(obj); ids.push(obj.get_instance_id()); } const staticObject = new ObjectType(); globalThis.__gc_test_objs.push(staticObject); ids.push(staticObject.get_instance_id()); return ids; })()"
	)
	if ids_value == null or not ids_value.is_valid():
		result = "%s js_gc setup eval failed: %s" % [backend_name, TestSupport.env_last_error(env)]

	var object_ids: Array = []
	if result.is_empty():
		var object_ids_result = _extract_script_id_array(env, backend_name, ids_value, "js_gc")
		if object_ids_result is String:
			result = object_ids_result
		else:
			object_ids = object_ids_result
	ids_value = null

	var object_count = object_ids.size()
	if result.is_empty():
		for object_id in object_ids:
			var native_object = instance_from_id(int(object_id))
			if native_object == null:
				result = "%s js_gc instance_from_id(%s) returned null during setup" % [backend_name, str(object_id)]
				break
			tracker.remember(native_object)

	if result.is_empty() and tracker.total_count() != object_count:
		result = "%s js_gc tracked count expected=%d actual=%d" % [backend_name, object_count, tracker.total_count()]

	if result.is_empty():
		_pump_backend_gc(env, backend)
		if tracker.living_count() != object_count:
			result = "%s js_gc retained while JS holds refs expected=%d actual=%d" % [backend_name, object_count, tracker.living_count()]

	if result.is_empty():
		env.eval("globalThis.__gc_test_objs = undefined")
		_pump_backend_gc(env, backend)
		if tracker.living_count() != 0:
			result = "%s js_gc released after JS refs cleared expected=0 actual=%d" % [backend_name, tracker.living_count()]

	if env.is_alive():
		env.dispose()
	return result


static func run_non_refcounted_unbind_suite(backend: Object, backend_info: Dictionary):
	var backend_name: String = backend_info["name"]
	if not _backend_has_low_memory_notification(backend_name):
		return skip("%s non_ref_unbind requires low_memory_notification support" % backend_name)

	var created = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])

	var env: Object = created["env"]
	var result = ""
	var tracker: WeakObjectTracker = WeakObjectTracker.new()
	var engine_node: Node = Node.new()
	var weak_engine_node: WeakRef = weakref(engine_node)
	tracker.remember(engine_node)

	env.set_global("engine_node", engine_node)

	var ids_script = script_for(
		backend_info,
		"local Node = load_type('Node'); _G.__gc_test_nodes = {}; local ids = {}; for i = 1, 19 do local node = Node(); table.insert(_G.__gc_test_nodes, node); table.insert(ids, node:get_instance_id()); end return ids",
		"(function () { const Node = load_type('Node'); globalThis.__gc_test_nodes = []; const ids = []; for (let i = 0; i < 19; i++) { const node = new Node(); globalThis.__gc_test_nodes.push(node); ids.push(node.get_instance_id()); } return ids; })()"
	)
	var ids_value = env.eval(ids_script)
	if ids_value == null or not ids_value.is_valid():
		result = "%s non_ref_unbind setup eval failed: %s" % [backend_name, TestSupport.env_last_error(env)]

	var object_ids: Array = []
	if result.is_empty():
		var object_ids_result = _extract_script_id_array(env, backend_name, ids_value, "non_ref_unbind")
		if object_ids_result is String:
			result = object_ids_result
		else:
			object_ids = object_ids_result
	ids_value = null

	var object_count = object_ids.size()
	if result.is_empty():
		for object_id in object_ids:
			var native_node = instance_from_id(int(object_id))
			if native_node == null:
				result = "%s non_ref_unbind instance_from_id(%s) returned null during setup" % [backend_name, str(object_id)]
				break
			tracker.remember(native_node)

	var expected_alive = object_count + 1
	if result.is_empty() and tracker.total_count() != expected_alive:
		result = "%s non_ref_unbind tracked count expected=%d actual=%d" % [backend_name, expected_alive, tracker.total_count()]

	if result.is_empty():
		_pump_backend_gc(env, backend)
		if tracker.living_count() != expected_alive:
			result = "%s non_ref_unbind retained while refs held expected=%d actual=%d" % [backend_name, expected_alive, tracker.living_count()]

	if result.is_empty():
		env.eval(script_for(backend_info, "_G.__gc_test_nodes = nil", "globalThis.__gc_test_nodes = undefined"))
		env.set_global("engine_node", null)
		_pump_backend_gc(env, backend)

		if tracker.living_count() != 1:
			result = "%s non_ref_unbind only engine-owned survives expected=1 actual=%d" % [backend_name, tracker.living_count()]
		elif weak_engine_node.get_ref() != engine_node:
			result = "%s non_ref_unbind engine-owned object preserved failed" % backend_name

	if env.is_alive():
		env.dispose()
	return result


static func run_same_backend_multi_env_suite(backend: Object, backend_info: Dictionary) -> String:
	var backend_name: String = backend_info["name"]
	var created_a: Dictionary = create_environment(backend, backend_name)
	if not bool(created_a["ok"]):
		return str(created_a["error"])
	var env_a: Object = created_a["env"]

	var created_b: Dictionary = create_environment(backend, backend_name)
	if not bool(created_b["ok"]):
		return _cleanup_dual_envs_and_return(str(created_b["error"]), env_a, null)
	var env_b: Object = created_b["env"]

	env_a.set_global("shared_value", 11)
	env_b.set_global("shared_value", 22)

	var script = script_for(backend_info, "return shared_value", "shared_value")
	if unwrap_script_value(env_a.eval(script)) != 11:
		return _cleanup_dual_envs_and_return("%s multi_env isolated globals env_a expected=11" % backend_name, env_a, env_b)

	if unwrap_script_value(env_b.eval(script)) != 22:
		return _cleanup_dual_envs_and_return("%s multi_env isolated globals env_b expected=22" % backend_name, env_a, env_b)

	var vector_a = env_a.eval(script_for(
		backend_info,
		"return load_type('Vector2')(3, 4):length()",
		"new (load_type('Vector2'))(3, 4).length()"
	))
	if unwrap_script_value(vector_a) != 5.0:
		return _cleanup_dual_envs_and_return("%s multi_env eval env_a expected=5.0 actual=%s" % [backend_name, str(vector_a)], env_a, env_b)

	var vector_b = env_b.eval(script_for(
		backend_info,
		"return load_type('Vector2')(6, 8):length()",
		"new (load_type('Vector2'))(6, 8).length()"
	))
	if unwrap_script_value(vector_b) != 10.0:
		return _cleanup_dual_envs_and_return("%s multi_env eval env_b expected=10.0 actual=%s" % [backend_name, str(vector_b)], env_a, env_b)

	env_b.dispose()
	if env_b.is_alive():
		return _cleanup_dual_envs_and_return("%s multi_env dispose env_b expected dead" % backend_name, env_a, env_b)

	if unwrap_script_value(env_a.eval(script)) != 11:
		return _cleanup_dual_envs_and_return("%s multi_env env_a survives env_b dispose expected=11" % backend_name, env_a, env_b)

	env_a.dispose()
	if env_a.is_alive():
		return "%s multi_env dispose env_a expected dead" % backend_name
	return ""
