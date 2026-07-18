// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import type { ApiEnum } from "../api-types.js";
import type { DocumentationScope } from "../documentation.js";
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

function emitEnumDefinition(enumName: string, enumDef: ApiEnum, documentation: DocumentationScope, indent = ""): string[] {
	const valueIndent = `${indent}\t`;
	const out = documentation.emit(`enum ${enumName} {`, [enumDef.description], indent);
	for (const value of enumDef.values) {
		out.push(...documentation.emit(`${sanitizeIdentifier(value.name)} = ${value.value},`, [value.description], valueIndent));
	}
	out.push(`${indent}}`);
	return out;
}

export function emitNestedEnums(
	ownerName: string,
	enums: ApiEnum[] | undefined,
	documentation: DocumentationScope,
): string[] {
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
		out.push(...emitEnumDefinition(enumName, enumDef, documentation, "\t"));
	}
	out.push("}");
	return out;
}

export function emitGlobalEnums(globalEnums: ApiEnum[], documentation: DocumentationScope): string[] {
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
			out.push(...emitEnumDefinition(enumName, enumDef, documentation));
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
		out.push(...emitEnumDefinition(enumName, enumDef, documentation, "\t"));
		out.push("}");
	}
	return out;
}
