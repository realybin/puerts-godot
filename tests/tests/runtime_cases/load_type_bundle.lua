-- SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
-- SPDX-License-Identifier: BSD-3-Clause

local root = _G.__puerts_cases or {}
_G.__puerts_cases = root

local function expect(condition, message)
	if condition then
		return ""
	end
	return message
end

local function expect_throws(fn, needle, label)
	local ok, err = pcall(fn)
	if ok then
		return label .. ": no throw"
	end
	if string.find(tostring(err), needle, 1, true) == nil then
		return label .. ": " .. tostring(err)
	end
	return ""
end

local function near(a, b, eps)
	return math.abs(a - b) <= eps
end

root.load_type = {
	exists = function()
		local err = expect(load_type ~= nil, "load_type function missing")
		if err ~= "" then
			return err
		end

		err = expect(load_type(backend_class_name) ~= nil, "backend class constructor missing")
		if err ~= "" then
			return err
		end

		err = expect_throws(function()
			return load_type("DefinitelyMissingType")
		end, "Type not found", "missing type check")
		if err ~= "" then
			return err
		end

		err = expect(load_type("Time").Month.MONTH_JANUARY == 1, "class enum mismatch")
		if err ~= "" then
			return err
		end

		return expect(load_type("Vector2") ~= nil, "builtin class constructor missing")
	end,

	backend_constructor = function()
		return expect(load_type(backend_class_name)():get_backend_id() == backend_object:get_backend_id(), "backend constructor mismatch")
	end,

	global_scope = function()
		local GlobalScope = load_type("GlobalScope")
		local err = expect(GlobalScope ~= nil, "GlobalScope type missing")
		if err ~= "" then
			return err
		end

		err = expect(near(GlobalScope.sin(math.pi * 0.5), 1.0, 1e-6), "GlobalScope utility mismatch")
		if err ~= "" then
			return err
		end

		err = expect(type(GlobalScope.Engine.get_frames_drawn) == "function", "GlobalScope singleton mismatch")
		if err ~= "" then
			return err
		end

		err = expect(GlobalScope.Key.KEY_ENTER > 0, "GlobalScope enum mismatch")
		if err ~= "" then
			return err
		end

		return expect(GlobalScope.Variant.Type.TYPE_INT == 2, "GlobalScope Variant enum mismatch")
	end,

	reflected_objects = function()
		local RandomNumberGenerator = load_type("RandomNumberGenerator")
		local rng = RandomNumberGenerator()
		rng.seed = 13579
		local sample = rng:randi()
		local err = expect(rng.seed == 13579 and sample >= 0, "rng reflected binding mismatch")
		if err ~= "" then
			return err
		end

		local NodeType = load_type("Node")
		local node = NodeType()
		err = expect(node:get_class() == "Node" and node:get_child_count() == 0, "node reflected binding mismatch")
		if err ~= "" then
			return err
		end
		err = expect(NodeType.NOTIFICATION_READY == 13, "node reflected constant mismatch")
		if err ~= "" then
			return err
		end
		err = expect_throws(function()
			NodeType.NOTIFICATION_READY = 99
		end, "read-only", "node reflected constant readonly")
		if err ~= "" then
			return err
		end

		local ObjectType = load_type("Object")
		local obj = ObjectType()
		err = expect(
			obj:get_class() == "Object" and
				obj:has_signal("script_changed") and
				ObjectType.ConnectFlags.CONNECT_DEFERRED == 1 and
				ObjectType.NOTIFICATION_PREDELETE == 1,
			"object reflected binding mismatch"
		)
		if err ~= "" then
			return err
		end
		err = expect(obj:call("get_instance_id") == obj:get_instance_id(), "object vararg return mismatch")
		if err ~= "" then
			return err
		end
		local script_changed = obj.script_changed
		err = expect(
			script_changed ~= nil and
				script_changed:is_null() == false and
				script_changed:get_name() == "script_changed" and
				script_changed:get_object_id() == obj:get_instance_id(),
			"object reflected signal property mismatch"
		)
		if err ~= "" then
			return err
		end
		err = expect_throws(function()
			ObjectType.ConnectFlags.CONNECT_DEFERRED = 99
		end, "read-only", "object reflected enum readonly")
		if err ~= "" then
			return err
		end

		local GraphNode = load_type("GraphNode")
		local GradientTexture2D = load_type("GradientTexture2D")
		local Color = load_type("Color")
		local graph_node = GraphNode()
		local slot_icon = GradientTexture2D()
		graph_node:set_slot(
			0,
			true,
			1,
			Color(1, 0, 0, 1),
			true,
			2,
			Color(0, 1, 0, 1),
			slot_icon,
			slot_icon,
			true
		)
		err = expect(
			graph_node:is_slot_enabled_left(0) and graph_node:is_slot_enabled_right(0),
			"reflected overflow arguments mismatch"
		)
		if err ~= "" then
			return err
		end

		return expect(
			load_type("FileAccess").file_exists("res://project.godot") and load_type("FileAccess").get_size("res://project.godot") > 0,
			"static reflected binding mismatch"
		)
	end,

	error_paths = function()
		local err = expect(load_type() == nil, "load_type empty arg should return nil")
		if err ~= "" then
			return err
		end

		err = expect_throws(function()
			return load_type("DirAccess")()
		end, "No constructor available", "missing constructor check")
		if err ~= "" then
			return err
		end

		err = expect_throws(function()
			return load_type("RandomNumberGenerator")(1)
		end, "zero-argument construction", "constructor arg rejection")
		if err ~= "" then
			return err
		end

		err = expect_throws(function()
			local RandomNumberGenerator = load_type("RandomNumberGenerator")
			local rng = RandomNumberGenerator()
			rng.seed = "oops"
		end, "Property type does not match", "property type rejection")
		if err ~= "" then
			return err
		end

		err = expect_throws(function()
			return load_type("Object")(1)
		end, "Argument count does not match the bound signature", "object static constructor arity rejection")
		if err ~= "" then
			return err
		end

		err = expect_throws(function()
			local Theme = load_type("Theme")
			local theme = Theme()
			theme.default_font = backend_object
		end, "Property type does not match", "object property type rejection")
		if err ~= "" then
			return err
		end

		err = expect_throws(function()
			load_type("Time").Month.MONTH_JANUARY = 2
		end, "read-only", "readonly enum rejection")
		if err ~= "" then
			return err
		end

		err = expect_throws(function()
			local LightmapGIData = load_type("LightmapGIData")
			local data = LightmapGIData()
			data.lightmap_textures = {}
		end, "read-only", "readonly property rejection")
		if err ~= "" then
			return err
		end

		return ""
	end,

	builtin_static_binding = function()
		local err = expect(load_type("Vector2")(3.0, 4.0):length() == 5.0, "vector2 constructor mismatch")
		if err ~= "" then
			return err
		end

		do
			local Vector2 = load_type("Vector2")
			err = expect(
				Vector2.ZERO.x == 0 and Vector2.ZERO.y == 0 and Vector2.ONE.x == 1 and Vector2.ONE.y == 1,
				"vector2 constants mismatch"
			)
			if err ~= "" then
				return err
			end
			err = expect_throws(function()
				Vector2.ZERO = nil
			end, "read-only", "vector2 constant readonly")
			if err ~= "" then
				return err
			end
			err = expect(Vector2.Axis and Vector2.Axis.AXIS_X == 0 and Vector2.Axis.AXIS_Y == 1, "vector2 axis enum mismatch")
			if err ~= "" then
				return err
			end
			err = expect_throws(function()
				Vector2.Axis = nil
			end, "read-only", "vector2 axis enum group readonly")
			if err ~= "" then
				return err
			end
			err = expect_throws(function()
				Vector2.Axis.AXIS_X = 7
			end, "read-only", "vector2 axis enum readonly")
			if err ~= "" then
				return err
			end
		end

		do
			local Projection = load_type("Projection")
			err = expect(Projection.Planes and Projection.Planes.PLANE_NEAR == 0 and Projection.Planes.PLANE_BOTTOM == 5, "projection planes enum mismatch")
			if err ~= "" then
				return err
			end
			err = expect_throws(function()
				Projection.Planes.PLANE_NEAR = 99
			end, "read-only", "projection planes enum readonly")
			if err ~= "" then
				return err
			end
		end

		do
			local Vector4 = load_type("Vector4")
			err = expect(Vector4.Axis and Vector4.Axis.AXIS_X == 0 and Vector4.Axis.AXIS_W == 3, "vector4 axis enum mismatch")
			if err ~= "" then
				return err
			end
		end

		do
			local Vector2 = load_type("Vector2")
			local v = Vector2()
			v.x = 8.0
			v.y = 6.0
			err = expect(v:length() == 10.0, "vector2 property mismatch")
			if err ~= "" then
				return err
			end
		end

		do
			local Vector3 = load_type("Vector3")
			local v = Vector3(1.0, 2.0, 3.0)
			v.z = 4.0
			err = expect(v:dot(Vector3(2.0, 0.0, 1.0)) == 6.0, "vector3 method mismatch")
			if err ~= "" then
				return err
			end
		end

		do
			local Vector2 = load_type("Vector2")
			local Rect2 = load_type("Rect2")
			local rect = Rect2(Vector2(2.0, 3.0), Vector2(4.0, 5.0))
			err = expect(rect:has_point(Vector2(3.0, 4.0)) and rect:get_area() == 20.0 and rect.position.x == 2.0, "rect2 mismatch")
			if err ~= "" then
				return err
			end
		end

		do
			local StringName = load_type("StringName")
			local name = StringName("player")
			err = expect(name:contains("lay") and name:length() == 6, "string_name methods mismatch")
			if err ~= "" then
				return err
			end
			err = expect(name:begins_with("pla") and name:ends_with("yer") and name:to_upper() == "PLAYER", "string_name extras mismatch")
			if err ~= "" then
				return err
			end
		end

		do
			local Vector2i = load_type("Vector2i")
			local v = Vector2i(6, 8)
			err = expect(math.floor(v:length()) == 10 and v.x == 6 and v.y == 8, "vector2i mismatch")
			if err ~= "" then
				return err
			end
		end

		do
			local NodePath = load_type("NodePath")
			local path = NodePath("Root/Child:leaf")
			err = expect(path:get_name_count() == 2 and path:get_subname_count() == 1, "node_path methods mismatch")
			if err ~= "" then
				return err
			end

			local abs_path = NodePath("/Root/Child:leaf")
			err = expect(abs_path:is_absolute() and abs_path:get_name(0) == "Root" and abs_path:get_subname(0) == "leaf", "node_path extras mismatch")
			if err ~= "" then
				return err
			end
		end

		do
			local Transform2D = load_type("Transform2D")
			local Vector2 = load_type("Vector2")
			local t = Transform2D(Vector2(1.0, 0.0), Vector2(0.0, 1.0), Vector2(2.0, 3.0))
			local determinant = t.determinant and t:determinant() or t:basis_determinant()
			err = expect(t.origin.x == 2.0 and t.origin.y == 3.0 and determinant == 1.0, "transform2d mismatch")
			if err ~= "" then
				return err
			end
		end

		do
			local Vector3 = load_type("Vector3")
			local Plane = load_type("Plane")
			local plane = Plane(Vector3(0.0, 1.0, 0.0), 2.0)
			err = expect(plane:has_point(Vector3(0.0, 2.0, 0.0), 0.001) and plane:distance_to(Vector3(0.0, 5.0, 0.0)) == 3.0, "plane mismatch")
			if err ~= "" then
				return err
			end
		end

		do
			local Vector3 = load_type("Vector3")
			local Basis = load_type("Basis")
			local Transform3D = load_type("Transform3D")
			local t = Transform3D(Basis(), Vector3(1.0, 2.0, 3.0))
			local inv = t:inverse()
			err = expect(inv.origin.x == -1.0 and inv.origin.y == -2.0 and inv.origin.z == -3.0, "transform3d inverse mismatch")
			if err ~= "" then
				return err
			end

			local moved = t:translated_local(Vector3(1.0, 0.0, 0.0))
			err = expect(moved.origin.x == 2.0 and moved.origin.y == 2.0 and moved.origin.z == 3.0, "transform3d extras mismatch")
			if err ~= "" then
				return err
			end
		end

		do
			local Projection = load_type("Projection")
			err = expect(Projection.create_orthogonal(-1.0, 1.0, -1.0, 1.0, 0.1, 10.0):determinant() ~= 0.0, "projection orthogonal mismatch")
			if err ~= "" then
				return err
			end

			local p = Projection.create_perspective(75.0, 1.0, 0.1, 10.0, false)
			err = expect(near(p:get_z_near(), 0.1, 0.001) and p:is_orthogonal() == false and p:inverse():determinant() ~= 0.0, "projection perspective mismatch")
			if err ~= "" then
				return err
			end
		end

		err = expect(load_type("RID")():is_valid() == false, "rid default mismatch")
		if err ~= "" then
			return err
		end

		do
			local PackedInt32Array = load_type("PackedInt32Array")
			local PackedFloat64Array = load_type("PackedFloat64Array")
			local PackedStringArray = load_type("PackedStringArray")
			local PackedVector2Array = load_type("PackedVector2Array")
			local PackedColorArray = load_type("PackedColorArray")
			local Vector2 = load_type("Vector2")
			local Color = load_type("Color")

			local ints = PackedInt32Array()
			ints:append(3)
			ints:set(0, 7)
			local floats = PackedFloat64Array()
			floats:append(1.5)
			local strings = PackedStringArray()
			strings:append("godot")
			local vectors = PackedVector2Array()
			vectors:append(Vector2(2.0, 4.0))
			local colors = PackedColorArray()
			colors:append(Color(0.1, 0.2, 0.3, 1.0))

			err = expect(
				ints:get(0) == 7 and ints:has(7) and floats:get(0) == 1.5 and strings:get(0) == "godot" and vectors:get(0).y == 4.0 and near(colors:get(0).b, 0.3, 0.001),
				"packed arrays mismatch"
			)
			if err ~= "" then
				return err
			end
		end

		do
			local PackedByteArray = load_type("PackedByteArray")
			local bytes = PackedByteArray()
			bytes:append(103)
			bytes:append(111)
			bytes:append(100)
			bytes:append(111)
			bytes:append(116)
			err = expect(bytes:get(1) == 111 and bytes:get_string_from_utf8() == "godot" and bytes:hex_encode() == "676f646f74", "packed byte array mismatch")
			if err ~= "" then
				return err
			end
		end

		do
			local Callable = load_type("Callable")
			local ArrayType = load_type("Array")
			local c = Callable(backend_object, "get_backend_name")
			local args = ArrayType()
			err = expect(c:is_valid() and c:get_method() == "get_backend_name" and c:callv(args) == backend_object:get_backend_name() and c:call() == backend_object:get_backend_name(), "callable mismatch")
			if err ~= "" then
				return err
			end
			c:call_deferred()
			err = expect(type(c.call_deferred) == "function", "callable call_deferred missing")
			if err ~= "" then
				return err
			end
		end

		do
			local Signal = load_type("Signal")
			local sig = Signal(backend_object, "script_changed")
			err = expect(sig:is_null() == false and sig:get_name() == "script_changed" and sig:get_object_id() == backend_object:get_instance_id(), "signal mismatch")
			if err ~= "" then
				return err
			end

			local sig_from_property = backend_object.script_changed
			err = expect(
				sig_from_property ~= nil and
				sig_from_property:is_null() == false and
				sig_from_property:get_name() == "script_changed" and
				sig_from_property:get_object_id() == backend_object:get_instance_id(),
				"signal property mismatch")
			if err ~= "" then
				return err
			end
		end

		do
			local ArrayType = load_type("Array")
			local arr = ArrayType()
			arr:append(1)
			arr:append("x")
			arr:set(0, 7)
			err = expect(arr:size() == 2 and arr:get(0) == 7 and arr:has("x"), "array mismatch")
			if err ~= "" then
				return err
			end
		end

		do
			local DictionaryType = load_type("Dictionary")
			local dict = DictionaryType()
			dict:set("answer", 42)
			dict:set("name", "godot")
			err = expect(dict:get("answer", nil) == 42 and dict:has("name") and dict:keys():size() == 2, "dictionary mismatch")
			if err ~= "" then
				return err
			end
		end

		err = expect_throws(function()
			return load_type("Basis")():tdotx(load_type("Vector2")(1.0, 2.0))
		end, "Argument type does not match", "direct method type rejection")
		if err ~= "" then
			return err
		end

		err = expect_throws(function()
			local Vector2 = load_type("Vector2")
			local Color = load_type("Color")
			local v = Vector2()
			return v.length(Color(0.1, 0.2, 0.3, 1.0))
		end, "Native object type does not match", "receiver type rejection")
		if err ~= "" then
			return err
		end

		err = expect_throws(function()
			local Signal = load_type("Signal")
			local Callable = load_type("Callable")
			local sig = Signal(backend_object, "script_changed")
			return sig.connect(Callable(backend_object, "get_backend_name"), 123)
		end, "Argument count does not match the bound signature", "overload receiver rejection")
		if err ~= "" then
			return err
		end

		err = expect_throws(function()
			local Signal = load_type("Signal")
			local sig = Signal(backend_object, "script_changed")
			return sig:connect({}, 0)
		end, "Argument type does not match", "overload plain table rejection")
		if err ~= "" then
			return err
		end

		err = expect_throws(function()
			local Signal = load_type("Signal")
			local sig = Signal(backend_object, "script_changed")
			return sig:connect()
		end, "Argument count does not match the bound signature", "overload arity rejection")
		if err ~= "" then
			return err
		end

		err = expect_throws(function()
			local Signal = load_type("Signal")
			local sig = Signal(backend_object, "script_changed")
			return sig:connect(backend_object, 0)
		end, "Argument type does not match", "overload native object rejection")
		if err ~= "" then
			return err
		end

		err = expect_throws(function()
			local Signal = load_type("Signal")
			local Vector2 = load_type("Vector2")
			local sig = Signal(backend_object, "script_changed")
			return sig:connect(Vector2(1.0, 2.0), 0)
		end, "Argument type does not match", "overload wrong builtin rejection")
		if err ~= "" then
			return err
		end

		err = expect_throws(function()
			local Vector2 = load_type("Vector2")
			return Vector2(1.0)
		end, "No constructor overload matches", "constructor arity rejection")
		if err ~= "" then
			return err
		end

		err = expect_throws(function()
			local Signal = load_type("Signal")
			local Vector2 = load_type("Vector2")
			return Signal(Vector2(1.0, 2.0), "script_changed")
		end, "No constructor overload matches", "constructor wrong object rejection")
		if err ~= "" then
			return err
		end

		do
			local Color = load_type("Color")
			local c = Color(0.1, 0.2, 0.3, 0.4)
			c.a = 1.0
			local white_with_alpha = Color(Color.WHITE, 0.5)
			err = expect(
				c.a == 1.0 and
					white_with_alpha.r == 1.0 and
					white_with_alpha.g == 1.0 and
					white_with_alpha.b == 1.0 and
					white_with_alpha.a == 0.5 and
					Color.TRANSPARENT.a == 0.0,
				"color property or constants mismatch"
			)
			if err ~= "" then
				return err
			end
			err = expect_throws(function()
				Color.WHITE = nil
			end, "read-only", "color constant readonly")
			if err ~= "" then
				return err
			end
		end

		return ""
	end,
}
