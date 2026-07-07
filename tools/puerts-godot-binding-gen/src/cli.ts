// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import { readFileSync } from "node:fs";
import { dirname, resolve } from "node:path";

import type { ClassSource, ClassesConfig, CliArgs, GenerateTarget } from "./types.js";

const DEFAULT_PROPERTY_RULES_JSON = "./config/puerts_builtin_property_rules.json";
const DEFAULT_REGISTER_FUNCTION = "register_puerts_builtin_bindings_generated";
const DEFAULT_GLOBAL_SCOPE_REGISTER_FUNCTION = "register_puerts_global_scope_generated";
const GENERATE_TARGETS = ["classes", "globalscope"] as const;
const CPP_IDENTIFIER_RE = /^[_A-Za-z][_0-9A-Za-z]*$/;

function isClassSource(value: string): value is ClassSource {
	return value === "builtin" || value === "classes";
}

function isGenerateTarget(value: string): value is GenerateTarget {
	return GENERATE_TARGETS.includes(value as GenerateTarget);
}

function readOptionValue(argv: string[], index: number, flag: string): string {
	const value = (argv[index + 1] ?? "").trim();
	if (!value) {
		throw new Error(`Missing value for ${flag}.`);
	}
	return value;
}

function readValidatedOption<T extends string>(
	argv: string[],
	index: number,
	flag: string,
	guard: (value: string) => value is T,
	expected: string,
): T {
	const value = readOptionValue(argv, index, flag);
	if (!guard(value)) {
		throw new Error(`Invalid ${flag} value: ${value}. Expected ${expected}.`);
	}
	return value;
}

function parseClassNamesFromText(raw: string): string[] {
	return raw
		.split(",")
		.map((x) => x.trim())
		.filter(Boolean);
}

function resolveClassNamesFromPuertsFile(classesFromPuertsFile: string): string[] {
	const puertsPath = resolve(process.cwd(), classesFromPuertsFile);
	const source = readFileSync(puertsPath, "utf8");
	const matches = [...source.matchAll(/PUERTS_SCRIPT_TYPE\(godot::([A-Za-z0-9_]+)\s*,/g)];
	let classNames = matches.map((m) => m[1]);

	if (classNames.length === 0) {
		const includeMatch = source.match(/#include\s+"([^"]*puerts_builtin_bindings[^"]*\.inc)"/);
		if (includeMatch) {
			const includePath = resolve(dirname(puertsPath), includeMatch[1]);
			const includeSource = readFileSync(includePath, "utf8");
			const includeMatches = [...includeSource.matchAll(/PUERTS_SCRIPT_TYPE\(godot::([A-Za-z0-9_]+)\s*,/g)];
			classNames = includeMatches.map((m) => m[1]);
		}
	}

	if (classNames.length === 0) {
		throw new Error(`No PUERTS_SCRIPT_TYPE(godot::...) entries found from ${classesFromPuertsFile}`);
	}

	return classNames;
}

function parseClassNamesFromJson(classesJson: string): {
	classNames: string[];
	classSource?: ClassSource;
	registerFunction?: string;
} {
	const jsonPath = resolve(process.cwd(), classesJson);
	const raw = readFileSync(jsonPath, "utf8");
	const parsed = JSON.parse(raw) as unknown;

	if (Array.isArray(parsed)) {
		return { classNames: parsed.map((x) => String(x).trim()).filter(Boolean) };
	}
	if (typeof parsed === "object" && parsed !== null) {
		const cfg = parsed as ClassesConfig;
		return {
			classNames: (cfg.classes ?? []).map((x) => String(x).trim()).filter(Boolean),
			classSource: cfg.source,
			registerFunction: cfg.register_function,
		};
	}
	throw new Error(`Invalid classes json format: ${classesJson}`);
}

function resolveClassSelection(argv: {
	classesArg: string;
	classesJson: string;
	classesFromPuertsFile: string;
	allBuiltin: boolean;
	classSource: ClassSource;
	registerFunction: string;
}): Pick<CliArgs, "classNames" | "classSource" | "registerFunction"> {
	let classNames = argv.classesArg ? parseClassNamesFromText(argv.classesArg) : [];
	let classSource = argv.classSource;
	let registerFunction = argv.registerFunction;

	if (argv.classesJson) {
		const parsed = parseClassNamesFromJson(argv.classesJson);
		classNames = parsed.classNames;
		classSource = parsed.classSource ?? classSource;
		registerFunction = parsed.registerFunction?.trim() || registerFunction;
	}

	if (argv.classesFromPuertsFile) {
		classNames = resolveClassNamesFromPuertsFile(argv.classesFromPuertsFile);
	}

	if (argv.allBuiltin) {
		classNames = [];
	}

	return {
		classNames: [...new Set(classNames)],
		classSource,
		registerFunction,
	};
}

export function parseArgs(argv: string[]): CliArgs {
	let input = "";
	let output = "";
	let classesArg = "";
	let classesJson = "";
	let classesFromPuertsFile = "";
	let propertyRulesJson = DEFAULT_PROPERTY_RULES_JSON;
	let classSource: ClassSource = "builtin";
	let registerFunction = DEFAULT_REGISTER_FUNCTION;
	let target: GenerateTarget = "classes";
	let allBuiltin = false;

	for (let i = 0; i < argv.length; i += 1) {
		const token = argv[i];
		switch (token) {
			case "--input":
				input = readOptionValue(argv, i, token);
				i += 1;
				break;
			case "--output":
				output = readOptionValue(argv, i, token);
				i += 1;
				break;
			case "--classes":
				classesArg = readOptionValue(argv, i, token);
				i += 1;
				break;
			case "--classes-json":
				classesJson = readOptionValue(argv, i, token);
				i += 1;
				break;
			case "--classes-from-puerts-file":
				classesFromPuertsFile = readOptionValue(argv, i, token);
				i += 1;
				break;
			case "--property-rules-json":
				propertyRulesJson = readOptionValue(argv, i, token);
				i += 1;
				break;
			case "--class-source":
				classSource = readValidatedOption(argv, i, token, isClassSource, "'builtin' or 'classes'");
				i += 1;
				break;
			case "--register-function":
				registerFunction = readOptionValue(argv, i, token);
				i += 1;
				break;
			case "--target":
				target = readValidatedOption(argv, i, token, isGenerateTarget, "'classes' or 'globalscope'");
				i += 1;
				break;
			case "--all-builtin":
				allBuiltin = true;
				break;
		}
	}

	if (!input || !output) {
		throw new Error(
			"Usage: node dist/index.js --input <extension_api.json> --output <output.inc> [--classes A,B,C | --classes-json <classes.json> | --classes-from-puerts-file <puerts_builtin_binding.cpp> | --all-builtin] [--class-source builtin|classes] [--register-function <identifier>]",
		);
	}

	if (target === "globalscope" && registerFunction === DEFAULT_REGISTER_FUNCTION) {
		registerFunction = DEFAULT_GLOBAL_SCOPE_REGISTER_FUNCTION;
	}

	const selection = resolveClassSelection({
		classesArg,
		classesJson,
		classesFromPuertsFile,
		allBuiltin,
		classSource,
		registerFunction,
	});

	if (!CPP_IDENTIFIER_RE.test(selection.registerFunction)) {
		throw new Error(`Invalid --register-function value: ${selection.registerFunction}. Expected a valid C++ identifier.`);
	}

	return {
		input,
		output,
		classNames: selection.classNames,
		propertyRulesJson,
		classSource: selection.classSource,
		registerFunction: selection.registerFunction,
		target,
	};
}
