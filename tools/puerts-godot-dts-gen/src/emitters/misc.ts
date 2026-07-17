// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import type { ApiData } from "../api-types.js";
import type { Context } from "../context.js";
import { NON_CLASS_BUILTIN_NAMES } from "../constants.js";
import { emitDeclaration } from "../documentation.js";
import { NATIVE_PRIMITIVE_TYPE_SPECS } from "../native-primitive-types.js";
import { sanitizeIdentifier } from "../naming.js";
import { mapType } from "../type-mapper.js";
import { emitGlobalEnums } from "./enum.js";
import { emitMethodDeclarations } from "./method.js";

export function emitGlobalScope(api: ApiData, ctx: Context): string[] {
	const out: string[] = [];

	out.push("class GlobalScope {");
	out.push(...emitMethodDeclarations(api.utility_functions, ctx, { indent: "\t", isStatic: true }));
	for (const singleton of api.singletons) {
		const declaration = `static readonly ${sanitizeIdentifier(singleton.name)}: ${mapType(singleton.type, ctx)};`;
		out.push(...emitDeclaration(declaration, [singleton.description], "\t"));
	}
	out.push("}");
	out.push("namespace GlobalScope {");
	for (const line of emitGlobalEnums(api.global_enums)) {
		out.push(`\t${line}`);
	}
	out.push("}");
	return out;
}

export function buildVariantUnion(ctx: Context): string {
	const builtinVariants = [...ctx.emittedBuiltinNames].sort().join(" | ");
	return builtinVariants
		? `null | Bool | Int | Float | String | ${builtinVariants} | Object`
		: "null | Bool | Int | Float | String | Object";
}

export function emitGodotModuleNamedExports(api: ApiData): string[] {
	const typeAliases = [...NATIVE_PRIMITIVE_TYPE_SPECS.map((spec) => spec.alias), "Real", "Variant"];
	const out = typeAliases.map((name) => `\texport type ${name} = godot.${name};`);
	const exported = new Set(typeAliases);
	exported.add("GlobalScope");
	out.push("\texport import GlobalScope = godot.GlobalScope;");
	const classNames = [
		...api.builtin_classes.filter((builtin) => !NON_CLASS_BUILTIN_NAMES.has(builtin.name)).map((builtin) => builtin.name),
		...api.classes.map((classDef) => classDef.name),
	];
	for (const className of classNames) {
		const name = sanitizeIdentifier(className);
		if (exported.has(name)) {
			continue;
		}
		exported.add(name);
		out.push(`\texport import ${name} = godot.${name};`);
	}
	return out;
}
