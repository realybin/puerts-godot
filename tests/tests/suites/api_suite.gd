# SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
# SPDX-License-Identifier: BSD-3-Clause

extends RefCounted

const TestSupport = preload("res://tests/support/puerts_support.gd")

static func script_for(backend_info: Dictionary, lua_code: String, ecmascript_code: String) -> String:
	return TestSupport.script_for(backend_info, lua_code, ecmascript_code)

static func create_environment(backend: Object, backend_name: String) -> Dictionary:
	return TestSupport.create_environment(backend, backend_name)

static func create_backend(backend_class: String) -> Object:
	return TestSupport.create_backend(backend_class)

static func unwrap_script_value(value: Variant) -> Variant:
	return TestSupport.unwrap_script_value(value)
static func create_cache_pool():
	return TestSupport.create_cache_pool()

static func create_lifecycle_value(env: Object, language: String):
	return TestSupport.create_lifecycle_value(env, language)
static func _native(value):
	return unwrap_script_value(value)


static func _dispose_if_alive(env: Object) -> void:
	if env != null and env.is_alive():
		env.dispose()


static func _cleanup_foreign_envs_and_return(error: String, env_a: Object, env_b: Object) -> String:
	_dispose_if_alive(env_a)
	_dispose_if_alive(env_b)
	return error


static func run_common_bridge_suite(env: Object, backend: Object, backend_info: Dictionary) -> String:
	var backend_name: String = backend_info["name"]

	var vector = Vector2(3.0, 4.0)
	env.set_global("vector_value", vector)
	var vector_result: Variant = _native(env.eval(script_for(backend_info, "return vector_value", "vector_value")))
	if vector_result != vector:
		return "%s builtin boxed roundtrip expected=%s actual=%s" % [backend_name, str(vector), str(vector_result)]

	var length_result: Variant = _native(env.eval(script_for(backend_info, "return vector_value:length()", "vector_value.length()")))
	if length_result != 5.0:
		return "%s builtin boxed method expected=5.0 actual=%s" % [backend_name, str(length_result)]

	var x_result: Variant = _native(env.eval(script_for(backend_info, "return vector_value.x", "vector_value.x")))
	if x_result != 3.0:
		return "%s builtin boxed property get expected=3.0 actual=%s" % [backend_name, str(x_result)]

	env.set_global("backend_object", backend)
	var backend_name_result: Variant = _native(env.eval(script_for(backend_info, "return backend_object:get_backend_name()", "backend_object.get_backend_name()")))
	var expected_backend_name: Variant = backend.call("get_backend_name")
	if backend_name_result != expected_backend_name:
		return "%s host object method call expected=%s actual=%s" % [backend_name, str(expected_backend_name), str(backend_name_result)]

	var backend_object_result: Variant = _native(env.get_global("backend_object"))
	if backend_object_result != backend:
		return "%s host object to_native expected backend object" % backend_name

	return ""


static func run_script_value_api_suite(env: Object, backend: Object, backend_info: Dictionary) -> String:
	var backend_name: String = backend_info["name"]
	var is_lua = backend_info["language"] == "lua"

	var int_value = env.eval("return 7" if is_lua else "7")
	if not int_value.is_int():
		return "%s script value is_int expected=true actual=false" % backend_name
	var int_to_int = int_value.to_int()
	if is_lua and int_to_int != 7:
		return "%s script value to_int lua expected=7 actual=%s" % [backend_name, str(int_to_int)]
	if not is_lua and typeof(int_to_int) != TYPE_INT:
		return "%s script value to_int expected int actual_type=%s actual=%s" % [backend_name, str(typeof(int_to_int)), str(int_to_int)]

	var float_value = env.eval("return 7.5" if is_lua else "7.5")
	if float_value.is_int():
		return "%s script value float is_int expected=false actual=true" % backend_name
	if not float_value.is_float():
		return "%s script value is_float expected=true actual=false" % backend_name
	if float_value.to_float() != 7.5:
		return "%s script value to_float expected=7.5 actual=%s" % [backend_name, str(float_value.to_float())]

	var string_value = env.eval("return 'hello'" if is_lua else "'hello'")
	if not string_value.is_string():
		return "%s script value is_string expected=true actual=false" % backend_name
	if string_value.to_string() != "hello":
		return "%s script value to_string expected=hello actual=%s" % [backend_name, str(string_value.to_string())]

	var bool_value = env.eval("return true" if is_lua else "true")
	if bool_value.to_bool() != true:
		return "%s script value to_bool expected=true actual=%s" % [backend_name, str(bool_value.to_bool())]

	var variadic_sum = env.eval(
		"return function(...) local sum = 0; for i = 1, select('#', ...) do sum = sum + select(i, ...) end; return sum end"
		if is_lua else
		"(...args) => args.reduce((sum, value) => sum + value, 0)"
	)
	var sum_result: Variant = _native(variadic_sum.call([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]))
	if sum_result != 55:
		return "%s script value overflow arguments expected=55 actual=%s" % [backend_name, str(sum_result)]

	var binary_bytes = PackedByteArray([1, 2, 3, 4])
	env.set_global("binary_payload", binary_bytes)
	var binary_value = env.get_global("binary_payload")
	# VARIANT_TO_SCRIPT_PACKED_BYTE_ARRAY_CAST
	if binary_value.is_binary():
		if binary_value.to_binary() != binary_bytes:
			return "%s script value to_binary expected=%s actual=%s" % [backend_name, str(binary_bytes), str(binary_value.to_binary())]
	# Boxed fallback when VARIANT_TO_SCRIPT_PACKED_BYTE_ARRAY_CAST is disabled.
	else:
		if binary_value.unwrap_native() != binary_bytes:
			return "%s script value PackedByteArray boxed fallback expected=%s actual=%s" % [backend_name, str(binary_bytes), str(binary_value.unwrap_native())]

	env.set_global("backend_object", backend)
	var backend_value = env.get_global("backend_object")
	if backend_value.unwrap_native() != backend:
		return "%s unwrap_native host object expected backend object" % backend_name

	var weak_node = Node.new()
	env.set_global("weak_node", weak_node)
	var weak_node_value = env.get_global("weak_node")
	if weak_node_value.unwrap_native() != weak_node:
		weak_node.free()
		return "%s unwrap_native weak host object expected same node" % backend_name
	weak_node.free()
	if weak_node_value.unwrap_native() != null:
		return "%s unwrap_native released weak host object expected null" % backend_name
	var weak_error_count = TestSupport.env_error_count(env)
	weak_node_value.get_property("name")
	if TestSupport.env_error_count(env) <= weak_error_count:
		return "%s get_property released weak host object error expected to be logged" % backend_name
	if TestSupport.env_last_error(env).find("Native object is no longer valid.") == -1:
		return "%s get_property released weak host object error expected to contain Native object is no longer valid. actual=%s" % [backend_name, TestSupport.env_last_error(env)]

	var vector = Vector2(6.0, 8.0)
	env.set_global("vector_boxed", vector)
	var vector_value = env.get_global("vector_boxed")
	if vector_value.unwrap_native() != vector:
		return "%s unwrap_native boxed builtin expected=%s actual=%s" % [backend_name, str(vector), str(vector_value.unwrap_native())]

	var native_vector_value = env.eval("return load_type('Vector2')(2.0, 5.0)" if is_lua else "new (load_type('Vector2'))(2.0, 5.0)")
	if native_vector_value.unwrap_native() != Vector2(2.0, 5.0):
		return "%s unwrap_native static bound builtin expected=Vector2(2,5) actual=%s" % [backend_name, str(native_vector_value.unwrap_native())]

	if string_value.unwrap_native() != null:
		return "%s unwrap_native non-native fallback expected=null actual=%s" % [backend_name, str(string_value.unwrap_native())]

	return ""


static func run_eval_basics_suite(backend: Object, backend_info: Dictionary) -> String:
	var backend_name: String = backend_info["name"]
	var supports_global_setter_exceptions = backend_name == "v8" or backend_name == "nodejs"
	var created = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])

	var env: Object = created["env"]
	var result = ""

	var error_message: Variant = _native(env.eval(script_for(backend_info, "error('boom')", "throw new Error('boom')")))
	if error_message != null:
		result = "%s eval failure result expected=null actual=%s" % [backend_name, str(error_message)]
	elif TestSupport.env_last_error(env).find("boom") == -1:
		result = "%s eval failure error expected contain boom actual=%s" % [backend_name, TestSupport.env_last_error(env)]

	if result.is_empty():
		env.set_global("foo", 21)

	if result.is_empty():
		error_message = _native(env.eval(script_for(backend_info, "error('boom')", "throw new Error('boom')")))
		if error_message != null:
			result = "%s eval failure before get_global expected=null actual=%s" % [backend_name, str(error_message)]
		elif TestSupport.env_last_error(env).find("boom") == -1:
			result = "%s eval failure before get_global error expected contain boom actual=%s" % [backend_name, TestSupport.env_last_error(env)]

	if result.is_empty():
		var foo_value = env.get_global("foo")
		if _native(foo_value) != 21:
			result = "%s get_global value expected=21 actual=%s" % [backend_name, str(_native(foo_value))]

	if result.is_empty() and supports_global_setter_exceptions:
		env.eval(script_for(
			backend_info,
			"",
			"Object.defineProperty(globalThis, 'boom_set', { configurable: true, set(_) { throw new Error('boom_set'); } });"
		))
		env.set_global("boom_set", 1)
		if TestSupport.env_last_error(env).find("boom_set") == -1:
			result = "%s set_global exception error expected contain boom_set actual=%s" % [backend_name, TestSupport.env_last_error(env)]

		if result.is_empty():
			env.set_global("bar", 42)
			if _native(env.get_global("bar")) != 42:
				result = "%s set_global success after exception value expected=42 actual=%s" % [backend_name, str(_native(env.get_global("bar")))]

	if result.is_empty():
		var eval_int: Variant = _native(env.eval(script_for(backend_info, "return 1 + 2", "1 + 2")))
		if eval_int != 3:
			result = "%s eval int expected=3 actual=%s" % [backend_name, str(eval_int)]

	if result.is_empty():
		var eval_string: Variant = _native(env.eval(script_for(backend_info, "return 'hello' .. ' world'", "'hello' + ' world'")))
		if eval_string != "hello world":
			result = "%s eval string expected=hello world actual=%s" % [backend_name, str(eval_string)]

	if result.is_empty():
		var long_text := "puerts-" + "x".repeat(600)
		env.set_global("long_text", long_text)
		var long_roundtrip: Variant = _native(env.get_global("long_text"))
		if long_roundtrip != long_text:
			result = "%s long string roundtrip length expected=%d actual=%d" % [backend_name, long_text.length(), str(long_roundtrip).length()]

	if result.is_empty():
		env.set_global("foo", 21)
		var roundtrip: Variant = _native(env.eval(script_for(backend_info, "return foo * 2", "foo * 2")))
		if roundtrip != 42:
			result = "%s global roundtrip expected=42 actual=%s" % [backend_name, str(roundtrip)]

	if env.is_alive():
		env.dispose()
	return result


static func run_bridge_suite(backend: Object, backend_info: Dictionary) -> String:
	var backend_name: String = backend_info["name"]
	var created = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])
	var env: Object = created["env"]
	var result = run_common_bridge_suite(env, backend, backend_info)
	if env.is_alive():
		env.dispose()
	return result


static func run_script_value_suite(backend: Object, backend_info: Dictionary) -> String:
	var backend_name: String = backend_info["name"]
	var created = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])
	var env: Object = created["env"]
	var result = run_script_value_api_suite(env, backend, backend_info)
	if env.is_alive():
		env.dispose()
	return result


static func run_foreign_script_value_suite(backend: Object, backend_info: Dictionary) -> String:
	var backend_name: String = backend_info["name"]
	var created_a: Dictionary = create_environment(backend, backend_name)
	if not bool(created_a["ok"]):
		return str(created_a["error"])
	var env_a: Object = created_a["env"]

	var created_b: Dictionary = create_environment(backend, backend_name)
	if not bool(created_b["ok"]):
		return _cleanup_foreign_envs_and_return(str(created_b["error"]), env_a, null)
	var env_b: Object = created_b["env"]

	var foreign_value = create_lifecycle_value(env_a, backend_info["language"])
	if foreign_value == null or not foreign_value.is_valid():
		return _cleanup_foreign_envs_and_return("%s foreign_script_value source is invalid" % backend_name, env_a, env_b)

	var local_value = env_b.eval(script_for(backend_info, "return { value = 9 }", "({ value: 9 })"))
	if local_value == null or not local_value.is_valid():
		return _cleanup_foreign_envs_and_return("%s foreign_script_value local value is invalid" % backend_name, env_a, env_b)

	var same_env_set_error_count := TestSupport.env_error_count(env_b)
	env_b.set_global("same_env_value", local_value)
	if TestSupport.env_error_count(env_b) != same_env_set_error_count:
		return _cleanup_foreign_envs_and_return("%s same env script value allowed expected no error actual=%s" % [backend_name, TestSupport.env_last_error(env_b)], env_a, env_b)

	var same_env_roundtrip: Variant = _native(env_b.eval(script_for(backend_info, "return same_env_value.value", "same_env_value.value")))
	if same_env_roundtrip != 9:
		return _cleanup_foreign_envs_and_return("%s same env script value roundtrip expected=9 actual=%s" % [backend_name, str(same_env_roundtrip)], env_a, env_b)

	env_b.set_global("foreign_value", foreign_value)
	if TestSupport.env_last_error(env_b).find("another PuertsEnvironment") == -1:
		return _cleanup_foreign_envs_and_return("%s foreign script value set_global error expected another PuertsEnvironment actual=%s" % [backend_name, TestSupport.env_last_error(env_b)], env_a, env_b)

	if _native(env_b.get_global("foreign_value")) != null:
		return _cleanup_foreign_envs_and_return("%s foreign script value set_global blocked expected null" % backend_name, env_a, env_b)

	var receiver = env_b.eval(script_for(
		backend_info,
		"return { echo = function(value) return value end }",
		"({ echo(value) { return value; } })"
	))
	if receiver == null or not receiver.is_valid():
		return _cleanup_foreign_envs_and_return("%s foreign_script_value receiver is invalid" % backend_name, env_a, env_b)

	var call_result = receiver.call_method("echo", [foreign_value])
	if _native(call_result) != null:
		return _cleanup_foreign_envs_and_return("%s foreign script value call blocked expected null actual=%s" % [backend_name, str(_native(call_result))], env_a, env_b)
	if TestSupport.env_last_error(env_b).find("another PuertsEnvironment") == -1:
		return _cleanup_foreign_envs_and_return("%s foreign script value call error expected another PuertsEnvironment actual=%s" % [backend_name, TestSupport.env_last_error(env_b)], env_a, env_b)

	return _cleanup_foreign_envs_and_return("", env_a, env_b)


static func run_object_graph_suite(backend: Object, backend_info: Dictionary) -> String:
	var backend_name: String = backend_info["name"]
	var created = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])

	var env: Object = created["env"]
	var result = ""

	env.set_global("foo", 21)
	var cached_global = env.eval(script_for(
		backend_info,
		"cached_global = { value = 9 }; return cached_global",
		"globalThis.cached_global = { value: 9 }; cached_global"
	))
	if cached_global == null or not cached_global.is_valid():
		result = "%s cached global is invalid: %s" % [backend_name, TestSupport.env_last_error(env)]

	if result.is_empty():
		var cached_global_again = env.get_global("cached_global")
		if cached_global_again == null or not cached_global_again.is_valid():
			result = "%s cached global reload is invalid: %s" % [backend_name, TestSupport.env_last_error(env)]
		elif cached_global_again.get_instance_id() != cached_global.get_instance_id():
			result = "%s global script value cache expected same instance id" % backend_name
		else:
			cached_global = null
			cached_global_again = null
			var rebuilt_global = env.get_global("cached_global")
			var rebuilt_global_again = env.get_global("cached_global")
			if rebuilt_global == null or not rebuilt_global.is_valid():
				result = "%s rebuilt global cache is invalid" % backend_name
			elif rebuilt_global_again.get_instance_id() != rebuilt_global.get_instance_id():
				result = "%s rebuilt global cache expected same instance id" % backend_name
			elif backend_info["language"] != "lua":
				env.eval("Object.freeze(cached_global)")
				rebuilt_global = null
				rebuilt_global_again = null
				var unrelated_global = env.eval("({ unrelated: true })")
				var frozen_global = env.get_global("cached_global")
				if unrelated_global == null or frozen_global == null or _native(frozen_global.get_property("value")) != 9:
					result = "%s frozen global cache identity mismatch" % backend_name

	var object_value: Variant = null
	if result.is_empty():
		object_value = env.eval(script_for(
			backend_info,
			"return { add = function(a, b) return a + b end, nested = { value = 7 } }",
			"({ add(a, b) { return a + b; }, nested: { value: 7 } })"
		))
		if object_value == null or not object_value.is_valid():
			result = "%s eval returned invalid script value: %s" % [backend_name, TestSupport.env_last_error(env)]

	if result.is_empty():
		if _native(object_value.call_method("add", [4, 5])) != 9:
			result = "%s call_method expected=9" % backend_name
		elif _native(object_value.call_method("add", [5, 6])) != 11:
			result = "%s call_method value_ref expected=11" % backend_name

	if result.is_empty():
		var add_value: Variant = object_value.get_property("add")
		if add_value == null or not add_value.is_valid():
			result = "%s add property is invalid: %s" % [backend_name, TestSupport.env_last_error(env)]
		elif _native(add_value.call([7, 8])) != 15:
			result = "%s call function value expected=15" % backend_name

	if result.is_empty():
		var non_callable_result: Variant = object_value.call_method("nested")
		if _native(non_callable_result) != null:
			result = "%s call_method non-callable result expected=null actual=%s" % [backend_name, str(_native(non_callable_result))]
		elif TestSupport.env_last_error(env).find("not callable: nested") == -1:
			result = "%s call_method non-callable error expected not callable: nested actual=%s" % [backend_name, TestSupport.env_last_error(env)]

	if result.is_empty():
		var nested_call_result: Variant = object_value.get_property("nested").call()
		if _native(nested_call_result) != null:
			result = "%s call non-function result expected=null actual=%s" % [backend_name, str(_native(nested_call_result))]
		elif TestSupport.env_last_error(env).find("not a function") == -1:
			result = "%s call non-function error expected not a function actual=%s" % [backend_name, TestSupport.env_last_error(env)]

	if result.is_empty():
		if _native(object_value.call_method("add", [6, 7])) != 13:
			result = "%s call_method clears callable error expected=13" % backend_name

	if result.is_empty():
		var nested_value: Variant = object_value.get_property("nested")
		if nested_value == null or not nested_value.is_valid():
			result = "%s nested property is invalid: %s" % [backend_name, TestSupport.env_last_error(env)]
		else:
			var nested_value_again: Variant = object_value.get_property("nested")
			if nested_value_again == null or not nested_value_again.is_valid():
				result = "%s nested property reload is invalid: %s" % [backend_name, TestSupport.env_last_error(env)]
			elif nested_value_again.get_instance_id() != nested_value.get_instance_id():
				result = "%s nested script value cache expected same instance id" % backend_name
			else:
				var value_ref: Variant = nested_value.get_property("value")
				if value_ref == null or not value_ref.is_valid():
					if backend_info["language"] != "lua":
						result = "%s value property is invalid: %s" % [backend_name, TestSupport.env_last_error(env)]
				elif _native(value_ref) != 7:
					result = "%s get_property value_ref expected=7 actual=%s" % [backend_name, str(_native(value_ref))]
				elif _native(env.get_global("foo")) != 21:
					result = "%s get_global value_ref expected=21 actual=%s" % [backend_name, str(_native(env.get_global("foo")))]

	if env.is_alive():
		env.dispose()
	return result
