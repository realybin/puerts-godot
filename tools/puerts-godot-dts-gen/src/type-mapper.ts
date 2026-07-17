// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import type { ApiType } from "./api-types.js";
import type { Context } from "./context.js";
import { nativePrimitiveAlias } from "./native-primitive-types.js";
import { cleanTypeName, sanitizeIdentifier, splitUnionCandidates } from "./naming.js";

function mapPrimitiveType(name: string, meta: string | undefined): string | null {
	const isNumericPrimitive = /^(?:u?int(?:8|16|32|64)?(?:_t)?|float|double|real_t)$/.test(name);
	const metaAlias = isNumericPrimitive ? nativePrimitiveAlias(meta) : null;
	if (metaAlias) {
		return metaAlias;
	}

	switch (name) {
		case "void":
			return "void";
		case "Nil":
			return "null";
		case "bool":
			return "Bool";
		case "int":
			return "Int";
		case "float":
			return "Float";
		case "double":
			return "Float64";
		case "real_t":
			return "Real";
		case "int8_t":
		case "uint8_t":
		case "int16_t":
		case "uint16_t":
		case "int32_t":
		case "uint32_t":
		case "int64_t":
		case "uint64_t":
		case "char32_t":
			return nativePrimitiveAlias(name);
		case "ObjectID":
			return "UInt64";
		case "String":
			return "String";
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

function mapEnumRef(raw: string, ctx: Context): string {
	const payload = raw.replace(/^enum::/, "").replace(/^bitfield::/, "");
	const mapped = payload
		.split(".")
		.map((part) => sanitizeIdentifier(part))
		.join(".");
	return ctx.globalEnumNames.has(payload) ? `GlobalScope.${mapped}` : mapped;
}

function mapSingleType(rawType: string, ctx: Context, meta?: string): string {
	const original = cleanTypeName(rawType);
	if (original.startsWith("enum::")) {
		return mapEnumRef(original, ctx);
	}
	if (original.startsWith("bitfield::")) {
		return "Bitfield";
	}
	if (original.startsWith("typedarray::")) {
		const inner = cleanTypeName(original.slice("typedarray::".length));
		return `Array<${mapType(inner, ctx)}>`;
	}
	if (original.startsWith("typeddictionary::")) {
		const [key, value] = original.slice("typeddictionary::".length).split(";", 2);
		return `Dictionary<${mapType(key, ctx)}, ${mapType(value, ctx)}>`;
	}
	if (original.includes("*")) {
		return "any";
	}

	const primitive = mapPrimitiveType(original, meta);
	if (primitive) {
		return primitive;
	}
	if (ctx.knownClassNames.has(original) || ctx.knownBuiltinNames.has(original)) {
		return original;
	}
	const nativeAlias = nativePrimitiveAlias(original);
	if (nativeAlias) {
		return nativeAlias;
	}
	return "any";
}

export function mapType(rawType: string | undefined, ctx: Context, meta?: string): string {
	if (!rawType) {
		return "void";
	}
	const parts = splitUnionCandidates(cleanTypeName(rawType));
	const mapped = [...new Set(parts.map((part) => mapSingleType(part, ctx, parts.length === 1 ? meta : undefined)))];
	return mapped.length === 1 ? mapped[0] : mapped.join(" | ");
}

export function mapApiType(apiType: ApiType, ctx: Context): string {
	return mapType(apiType.type, ctx, apiType.meta);
}

export function widenArgumentType(rawType: string, mappedType: string): string {
	const t = cleanTypeName(rawType);
	if (t === "StringName" || t === "NodePath") {
		return `${mappedType} | String`;
	}
	if (t === "PackedByteArray") {
		return `${mappedType}`;
		// return `${mappedType} | Uint8Array | ArrayBuffer`; // When enable PackedByteArray casting
	}
	if (t === "Array") {
		return "Array<any>";
	}
	if (t === "Dictionary") {
		return "Dictionary<any, any>";
	}
	return mappedType;
}
