// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import type { ApiData, ApiMethod } from "../api-types.js";
import type { Context } from "../context.js";
import { NON_CLASS_BUILTIN_NAMES } from "../constants.js";
import { NATIVE_PRIMITIVE_TYPE_SPECS } from "../native-primitive-types.js";
import { sanitizeIdentifier } from "../naming.js";
import { mapType } from "../type-mapper.js";
import { emitGlobalEnums } from "./enum.js";
import { emitMethodSignature } from "./method.js";

function groupMethodsByName(functions: ApiMethod[]): ApiMethod[][] {
	const grouped = new Map<string, ApiMethod[]>();
	for (const fn of functions) {
		const arr = grouped.get(fn.name) ?? [];
		arr.push(fn);
		grouped.set(fn.name, arr);
	}
	return [...grouped.values()];
}

function emitMethodGroup(overloads: ApiMethod[], ctx: Context, isStatic: boolean): string[] {
	const out: string[] = [];
	for (const overload of overloads) {
		out.push(emitMethodSignature({ ...overload, is_static: isStatic }, ctx));
	}
	return out;
}

export function emitGlobalScope(api: ApiData, ctx: Context): string[] {
	const out: string[] = [];

	out.push("class GlobalScope {");
	for (const signature of groupMethodsByName(api.utility_functions).flatMap((overloads) => emitMethodGroup(overloads, ctx, true))) {
		out.push(`\t${signature}`);
	}
	for (const singleton of api.singletons) {
		out.push(`\tstatic readonly ${sanitizeIdentifier(singleton.name)}: ${mapType(singleton.type, ctx)};`);
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
	const out: string[] = [];
	const exported = new Set<string>();
	for (const spec of NATIVE_PRIMITIVE_TYPE_SPECS) {
		exported.add(spec.alias);
		out.push(`\texport type ${spec.alias} = godot.${spec.alias};`);
	}
	exported.add("Real");
	out.push("\texport type Real = godot.Real;");
	exported.add("GlobalScope");
	out.push("\texport type GlobalScope = godot.GlobalScope;");
	out.push("\texport const GlobalScope: typeof godot.GlobalScope;");
	for (const builtin of api.builtin_classes) {
		if (NON_CLASS_BUILTIN_NAMES.has(builtin.name)) {
			continue;
		}
		const name = sanitizeIdentifier(builtin.name);
		if (exported.has(name)) {
			continue;
		}
		exported.add(name);
		out.push(`\texport type ${name} = godot.${name};`);
		out.push(`\texport const ${name}: typeof godot.${name};`);
	}
	for (const cls of api.classes) {
		const name = sanitizeIdentifier(cls.name);
		if (exported.has(name)) {
			continue;
		}
		exported.add(name);
		out.push(`\texport type ${name} = godot.${name};`);
		out.push(`\texport const ${name}: typeof godot.${name};`);
	}
	return out;
}
