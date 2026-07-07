// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import type { ApiData } from "./types.js";

export function toSnakeCase(input: string): string {
	return input
		.replace(/([A-Z]+)([A-Z][a-z])/g, "$1_$2")
		.replace(/([a-z0-9])([A-Z])/g, "$1_$2")
		.toLowerCase()
		.replace(/2_d\b/g, "2d")
		.replace(/3_d\b/g, "3d")
		.replace(/4_d\b/g, "4d");
}

export function sanitizeIdentifier(name: string): string {
	const replaced = name.replace(/[^a-zA-Z0-9_]/g, "_");
	if (/^[0-9]/.test(replaced)) {
		return `_${replaced}`;
	}
	return replaced || "arg";
}

export function escapeRegex(text: string): string {
	return text.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

export function stripCppComments(source: string): string {
	let out = source.replace(/\/\*[\s\S]*?\*\//g, "");
	out = out.replace(/\/\/.*$/gm, "");
	return out;
}

export function toIncludeGuardMacro(outputPath: string): string {
	const base = outputPath.toUpperCase().replace(/[^A-Z0-9]/g, "_");
	return `PUERTS_GODOT_${base}`;
}

export function resolveApiVersion(data: ApiData): string {
	let apiVersion = data.header?.version_full_name;
	if (!apiVersion) {
		const hasNumericVersion = [
			data.header?.version_major,
			data.header?.version_minor,
			data.header?.version_patch,
		].every((v) => typeof v === "number");
		if (hasNumericVersion) {
			apiVersion = `Godot Engine v${data.header!.version_major}.${data.header!.version_minor}.${data.header!.version_patch}`;
		} else {
			apiVersion = "unknown";
		}
	}
	return apiVersion;
}
