import assert from "node:assert/strict";
import test from "node:test";

import { generateClassBinding } from "../dist/class_codegen.js";
import { collectPropertyBindings } from "../dist/property_bindings.js";

function generateMethods(methods) {
	return generateClassBinding({ name: "Example", is_instantiable: false }, "classes", methods, [], [], []).code;
}

test("API defaults produce every supported arity without depending on C++ header declarations", () => {
	const code = generateMethods([{
		name: "configure", is_const: true,
		arguments: [{ name: "enabled", type: "bool" }, { name: "flags", type: "bitfield::Object.ConnectFlags", default_value: "0" }],
	}]);
	assert.match(code, /configure_overload0_arity1\(const godot::Example &p_self, bool p_arg_0\)/);
	assert.match(code, /configure_overload0_arity2\(const godot::Example &p_self, bool p_arg_0, godot::BitField<godot::Object::ConnectFlags> p_arg_1\)/);
	assert.match(code, /return p_self.configure\(p_arg_0\);/);
	assert.match(code, /return p_self.configure\(p_arg_0, p_arg_1\);/);
});

test("mixed static and instance signatures fail rather than discard overloads", () => {
	assert.throws(() => generateMethods([{ name: "read", is_static: true }, { name: "read" }]), /Mixed static and instance overloads/);
});

test("stale property adapters fail rather than silently omit the property", () => {
	const rules = new Map([["Example", [{ class: "Example", name: "missing", type: "int", kind: "member_expr", member_expr: "value" }]]]);
	assert.throws(() => collectPropertyBindings({ name: "Example", members: [] }, rules, "generated"), /Property rule does not match/);
});

test("explicit C++ adapters preserve the script signature", () => {
	const methods = [{ name: "grow_side", is_const: true, arguments: [{ name: "side", type: "int" }] }];
	const adapters = new Map([["Example.grow_side", { cpp_name: "grow_side_bind", native_signature: "godot::Rect2 (godot::Example::*)(godot::Side) const" }]]);
	const code = generateClassBinding({ name: "Example", is_instantiable: false }, "classes", methods, [], [], [], adapters).code;
	assert.match(code, /int64_t p_arg_0/);
	assert.ok(code.includes("decltype(static_cast<godot::Rect2 (godot::Example::*)(godot::Side) const>(&godot::Example::grow_side_bind))"));
	assert.ok(code.includes("static_cast<puerts::internal::native_parameter_t<"));
});

test("script scalar boundaries preserve double precision and signed 64-bit returns", () => {
	const code = generateMethods([{ name: "convert", return_type: "int", arguments: [{ name: "value", type: "float" }] }]);
	assert.match(code, /double p_arg_0/);
	assert.ok(code.includes("static_cast<int64_t>(p_self.convert(static_cast<puerts::internal::native_parameter_t<decltype(&godot::Example::convert), 0>>(p_arg_0)))"));
});

test("scalar properties convert using their native member or setter parameter types", () => {
 const rules = new Map([["Example", [
  { class: "Example", name: "component", type: "float", kind: "member_expr", member_expr: "normal.x" },
  { class: "Example", name: "channel", type: "int", kind: "method", getter: "get_channel", setter: "set_channel" },
 ]]]);
 const bindings = collectPropertyBindings({ name: "Example", members: [{ name: "component" }, { name: "channel" }] }, rules, "generated");
 assert.ok(bindings[0].helperCode.includes("static_cast<decltype(p_self.normal.x)>(p_value)"));
 assert.ok(bindings[1].helperCode.includes("native_parameter_t<decltype(&godot::Example::set_channel), 0>"));
 assert.ok(!bindings.some(binding => binding.helperCode.includes("eastl::move")));
});

test("explicit script defaults are supplied and stale metadata is rejected", () => {
 const adapters = new Map([["Example.read", { default_argument: { type: "Variant", value: "null", expression: "godot::Variant()" } }]]);
 const generate = value => generateClassBinding({ name: "Example", is_instantiable: false }, "classes",
  [{ name: "read", arguments: [{ name: "fallback", type: "Variant", default_value: value }] }], [], [], [], adapters).code;
 assert.ok(generate("null").includes("p_self.read(godot::Variant())"));
 assert.throws(() => generate("1"), /Unexpected explicit default argument metadata/);
});
