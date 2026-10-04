// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

(() => {
	function expect(condition, message) {
		if (!condition) throw new Error(message);
	}

	function expectThrows(fn, needle, label) {
		let caught;
		try { fn(); } catch (error) { caught = error; }
		expect(caught !== undefined, `${label}: no throw`);
		expect(String(caught).includes(needle), `${label}: ${caught}`);
	}

	function near(a, b, eps) {
		return Math.abs(a - b) <= eps;
	}

	globalThis.binding_cases = {
		exists() {
			expect(typeof load_type === "function" && typeof to_callable === "function", "global helper missing");

			expect(typeof load_type(backend_class_name) === "function", "backend class constructor missing");

			expectThrows(() => load_type("DefinitelyMissingType"), "Type not found", "missing type check");

			expect(load_type("Time").Month.MONTH_JANUARY === 1, "class enum mismatch");

			expect(typeof load_type("Vector2") === "function", "builtin class constructor missing");
		},

		backend_constructor() {
			expect(new (load_type(backend_class_name))().get_backend_id() === backend_object.get_backend_id(), "backend constructor mismatch");
		},

		global_scope() {
			const GlobalScope = load_type("GlobalScope");
			expect(typeof GlobalScope === "function", "GlobalScope type missing");

			expect(near(GlobalScope.sin(Math.PI * 0.5), 1.0, 1e-6), "GlobalScope utility mismatch");

			expect(typeof GlobalScope.Engine.get_frames_drawn === "function", "GlobalScope singleton mismatch");

			expect(GlobalScope.Key.KEY_ENTER > 0, "GlobalScope enum mismatch");

			expect(GlobalScope.Variant.Type.TYPE_INT === 2, "GlobalScope Variant enum mismatch");
		},

		reflected_objects() {
			const Image = load_type("Image");
			const image = Image.create_empty(4, 4, true, Image.Format.FORMAT_RGBA8);
			expect(image.has_mipmaps(), "image starts with mipmaps");
			expectThrows(() => image.clear_mipmaps(1), "Too many arguments", "void method arity rejection");
			expect(image.has_mipmaps(), "rejected call leaves mipmaps intact");
			expect(image.clear_mipmaps() === null, "void method returns null");
			expect(!image.has_mipmaps(), "void method clears mipmaps");

			const RandomNumberGenerator = load_type("RandomNumberGenerator");
			const rng = new RandomNumberGenerator();
			rng.seed = 13579;
			const sample = rng.randi();
			expect(rng.seed === 13579 && sample >= 0, "rng reflected binding mismatch");

			const NodeType = load_type("Node");
			const node = new NodeType();
			expect(node.get_class() === "Node" && node.get_child_count() === 0, "node reflected binding mismatch");
			expect(NodeType.NOTIFICATION_READY === 13, "node reflected constant mismatch");
			expectThrows(() => {
				NodeType.NOTIFICATION_READY = 99;
			}, "read-only", "node reflected constant readonly");

			const ObjectType = load_type("Object");
			const obj = new ObjectType();
			expect(
				obj.get_class() === "Object" &&
					obj.has_signal("script_changed") &&
					ObjectType.ConnectFlags.CONNECT_DEFERRED === 1 &&
					ObjectType.NOTIFICATION_PREDELETE === 1,
				"object reflected binding mismatch"
			);
			expect(obj.call("get_instance_id") === obj.get_instance_id(), "object vararg return mismatch");
			const scriptChanged = obj.script_changed;
			expect(
				typeof scriptChanged === "object" &&
					!scriptChanged.is_null() &&
					scriptChanged.get_name() === "script_changed" &&
					scriptChanged.get_object_id() === obj.get_instance_id(),
				"object reflected signal property mismatch"
			);
			expectThrows(() => {
				ObjectType.ConnectFlags.CONNECT_DEFERRED = 99;
			}, "read-only", "object reflected enum readonly");

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
			expect(
				graphNode.is_slot_enabled_left(0) && graphNode.is_slot_enabled_right(0),
				"reflected overflow arguments mismatch"
			);

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
			expect(
				buildBoxPlanes !== null &&
					planes &&
					planeCount > 0,
				`geometry3d reflected binding mismatch count=${planeCount} sizeType=${typeof (planes && planes.size)} lengthType=${typeof (planes && planes.length)}`
			);

			const ArrayType = load_type("Array");
			const CodeEdit = load_type("CodeEdit");
			const GlobalScope = load_type("GlobalScope");
			const prefixes = new ArrayType();
			prefixes.push_back("#");
			const codeEdit = new CodeEdit();
			codeEdit.set_auto_indent_prefixes(prefixes);
			const storedPrefixes = codeEdit.get_auto_indent_prefixes();
			expect(
				!prefixes.is_typed() &&
					storedPrefixes.is_typed() &&
					storedPrefixes.get_typed_builtin() === GlobalScope.Variant.Type.TYPE_STRING &&
					storedPrefixes.get(0) === "#",
				"untyped array conversion mismatch"
			);

			const FileDialog = load_type("FileDialog");
			const filters = new ArrayType();
			filters.push_back("*.txt");
			const fileDialog = new FileDialog();
			fileDialog.set_filters(filters);
			const storedFilters = fileDialog.get_filters();
			expect(
				storedFilters.size() === 1 && storedFilters.get(0) === "*.txt",
				"array to reflected packed array conversion mismatch"
			);

			const SystemFont = load_type("SystemFont");
			const fallbackFont = new SystemFont();
			const untypedFallbacks = new ArrayType();
			untypedFallbacks.push_back(fallbackFont);
			const font = new SystemFont();
			font.set_fallbacks(untypedFallbacks);
			const storedFallbacks = font.get_fallbacks();
			expect(
				storedFallbacks.is_typed() &&
					storedFallbacks.get_typed_builtin() === GlobalScope.Variant.Type.TYPE_OBJECT &&
					String(storedFallbacks.get_typed_class_name()) === "Font" &&
					storedFallbacks.get(0) === fallbackFont,
				"untyped object array conversion mismatch"
			);

			const DictionaryType = load_type("Dictionary");
			const bracePairs = new DictionaryType();
			bracePairs.set("(", ")");
			codeEdit.set_auto_brace_completion_pairs(bracePairs);
			expect(
				codeEdit.get_auto_brace_completion_pairs().get("(", null) === ")",
				"untyped dictionary argument mismatch"
			);

			const GraphEdit = load_type("GraphEdit");
			const typeNames = new DictionaryType();
			typeNames.set(1, "flow");
			const graphEdit = new GraphEdit();
			graphEdit.type_names = typeNames;
			const storedTypeNames = graphEdit.type_names;
			expect(
				!storedTypeNames.is_typed() &&
					storedTypeNames.get(1, null) === "flow",
				"untyped dictionary property passthrough mismatch"
			);

			const StyleBoxFlat = load_type("StyleBoxFlat");
			const styleBox = new StyleBoxFlat();
			styleBox.border_width_left = 7;
			expect(styleBox.border_width_left === 7, "indexed property fallback mismatch");

			expect(
				load_type("FileAccess").file_exists("res://project.godot") && load_type("FileAccess").get_size("res://project.godot") > 0,
				"static reflected binding mismatch"
			);
		},

		error_paths() {
			expect(load_type() === null, "load_type empty arg should return null");

			expectThrows(() => new (load_type("DirAccess"))(), "No constructor available", "missing constructor check");

			expectThrows(() => to_callable(), "expects exactly one argument", "to_callable arity rejection");

			expectThrows(() => to_callable({}), "expects a script function", "to_callable type rejection");

			expectThrows(() => to_callable(new (load_type("Callable"))()), "expects a script function", "to_callable Callable rejection");

			expectThrows(
				() => new (load_type("RandomNumberGenerator"))(1),
				"zero-argument construction",
				"constructor arg rejection"
			);

			expectThrows(() => {
				const RandomNumberGenerator = load_type("RandomNumberGenerator");
				const rng = new RandomNumberGenerator();
				rng.seed = "oops";
			}, "Invalid argument", "property type rejection");

			expectThrows(
				() => new (load_type("Object"))(1),
				"Argument count does not match the bound signature",
				"object static constructor arity rejection"
			);

			expectThrows(() => {
				load_type("Time").Month.MONTH_JANUARY = 2;
			}, "read-only", "readonly enum rejection");

			expectThrows(() => {
				const LightmapGIData = load_type("LightmapGIData");
				const data = new LightmapGIData();
				data.lightmap_textures = [];
			}, "read-only", "readonly property rejection");

		},

		static_argument_conversion() {
			const Vector2 = load_type("Vector2");
			const Vector2i = load_type("Vector2i");
			const vector = new Vector2(3, 4);
			expect(vector.dot(vector) === 25 && vector.dot(boxed_vector) === 25, "raw and boxed argument conversion");
			expect(vector.dot(new Vector2i(3, 4)) === 25 && vector.dot(boxed_int_vector) === 25, "cross-type argument conversion");
			const copy = new Vector2(vector);
			copy.x = 10;
			expect(vector.x === 3, "copy constructor keeps value independence");
			const normalized = vector.normalized();
			expect(near(normalized.length(), 1, 1e-6) && vector.length() === 5, "returned value keeps value independence");
			boxed_vector.x = 0;
			expect(boxed_vector.length() === 4, "boxed receiver mutation writes back");
			const ints = new (load_type("PackedInt32Array"))();
			ints.append(-3.75);
			ints.append(2147483648);
			expect(ints.get(0) === -3 && ints.get(1) === -2147483648, "numeric fallback preserves narrowing");
			expectThrows(() => ints.append("oops"), "Argument type does not match", "scalar type rejection");
			const floats = new (load_type("PackedFloat64Array"))();
			floats.append(3);
			floats.append(1.25);
			expect(floats.get(0) === 3 && floats.get(1) === 1.25, "integer and floating-point scalar conversion");
			const wide = new (load_type("PackedInt64Array"))();
			wide.append(4294967297n);
			floats.append(4294967297n);
			expect(BigInt(wide.get(0)) === 4294967297n && floats.get(2) === 4294967297, "64-bit integer fallback conversion");
			backend_object.set_block_signals(true);
			expect(backend_object.is_blocking_signals(), "boolean argument conversion");
			backend_object.set_block_signals(false);
		},

		builtin_static_binding() {
			expect(new (load_type("Vector2"))(3.0, 4.0).length() === 5.0, "vector2 constructor mismatch");
			const defaultLimited = new (load_type("Vector2"))(3.0, 4.0).limit_length();
			expect(near(defaultLimited.length(), 1.0, 1e-6), "instance method default argument mismatch");

			{
				const Vector2 = load_type("Vector2");
				expect(
					Vector2.ZERO.x === 0 && Vector2.ZERO.y === 0 && Vector2.ONE.x === 1 && Vector2.ONE.y === 1,
					"vector2 constants mismatch"
				);
				expectThrows(() => {
					Vector2.ZERO = null;
				}, "read-only", "vector2 constant readonly");
				expect(
					Vector2.Axis && Vector2.Axis.AXIS_X === 0 && Vector2.Axis.AXIS_Y === 1,
					"vector2 axis enum mismatch"
				);
				expectThrows(() => {
					Vector2.Axis = null;
				}, "read-only", "vector2 axis enum group readonly");
				expectThrows(() => {
					Vector2.Axis.AXIS_X = 7;
				}, "read-only", "vector2 axis enum readonly");
			}

			{
				const Vector2 = load_type("Vector2");
				const v = new Vector2();
				v.x = 8.0;
				v.y = 6.0;
				expect(v.length() === 10.0, "vector2 property mismatch");
			}

			{
				const Vector2 = load_type("Vector2");
				const Rect2 = load_type("Rect2");
				const rect = new Rect2(new Vector2(2.0, 3.0), new Vector2(4.0, 5.0));
				expect(rect.has_point(new Vector2(3.0, 4.0)) && rect.get_area() === 20.0 && rect.position.x === 2.0, "rect2 mismatch");
			}

			{
				const StringName = load_type("StringName");
				const name = new StringName("player");
				expect(name.contains("lay") && name.length() === 6, "string_name methods mismatch");
				expect(name.begins_with("pla") && name.ends_with("yer") && name.to_upper() === "PLAYER", "string_name extras mismatch");
				const parts = new StringName("left,right").split(",");
				expect(parts.size() === 2 && parts.get(0) === "left" && parts.get(1) === "right", "partial default arguments mismatch");
			}

			{
				const Color = load_type("Color");
				const red = Color.from_hsv(0.0, 1.0, 1.0);
				expect(near(red.r, 1.0, 1e-6) && near(red.a, 1.0, 1e-6), "static method default argument mismatch");
			}

			expect(new (load_type("RID"))().is_valid() === false, "rid default mismatch");

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

				expect(
					ints.get(0) === 7 && ints.has(7) && floats.get(0) === 1.5 && strings.get(0) === "godot" && vectors.get(0).y === 4.0 && near(colors.get(0).b, 0.3, 0.001),
					"packed arrays mismatch"
				);
			}

			{
				const PackedByteArray = load_type("PackedByteArray");
				const bytes = new PackedByteArray();
				bytes.append(103);
				bytes.append(111);
				bytes.append(100);
				bytes.append(111);
				bytes.append(116);
				expect(bytes.get(1) === 111 && bytes.get_string_from_utf8() === "godot" && bytes.hex_encode() === "676f646f74", "packed byte array mismatch");
			}

			{
				const Callable = load_type("Callable");
				const ArrayType = load_type("Array");
				const c = new Callable(backend_object, "get_backend_name");
				const args = new ArrayType();
				expect(c.is_valid() && c.get_method() === "get_backend_name" && c.callv(args) === backend_object.get_backend_name() && c.call() === backend_object.get_backend_name(), "callable mismatch");
				c.call_deferred();
				expect(typeof c.call_deferred === "function", "callable call_deferred missing");
				expect(args.reduce(c) === null, "array reduce Variant default mismatch");

				const add = to_callable((a, b) => a + b);
				expect(add.is_custom() && add.is_valid() && add.call(20, 22) === 42, "script callable mismatch");

				let signalCalls = 0;
				const onScriptChanged = () => {
					signalCalls += 1;
				};
				const callback = to_callable(onScriptChanged);
				const equivalentCallback = to_callable(onScriptChanged);
				const signal = backend_object.script_changed;
				expectThrows(() => signal.connect(onScriptChanged), "Argument type does not match", "implicit callable rejection");
				expect(signal.connect(callback) === 0 && signal.is_connected(equivalentCallback), "script callable connect mismatch");
				signal.emit();
				signal.disconnect(equivalentCallback);
				signal.emit();
				expect(signalCalls === 1 && !signal.is_connected(callback), "script callable disconnect mismatch");
			}

			{
				const Signal = load_type("Signal");
				const sig = new Signal(backend_object, "script_changed");
				expect(!sig.is_null() && sig.get_name() === "script_changed" && sig.get_object_id() === backend_object.get_instance_id(), "signal mismatch");

				const sigFromProperty = backend_object.script_changed;
				expect(
					typeof sigFromProperty === "object" &&
						!sigFromProperty.is_null() &&
						sigFromProperty.get_name() === "script_changed" &&
						sigFromProperty.get_object_id() === backend_object.get_instance_id(),
					"signal property mismatch"
				);
			}

			{
				const ArrayType = load_type("Array");
				const arr = new ArrayType();
				arr.append(1);
				arr.append("x");
				arr.set(0, 7);
				expect(arr.size() === 2 && arr.get(0) === 7 && arr.has("x"), "array mismatch");
			}

			{
				const ArrayType = load_type("Array");
				const PackedStringArray = load_type("PackedStringArray");
				const parts = new ArrayType();
				parts.push_back("a");
				parts.push_back("b");
				const packedParts = new PackedStringArray();
				packedParts.push_back("a");
				packedParts.push_back("b");
				const mergedParts = PackedStringArray.op_Addition(packedParts, parts);
				expect(
					mergedParts.size() === 4 && mergedParts.get(0) === "a" && mergedParts.get(3) === "b",
					"array to packed array conversion mismatch"
				);
			}

			{
				const DictionaryType = load_type("Dictionary");
				const dict = new DictionaryType();
				dict.set("answer", 42);
				dict.set("name", "godot");
				expect(dict.get("answer", null) === 42 && dict.has("name") && dict.keys().size() === 2, "dictionary mismatch");
				expect(dict.get("missing") === null, "dictionary get Variant default mismatch");
				expect(dict.get_or_add("inserted") === null && dict.has("inserted"), "dictionary get_or_add Variant default mismatch");
			}

			backend_object.set_meta("__puerts_default_arg_meta__", 42);
			expect(backend_object.get_meta("__puerts_default_arg_meta__") === 42, "object method default argument mismatch");
			backend_object.remove_meta("__puerts_default_arg_meta__");

			expectThrows(() => new (load_type("Basis"))().tdotx(new (load_type("Vector2"))(1.0, 2.0)), "Argument type does not match", "direct method type rejection");

			expectThrows(() => {
				const Vector2 = load_type("Vector2");
				const Color = load_type("Color");
				return Vector2.prototype.length.call(new Color(0.1, 0.2, 0.3, 1.0));
			}, "Native object type does not match", "receiver type rejection");

			expectThrows(() => {
				const Signal = load_type("Signal");
				const Callable = load_type("Callable");
				return Signal.prototype.connect.call(new Callable(backend_object, "get_backend_name"), 123);
			}, "Native object type does not match", "overload receiver rejection");

			expectThrows(() => {
				const Signal = load_type("Signal");
				const sig = new Signal(backend_object, "script_changed");
				return sig.connect({}, 0);
			}, "Argument type does not match", "overload plain object rejection");

			expectThrows(() => {
				const Signal = load_type("Signal");
				const sig = new Signal(backend_object, "script_changed");
				return sig.connect();
			}, "Argument count does not match", "overload arity rejection");

			expectThrows(() => {
				const Signal = load_type("Signal");
				const sig = new Signal(backend_object, "script_changed");
				return sig.connect(backend_object, 0);
			}, "Argument type does not match", "overload native object rejection");

			expectThrows(() => {
				const Signal = load_type("Signal");
				const Vector2 = load_type("Vector2");
				const sig = new Signal(backend_object, "script_changed");
				return sig.connect(new Vector2(1.0, 2.0), 0);
			}, "Argument type does not match", "overload wrong builtin rejection");

			expectThrows(() => {
				const Vector2 = load_type("Vector2");
				return new Vector2(1.0);
			}, "No constructor overload matches", "constructor arity rejection");

			expectThrows(() => {
				const Signal = load_type("Signal");
				const Vector2 = load_type("Vector2");
				return new Signal(new Vector2(1.0, 2.0), "script_changed");
			}, "No constructor overload matches", "constructor wrong object rejection");

			{
				const Color = load_type("Color");
				const c = new Color(0.1, 0.2, 0.3, 0.4);
				c.a = 1.0;
				const whiteWithAlpha = new Color(Color.WHITE, 0.5);
				expect(
					c.a === 1.0 &&
						whiteWithAlpha.r === 1.0 &&
						whiteWithAlpha.g === 1.0 &&
						whiteWithAlpha.b === 1.0 &&
						whiteWithAlpha.a === 0.5 &&
						Color.TRANSPARENT.a === 0.0,
					"color property or constants mismatch"
				);
				expectThrows(() => {
					Color.WHITE = null;
				}, "read-only", "color constant readonly");
			}

			{
				const ArrayType = load_type("Array");
				const Color = load_type("Color");
				const DictionaryType = load_type("Dictionary");
				const Vector2 = load_type("Vector2");

				const sum = Vector2.op_Addition(new Vector2(1.0, 2.0), new Vector2(3.0, 4.0));
				expect(sum.x === 4.0 && sum.y === 6.0, "vector2 op_Addition mismatch");

				const scaled = Vector2.op_Multiply(new Vector2(2.0, 3.0), 2.0);
				expect(scaled.x === 4.0 && scaled.y === 6.0, "vector2 op_Multiply mismatch");

				const negated = Vector2.op_UnaryNegation(new Vector2(2.0, -3.0));
				expect(negated.x === -2.0 && negated.y === 3.0, "vector2 op_UnaryNegation mismatch");

				expect(Vector2.op_Equality(new Vector2(5.0, 6.0), new Vector2(5.0, 6.0)) === true, "vector2 op_Equality mismatch");

				const arr = new ArrayType();
				arr.append(new Vector2(8.0, 9.0));
				expect(Vector2.op_In(new Vector2(8.0, 9.0), arr) === true, "vector2 op_In array mismatch");

				const dict = new DictionaryType();
				dict.set(new Vector2(1.0, 2.0), "hit");
				expect(Vector2.op_In(new Vector2(1.0, 2.0), dict) === true, "vector2 op_In dictionary mismatch");

				const merged = ArrayType.op_Addition(arr, arr);
				expect(merged.size() === 2 && merged.get(0).x === 8.0 && merged.get(1).y === 9.0, "array op_Addition mismatch");

				const tinted = Color.op_Multiply(new Color(0.25, 0.5, 0.75, 1.0), 2.0);
				expect(near(tinted.r, 0.5, 0.001) && near(tinted.g, 1.0, 0.001), "color op_Multiply mismatch");

				expect(typeof Vector2.op_Addition === "function" && typeof Color.op_Multiply === "function", "operator reflection mismatch");
			}

		}
	};
})();
