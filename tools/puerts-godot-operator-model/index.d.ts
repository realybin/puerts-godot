// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

export type ApiOperator = {
	name: string;
	right_type?: string;
	return_type: string;
};

export type OperatorKind = "unary" | "binary";

export type OperatorSpec = {
	scriptName: string;
	variantEnum: string;
	kind: OperatorKind;
};

export type NormalizedOperator = {
	name: string;
	scriptName: string;
	variantEnum: string;
	kind: OperatorKind;
	leftType: string;
	rightType?: string;
	returnType: string;
};

export declare const OPERATOR_SPECS: Readonly<Record<string, Readonly<OperatorSpec>>>;
export declare function getOperatorSpec(name: string): Readonly<OperatorSpec>;
export declare function normalizeOperators(leftType: string, operators?: ApiOperator[]): NormalizedOperator[];
