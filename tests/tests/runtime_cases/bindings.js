// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: Apache-2.0

(() => {
	function expect(condition, message) {
		if (!condition) throw new Error(message);
	}

	function expectThrows(fn, needle, label) {
		try {
			fn();
		} catch (error) {
			expect(String(error).includes(needle), `${label}: ${error}`);
			return;
		}
		throw new Error(`${label}: no throw`);
	}

	globalThis.binding_cases = {
		type_loading() {
			const Backend = load_type(backend_class_name);
			expect(new Backend().get_backend_id() === backend_object.get_backend_id(), "registered backend constructor");
			expectThrows(() => load_type("DefinitelyMissingType"), "Type not found", "missing type");

			const GlobalScope = load_type("GlobalScope");
			expect(Math.abs(GlobalScope.sin(Math.PI / 2) - 1) < 1e-6, "global utility");
			const frames = GlobalScope.Engine.get_frames_drawn();
			expect(frames != null && frames >= 0, "global singleton");
			expect(GlobalScope.Variant.Type.TYPE_INT === 2, "nested global enum");
		},

		reflected_objects() {
			const Image = load_type("Image");
			const image = Image.create_empty(4, 4, true, Image.Format.FORMAT_RGBA8);
			expectThrows(() => image.clear_mipmaps(1), "Too many arguments", "reflected arity rejection");
			expect(image.has_mipmaps(), "rejected call leaves receiver intact");
			expect(image.clear_mipmaps() === null && !image.has_mipmaps(), "reflected void method");

			const graphNode = new (load_type("GraphNode"))();
			const icon = new (load_type("GradientTexture2D"))();
			const Color = load_type("Color");
			graphNode.set_slot(0, true, 1, Color.WHITE, true, 2, Color.WHITE, icon, icon, false);
			expect(graphNode.get_slot_custom_icon_right(0) === icon && !graphNode.is_slot_draw_stylebox(0), "reflected arguments exceed inline capacity");

			const style = new (load_type("StyleBoxFlat"))();
			style.border_width_left = 7;
			expect(style.border_width_left === 7, "indexed property fallback");
		},

		container_conversion() {
			const ArrayType = load_type("Array");
			const CodeEdit = load_type("CodeEdit");
			const types = load_type("GlobalScope").Variant.Type;
			const editor = new CodeEdit();
			const prefixes = new ArrayType();
			prefixes.push_back("#");
			editor.set_auto_indent_prefixes(prefixes);
			const typed = editor.get_auto_indent_prefixes();
			expect(!prefixes.is_typed() && typed.get_typed_builtin() === types.TYPE_STRING && typed.get(0) === "#", "untyped array to typed strings");

			const SystemFont = load_type("SystemFont");
			const fallback = new SystemFont();
			const font = new SystemFont();
			const fonts = new ArrayType();
			fonts.push_back(fallback);
			font.set_fallbacks(fonts);
			const fallbacks = font.get_fallbacks();
			expect(String(fallbacks.get_typed_class_name()) === "Font" && fallbacks.get(0) === fallback, "untyped array to typed objects");

			const Dictionary = load_type("Dictionary");
			const pairs = new Dictionary();
			pairs.set("(", ")");
			editor.set_auto_brace_completion_pairs(pairs);
			expect(editor.get_auto_brace_completion_pairs().get("(") === ")", "reflected dictionary argument");

			const PackedStringArray = load_type("PackedStringArray");
			const packed = new PackedStringArray();
			packed.append("!");
			const joined = PackedStringArray.op_Addition(packed, prefixes);
			expect(joined.size() === 2 && joined.get(1) === "#", "Array to packed array in generated binding");

			const bytes = new (load_type("PackedByteArray"))();
			bytes.append(65);
			bytes.append(0);
			expect(bytes.get(0) === 65 && bytes.hex_encode() === "4100", "packed bytes round trip through generated methods");
		},

		builtin_bindings() {
			const Vector2 = load_type("Vector2");
			const vector = new Vector2(3, 4);
			expect(Math.abs(vector.limit_length().length() - 1) < 1e-6, "instance method default argument");
			const Color = load_type("Color");
			expect(Color.from_hsv(0, 1, 1).a === 1, "static method default argument");
			const dict = new (load_type("Dictionary"))();
			expect(dict.get_or_add("missing") === null && dict.has("missing"), "explicit Variant default");
			expect(Vector2.ZERO.x === 0 && Vector2.Axis === load_type("Vector2.Axis"), "constant and enum scope identity");
			expectThrows(() => { Vector2.Axis.AXIS_X = 7; }, "read-only", "enum member");
		},

		static_argument_conversion() {
			const Vector2 = load_type("Vector2");
			const vector = new Vector2(3, 4);
			expect(vector.dot(boxed_vector) === 25 && vector.dot(boxed_int_vector) === 25, "boxed and cross-type arguments");
			const copy = new Vector2(vector);
			copy.x = 10;
			expect(vector.x === 3, "copy constructor preserves value independence");
			boxed_vector.x = 0;
			expect(boxed_vector.length() === 4, "boxed receiver mutation");

			const ints = new (load_type("PackedInt32Array"))();
			ints.append(-3.75);
			ints.append(2147483648);
			expect(ints.get(0) === -3 && ints.get(1) === -2147483648, "numeric narrowing");
			const wide = new (load_type("PackedInt64Array"))();
			wide.append(4294967297);
			expect(BigInt(wide.get(0)) === 4294967297n, "64-bit integer conversion");
			const vectors = new (load_type("PackedVector2Array"))();
			vectors.append(vector);
			expect(vectors.get(0).y === 4, "packed array boxes value elements");
		},

		binding_contracts() {
			const ObjectType = load_type("Object");
			const generator = new (load_type("RandomNumberGenerator"))();
			expect(generator instanceof ObjectType, "canonical base binding");
			expect(ObjectType.prototype.get_class.call(generator) === "RandomNumberGenerator", "base method accepts reflected receiver");
			expect(generator.call("get_instance_id") === generator.get_instance_id(), "object vararg method");
			expect(load_type("GlobalScope").instance_from_id(generator.get_instance_id()) === generator, "object wrapper identity");

			const Vector2 = load_type("Vector2");
			const Vector2i = load_type("Vector2i");
			const vector = new Vector2i(2, 4);
			const fractional = Vector2i.op_Multiply(vector, 0.5);
			const integral = Vector2i.op_Multiply(vector, 2);
			expect(fractional instanceof Vector2 && fractional.x === 1 && fractional.y === 2, "fractional overload");
			expect(integral instanceof Vector2i && integral.x === 4 && integral.y === 8, "integer overload");
			expect(vector.x === 2 && vector.y === 4, "operator preserves source value");

			const signal = backend_object.script_changed;
			expect(signal.get_object_id() === backend_object.get_instance_id(), "bound signal property");
			let calls = 0;
			const onSignal = () => { calls += 1; };
			const callback = to_callable(onSignal);
			const equivalent = to_callable(onSignal);
			expect(signal.connect(callback) === 0 && signal.is_connected(equivalent), "script Callable equality");
			signal.emit();
			signal.disconnect(equivalent);
			signal.emit();
			expect(calls === 1, "equivalent Callable disconnects the signal");
		},

		binding_errors() {
			const Vector2 = load_type("Vector2");
			expectThrows(() => new Vector2(1), "No constructor overload matches", "constructor arity");
			expectThrows(() => Vector2.prototype.length.call(load_type("Color").WHITE), "Native object type does not match", "wrong receiver");
			expectThrows(() => backend_object.script_changed.connect({}, 0), "Argument type does not match", "overloaded argument type");
			expectThrows(() => to_callable({}), "expects a script function", "Callable input type");
		},
	};
})();
