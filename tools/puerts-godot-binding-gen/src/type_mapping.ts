// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
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
	if (t.startsWith("enum::") || t.startsWith("bitfield::")) return { kind: "enum_like" };
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
		case "enum_like":
			return options.intType;
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
		floatType: "godot::real_t",
		typeMode: "value",
	});
}

export function mapCtorType(typeName: string): string {
	return mapParsedType(parseApiType(typeName), {
		voidType: "const godot::Variant &",
		intType: "int64_t",
		floatType: "godot::real_t",
		typeMode: "const_ref",
	});
}

export function mapMethodReturnType(typeName?: string): string {
	return mapParsedType(parseApiType(typeName ?? "void"), {
		voidType: "void",
		intType: "int64_t",
		floatType: "godot::real_t",
		typeMode: "value",
	});
}

export function mapMethodArgType(typeName: string): string {
	return mapParsedType(parseApiType(typeName), {
		voidType: "",
		intType: "int64_t",
		floatType: "godot::real_t",
		typeMode: "const_ref",
	});
}

export function mapOperatorArgType(typeName: string): string {
	return mapMethodArgType(typeName);
}

export function methodPointerExprFromApi(className: string, method: ApiMethod): string {
	const args = (method.arguments ?? [])
		.map((a) => mapMethodArgType(a.type))
		.filter(Boolean)
		.join(", ");
	const returnType = mapMethodReturnType(method.return_type);
	if (method.is_static) {
		return `static_cast<${returnType} (*)(${args})>(&godot::${className}::${method.name})`;
	}
	const constSuffix = method.is_const ? " const" : "";
	return `static_cast<${returnType} (godot::${className}::*)(${args})${constSuffix}>(&godot::${className}::${method.name})`;
}
