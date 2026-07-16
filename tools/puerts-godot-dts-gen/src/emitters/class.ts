// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import { normalizeOperators } from "../../../puerts-godot-operator-model/index.js";
import type { ApiBuiltinClass, ApiClass, ApiMethod } from "../api-types.js";
import type { Context } from "../context.js";
import { sanitizeIdentifier } from "../naming.js";
import { mapApiType, mapType } from "../type-mapper.js";
import { collectEnumConstantNames, emitNestedEnums } from "./enum.js";
import { emitArgumentList, emitMethodSignature } from "./method.js";

function emitMethods(methods: ApiMethod[] | undefined, ctx: Context): string[] {
	const out: string[] = [];
	const grouped = new Map<string, ApiMethod[]>();
	for (const method of methods ?? []) {
		const arr = grouped.get(method.name) ?? [];
		arr.push(method);
		grouped.set(method.name, arr);
	}
	for (const overloads of grouped.values()) {
		for (const method of overloads) {
			out.push(`\t${emitMethodSignature(method, ctx)}`);
		}
	}
	return out;
}

function emitOperatorMethods(builtin: ApiBuiltinClass, ctx: Context): string[] {
	const operatorMethods: ApiMethod[] = normalizeOperators(builtin.name, builtin.operators).map((operator) => ({
		name: operator.scriptName,
		is_static: true,
		return_type: operator.returnType,
		arguments:
			operator.kind === "unary"
				? [{ name: "value", type: operator.leftType }]
				: [
						{ name: "left", type: operator.leftType },
						{ name: "right", type: operator.rightType ?? "Variant" },
					],
	}));
	return emitMethods(operatorMethods, ctx);
}

export function emitObjectClass(classDef: ApiClass, ctx: Context): string[] {
	const out: string[] = [];
	const className = sanitizeIdentifier(classDef.name);
	const parent = classDef.inherits ? sanitizeIdentifier(classDef.inherits) : "";
	out.push(`class ${className}${parent ? ` extends ${parent}` : ""} {`);
	if (classDef.is_instantiable) {
		out.push("\tconstructor(...args: any[]);");
	}
	for (const property of classDef.properties ?? []) {
		const propertyName = sanitizeIdentifier(property.name);
		const readonly = !property.setter ? "readonly " : "";
		out.push(`\t${readonly}${propertyName}: ${mapApiType(property, ctx)};`);
	}
	for (const signal of classDef.signals ?? []) {
		out.push(`\treadonly ${sanitizeIdentifier(signal.name)}: Signal;`);
	}
	const enumConstantNames = collectEnumConstantNames(classDef.enums);
	for (const constant of classDef.constants ?? []) {
		if (enumConstantNames.has(constant.name)) {
			continue;
		}
		const typeName = constant.type ? mapType(constant.type, ctx, constant.meta) : "Int64";
		out.push(`\tstatic readonly ${sanitizeIdentifier(constant.name)}: ${typeName};`);
	}
	out.push(...emitMethods(classDef.methods, ctx));
	out.push("}");
	out.push(...emitNestedEnums(className, classDef.enums));
	return out;
}

export function emitBuiltinClass(builtin: ApiBuiltinClass, ctx: Context): string[] {
	const out: string[] = [];
	const className = sanitizeIdentifier(builtin.name);
	out.push(`class ${className} {`);
	const ctors = builtin.constructors ?? [];
	if (ctors.length === 0) {
		out.push("\tconstructor(...args: any[]);");
	} else {
		for (const ctor of ctors) {
			const argTexts = emitArgumentList(ctor.arguments, ctx);
			out.push(`\tconstructor(${argTexts.join(", ")});`);
		}
	}
	for (const member of builtin.members ?? []) {
		out.push(`\t${sanitizeIdentifier(member.name)}: ${mapApiType(member, ctx)};`);
	}
	const enumConstantNames = collectEnumConstantNames(builtin.enums);
	for (const constant of builtin.constants ?? []) {
		if (enumConstantNames.has(constant.name)) {
			continue;
		}
		const typeName = constant.type ? mapType(constant.type, ctx, constant.meta) : "Int64";
		out.push(`\tstatic readonly ${sanitizeIdentifier(constant.name)}: ${typeName};`);
	}
	out.push(...emitOperatorMethods(builtin, ctx));
	out.push(...emitMethods(builtin.methods, ctx));
	out.push("}");
	out.push(...emitNestedEnums(className, builtin.enums));
	return out;
}
