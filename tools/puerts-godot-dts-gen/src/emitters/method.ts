// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import type { ApiArgument, ApiMethod } from "../api-types.js";
import type { Context } from "../context.js";
import { sanitizeIdentifier } from "../naming.js";
import { mapType, widenArgumentType } from "../type-mapper.js";

function methodReturnType(method: ApiMethod, ctx: Context): string {
	if (method.return_value?.type) {
		return mapType(method.return_value.type, ctx);
	}
	return mapType(method.return_type, ctx);
}

export function emitArgumentList(args: ApiArgument[] | undefined, ctx: Context): string[] {
	return (args ?? []).map((arg, index) => {
		const name = sanitizeIdentifier(arg.name || `arg${index}`);
		const mapped = mapType(arg.type, ctx);
		const widened = widenArgumentType(arg.type, mapped);
		const optional = arg.default_value !== undefined ? "?" : "";
		return `${name}${optional}: ${widened}`;
	});
}

export function emitMethodSignature(method: ApiMethod, ctx: Context): string {
	const methodName = sanitizeIdentifier(method.name);
	const argTexts = emitArgumentList(method.arguments, ctx);
	if (method.is_vararg) {
		argTexts.push("...varargs: any[]");
	}
	return `${method.is_static ? "static " : ""}${methodName}(${argTexts.join(", ")}): ${methodReturnType(method, ctx)};`;
}
