// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import { RESERVED } from "./constants.js";

export function cleanTypeName(input: string): string {
	return input.trim().replace(/^const\s+/, "").replace(/\s*&$/, "").replace(/^\d+\/\d+:/, "");
}

export function splitUnionCandidates(typeName: string): string[] {
	const raw = typeName
		.split(",")
		.map((s) => s.trim())
		.filter(Boolean);
	const includes = raw.filter((s) => !s.startsWith("-"));
	return includes.length > 0 ? includes : raw.map((s) => s.replace(/^-/, ""));
}

export function sanitizeIdentifier(name: string): string {
	let n = name.trim();
	if (!/^[$A-Za-z_][$\w]*$/.test(n)) {
		n = n.replace(/[^$A-Za-z0-9_]/g, "_");
		if (!/^[$A-Za-z_]/.test(n)) {
			n = `_${n}`;
		}
	}
	return RESERVED.has(n) ? `${n}_` : n;
}
