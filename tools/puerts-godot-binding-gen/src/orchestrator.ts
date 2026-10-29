// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, resolve } from "node:path";

import { normalizeOperators } from "../../puerts-godot-operator-model/index.js";
import { generateClassBinding, generateEnumBindings } from "./class_codegen.js";
import { generateGlobalScopeBinding } from "./global_scope_codegen.js";
import { collectPropertyBindings } from "./property_bindings.js";
import { resolveApiVersion, toIncludeGuardMacro, toSnakeCase } from "./text_utils.js";
import type {
	ApiClassLike,
	ApiData,
	CliArgs,
	GeneratedClassBinding,
	GeneratedEnumBinding,
	PropertyRule,
	MethodAdapter,
	BindingRulesConfig,
} from "./types.js";

function loadBindingRules(bindingRulesPath: string): BindingRulesConfig {
	const rules = JSON.parse(readFileSync(bindingRulesPath, "utf8")) as BindingRulesConfig;
	for (const key of Object.keys(rules)) {
		if (!["methods", "properties", "excluded_methods", "excluded_properties"].includes(key)) {
			throw new Error(`Unknown binding rules field: ${key}.`);
		}
	}
	for (const [name, rule] of Object.entries(rules.methods ?? {})) {
		for (const key of Object.keys(rule)) {
			if (!["cpp_name", "native_signature", "default_argument", "reason"].includes(key)) {
				throw new Error(`Unknown method rule field: ${name}.${key}.`);
			}
		}
	}
	return rules;
}

function groupByClassName(customRules: PropertyRule[]): Map<string, PropertyRule[]> {
	const grouped = new Map<string, PropertyRule[]>();
	for (const rule of customRules) {
		const rules = grouped.get(rule.class);
		if (rules) {
			rules.push(rule);
			continue;
		}
		grouped.set(rule.class, [rule]);
	}
	return grouped;
}

function getSourceClassCollection(data: ApiData, classSource: CliArgs["classSource"]): ApiClassLike[] {
	const classes = classSource === "builtin" ? data.builtin_classes ?? [] : data.classes ?? [];
	if (classes.length > 0) {
		return classes;
	}

	throw new Error(`No classes found in extension_api.${classSource === "builtin" ? "builtin_classes" : "classes"}.`);
}

function dedupe<T>(values: T[]): T[] {
	return [...new Set(values)];
}

function resolveSelectedClasses(sourceClasses: ApiClassLike[], args: CliArgs): ApiClassLike[] {
	if (args.classNames.length === 0) {
		return sourceClasses;
	}

	const classMap = new Map(sourceClasses.map((cls) => [cls.name, cls]));
	const missingNames = args.classNames.filter((name) => !classMap.has(name));
	if (missingNames.length > 0) {
		throw new Error(`Classes are missing from the selected API: ${missingNames.join(", ")}.`);
	}
	return args.classNames.flatMap((name) => {
		const found = classMap.get(name);
		return found ? [found] : [];
	});
}

function writeGeneratedFile(outputPath: string, content: string): void {
	mkdirSync(dirname(outputPath), { recursive: true });
	writeFileSync(outputPath, content, "utf8");
}

function wrapWithGodotVersionMacro(content: string, version: string): string {
	if (!version) return content;
	const [major, minor] = version.split(".");
	const condition = `GODOT_VERSION_MAJOR == ${major} && GODOT_VERSION_MINOR == ${minor}`;
	const lines = content.trimEnd().split("\n");
	const guardStart = lines.findIndex((line) => line.startsWith("#ifndef "));
	const includeGuard = guardStart >= 0 ? lines[guardStart].slice("#ifndef ".length) : "";
	const guardDefine = guardStart + 1;
	const guardEnd = lines.lastIndexOf(`#endif // ${includeGuard}`);
	if (!includeGuard || lines[guardDefine] !== `#define ${includeGuard}` || guardEnd <= guardDefine) {
		throw new Error("Generated binding does not contain the expected include guard");
	}

	const body = lines.slice(guardDefine + 1, guardEnd);
	while (body[0] === "") body.shift();
	while (body.at(-1) === "") body.pop();
	return [
		...lines.slice(0, guardDefine + 1),
		"",
		"#include <godot_cpp/core/version.hpp>",
		`#if ${condition}`,
		"",
		...body,
		"",
		`#endif // ${condition}`,
		lines[guardEnd],
		...lines.slice(guardEnd + 1),
		"",
	].join("\n");
}

function resolveBuiltinTypeInclude(typeName: string): string | null {
	const trimmed = typeName.trim();
	if (!trimmed || trimmed === "Variant") {
		return null;
	}
	if (trimmed === "Object") {
		return "#include <godot_cpp/classes/object.hpp>";
	}
	if (trimmed === "bool" || trimmed === "int" || trimmed === "float" || trimmed === "Nil") {
		return null;
	}
	if (trimmed.startsWith("enum::") || trimmed.startsWith("bitfield::")) {
		return null;
	}
	return `#include <godot_cpp/variant/${toSnakeCase(trimmed)}.hpp>`;
}

function buildGeneratedArtifacts(
	selectedClasses: ApiClassLike[],
	args: CliArgs,
	customRulesByClass: Map<string, PropertyRule[]>,
	helperNamespace: string,
	excludedMethods: ReadonlySet<string>,
	excludedProperties: ReadonlySet<string>,
	methodAdapters: ReadonlyMap<string, MethodAdapter>,
): {
	generated: GeneratedClassBinding[];
	generatedEnums: GeneratedEnumBinding[];
	propertyHelpers: string[];
} {
	const generated: GeneratedClassBinding[] = [];
	const generatedEnums: GeneratedEnumBinding[] = [];
	const propertyHelpers: string[] = [];

	for (const cls of selectedClasses) {
		for (const key of methodAdapters.keys()) {
			if (key.startsWith(`${cls.name}.`) && !(cls.methods ?? []).some(method => key === `${cls.name}.${method.name}`)) {
				throw new Error(`Method rule does not match the selected API: ${key}.`);
			}
		}
		const apiMethods = (cls.methods ?? []).filter((method) => !excludedMethods.has(`${cls.name}.${method.name}`));
		const nonVarargMethods = apiMethods.filter((m) => !m.is_vararg);
		const varargMethods = apiMethods.filter((m) => !!m.is_vararg);
		const enumBindings = generateEnumBindings(cls);
		generatedEnums.push(...enumBindings);

		const propertyBindings = collectPropertyBindings(
			cls,
			customRulesByClass,
			helperNamespace,
			excludedProperties,
		);
		for (const p of propertyBindings) {
			if (p.helperCode) {
				propertyHelpers.push(p.helperCode);
			}
		}

		generated.push(
			generateClassBinding(
				cls,
				args.classSource,
				nonVarargMethods,
				varargMethods,
				propertyBindings,
				enumBindings,
				methodAdapters,
			),
		);
	}

	return { generated, generatedEnums, propertyHelpers };
}

function renderOutput(
	outputPath: string,
	args: CliArgs,
	data: ApiData,
	selectedClasses: ApiClassLike[],
	generated: GeneratedClassBinding[],
	generatedEnums: GeneratedEnumBinding[],
	propertyHelpers: string[],
): string {
	const generatedNamespace = "puerts_generated";
	const inheritedIncludeLines = selectedClasses
		.map((c) => c.inherits?.trim())
		.filter((name): name is string => !!name && name.length > 0)
		.map((name) =>
			args.classSource === "builtin"
				? `#include <godot_cpp/variant/${toSnakeCase(name)}.hpp>`
				: `#include <godot_cpp/classes/${toSnakeCase(name)}.hpp>`,
		);
	const operatorIncludeLines =
		args.classSource !== "builtin"
			? []
			: selectedClasses.flatMap((cls) => {
					if (!("operators" in cls)) {
						return [];
					}
					return normalizeOperators(cls.name, cls.operators).flatMap((operator) => {
						const includes = [resolveBuiltinTypeInclude(operator.leftType), resolveBuiltinTypeInclude(operator.returnType)];
						if (operator.rightType) {
							includes.push(resolveBuiltinTypeInclude(operator.rightType));
						}
						return includes.filter((include): include is string => include !== null);
					});
				});

	const includeLines = [
		...generated.map((g) =>
			g.classSource === "builtin"
				? `#include <godot_cpp/variant/${toSnakeCase(g.className)}.hpp>`
				: `#include <godot_cpp/classes/${toSnakeCase(g.className)}.hpp>`,
		),
		...inheritedIncludeLines,
		...operatorIncludeLines,
	];
	const uniqueIncludes = dedupe(includeLines).sort();
	const scriptTypeLines = [
		...generated.map((c) => `PUERTS_SCRIPT_TYPE(godot::${c.className}, "${c.className}")`),
	];
	const registerCalls = [...generatedEnums.map((e) => `\t${e.registrationFunctionName}();`), ...generated.map((g) => `\t${g.registrationFunctionName}();`)].join("\n");
	const includeGuard = toIncludeGuardMacro(basename(outputPath));
	const apiVersion = resolveApiVersion(data);

	return [
		"// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors",
		"// SPDX-License-Identifier: BSD-3-Clause",
		"",
		"// AUTO-GENERATED FILE. DO NOT EDIT.",
		"// Generated by tools/puerts-godot-binding-gen from extension_api.json.",
		`// api_version: ${apiVersion}`,
		"",
		`#ifndef ${includeGuard}`,
		`#define ${includeGuard}`,
		"",
		...uniqueIncludes,
		"",
		...scriptTypeLines,
		"",
		...(propertyHelpers.length > 0
			? [
				`namespace ${generatedNamespace} {`,
				...propertyHelpers.flatMap((h) => [h, ""]),
				`} // namespace ${generatedNamespace}`,
				"",
			]
			: []),
		...generatedEnums.map((e) => e.code.trimEnd()),
		...(generatedEnums.length > 0 ? [""] : []),
		...generated.map((g) => g.code.trimEnd()),
		"",
		`inline void ${args.registerFunction}() {`,
		registerCalls,
		"}",
		"",
		`#endif // ${includeGuard}`,
		"",
	].join("\n");
}

export function generateBindings(args: CliArgs): void {
	const cwd = process.cwd();
	const inputPath = resolve(cwd, args.input);
	const outputPath = resolve(cwd, args.output);
	const bindingRulesPath = resolve(cwd, args.bindingRulesJson);
	const raw = readFileSync(inputPath, "utf8");
	const data = JSON.parse(raw) as ApiData;
	const major = data.header?.version_major;
	const minor = data.header?.version_minor;
	if (!Number.isInteger(major) || !Number.isInteger(minor) || major! < 0 || minor! < 0) {
		throw new Error("API header must contain valid version_major and version_minor.");
	}
	const apiVersion = `${major}.${minor}`;

	if (args.target === "globalscope") {
		const out = generateGlobalScopeBinding(args, data);
		writeGeneratedFile(outputPath, wrapWithGodotVersionMacro(out, apiVersion));
		console.log(`Generated GlobalScope bindings: ${outputPath}`);
		return;
	}

	const sourceClasses = getSourceClassCollection(data, args.classSource);
	const selectedClasses = resolveSelectedClasses(sourceClasses, args);
	if (selectedClasses.length === 0) {
		throw new Error("No classes selected for generation.");
	}

	const helperNamespace = "puerts_generated";
	const rules = loadBindingRules(bindingRulesPath);
	const customRules = rules.properties ?? [];
	const customRulesByClass = groupByClassName(customRules);
	const { generated, generatedEnums, propertyHelpers } = buildGeneratedArtifacts(
		selectedClasses,
		args,
		customRulesByClass,
		helperNamespace,
		new Set(rules.excluded_methods ?? []),
		new Set(rules.excluded_properties ?? []),
		new Map(Object.entries(rules.methods ?? {})),
	);
	const out = renderOutput(
		outputPath,
		args,
		data,
		selectedClasses,
		generated,
		generatedEnums,
		propertyHelpers,
	);

	writeGeneratedFile(outputPath, wrapWithGodotVersionMacro(out, apiVersion));

	console.log(`Generated ${generated.length}/${selectedClasses.length} classes: ${outputPath}`);
}
