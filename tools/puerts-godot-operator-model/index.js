// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

const OPERATOR_SPECS = Object.freeze({
	"==": Object.freeze({ scriptName: "op_Equality", variantEnum: "OP_EQUAL", kind: "binary" }),
	"!=": Object.freeze({ scriptName: "op_Inequality", variantEnum: "OP_NOT_EQUAL", kind: "binary" }),
	"<": Object.freeze({ scriptName: "op_LessThan", variantEnum: "OP_LESS", kind: "binary" }),
	"<=": Object.freeze({ scriptName: "op_LessThanOrEqual", variantEnum: "OP_LESS_EQUAL", kind: "binary" }),
	">": Object.freeze({ scriptName: "op_GreaterThan", variantEnum: "OP_GREATER", kind: "binary" }),
	">=": Object.freeze({ scriptName: "op_GreaterThanOrEqual", variantEnum: "OP_GREATER_EQUAL", kind: "binary" }),
	"+": Object.freeze({ scriptName: "op_Addition", variantEnum: "OP_ADD", kind: "binary" }),
	"-": Object.freeze({ scriptName: "op_Subtraction", variantEnum: "OP_SUBTRACT", kind: "binary" }),
	"*": Object.freeze({ scriptName: "op_Multiply", variantEnum: "OP_MULTIPLY", kind: "binary" }),
	"/": Object.freeze({ scriptName: "op_Division", variantEnum: "OP_DIVIDE", kind: "binary" }),
	"%": Object.freeze({ scriptName: "op_Modulus", variantEnum: "OP_MODULE", kind: "binary" }),
	"**": Object.freeze({ scriptName: "op_Power", variantEnum: "OP_POWER", kind: "binary" }),
	"<<": Object.freeze({ scriptName: "op_LeftShift", variantEnum: "OP_SHIFT_LEFT", kind: "binary" }),
	">>": Object.freeze({ scriptName: "op_RightShift", variantEnum: "OP_SHIFT_RIGHT", kind: "binary" }),
	"&": Object.freeze({ scriptName: "op_BitwiseAnd", variantEnum: "OP_BIT_AND", kind: "binary" }),
	"|": Object.freeze({ scriptName: "op_BitwiseOr", variantEnum: "OP_BIT_OR", kind: "binary" }),
	"^": Object.freeze({ scriptName: "op_ExclusiveOr", variantEnum: "OP_BIT_XOR", kind: "binary" }),
	"and": Object.freeze({ scriptName: "op_LogicalAnd", variantEnum: "OP_AND", kind: "binary" }),
	"or": Object.freeze({ scriptName: "op_LogicalOr", variantEnum: "OP_OR", kind: "binary" }),
	"xor": Object.freeze({ scriptName: "op_LogicalExclusiveOr", variantEnum: "OP_XOR", kind: "binary" }),
	"in": Object.freeze({ scriptName: "op_In", variantEnum: "OP_IN", kind: "binary" }),
	"unary+": Object.freeze({ scriptName: "op_UnaryPlus", variantEnum: "OP_POSITIVE", kind: "unary" }),
	"unary-": Object.freeze({ scriptName: "op_UnaryNegation", variantEnum: "OP_NEGATE", kind: "unary" }),
	"not": Object.freeze({ scriptName: "op_LogicalNot", variantEnum: "OP_NOT", kind: "unary" }),
	"~": Object.freeze({ scriptName: "op_OnesComplement", variantEnum: "OP_BIT_NEGATE", kind: "unary" }),
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
