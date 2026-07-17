// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import type { ApiArgument, ApiMethod } from "../api-types.js";
import type { Context } from "../context.js";
import { emitDeclaration } from "../documentation.js";
import { sanitizeIdentifier } from "../naming.js";
import { mapApiType, mapType, widenArgumentType } from "../type-mapper.js";

function methodReturnType(method: ApiMethod, ctx: Context): string {
	if (method.return_value?.type) {
		return mapApiType(method.return_value, ctx);
	}
	return mapType(method.return_type, ctx);
}

export function emitArgumentList(args: ApiArgument[] | undefined, ctx: Context): string[] {
	return (args ?? []).map((arg, index) => {
		const name = sanitizeIdentifier(arg.name || `arg${index}`);
		const mapped = mapApiType(arg, ctx);
		const widened = widenArgumentType(arg.type, mapped);
		const optional = arg.default_value !== undefined ? "?" : "";
		return `${name}${optional}: ${widened}`;
	});
}

function emitMethodSignature(method: ApiMethod, ctx: Context): string {
	const methodName = sanitizeIdentifier(method.name);
	const argTexts = emitArgumentList(method.arguments, ctx);
	if (method.is_vararg) {
		argTexts.push("...varargs: any[]");
	}
	return `${method.is_static ? "static " : ""}${methodName}(${argTexts.join(", ")}): ${methodReturnType(method, ctx)};`;
}

function emitMethodDeclaration(method: ApiMethod, ctx: Context, indent = ""): string[] {
	return emitDeclaration(emitMethodSignature(method, ctx), [method.description], indent);
}

export function emitMethodDeclarations(
	methods: ApiMethod[] | undefined,
	ctx: Context,
	options: { indent?: string; isStatic?: boolean } = {},
): string[] {
	const grouped = new Map<string, ApiMethod[]>();
	for (const method of methods ?? []) {
		const overloads = grouped.get(method.name) ?? [];
		overloads.push(method);
		grouped.set(method.name, overloads);
	}

	const out: string[] = [];
	for (const overloads of grouped.values()) {
		for (const method of overloads) {
			const declaration = options.isStatic ? { ...method, is_static: true } : method;
			out.push(...emitMethodDeclaration(declaration, ctx, options.indent));
		}
	}
	return out;
}
