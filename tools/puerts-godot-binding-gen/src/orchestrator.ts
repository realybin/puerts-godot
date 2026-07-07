// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, resolve } from "node:path";

import { normalizeOperators } from "../../puerts-godot-operator-model/index.js";
import { generateClassBinding, generateEnumBindings } from "./class_codegen.js";
import { generateGlobalScopeBinding } from "./global_scope_codegen.js";
import { analyzeHeaderMethods, readClassHeaderSource } from "./header_analysis.js";
import { collectPropertyBindings } from "./property_bindings.js";
import { resolveApiVersion, toIncludeGuardMacro, toSnakeCase } from "./text_utils.js";
import type {
	ApiClassLike,
	ApiData,
	CliArgs,
	GeneratedClassBinding,
	GeneratedEnumBinding,
	PropertyRule,
	PropertyRulesConfig,
} from "./types.js";

function loadPropertyRules(propertyRulesPath: string): PropertyRule[] {
	try {
		const propertyRaw = readFileSync(propertyRulesPath, "utf8");
		const propertyRulesConfig = JSON.parse(propertyRaw) as PropertyRulesConfig;
		return propertyRulesConfig.custom_properties ?? [];
	} catch (error) {
		if (isMissingFileError(error)) {
			return [];
		}
		throw error;
	}
}

function isMissingFileError(error: unknown): error is NodeJS.ErrnoException {
	return typeof error === "object" && error !== null && "code" in error && error.code === "ENOENT";
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

function warnMissingClasses(args: CliArgs, missingNames: string[]): void {
	if (missingNames.length === 0) {
		return;
	}

	console.warn(
		`Warning: class not found in extension_api.${args.classSource === "builtin" ? "builtin_classes" : "classes"}, skipped: ${missingNames.join(", ")}`,
	);
}

function resolveSelectedClasses(sourceClasses: ApiClassLike[], args: CliArgs): ApiClassLike[] {
	if (args.classNames.length === 0) {
		return sourceClasses;
	}

	const classMap = new Map(sourceClasses.map((cls) => [cls.name, cls]));
	const missingNames = args.classNames.filter((name) => !classMap.has(name));
	warnMissingClasses(args, missingNames);
	return args.classNames.flatMap((name) => {
		const found = classMap.get(name);
		return found ? [found] : [];
	});
}

function writeGeneratedFile(outputPath: string, content: string): void {
	mkdirSync(dirname(outputPath), { recursive: true });
	writeFileSync(outputPath, content, "utf8");
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
	projectRoot: string,
	args: CliArgs,
	customRulesByClass: Map<string, PropertyRule[]>,
	helperNamespace: string,
): {
	generated: GeneratedClassBinding[];
	generatedEnums: GeneratedEnumBinding[];
	propertyHelpers: string[];
	missingPropertyNotes: string[];
} {
	const generated: GeneratedClassBinding[] = [];
	const generatedEnums: GeneratedEnumBinding[] = [];
	const propertyHelpers: string[] = [];
	const missingPropertyNotes: string[] = [];

	for (const cls of selectedClasses) {
		const apiMethods = cls.methods ?? [];
		const nonVarargMethods = apiMethods.filter((m) => !m.is_vararg);
		const varargMethods = apiMethods.filter((m) => !!m.is_vararg);
		const enumBindings = generateEnumBindings(cls);
		generatedEnums.push(...enumBindings);

		let headerAnalysis = {
			methodNameCounts: new Map<string, number>(),
			topLevelMembers: new Set<string>(),
			source: "",
		};
		let filteredMethods = nonVarargMethods;
		const headerSource = readClassHeaderSource(projectRoot, cls.name, args.classSource);
		if (headerSource !== null) {
			headerAnalysis = analyzeHeaderMethods(headerSource, nonVarargMethods.map((m) => m.name), cls.name);
			const missingInHeader = [
				...new Set(nonVarargMethods.filter((m) => (headerAnalysis.methodNameCounts.get(m.name) ?? 0) === 0).map((m) => m.name)),
			];
			if (missingInHeader.length > 0) {
				console.warn(
					`Warning: skipped methods missing in header for ${cls.name}: ${missingInHeader.join(", ")}`,
				);
			}
			filteredMethods = nonVarargMethods.filter((m) => (headerAnalysis.methodNameCounts.get(m.name) ?? 0) > 0);
		} else {
			console.warn(`Warning: header not found for ${cls.name}, keep all non-vararg methods from JSON.`);
		}

		const propertyBindings = collectPropertyBindings(
			cls,
			headerAnalysis,
			customRulesByClass,
			missingPropertyNotes,
			helperNamespace,
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
				filteredMethods,
				varargMethods,
				propertyBindings,
				enumBindings,
				headerAnalysis,
			),
		);
	}

	return { generated, generatedEnums, propertyHelpers, missingPropertyNotes };
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
	const generatedNamespace = `${args.registerFunction}_generated_detail`;
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
	const uniqueIncludes = dedupe(includeLines);
	const enumTagDeclLines = generatedEnums.length > 0
		? [
			"namespace puerts_generated_enum_tags {",
			...generatedEnums.map((e) => `\tstruct ${e.tagStructName} {};`),
			"} // namespace puerts_generated_enum_tags",
			"",
		]
		: [];
	const scriptTypeLines = [
		...generated.map((c) => `PUERTS_SCRIPT_TYPE(godot::${c.className}, "${c.className}")`),
		...generatedEnums.map((e) => e.scriptTypeLine),
	];
	const registerCalls = [...generatedEnums.map((e) => e.registerCall), ...generated.map((g) => `\t${g.functionName}();`)].join("\n");
	const includeGuard = toIncludeGuardMacro(basename(outputPath));
	const apiVersion = resolveApiVersion(data);

	return [
		"// AUTO-GENERATED FILE. DO NOT EDIT.",
		"// Generated by tools/puerts-godot-binding-gen from extension_api.json.",
		`// generated_at: ${new Date().toISOString()}`,
		`// api_version: ${apiVersion}`,
		"// Method bindings are generated directly from extension_api.json.",
		"",
		`#ifndef ${includeGuard}`,
		`#define ${includeGuard}`,
		"",
		...uniqueIncludes,
		"",
		...enumTagDeclLines,
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
	const propertyRulesPath = resolve(cwd, args.propertyRulesJson);
	const projectRoot = resolve(dirname(dirname(dirname(inputPath))));
	const raw = readFileSync(inputPath, "utf8");
	const data = JSON.parse(raw) as ApiData;

	if (args.target === "globalscope") {
		const out = generateGlobalScopeBinding(args, data);
		writeGeneratedFile(outputPath, out);
		console.log(`Generated GlobalScope bindings: ${outputPath}`);
		return;
	}

	const sourceClasses = getSourceClassCollection(data, args.classSource);
	const selectedClasses = resolveSelectedClasses(sourceClasses, args);
	if (selectedClasses.length === 0) {
		throw new Error("No classes selected for generation.");
	}

	const helperNamespace = `${args.registerFunction}_generated_detail`;
	const customRules = loadPropertyRules(propertyRulesPath);
	const customRulesByClass = groupByClassName(customRules);
	const { generated, generatedEnums, propertyHelpers, missingPropertyNotes } = buildGeneratedArtifacts(
		selectedClasses,
		projectRoot,
		args,
		customRulesByClass,
		helperNamespace,
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

	writeGeneratedFile(outputPath, out);
	if (missingPropertyNotes.length > 0) {
		console.warn(`Warning: skipped properties: ${dedupe(missingPropertyNotes).join(", ")}`);
	}
	console.log(`Generated ${generated.length}/${selectedClasses.length} classes: ${outputPath}`);
}
