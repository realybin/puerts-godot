// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import type { Context } from "./context.js";
import { cleanTypeName, sanitizeIdentifier, splitUnionCandidates } from "./naming.js";

function mapPrimitiveType(name: string): string | null {
	switch (name) {
		case "void":
			return "void";
		case "Nil":
			return "null";
		case "bool":
			return "boolean";
		case "int":
		case "float":
		case "double":
		case "real_t":
		case "int8_t":
		case "uint8_t":
		case "int16_t":
		case "uint16_t":
		case "int32_t":
		case "uint32_t":
		case "int64_t":
		case "uint64_t":
		case "ObjectID":
		case "RID_OwnerBase":
			return "number";
		case "String":
			return "string";
		case "Variant":
			return "Variant";
		case "Array":
			return "Array";
		case "Dictionary":
			return "Dictionary";
		default:
			return null;
	}
}

function mapEnumRef(raw: string): string {
	const payload = raw.replace(/^enum::/, "").replace(/^bitfield::/, "");
	return payload
		.split(".")
		.map((part) => sanitizeIdentifier(part))
		.join(".");
}

function mapSingleType(rawType: string, ctx: Context): string {
	const original = cleanTypeName(rawType);
	if (original.startsWith("enum::")) {
		return mapEnumRef(original);
	}
	if (original.startsWith("bitfield::")) {
		return "number";
	}
	if (original.startsWith("typedarray::")) {
		const inner = cleanTypeName(original.slice("typedarray::".length));
		return `globalThis.Array<${mapType(inner, ctx)}>`;
	}
	if (original.includes("*")) {
		return "any";
	}

	const primitive = mapPrimitiveType(original);
	if (primitive) {
		return primitive;
	}
	if (ctx.knownClassNames.has(original) || ctx.knownBuiltinNames.has(original)) {
		return original;
	}
	if (/^(u?int|float|double)\d*_t?$/.test(original)) {
		return "number";
	}
	return "any";
}

export function mapType(rawType: string | undefined, ctx: Context): string {
	if (!rawType) {
		return "void";
	}
	const parts = splitUnionCandidates(cleanTypeName(rawType));
	const mapped = [...new Set(parts.map((part) => mapSingleType(part, ctx)))];
	return mapped.length === 1 ? mapped[0] : mapped.join(" | ");
}

export function widenArgumentType(rawType: string, mappedType: string): string {
	const t = cleanTypeName(rawType);
	if (t === "StringName" || t === "NodePath") {
		return `${mappedType} | string`;
	}
	if (t === "PackedByteArray") {
		return `${mappedType} | Uint8Array | ArrayBuffer`;
	}
	return mappedType;
}
