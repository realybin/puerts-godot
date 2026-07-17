// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import { normalizeOperators } from "../../../puerts-godot-operator-model/index.js";
import type { ApiBuiltinClass, ApiClass, ApiMethod } from "../api-types.js";
import type { Context } from "../context.js";
import { emitDeclaration, godotClassMetadata } from "../documentation.js";
import { sanitizeIdentifier } from "../naming.js";
import { mapApiType, mapType } from "../type-mapper.js";
import { collectEnumConstantNames, emitNestedEnums } from "./enum.js";
import { emitArgumentList, emitMethodDeclarations } from "./method.js";

function emitOperatorMethods(builtin: ApiBuiltinClass, ctx: Context): string[] {
	const operatorMethods: ApiMethod[] = normalizeOperators(builtin.name, builtin.operators).map((operator, index) => ({
		name: operator.scriptName,
		description: builtin.operators?.[index]?.description,
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
	return emitMethodDeclarations(operatorMethods, ctx, { indent: "\t" });
}

export function emitObjectClass(classDef: ApiClass, ctx: Context): string[] {
	const out: string[] = [];
	const className = sanitizeIdentifier(classDef.name);
	const parent = classDef.inherits ? sanitizeIdentifier(classDef.inherits) : "";
	const documentation = [
		classDef.brief_description,
		classDef.description,
		godotClassMetadata(classDef.is_refcounted, classDef.is_instantiable),
	];
	out.push(...emitDeclaration(`class ${className}${parent ? ` extends ${parent}` : ""} {`, documentation));
	if (classDef.is_instantiable) {
		out.push("\tconstructor(...args: any[]);");
	}
	for (const property of classDef.properties ?? []) {
		const propertyName = sanitizeIdentifier(property.name);
		const readonly = !property.setter ? "readonly " : "";
		out.push(...emitDeclaration(`${readonly}${propertyName}: ${mapApiType(property, ctx)};`, [property.description], "\t"));
	}
	for (const signal of classDef.signals ?? []) {
		out.push(...emitDeclaration(`readonly ${sanitizeIdentifier(signal.name)}: Signal;`, [signal.description], "\t"));
	}
	const enumConstantNames = collectEnumConstantNames(classDef.enums);
	for (const constant of classDef.constants ?? []) {
		if (enumConstantNames.has(constant.name)) {
			continue;
		}
		const typeName = constant.type ? mapType(constant.type, ctx, constant.meta) : "Int64";
		out.push(...emitDeclaration(`static readonly ${sanitizeIdentifier(constant.name)}: ${typeName};`, [constant.description], "\t"));
	}
	out.push(...emitMethodDeclarations(classDef.methods, ctx, { indent: "\t" }));
	out.push("}");
	out.push(...emitNestedEnums(className, classDef.enums));
	return out;
}

export function emitBuiltinClass(builtin: ApiBuiltinClass, ctx: Context): string[] {
	const out: string[] = [];
	const className = sanitizeIdentifier(builtin.name);
	out.push(...emitDeclaration(`class ${className} {`, [builtin.brief_description, builtin.description]));
	const ctors = builtin.constructors ?? [];
	if (ctors.length === 0) {
		out.push("\tconstructor(...args: any[]);");
	} else {
		for (const ctor of ctors) {
			const argTexts = emitArgumentList(ctor.arguments, ctx);
			out.push(...emitDeclaration(`constructor(${argTexts.join(", ")});`, [ctor.description], "\t"));
		}
	}
	for (const member of builtin.members ?? []) {
		out.push(...emitDeclaration(`${sanitizeIdentifier(member.name)}: ${mapApiType(member, ctx)};`, [member.description], "\t"));
	}
	const enumConstantNames = collectEnumConstantNames(builtin.enums);
	for (const constant of builtin.constants ?? []) {
		if (enumConstantNames.has(constant.name)) {
			continue;
		}
		const typeName = constant.type ? mapType(constant.type, ctx, constant.meta) : "Int64";
		out.push(...emitDeclaration(`static readonly ${sanitizeIdentifier(constant.name)}: ${typeName};`, [constant.description], "\t"));
	}
	out.push(...emitOperatorMethods(builtin, ctx));
	out.push(...emitMethodDeclarations(builtin.methods, ctx, { indent: "\t" }));
	out.push("}");
	out.push(...emitNestedEnums(className, builtin.enums));
	return out;
}
