-- SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
-- SPDX-License-Identifier: Apache-2.0

local expect = assert

local function expect_throws(fn, needle, label)
	local ok, err = pcall(fn)
	expect(not ok, label .. ": no throw")
	expect(string.find(tostring(err), needle, 1, true) ~= nil, label .. ": " .. tostring(err))
end

_G.binding_cases = {
	type_loading = function()
		local Backend = load_type(backend_class_name)
		expect(Backend():get_backend_id() == backend_object:get_backend_id(), "registered backend constructor")
		expect_throws(function()
			load_type("DefinitelyMissingType")
		end, "Type not found", "missing type")

		local GlobalScope = load_type("GlobalScope")
		expect(math.abs(GlobalScope.sin(math.pi / 2) - 1) < 1e-6, "global utility")
		expect(GlobalScope.Engine:get_frames_drawn() >= 0, "global singleton")
		expect(GlobalScope.Variant.Type.TYPE_INT == 2, "nested global enum")
	end,

	reflected_objects = function()
		local Image = load_type("Image")
		local image = Image.create_empty(4, 4, true, Image.Format.FORMAT_RGBA8)
		expect_throws(function()
			image:clear_mipmaps(1)
		end, "Too many arguments", "reflected arity rejection")
		expect(image:has_mipmaps(), "rejected call leaves receiver intact")
		expect(image:clear_mipmaps() == nil and not image:has_mipmaps(), "reflected void method")

		local graph_node = load_type("GraphNode")()
		local icon = load_type("GradientTexture2D")()
		local Color = load_type("Color")
		graph_node:set_slot(0, true, 1, Color.WHITE, true, 2, Color.WHITE, icon, icon, false)
		expect(graph_node:get_slot_custom_icon_right(0) == icon and not graph_node:is_slot_draw_stylebox(0), "reflected arguments exceed inline capacity")

		local style = load_type("StyleBoxFlat")()
		style.border_width_left = 7
		expect(style.border_width_left == 7, "indexed property fallback")
	end,

	container_conversion = function()
		local ArrayType = load_type("Array")
		local CodeEdit = load_type("CodeEdit")
		local types = load_type("GlobalScope").Variant.Type
		local editor = CodeEdit()
		local prefixes = ArrayType()
		prefixes:push_back("#")
		editor:set_auto_indent_prefixes(prefixes)
		local typed = editor:get_auto_indent_prefixes()
		expect(not prefixes:is_typed() and typed:get_typed_builtin() == types.TYPE_STRING and typed:get(0) == "#", "untyped array to typed strings")

		local SystemFont = load_type("SystemFont")
		local fallback = SystemFont()
		local font = SystemFont()
		local fonts = ArrayType()
		fonts:push_back(fallback)
		font:set_fallbacks(fonts)
		local fallbacks = font:get_fallbacks()
		local StringName = load_type("StringName")
		expect(StringName.op_Equality(fallbacks:get_typed_class_name(), "Font") and fallbacks:get(0) == fallback, "untyped array to typed objects")

		local Dictionary = load_type("Dictionary")
		local pairs = Dictionary()
		pairs:set("(", ")")
		editor:set_auto_brace_completion_pairs(pairs)
		expect(editor:get_auto_brace_completion_pairs():get("(") == ")", "reflected dictionary argument")

		local PackedStringArray = load_type("PackedStringArray")
		local packed = PackedStringArray()
		packed:append("!")
		local joined = PackedStringArray.op_Addition(packed, prefixes)
		expect(joined:size() == 2 and joined:get(1) == "#", "Array to packed array in generated binding")

		local bytes = load_type("PackedByteArray")()
		bytes:append(65)
		bytes:append(0)
		expect(bytes:get(0) == 65 and bytes:hex_encode() == "4100", "packed bytes round trip through generated methods")
	end,

	builtin_bindings = function()
		local Vector2 = load_type("Vector2")
		local vector = Vector2(3, 4)
		expect(math.abs(vector:limit_length():length() - 1) < 1e-6, "instance method default argument")
		local Color = load_type("Color")
		expect(Color.from_hsv(0, 1, 1).a == 1, "static method default argument")
		local dict = load_type("Dictionary")()
		expect(dict:get_or_add("missing") == nil and dict:has("missing"), "explicit Variant default")
		expect(Vector2.ZERO.x == 0 and getmetatable(Vector2.Axis) == getmetatable(load_type("Vector2.Axis")), "constant and enum scope identity")
		expect_throws(function()
			Vector2.Axis.AXIS_X = 7
		end, "read-only", "enum member")
	end,

	static_argument_conversion = function()
		local Vector2 = load_type("Vector2")
		local vector = Vector2(3, 4)
		expect(vector:dot(boxed_vector) == 25 and vector:dot(boxed_int_vector) == 25, "boxed and cross-type arguments")
		local copy = Vector2(vector)
		copy.x = 10
		expect(vector.x == 3, "copy constructor preserves value independence")
		boxed_vector.x = 0
		expect(boxed_vector:length() == 4, "boxed receiver mutation")

		local ints = load_type("PackedInt32Array")()
		ints:append(-3.75)
		ints:append(2147483648)
		expect(ints:get(0) == -3 and ints:get(1) == -2147483648, "numeric narrowing")
		local wide = load_type("PackedInt64Array")()
		wide:append(4294967297)
		expect(wide:get(0) == 4294967297, "64-bit integer conversion")
		local vectors = load_type("PackedVector2Array")()
		vectors:append(vector)
		expect(vectors:get(0).y == 4, "packed array boxes value elements")
	end,

	binding_contracts = function()
		local ObjectType = load_type("Object")
		local generator = load_type("RandomNumberGenerator")()
		expect(ObjectType().get_class(generator) == "RandomNumberGenerator", "base method accepts reflected receiver")
		expect(generator:call("get_instance_id") == generator:get_instance_id(), "object vararg method")
		expect(load_type("GlobalScope").instance_from_id(generator:get_instance_id()) == generator, "object wrapper identity")

		local Vector2i = load_type("Vector2i")
		local vector = Vector2i(2, 4)
		local fractional = Vector2i.op_Multiply(vector, 0.5)
		local integral = Vector2i.op_Multiply(vector, 2)
		expect(fractional.x == 1 and fractional.y == 2, "fractional overload")
		expect(integral.x == 4 and integral.y == 8, "integer overload")
		expect(vector.x == 2 and vector.y == 4, "operator preserves source value")

		local signal = backend_object.script_changed
		expect(signal:get_object_id() == backend_object:get_instance_id(), "bound signal property")
		local calls = 0
		local function on_signal()
			calls = calls + 1
		end
		local callback = to_callable(on_signal)
		local equivalent = to_callable(on_signal)
		expect(signal:connect(callback) == 0 and signal:is_connected(equivalent), "script Callable equality")
		signal:emit()
		signal:disconnect(equivalent)
		signal:emit()
		expect(calls == 1, "equivalent Callable disconnects the signal")
	end,

	binding_errors = function()
		local Vector2 = load_type("Vector2")
		expect_throws(function()
			Vector2(1)
		end, "No constructor overload matches", "constructor arity")
		expect_throws(function()
			Vector2().length(load_type("Color").WHITE)
		end, "Native object type does not match", "wrong receiver")
		expect_throws(function()
			backend_object.script_changed:connect({}, 0)
		end, "Argument type does not match", "overloaded argument type")
		expect_throws(function()
			to_callable({})
		end, "expects a script function", "Callable input type")
	end,
}
