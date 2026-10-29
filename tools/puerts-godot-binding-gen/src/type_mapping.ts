// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

import type { ApiMethod, ParsedApiType } from "./types.js";

export function normalizeGodotTypeName(typeName: string): string {
	return typeName.startsWith("godot::") ? typeName : `godot::${typeName}`;
}

export function parseApiType(typeName: string): ParsedApiType {
	const t = typeName.trim();
	if (!t || t === "void" || t === "Nil") return { kind: "void" };
	if (t === "int") return { kind: "int" };
	if (t === "float") return { kind: "float" };
	if (t === "bool") return { kind: "bool" };
	if (t === "Object") return { kind: "object" };
	if (t.startsWith("enum::")) return { kind: "enum_like", value: normalizeGodotTypeName(t.slice(6).replaceAll(".", "::")) };
	if (t.startsWith("bitfield::")) return { kind: "enum_like", value: `godot::BitField<${normalizeGodotTypeName(t.slice(10).replaceAll(".", "::"))}>` };
	if (t.startsWith("typedarray::")) {
		return { kind: "typed_array", base: t.slice("typedarray::".length) };
	}
	if (t.startsWith("typeddictionary::")) {
		return { kind: "typed_dictionary", body: t.slice("typeddictionary::".length).replace(";", ", ") };
	}
	if (t.endsWith("*")) return { kind: "pointer", value: normalizeGodotTypeName(t) };
	return { kind: "named", value: normalizeGodotTypeName(t) };
}

type TypeMappingMode = "value" | "const_ref";

function mapParsedType(
	parsed: ParsedApiType,
	options: {
		voidType: string;
		intType: string;
		floatType: string;
		typeMode: TypeMappingMode;
	},
): string {
	switch (parsed.kind) {
		case "void":
			return options.voidType;
		case "int":
			return options.intType;
		case "enum_like":
			return parsed.value;
		case "float":
			return options.floatType;
		case "bool":
			return "bool";
		case "object":
			return "godot::Object *";
		case "typed_array":
			return wrapRef(`godot::TypedArray<godot::${parsed.base}>`, options.typeMode);
		case "typed_dictionary":
			return wrapRef(`godot::TypedDictionary<godot::${parsed.body}>`, options.typeMode);
		case "pointer":
			return parsed.value;
		case "named":
			return wrapRef(parsed.value, options.typeMode);
	}
}

function wrapRef(typeName: string, mode: TypeMappingMode): string {
	return mode === "const_ref" ? `const ${typeName} &` : typeName;
}

export function mapPropertyValueType(typeName: string): string {
	return mapParsedType(parseApiType(typeName), {
		voidType: "godot::Variant",
		intType: "int64_t",
		floatType: "double",
		typeMode: "value",
	});
}

export function mapCtorType(typeName: string): string {
	return mapParsedType(parseApiType(typeName), {
		voidType: "const godot::Variant &",
		intType: "int64_t",
		floatType: "double",
		typeMode: "const_ref",
	});
}

export function mapMethodReturnType(typeName?: string): string {
	return mapParsedType(parseApiType(typeName ?? "void"), {
		voidType: "void",
		intType: "int64_t",
		floatType: "double",
		typeMode: "value",
	});
}

export function mapApiMethodReturnType(method: ApiMethod): string {
	return mapMethodReturnType(method.return_type ?? method.return_value?.type);
}

export function mapMethodArgType(typeName: string): string {
	return mapParsedType(parseApiType(typeName), {
		voidType: "",
		intType: "int64_t",
		floatType: "double",
		typeMode: "const_ref",
	});
}

export function mapOperatorArgType(typeName: string): string {
	return mapMethodArgType(typeName);
}
