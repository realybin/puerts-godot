// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

(() => {
	const root = globalThis.__puerts_cases || (globalThis.__puerts_cases = {});

	function expect(condition, message) {
		return condition ? "" : message;
	}

	function expectThrows(fn, needle, label) {
		try {
			fn();
			return `${label}: no throw`;
		} catch (error) {
			const text = String(error);
			if (text.indexOf(needle) === -1) {
				return `${label}: ${text}`;
			}
			return "";
		}
	}

	function near(a, b, eps) {
		return Math.abs(a - b) <= eps;
	}

	root.load_type = {
		exists() {
			let error = expect(typeof load_type === "function", "load_type function missing");
			if (error) {
				return error;
			}

			error = expect(typeof load_type(backend_class_name) === "function", "backend class constructor missing");
			if (error) {
				return error;
			}

			error = expectThrows(() => load_type("DefinitelyMissingType"), "Type not found", "missing type check");
			if (error) {
				return error;
			}

			error = expect(load_type("Time").Month.MONTH_JANUARY === 1, "class enum mismatch");
			if (error) {
				return error;
			}

			return expect(typeof load_type("Vector2") === "function", "builtin class constructor missing");
		},

		backend_constructor() {
			return expect(new (load_type(backend_class_name))().get_backend_id() === backend_object.get_backend_id(), "backend constructor mismatch");
		},

		global_scope() {
			const GlobalScope = load_type("GlobalScope");
			let error = expect(typeof GlobalScope === "function", "GlobalScope type missing");
			if (error) {
				return error;
			}

			error = expect(near(GlobalScope.sin(Math.PI * 0.5), 1.0, 1e-6), "GlobalScope utility mismatch");
			if (error) {
				return error;
			}

			error = expect(typeof GlobalScope.Engine.get_frames_drawn === "function", "GlobalScope singleton mismatch");
			if (error) {
				return error;
			}

			error = expect(GlobalScope.Key.KEY_ENTER > 0, "GlobalScope enum mismatch");
			if (error) {
				return error;
			}

			return expect(GlobalScope.Variant.Type.TYPE_INT === 2, "GlobalScope Variant enum mismatch");
		},

		reflected_objects() {
			const RandomNumberGenerator = load_type("RandomNumberGenerator");
			const rng = new RandomNumberGenerator();
			rng.seed = 13579;
			const sample = rng.randi();
			let error = expect(rng.seed === 13579 && sample >= 0, "rng reflected binding mismatch");
			if (error) {
				return error;
			}

			const NodeType = load_type("Node");
			const node = new NodeType();
			error = expect(node.get_class() === "Node" && node.get_child_count() === 0, "node reflected binding mismatch");
			if (error) {
				return error;
			}
			error = expect(NodeType.NOTIFICATION_READY === 13, "node reflected constant mismatch");
			if (error) {
				return error;
			}
			error = expectThrows(() => {
				NodeType.NOTIFICATION_READY = 99;
			}, "read-only", "node reflected constant readonly");
			if (error) {
				return error;
			}

			const ObjectType = load_type("Object");
			const obj = new ObjectType();
			error = expect(
				obj.get_class() === "Object" &&
					obj.has_signal("script_changed") &&
					ObjectType.ConnectFlags.CONNECT_DEFERRED === 1 &&
					ObjectType.NOTIFICATION_PREDELETE === 1,
				"object reflected binding mismatch"
			);
			if (error) {
				return error;
			}
			error = expect(obj.call("get_instance_id") === obj.get_instance_id(), "object vararg return mismatch");
			if (error) {
				return error;
			}
			const scriptChanged = obj.script_changed;
			error = expect(
				typeof scriptChanged === "object" &&
					!scriptChanged.is_null() &&
					scriptChanged.get_name() === "script_changed" &&
					scriptChanged.get_object_id() === obj.get_instance_id(),
				"object reflected signal property mismatch"
			);
			if (error) {
				return error;
			}
			error = expectThrows(() => {
				ObjectType.ConnectFlags.CONNECT_DEFERRED = 99;
			}, "read-only", "object reflected enum readonly");
			if (error) {
				return error;
			}

			const GraphNode = load_type("GraphNode");
			const GradientTexture2D = load_type("GradientTexture2D");
			const Color = load_type("Color");
			const graphNode = new GraphNode();
			const slotIcon = new GradientTexture2D();
			graphNode.set_slot(
				0,
				true,
				1,
				new Color(1, 0, 0, 1),
				true,
				2,
				new Color(0, 1, 0, 1),
				slotIcon,
				slotIcon,
				true
			);
			error = expect(
				graphNode.is_slot_enabled_left(0) && graphNode.is_slot_enabled_right(0),
				"reflected overflow arguments mismatch"
			);
			if (error) {
				return error;
			}

			const Geometry3DType = load_type("Geometry3D");
			const geometry = new Geometry3DType();
			const Vector3 = load_type("Vector3");
			const buildBoxPlanes =
				typeof geometry.build_box_planes === "function"
					? geometry.build_box_planes.bind(geometry)
					: typeof Geometry3DType.build_box_planes === "function"
						? Geometry3DType.build_box_planes.bind(Geometry3DType)
						: null;
			const planes = buildBoxPlanes ? buildBoxPlanes(new Vector3(1, 2, 3)) : null;
			const planeCount =
				planes == null
					? -1
					: typeof planes.size === "function"
						? planes.size()
						: typeof planes.length === "number"
							? planes.length
							: -1;
			error = expect(
				buildBoxPlanes !== null &&
					planes &&
					planeCount > 0,
				`geometry3d reflected binding mismatch count=${planeCount} sizeType=${typeof (planes && planes.size)} lengthType=${typeof (planes && planes.length)}`
			);
			if (error) {
				return error;
			}

			return expect(
				load_type("FileAccess").file_exists("res://project.godot") && load_type("FileAccess").get_size("res://project.godot") > 0,
				"static reflected binding mismatch"
			);
		},

		error_paths() {
			let error = expect(load_type() === null, "load_type empty arg should return null");
			if (error) {
				return error;
			}

			error = expectThrows(() => new (load_type("DirAccess"))(), "No constructor available", "missing constructor check");
			if (error) {
				return error;
			}

			error = expectThrows(
				() => new (load_type("RandomNumberGenerator"))(1),
				"zero-argument construction",
				"constructor arg rejection"
			);
			if (error) {
				return error;
			}

			error = expectThrows(() => {
				const RandomNumberGenerator = load_type("RandomNumberGenerator");
				const rng = new RandomNumberGenerator();
				rng.seed = "oops";
			}, "Property type does not match", "property type rejection");
			if (error) {
				return error;
			}

			error = expectThrows(
				() => new (load_type("Object"))(1),
				"Argument count does not match the bound signature",
				"object static constructor arity rejection"
			);
			if (error) {
				return error;
			}

			error = expectThrows(() => {
				const Theme = load_type("Theme");
				const theme = new Theme();
				theme.default_font = backend_object;
			}, "Property type does not match", "object property type rejection");
			if (error) {
				return error;
			}

			error = expectThrows(() => {
				load_type("Time").Month.MONTH_JANUARY = 2;
			}, "read-only", "readonly enum rejection");
			if (error) {
				return error;
			}

			error = expectThrows(() => {
				const LightmapGIData = load_type("LightmapGIData");
				const data = new LightmapGIData();
				data.lightmap_textures = [];
			}, "read-only", "readonly property rejection");
			if (error) {
				return error;
			}

			return "";
		},

		builtin_static_binding() {
			let error = expect(new (load_type("Vector2"))(3.0, 4.0).length() === 5.0, "vector2 constructor mismatch");
			if (error) {
				return error;
			}

			{
				const Vector2 = load_type("Vector2");
				error = expect(
					Vector2.ZERO.x === 0 && Vector2.ZERO.y === 0 && Vector2.ONE.x === 1 && Vector2.ONE.y === 1,
					"vector2 constants mismatch"
				);
				if (error) {
					return error;
				}
				error = expectThrows(() => {
					Vector2.ZERO = null;
				}, "read-only", "vector2 constant readonly");
				if (error) {
					return error;
				}
				error = expect(
					Vector2.Axis && Vector2.Axis.AXIS_X === 0 && Vector2.Axis.AXIS_Y === 1,
					"vector2 axis enum mismatch"
				);
				if (error) {
					return error;
				}
				error = expectThrows(() => {
					Vector2.Axis = null;
				}, "read-only", "vector2 axis enum group readonly");
				if (error) {
					return error;
				}
				error = expectThrows(() => {
					Vector2.Axis.AXIS_X = 7;
				}, "read-only", "vector2 axis enum readonly");
				if (error) {
					return error;
				}
			}

			{
				const Projection = load_type("Projection");
				error = expect(
					Projection.Planes && Projection.Planes.PLANE_NEAR === 0 && Projection.Planes.PLANE_BOTTOM === 5,
					"projection planes enum mismatch"
				);
				if (error) {
					return error;
				}
				error = expectThrows(() => {
					Projection.Planes.PLANE_NEAR = 99;
				}, "read-only", "projection planes enum readonly");
				if (error) {
					return error;
				}
			}

			{
				const Vector4 = load_type("Vector4");
				error = expect(
					Vector4.Axis && Vector4.Axis.AXIS_X === 0 && Vector4.Axis.AXIS_W === 3,
					"vector4 axis enum mismatch"
				);
				if (error) {
					return error;
				}
			}

			{
				const Vector2 = load_type("Vector2");
				const v = new Vector2();
				v.x = 8.0;
				v.y = 6.0;
				error = expect(v.length() === 10.0, "vector2 property mismatch");
				if (error) {
					return error;
				}
			}

			{
				const Vector3 = load_type("Vector3");
				const v = new Vector3(1.0, 2.0, 3.0);
				v.z = 4.0;
				error = expect(v.dot(new Vector3(2.0, 0.0, 1.0)) === 6.0, "vector3 method mismatch");
				if (error) {
					return error;
				}
			}

			{
				const Vector2 = load_type("Vector2");
				const Rect2 = load_type("Rect2");
				const rect = new Rect2(new Vector2(2.0, 3.0), new Vector2(4.0, 5.0));
				error = expect(rect.has_point(new Vector2(3.0, 4.0)) && rect.get_area() === 20.0 && rect.position.x === 2.0, "rect2 mismatch");
				if (error) {
					return error;
				}
			}

			{
				const StringName = load_type("StringName");
				const name = new StringName("player");
				error = expect(name.contains("lay") && name.length() === 6, "string_name methods mismatch");
				if (error) {
					return error;
				}
				error = expect(name.begins_with("pla") && name.ends_with("yer") && name.to_upper() === "PLAYER", "string_name extras mismatch");
				if (error) {
					return error;
				}
			}

			{
				const Vector2i = load_type("Vector2i");
				const v = new Vector2i(6, 8);
				error = expect(Math.floor(v.length()) === 10 && v.x === 6 && v.y === 8, "vector2i mismatch");
				if (error) {
					return error;
				}
			}

			{
				const NodePath = load_type("NodePath");
				const path = new NodePath("Root/Child:leaf");
				error = expect(path.get_name_count() === 2 && path.get_subname_count() === 1, "node_path methods mismatch");
				if (error) {
					return error;
				}

				const absPath = new NodePath("/Root/Child:leaf");
				error = expect(absPath.is_absolute() && absPath.get_name(0) === "Root" && absPath.get_subname(0) === "leaf", "node_path extras mismatch");
				if (error) {
					return error;
				}
			}

			{
				const Transform2D = load_type("Transform2D");
				const Vector2 = load_type("Vector2");
				const t = new Transform2D(new Vector2(1.0, 0.0), new Vector2(0.0, 1.0), new Vector2(2.0, 3.0));
				const determinant = typeof t.determinant === "function" ? t.determinant() : t.basis_determinant();
				error = expect(t.origin.x === 2.0 && t.origin.y === 3.0 && determinant === 1.0, "transform2d mismatch");
				if (error) {
					return error;
				}
			}

			{
				const Vector3 = load_type("Vector3");
				const Plane = load_type("Plane");
				const plane = new Plane(new Vector3(0.0, 1.0, 0.0), 2.0);
				error = expect(plane.has_point(new Vector3(0.0, 2.0, 0.0), 0.001) && plane.distance_to(new Vector3(0.0, 5.0, 0.0)) === 3.0, "plane mismatch");
				if (error) {
					return error;
				}
			}

			{
				const Vector3 = load_type("Vector3");
				const Basis = load_type("Basis");
				const Transform3D = load_type("Transform3D");
				const t = new Transform3D(new Basis(), new Vector3(1.0, 2.0, 3.0));
				const inv = t.inverse();
				error = expect(inv.origin.x === -1.0 && inv.origin.y === -2.0 && inv.origin.z === -3.0, "transform3d inverse mismatch");
				if (error) {
					return error;
				}

				const moved = t.translated_local(new Vector3(1.0, 0.0, 0.0));
				error = expect(moved.origin.x === 2.0 && moved.origin.y === 2.0 && moved.origin.z === 3.0, "transform3d extras mismatch");
				if (error) {
					return error;
				}
			}

			{
				const Projection = load_type("Projection");
				error = expect(Projection.create_orthogonal(-1.0, 1.0, -1.0, 1.0, 0.1, 10.0).determinant() !== 0.0, "projection orthogonal mismatch");
				if (error) {
					return error;
				}
				const p = Projection.create_perspective(75.0, 1.0, 0.1, 10.0, false);
				error = expect(near(p.get_z_near(), 0.1, 0.001) && !p.is_orthogonal() && p.inverse().determinant() !== 0.0, "projection perspective mismatch");
				if (error) {
					return error;
				}
			}

			error = expect(new (load_type("RID"))().is_valid() === false, "rid default mismatch");
			if (error) {
				return error;
			}

			{
				const PackedInt32Array = load_type("PackedInt32Array");
				const PackedFloat64Array = load_type("PackedFloat64Array");
				const PackedStringArray = load_type("PackedStringArray");
				const PackedVector2Array = load_type("PackedVector2Array");
				const PackedColorArray = load_type("PackedColorArray");
				const Vector2 = load_type("Vector2");
				const Color = load_type("Color");

				const ints = new PackedInt32Array();
				ints.append(3);
				ints.set(0, 7);
				const floats = new PackedFloat64Array();
				floats.append(1.5);
				const strings = new PackedStringArray();
				strings.append("godot");
				const vectors = new PackedVector2Array();
				vectors.append(new Vector2(2.0, 4.0));
				const colors = new PackedColorArray();
				colors.append(new Color(0.1, 0.2, 0.3, 1.0));

				error = expect(
					ints.get(0) === 7 && ints.has(7) && floats.get(0) === 1.5 && strings.get(0) === "godot" && vectors.get(0).y === 4.0 && near(colors.get(0).b, 0.3, 0.001),
					"packed arrays mismatch"
				);
				if (error) {
					return error;
				}
			}

			{
				const PackedByteArray = load_type("PackedByteArray");
				const bytes = new PackedByteArray();
				bytes.append(103);
				bytes.append(111);
				bytes.append(100);
				bytes.append(111);
				bytes.append(116);
				error = expect(bytes.get(1) === 111 && bytes.get_string_from_utf8() === "godot" && bytes.hex_encode() === "676f646f74", "packed byte array mismatch");
				if (error) {
					return error;
				}
			}

			{
				const Callable = load_type("Callable");
				const ArrayType = load_type("Array");
				const c = new Callable(backend_object, "get_backend_name");
				const args = new ArrayType();
				error = expect(c.is_valid() && c.get_method() === "get_backend_name" && c.callv(args) === backend_object.get_backend_name() && c.call() === backend_object.get_backend_name(), "callable mismatch");
				if (error) {
					return error;
				}
				c.call_deferred();
				error = expect(typeof c.call_deferred === "function", "callable call_deferred missing");
				if (error) {
					return error;
				}
			}

			{
				const Signal = load_type("Signal");
				const sig = new Signal(backend_object, "script_changed");
				error = expect(!sig.is_null() && sig.get_name() === "script_changed" && sig.get_object_id() === backend_object.get_instance_id(), "signal mismatch");
				if (error) {
					return error;
				}

				const sigFromProperty = backend_object.script_changed;
				error = expect(
					typeof sigFromProperty === "object" &&
						!sigFromProperty.is_null() &&
						sigFromProperty.get_name() === "script_changed" &&
						sigFromProperty.get_object_id() === backend_object.get_instance_id(),
					"signal property mismatch"
				);
				if (error) {
					return error;
				}
			}

			{
				const ArrayType = load_type("Array");
				const arr = new ArrayType();
				arr.append(1);
				arr.append("x");
				arr.set(0, 7);
				error = expect(arr.size() === 2 && arr.get(0) === 7 && arr.has("x"), "array mismatch");
				if (error) {
					return error;
				}
			}

			{
				const DictionaryType = load_type("Dictionary");
				const dict = new DictionaryType();
				dict.set("answer", 42);
				dict.set("name", "godot");
				error = expect(dict.get("answer", null) === 42 && dict.has("name") && dict.keys().size() === 2, "dictionary mismatch");
				if (error) {
					return error;
				}
			}

			error = expectThrows(() => new (load_type("Basis"))().tdotx(new (load_type("Vector2"))(1.0, 2.0)), "Argument type does not match", "direct method type rejection");
			if (error) {
				return error;
			}

			error = expectThrows(() => {
				const Vector2 = load_type("Vector2");
				const Color = load_type("Color");
				return Vector2.prototype.length.call(new Color(0.1, 0.2, 0.3, 1.0));
			}, "Native object type does not match", "receiver type rejection");
			if (error) {
				return error;
			}

			error = expectThrows(() => {
				const Signal = load_type("Signal");
				const Callable = load_type("Callable");
				return Signal.prototype.connect.call(new Callable(backend_object, "get_backend_name"), 123);
			}, "Argument count does not match the bound signature", "overload receiver rejection");
			if (error) {
				return error;
			}

			error = expectThrows(() => {
				const Signal = load_type("Signal");
				const sig = new Signal(backend_object, "script_changed");
				return sig.connect({}, 0);
			}, "Argument type does not match", "overload plain object rejection");
			if (error) {
				return error;
			}

			error = expectThrows(() => {
				const Signal = load_type("Signal");
				const sig = new Signal(backend_object, "script_changed");
				return sig.connect();
			}, "Argument count does not match the bound signature", "overload arity rejection");
			if (error) {
				return error;
			}

			error = expectThrows(() => {
				const Signal = load_type("Signal");
				const sig = new Signal(backend_object, "script_changed");
				return sig.connect(backend_object, 0);
			}, "Argument type does not match", "overload native object rejection");
			if (error) {
				return error;
			}

			error = expectThrows(() => {
				const Signal = load_type("Signal");
				const Vector2 = load_type("Vector2");
				const sig = new Signal(backend_object, "script_changed");
				return sig.connect(new Vector2(1.0, 2.0), 0);
			}, "Argument type does not match", "overload wrong builtin rejection");
			if (error) {
				return error;
			}

			error = expectThrows(() => {
				const Vector2 = load_type("Vector2");
				return new Vector2(1.0);
			}, "No constructor overload matches", "constructor arity rejection");
			if (error) {
				return error;
			}

			error = expectThrows(() => {
				const Signal = load_type("Signal");
				const Vector2 = load_type("Vector2");
				return new Signal(new Vector2(1.0, 2.0), "script_changed");
			}, "No constructor overload matches", "constructor wrong object rejection");
			if (error) {
				return error;
			}

			{
				const Color = load_type("Color");
				const c = new Color(0.1, 0.2, 0.3, 0.4);
				c.a = 1.0;
				const whiteWithAlpha = new Color(Color.WHITE, 0.5);
				error = expect(
					c.a === 1.0 &&
						whiteWithAlpha.r === 1.0 &&
						whiteWithAlpha.g === 1.0 &&
						whiteWithAlpha.b === 1.0 &&
						whiteWithAlpha.a === 0.5 &&
						Color.TRANSPARENT.a === 0.0,
					"color property or constants mismatch"
				);
				if (error) {
					return error;
				}
				error = expectThrows(() => {
					Color.WHITE = null;
				}, "read-only", "color constant readonly");
				if (error) {
					return error;
				}
			}

			{
				const ArrayType = load_type("Array");
				const Color = load_type("Color");
				const DictionaryType = load_type("Dictionary");
				const Transform2D = load_type("Transform2D");
				const Vector2 = load_type("Vector2");

				const sum = Vector2.op_Addition(new Vector2(1.0, 2.0), new Vector2(3.0, 4.0));
				error = expect(sum.x === 4.0 && sum.y === 6.0, "vector2 op_Addition mismatch");
				if (error) {
					return error;
				}

				const scaled = Vector2.op_Multiply(new Vector2(2.0, 3.0), 2.0);
				error = expect(scaled.x === 4.0 && scaled.y === 6.0, "vector2 op_Multiply mismatch");
				if (error) {
					return error;
				}

				const negated = Vector2.op_UnaryNegation(new Vector2(2.0, -3.0));
				error = expect(negated.x === -2.0 && negated.y === 3.0, "vector2 op_UnaryNegation mismatch");
				if (error) {
					return error;
				}

				error = expect(Vector2.op_Equality(new Vector2(5.0, 6.0), new Vector2(5.0, 6.0)) === true, "vector2 op_Equality mismatch");
				if (error) {
					return error;
				}

				const arr = new ArrayType();
				arr.append(new Vector2(8.0, 9.0));
				error = expect(Vector2.op_In(new Vector2(8.0, 9.0), arr) === true, "vector2 op_In array mismatch");
				if (error) {
					return error;
				}

				const dict = new DictionaryType();
				dict.set(new Vector2(1.0, 2.0), "hit");
				error = expect(Vector2.op_In(new Vector2(1.0, 2.0), dict) === true, "vector2 op_In dictionary mismatch");
				if (error) {
					return error;
				}

				const merged = ArrayType.op_Addition(arr, arr);
				error = expect(merged.size() === 2 && merged.get(0).x === 8.0 && merged.get(1).y === 9.0, "array op_Addition mismatch");
				if (error) {
					return error;
				}

				const tinted = Color.op_Multiply(new Color(0.25, 0.5, 0.75, 1.0), 2.0);
				error = expect(near(tinted.r, 0.5, 0.001) && near(tinted.g, 1.0, 0.001), "color op_Multiply mismatch");
				if (error) {
					return error;
				}

				error = expect(typeof Vector2.op_Addition === "function" && typeof Color.op_Multiply === "function", "operator reflection mismatch");
				if (error) {
					return error;
				}
			}

			return "";
		}
	};
})();
