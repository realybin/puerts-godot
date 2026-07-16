// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

export type NativePrimitiveTypeSpec = {
	alias: string;
	javascriptType: "boolean" | "number" | "string";
	nativeType: string;
	description: string;
	metaNames?: string[];
	mayLosePrecision?: boolean;
};

export const NATIVE_PRIMITIVE_TYPE_SPECS: NativePrimitiveTypeSpec[] = [
	{
		alias: "Bool",
		javascriptType: "boolean",
		nativeType: "bool",
		description: "Boolean value",
	},
	{
		alias: "String",
		javascriptType: "string",
		nativeType: "String",
		description: "UTF-32 string value",
	},
	{
		alias: "Int",
		javascriptType: "number",
		nativeType: "int",
		description: "integer value",
		mayLosePrecision: true,
	},
	{
		alias: "Float",
		javascriptType: "number",
		nativeType: "float",
		description: "floating point",
	},
	{
		alias: "Bitfield",
		javascriptType: "number",
		nativeType: "bitfield",
		description: "integer bit mask",
	},
	{
		alias: "Float32",
		javascriptType: "number",
		nativeType: "float",
		description: "32-bit floating-point",
		metaNames: ["float"],
	},
	{
		alias: "Float64",
		javascriptType: "number",
		nativeType: "double",
		description: "64-bit floating-point",
		metaNames: ["double"],
	},
	{
		alias: "Int8",
		javascriptType: "number",
		nativeType: "int8_t",
		description: "signed 8-bit integer",
		metaNames: ["int8", "int8_t"],
	},
	{
		alias: "UInt8",
		javascriptType: "number",
		nativeType: "uint8_t",
		description: "unsigned 8-bit integer",
		metaNames: ["uint8", "uint8_t"],
	},
	{
		alias: "Int16",
		javascriptType: "number",
		nativeType: "int16_t",
		description: "signed 16-bit integer",
		metaNames: ["int16", "int16_t"],
	},
	{
		alias: "UInt16",
		javascriptType: "number",
		nativeType: "uint16_t",
		description: "unsigned 16-bit integer",
		metaNames: ["uint16", "uint16_t"],
	},
	{
		alias: "Int32",
		javascriptType: "number",
		nativeType: "int32_t",
		description: "signed 32-bit integer",
		metaNames: ["int32", "int32_t"],
	},
	{
		alias: "UInt32",
		javascriptType: "number",
		nativeType: "uint32_t",
		description: "unsigned 32-bit integer",
		metaNames: ["uint32", "uint32_t"],
	},
	{
		alias: "Int64",
		javascriptType: "number",
		nativeType: "int64_t",
		description: "signed 64-bit integer",
		metaNames: ["int64", "int64_t"],
		mayLosePrecision: true,
	},
	{
		alias: "UInt64",
		javascriptType: "number",
		nativeType: "uint64_t",
		description: "unsigned 64-bit integer",
		metaNames: ["uint64", "uint64_t"],
		mayLosePrecision: true,
	},
	{
		alias: "Char32",
		javascriptType: "number",
		nativeType: "char32_t",
		description: "32-bit Unicode code point",
		metaNames: ["char32", "char32_t"],
	},
];

const NATIVE_PRIMITIVE_ALIAS_BY_META = new Map(
	NATIVE_PRIMITIVE_TYPE_SPECS.flatMap((spec) =>
		(spec.metaNames ?? []).map((name) => [name, spec.alias] as const),
	),
);

export function nativePrimitiveAlias(meta: string | undefined): string | null {
	return meta ? (NATIVE_PRIMITIVE_ALIAS_BY_META.get(meta) ?? null) : null;
}

export function emitNativePrimitiveAliases(precision: string | undefined): string[] {
	const out: string[] = [];
	for (const spec of NATIVE_PRIMITIVE_TYPE_SPECS) {
		out.push("/**");
		out.push(` * JavaScript \`${spec.javascriptType}\`; native \`${spec.nativeType}\` (${spec.description}).`);
		if (spec.mayLosePrecision) {
			out.push(" * Values outside the JavaScript safe integer range may lose precision.");
		}
		out.push(` * @nativeType ${spec.nativeType}`);
		out.push(" */");
		out.push(`type ${spec.alias} = ${spec.javascriptType};`);
	}

	const realAlias = precision === "single" ? "Float32" : precision === "double" ? "Float64" : "Float";
	const realDescription =
		precision === "single"
			? "`float` for this single-precision API"
			: precision === "double"
				? "`double` for this double-precision API"
				: "`float` or `double`, depending on the Godot build";
	out.push("/**");
	out.push(` * JavaScript \`number\`; native Godot \`real_t\` (${realDescription}).`);
	out.push(" * @nativeType real_t");
	out.push(" */");
	out.push(`type Real = ${realAlias};`);
	return out;
}
