// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

const OPERATOR_SPECS = Object.freeze({
	"==": Object.freeze({ scriptName: "op_Eq", variantEnum: "OP_EQUAL", kind: "binary" }),
	"!=": Object.freeze({ scriptName: "op_Ne", variantEnum: "OP_NOT_EQUAL", kind: "binary" }),
	"<": Object.freeze({ scriptName: "op_Lt", variantEnum: "OP_LESS", kind: "binary" }),
	"<=": Object.freeze({ scriptName: "op_Le", variantEnum: "OP_LESS_EQUAL", kind: "binary" }),
	">": Object.freeze({ scriptName: "op_Gt", variantEnum: "OP_GREATER", kind: "binary" }),
	">=": Object.freeze({ scriptName: "op_Ge", variantEnum: "OP_GREATER_EQUAL", kind: "binary" }),
	"+": Object.freeze({ scriptName: "op_Add", variantEnum: "OP_ADD", kind: "binary" }),
	"-": Object.freeze({ scriptName: "op_Sub", variantEnum: "OP_SUBTRACT", kind: "binary" }),
	"*": Object.freeze({ scriptName: "op_Mul", variantEnum: "OP_MULTIPLY", kind: "binary" }),
	"/": Object.freeze({ scriptName: "op_Div", variantEnum: "OP_DIVIDE", kind: "binary" }),
	"%": Object.freeze({ scriptName: "op_Mod", variantEnum: "OP_MODULE", kind: "binary" }),
	"**": Object.freeze({ scriptName: "op_Pow", variantEnum: "OP_POWER", kind: "binary" }),
	"<<": Object.freeze({ scriptName: "op_Shl", variantEnum: "OP_SHIFT_LEFT", kind: "binary" }),
	">>": Object.freeze({ scriptName: "op_Shr", variantEnum: "OP_SHIFT_RIGHT", kind: "binary" }),
	"&": Object.freeze({ scriptName: "op_BitAnd", variantEnum: "OP_BIT_AND", kind: "binary" }),
	"|": Object.freeze({ scriptName: "op_BitOr", variantEnum: "OP_BIT_OR", kind: "binary" }),
	"^": Object.freeze({ scriptName: "op_BitXor", variantEnum: "OP_BIT_XOR", kind: "binary" }),
	"and": Object.freeze({ scriptName: "op_And", variantEnum: "OP_AND", kind: "binary" }),
	"or": Object.freeze({ scriptName: "op_Or", variantEnum: "OP_OR", kind: "binary" }),
	"xor": Object.freeze({ scriptName: "op_Xor", variantEnum: "OP_XOR", kind: "binary" }),
	"in": Object.freeze({ scriptName: "op_In", variantEnum: "OP_IN", kind: "binary" }),
	"unary+": Object.freeze({ scriptName: "op_Pos", variantEnum: "OP_POSITIVE", kind: "unary" }),
	"unary-": Object.freeze({ scriptName: "op_Neg", variantEnum: "OP_NEGATE", kind: "unary" }),
	"not": Object.freeze({ scriptName: "op_Not", variantEnum: "OP_NOT", kind: "unary" }),
	"~": Object.freeze({ scriptName: "op_BitNot", variantEnum: "OP_BIT_NEGATE", kind: "unary" }),
});

export { OPERATOR_SPECS };

export function getOperatorSpec(name) {
	const spec = OPERATOR_SPECS[name];
	if (!spec) {
		throw new Error(`Unsupported operator in extension_api.json: ${name}`);
	}
	return spec;
}

export function normalizeOperators(leftType, operators = []) {
	return operators.map((operator) => {
		const spec = getOperatorSpec(operator.name);
		return {
			name: operator.name,
			scriptName: spec.scriptName,
			variantEnum: spec.variantEnum,
			kind: spec.kind,
			leftType,
			rightType: spec.kind === "binary" ? operator.right_type ?? "Variant" : undefined,
			returnType: operator.return_type,
		};
	});
}
