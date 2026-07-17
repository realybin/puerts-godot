// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import type { ApiEnum } from "../api-types.js";
import { emitDeclaration } from "../documentation.js";
import { sanitizeIdentifier } from "../naming.js";

export function collectEnumConstantNames(enums: ApiEnum[] | undefined): Set<string> {
	const names = new Set<string>();
	for (const enumDef of enums ?? []) {
		for (const value of enumDef.values) {
			names.add(value.name);
		}
	}
	return names;
}

export function emitNestedEnums(ownerName: string, enums: ApiEnum[] | undefined): string[] {
	const enumDefs = enums ?? [];
	if (enumDefs.length === 0) {
		return [];
	}
	const out: string[] = [];
	const emitted = new Set<string>();
	out.push(`namespace ${ownerName} {`);
	for (const enumDef of enumDefs) {
		const enumName = sanitizeIdentifier(enumDef.name.includes(".") ? enumDef.name.split(".").at(-1)! : enumDef.name);
		if (emitted.has(enumName)) {
			continue;
		}
		emitted.add(enumName);
		out.push(...emitDeclaration(`enum ${enumName} {`, [enumDef.description], "\t"));
		for (const value of enumDef.values) {
			out.push(...emitDeclaration(`${sanitizeIdentifier(value.name)} = ${value.value},`, [value.description], "\t\t"));
		}
		out.push("\t}");
	}
	out.push("}");
	return out;
}

export function emitGlobalEnums(globalEnums: ApiEnum[]): string[] {
	const out: string[] = [];
	const emitted = new Set<string>();
	for (const enumDef of globalEnums) {
		const parts = enumDef.name.split(".");
		if (parts.length === 1) {
			const enumName = sanitizeIdentifier(parts[0]);
			if (emitted.has(enumName)) {
				continue;
			}
			emitted.add(enumName);
			out.push(...emitDeclaration(`enum ${enumName} {`, [enumDef.description]));
			for (const value of enumDef.values) {
				out.push(...emitDeclaration(`${sanitizeIdentifier(value.name)} = ${value.value},`, [value.description], "\t"));
			}
			out.push("}");
			continue;
		}
		const root = sanitizeIdentifier(parts[0]);
		const enumName = sanitizeIdentifier(parts.slice(1).join("_"));
		const scopedName = `${root}.${enumName}`;
		if (emitted.has(scopedName)) {
			continue;
		}
		emitted.add(scopedName);
		out.push(`namespace ${root} {`);
		out.push(...emitDeclaration(`enum ${enumName} {`, [enumDef.description], "\t"));
		for (const value of enumDef.values) {
			out.push(...emitDeclaration(`${sanitizeIdentifier(value.name)} = ${value.value},`, [value.description], "\t\t"));
		}
		out.push("\t}");
		out.push("}");
	}
	return out;
}
