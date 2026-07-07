// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import { readFileSync } from "node:fs";
import { join } from "node:path";

import { escapeRegex, stripCppComments, toSnakeCase } from "./text_utils.js";
import type { ClassSource, HeaderAnalysis } from "./types.js";

export function countMethodOverloadsInHeader(cleanedHeader: string, methodName: string): number {
	const re = new RegExp(
		`(?:^|\\n)[ \\t]*(static\\s+)?([A-Za-z0-9_:<>,*&~][A-Za-z0-9_:<>,*&~ \\t]*)\\b(?:[A-Za-z_][A-Za-z0-9_]*::)?${escapeRegex(methodName)}\\s*\\(([^;{}]*)\\)\\s*(const\\s*)?(?:;|\\{)`,
		"g",
	);
	const signatures = new Set<string>();
	let match: RegExpExecArray | null = re.exec(cleanedHeader);
	while (match) {
		const prefix = (match[2] ?? "").trim();
		if (/^(return|if|for|while|switch|else|do)\b/.test(prefix)) {
			match = re.exec(cleanedHeader);
			continue;
		}
		const isStatic = !!match[1];
		const argsRaw = match[3] ?? "";
		const isConst = !!match[4];
		const argsNoDefault = argsRaw
			.replace(/\s*=\s*[^,]+/g, "")
			.replace(/\s+/g, " ")
			.trim();
		const key = `${isStatic ? "S" : "I"}|${isConst ? "C" : "N"}|${argsNoDefault}`;
		signatures.add(key);
		match = re.exec(cleanedHeader);
	}
	return signatures.size;
}

function extractTopLevelMembers(source: string, className: string): Set<string> {
	const members = new Set<string>();
	const cleaned = stripCppComments(source);
	const classRe = new RegExp(`\\b(?:struct|class)\\b[^\\n{;]*\\b${escapeRegex(className)}\\b[^\\n{;]*\\{`);
	const match = classRe.exec(cleaned);
	if (!match) {
		return members;
	}

	let i = (match.index ?? 0) + match[0].length;
	let depth = 1;
	let statement = "";

	while (i < cleaned.length && depth > 0) {
		const ch = cleaned[i];
		if (depth === 1) {
			if (ch === "{") {
				depth += 1;
				statement = "";
				i += 1;
				continue;
			}
			if (ch === "}") {
				depth -= 1;
				statement = "";
				i += 1;
				continue;
			}
			statement += ch;
			if (ch === ";") {
				const stmt = statement.trim();
				statement = "";
				if (!stmt || stmt.includes("(") || stmt.includes(")")) {
					i += 1;
					continue;
				}
				if (/^(public|private|protected)\s*:/.test(stmt)) {
					i += 1;
					continue;
				}
				if (/^(using|typedef|friend|template|static_assert|enum|struct|class)\b/.test(stmt)) {
					i += 1;
					continue;
				}
				const withoutInit = stmt.replace(/=[^;]+;/, ";");
				const declarations = withoutInit.replace(/;$/, "").split(",");
				for (const declRaw of declarations) {
					const decl = declRaw.trim();
					const m = decl.match(/([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*$/);
					if (m) {
						members.add(m[1]);
					}
				}
			}
			i += 1;
			continue;
		}

		if (ch === "{") depth += 1;
		if (ch === "}") depth -= 1;
		i += 1;
	}

	return members;
}

export function hasLikelyMemberDeclaration(cleanedHeader: string, memberName: string): boolean {
	const re = new RegExp(
		`(?:^|\\n)\\s*(?:const\\s+|volatile\\s+|mutable\\s+)?(?:[A-Za-z_][A-Za-z0-9_:<>]*\\s+)+[*&\\s]*${escapeRegex(memberName)}\\b\\s*(?:\\[[^\\]]*\\])?\\s*(?:=\\s*[^;]+)?;`,
		"m",
	);
	return re.test(cleanedHeader);
}

export function analyzeHeaderMethods(source: string, methodNames: string[], className: string): HeaderAnalysis {
	const methodNameCounts = new Map<string, number>();
	const cleaned = stripCppComments(source);

	for (const name of [...new Set(methodNames)]) {
		methodNameCounts.set(name, countMethodOverloadsInHeader(cleaned, name));
	}

	return {
		methodNameCounts,
		topLevelMembers: extractTopLevelMembers(source, className),
		source: cleaned,
	};
}

export function readClassHeaderSource(projectRoot: string, className: string, classSource: ClassSource): string | null {
	const stem = `${toSnakeCase(className)}.hpp`;
	const candidates =
		classSource === "builtin"
			? [
				join(projectRoot, "godot-cpp", "include", "godot_cpp", "variant", stem),
				join(projectRoot, "godot-cpp", "gen", "include", "godot_cpp", "variant", stem),
			]
			: [
				join(projectRoot, "godot-cpp", "include", "godot_cpp", "classes", stem),
				join(projectRoot, "godot-cpp", "gen", "include", "godot_cpp", "classes", stem),
			];
	for (const candidate of candidates) {
		try {
			return readFileSync(candidate, "utf8");
		} catch {
			// try next candidate
		}
	}
	return null;
}
