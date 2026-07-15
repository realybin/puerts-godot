// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import { countMethodOverloadsInHeader, hasLikelyMemberDeclaration } from "./header_analysis.js";
import { mapPropertyValueType } from "./type_mapping.js";
import { sanitizeIdentifier, toSnakeCase } from "./text_utils.js";
import type { ApiClassLike, BoundPropertyBinding, HeaderAnalysis, PropertyRule } from "./types.js";

function makePropertyHelperBaseName(className: string, propertyName: string): string {
	return `${sanitizeIdentifier(toSnakeCase(className))}_${sanitizeIdentifier(propertyName)}`;
}

function buildPropertyGetterExpr(rule: PropertyRule): string {
	if (rule.kind === "method") {
		return `instance.get()->${rule.getter!}()`;
	}
	if (rule.kind === "indexed_method") {
		return `instance.get()->${rule.getter!}(${rule.index!})`;
	}
	return `instance.get()->${rule.member_expr!}`;
}

function buildPropertySetterStatement(rule: PropertyRule): string {
	if (rule.kind === "method") {
		return `instance.get()->${rule.setter!}(eastl::move(value));`;
	}
	if (rule.kind === "indexed_method") {
		return `instance.get()->${rule.setter!}(${rule.index!}, eastl::move(value));`;
	}
	return `instance.get()->${rule.member_expr!} = eastl::move(value);`;
}

function generatePropertyHelperCode(rule: PropertyRule): { getterName: string; setterName: string; code: string } {
	const typeName = mapPropertyValueType(rule.type);
	const base = makePropertyHelperBaseName(rule.class, rule.name);
	const getterName = `${base}_getter_generated`;
	const setterName = `${base}_setter_generated`;
	const getterExpr = buildPropertyGetterExpr(rule);
	const setterStmt = buildPropertySetterStatement(rule);

	const code = [
		`inline void ${getterName}(pesapi_ffi *apis, pesapi_callback_info info) {`,
		"\tpuerts::internal::callback_context context(apis, info);",
		"\tif (!context.require()) {",
		"\t\treturn;",
		"\t}",
		"",
		`\tpuerts::internal::receiver<godot::${rule.class}> instance = puerts::internal::resolve_receiver<godot::${rule.class}>(apis, info, context);`,
		"\tif (!instance.is_valid()) {",
		"\t\treturn;",
		"\t}",
		"",
		`\tpuerts::internal::write_return<${typeName}>(apis, info, context.env, context.environment, ${getterExpr});`,
		"}",
		"",
		`inline void ${setterName}(pesapi_ffi *apis, pesapi_callback_info info) {`,
		"\tpuerts::internal::callback_context context(apis, info);",
		`\tif (!context.require() || !puerts::internal::check_arity<${typeName}>(context)) {`,
		"\t\treturn;",
		"\t}",
		"",
		`\tpuerts::internal::receiver<godot::${rule.class}> instance = puerts::internal::resolve_receiver<godot::${rule.class}>(apis, info, context);`,
		"\tif (!instance.is_valid()) {",
		"\t\treturn;",
		"\t}",
		"",
		`\tif (!puerts::internal::convert_arg_with<false, ${typeName}>(apis, info, context, 0, [&](auto &&value) {`,
		`\t\t\t${setterStmt}`,
		"\t\t})) {",
		"\t\treturn;",
		"\t}",
		"",
		"\tinstance.write_back();",
		"}",
	].join("\n");

	return { getterName, setterName, code };
}

export function collectPropertyBindings(
	cls: ApiClassLike,
	headerAnalysis: HeaderAnalysis,
	customRulesByClass: Map<string, PropertyRule[]>,
	missingPropertyNotes: string[],
	helperNamespace: string,
): BoundPropertyBinding[] {
	const bindings: BoundPropertyBinding[] = [];
	const customRules = customRulesByClass.get(cls.name) ?? [];
	const apiMemberNames = new Set((cls.members ?? []).map((m) => m.name));
	const usedCustomNames = new Set<string>();
	const hasHeaderSource = headerAnalysis.source.length > 0;
	const customMethodCountCache = new Map<string, number>();
	const memberDeclCache = new Map<string, boolean>();
	const resolveMethodCount = (methodName: string): number => {
		const known = headerAnalysis.methodNameCounts.get(methodName);
		if (known !== undefined) {
			return known;
		}
		const cached = customMethodCountCache.get(methodName);
		if (cached !== undefined) {
			return cached;
		}
		const count = hasHeaderSource ? countMethodOverloadsInHeader(headerAnalysis.source, methodName) : 0;
		customMethodCountCache.set(methodName, count);
		return count;
	};
	const hasMember = (memberName: string): boolean => {
		if (headerAnalysis.topLevelMembers.has(memberName)) {
			return true;
		}
		const cached = memberDeclCache.get(memberName);
		if (cached !== undefined) {
			return cached;
		}
		const exists = hasHeaderSource ? hasLikelyMemberDeclaration(headerAnalysis.source, memberName) : true;
		memberDeclCache.set(memberName, exists);
		return exists;
	};

	for (const rule of customRules) {
		if (!apiMemberNames.has(rule.name)) {
			missingPropertyNotes.push(`${rule.class}.${rule.name} (missing in extension_api members)`);
			continue;
		}

		if ((rule.kind === "method" || rule.kind === "indexed_method") && hasHeaderSource) {
			const getterCount = resolveMethodCount(rule.getter!);
			const setterCount = resolveMethodCount(rule.setter!);
			if (getterCount === 0 || setterCount === 0) {
				missingPropertyNotes.push(
					`${rule.class}.${rule.name} (missing method: ${getterCount === 0 ? rule.getter : ""}${getterCount === 0 && setterCount === 0 ? "/" : ""}${setterCount === 0 ? rule.setter : ""})`,
				);
				continue;
			}
		}

		if (rule.kind === "member_expr" && hasHeaderSource) {
			const root = rule.member_expr!.split(/[.\[]/)[0];
			if (!hasMember(root)) {
				missingPropertyNotes.push(`${rule.class}.${rule.name} (missing member root: ${root})`);
				continue;
			}
		}

		const helper = generatePropertyHelperCode(rule);
		bindings.push({
			name: rule.name,
			bindingLine: `\t\t\t.property("${rule.name}", &${helperNamespace}::${helper.getterName}, &${helperNamespace}::${helper.setterName})`,
			helperCode: helper.code,
		});
		usedCustomNames.add(rule.name);
	}

	for (const member of cls.members ?? []) {
		if (usedCustomNames.has(member.name)) {
			continue;
		}
		if (!hasMember(member.name)) {
			continue;
		}
		bindings.push({
			name: member.name,
			bindingLine: `\t\t\t.property("${member.name}", puerts::make_property<&godot::${cls.name}::${member.name}>())`,
		});
	}

	return bindings;
}
