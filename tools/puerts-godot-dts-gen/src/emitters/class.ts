// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import { normalizeOperators } from "../../../puerts-godot-operator-model/index.js";
import type { ApiBuiltinClass, ApiClass, ApiConstant, ApiEnum, ApiMethod } from "../api-types.js";
import { getBuiltinGenericDefinition } from "../builtin-generics.js";
import type { GenerationContext } from "../context.js";
import type { DocumentationScope } from "../documentation.js";
import { godotClassMetadata } from "../documentation.js";
import { sanitizeIdentifier } from "../naming.js";
import { mapApiType, mapType } from "../type-mapper.js";
import { collectEnumConstantNames, emitNestedEnums } from "./enum.js";
import { emitArgumentList, emitMethodDeclarations } from "./method.js";

function emitConstants(
	constants: ApiConstant[] | undefined,
	enums: ApiEnum[] | undefined,
	ctx: GenerationContext,
	documentation: DocumentationScope,
): string[] {
	const enumConstantNames = collectEnumConstantNames(enums);
	const out: string[] = [];
	for (const constant of constants ?? []) {
		if (enumConstantNames.has(constant.name)) {
			continue;
		}
		const typeName = constant.type ? mapType(constant.type, ctx, constant.meta) : "Int64";
		out.push(
			...documentation.emit(
				`static readonly ${sanitizeIdentifier(constant.name)}: ${typeName};`,
				[constant.description],
				"\t",
			),
		);
	}
	return out;
}

function emitOperatorMethods(builtin: ApiBuiltinClass, ctx: GenerationContext, documentation: DocumentationScope): string[] {
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
	return emitMethodDeclarations(operatorMethods, ctx, { documentation, indent: "\t" });
}

export function emitObjectClass(classDef: ApiClass, ctx: GenerationContext): string[] {
	const out: string[] = [];
	const className = sanitizeIdentifier(classDef.name);
	const parent = classDef.inherits ? sanitizeIdentifier(classDef.inherits) : "";
	const documentation = ctx.documentation.forOwner(classDef.name);
	const classDocumentation = [
		classDef.brief_description,
		classDef.description,
		godotClassMetadata(classDef.is_refcounted, classDef.is_instantiable),
	];
	out.push(...documentation.emit(`class ${className}${parent ? ` extends ${parent}` : ""} {`, classDocumentation));
	if (classDef.is_instantiable) {
		out.push(`\tconstructor(...args: ${ctx.options.unknownType}[]);`);
	}
	for (const property of classDef.properties ?? []) {
		const propertyName = sanitizeIdentifier(property.name);
		const readonly = !property.setter ? "readonly " : "";
		out.push(...documentation.emit(`${readonly}${propertyName}: ${mapApiType(property, ctx)};`, [property.description], "\t"));
	}
	for (const signal of classDef.signals ?? []) {
		out.push(...documentation.emit(`readonly ${sanitizeIdentifier(signal.name)}: Signal;`, [signal.description], "\t"));
	}
	out.push(...emitConstants(classDef.constants, classDef.enums, ctx, documentation));
	out.push(...emitMethodDeclarations(classDef.methods, ctx, { documentation, indent: "\t" }));
	out.push("}");
	out.push(...emitNestedEnums(className, classDef.enums, documentation));
	return out;
}

export function emitBuiltinClass(builtin: ApiBuiltinClass, ctx: GenerationContext): string[] {
	const out: string[] = [];
	const className = sanitizeIdentifier(builtin.name);
	const generic = getBuiltinGenericDefinition(builtin.name);
	const documentation = ctx.documentation.forOwner(builtin.name);
	out.push(...documentation.emit(`class ${className}${generic?.typeParameters ?? ""} {`, [builtin.brief_description, builtin.description]));
	for (const member of generic?.members ?? []) {
		out.push(`\t${member}`);
	}
	const ctors = builtin.constructors ?? [];
	if (ctors.length === 0) {
		out.push(`\tconstructor(...args: ${ctx.options.unknownType}[]);`);
	} else {
		for (const ctor of ctors) {
			const argTexts = emitArgumentList(ctor.arguments, ctx);
			out.push(...documentation.emit(`constructor(${argTexts.join(", ")});`, [ctor.description], "\t"));
		}
	}
	for (const member of builtin.members ?? []) {
		out.push(...documentation.emit(`${sanitizeIdentifier(member.name)}: ${mapApiType(member, ctx)};`, [member.description], "\t"));
	}
	out.push(...emitConstants(builtin.constants, builtin.enums, ctx, documentation));
	out.push(...emitOperatorMethods(builtin, ctx, documentation));
	out.push(
		...emitMethodDeclarations(builtin.methods, ctx, {
			documentation,
			indent: "\t",
			typeOverrides: generic?.methodTypes,
		}),
	);
	out.push("}");
	out.push(...emitNestedEnums(className, builtin.enums, documentation));
	return out;
}
