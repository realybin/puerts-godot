// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

import {
	sanitizeIdentifier,
	toSnakeCase
} from "./text_utils.js";
import {mapPropertyValueType} from "./type_mapping.js";
import type {ApiClassLike, BoundPropertyBinding, PropertyRule} from "./types.js";

function makePropertyHelperBaseName(className: string, propertyName: string): string {
	return `${sanitizeIdentifier(toSnakeCase(className))}_${sanitizeIdentifier(propertyName)}`;
}

function buildPropertyGetterExpression(rule: PropertyRule): string {
	if (rule.kind === "method") {
		return `p_self.${rule.getter!}()`;
	}
	if (rule.kind === "indexed_method") {
		return `p_self.${rule.getter!}(${rule.index!})`;
	}
	return `p_self.${rule.member_expr!}`;
}

function buildPropertySetterStatement(rule: PropertyRule): string {
	const scalar = rule.type === "int" || rule.type === "float";
	if (rule.kind === "method" || rule.kind === "indexed_method") {
		const index = rule.kind === "indexed_method" ? 1 : 0;
		const value = scalar
			? `static_cast<puerts::internal::native_parameter_t<decltype(&godot::${rule.class}::${rule.setter!}), ${index}>>(p_value)`
			: "eastl::move(p_value)";
		const argumentsList = rule.kind === "indexed_method" ? `${rule.index!}, ${value}` : value;
		return `p_self.${rule.setter!}(${argumentsList});`;
	}
	const value = scalar ? `static_cast<decltype(p_self.${rule.member_expr!})>(p_value)` : "eastl::move(p_value)";
	return `p_self.${rule.member_expr!} = ${value};`;
}

function generatePropertyHelperCode(rule: PropertyRule): { getterName: string; setterName: string; code: string } {
	const typeName = mapPropertyValueType(rule.type);
	const base = makePropertyHelperBaseName(rule.class, rule.name);
	const getterName = `${base}_getter`;
	const setterName = `${base}_setter`;
	const getterExpression = buildPropertyGetterExpression(rule);
	const setterStatement = buildPropertySetterStatement(rule);

	const code = [
		`inline ${typeName} ${getterName}(const godot::${rule.class} &p_self) {`,
		`\treturn ${getterExpression};`,
		"}",
		"",
		`inline void ${setterName}(godot::${rule.class} &p_self, ${typeName} p_value) {`,
		`\t${setterStatement}`,
		"}",
	].join("\n");

	return {getterName, setterName, code};
}

export function collectPropertyBindings(
	cls: ApiClassLike,
	customRulesByClass: Map<string, PropertyRule[]>,
	helperNamespace: string,
	excludedProperties: ReadonlySet<string> = new Set(),
): BoundPropertyBinding[] {
	const bindings: BoundPropertyBinding[] = [];
	const customRules = customRulesByClass.get(cls.name) ?? [];
	const apiMemberNames = new Set((cls.members ?? []).map((m) => m.name));
	const usedCustomNames = new Set<string>();

	for (const rule of customRules) {
		if (excludedProperties.has(`${cls.name}.${rule.name}`)) {
			continue;
		}
		if (!apiMemberNames.has(rule.name)) {
			throw new Error(`Property rule does not match the selected API: ${rule.class}.${rule.name}.`);
		}

		if (usedCustomNames.has(rule.name)) {
			throw new Error(`Duplicate property adapter: ${rule.class}.${rule.name}.`);
		}
		if ((rule.kind === "method" || rule.kind === "indexed_method") && (!rule.getter || !rule.setter)) {
			throw new Error(`Property adapter requires a getter and setter: ${rule.class}.${rule.name}.`);
		}
		if (rule.kind === "indexed_method" && !Number.isInteger(rule.index)) {
			throw new Error(`Indexed property adapter requires an integer index: ${rule.class}.${rule.name}.`);
		}
		if (rule.kind === "member_expr" && !rule.member_expr) {
			throw new Error(`Property adapter requires a member expression: ${rule.class}.${rule.name}.`);
		}
		const helper = generatePropertyHelperCode(rule);
		bindings.push({
			name: rule.name,
			bindingLine: `\t\t\t.property<&${helperNamespace}::${helper.getterName}, &${helperNamespace}::${helper.setterName}>("${rule.name}")`,
			helperCode: helper.code,
		});
		usedCustomNames.add(rule.name);
	}

	for (const member of cls.members ?? []) {
		if (excludedProperties.has(`${cls.name}.${member.name}`)) {
			continue;
		}
		if (usedCustomNames.has(member.name)) {
			continue;
		}
		bindings.push({
			name: member.name,
			bindingLine: `\t\t\t.property<&godot::${cls.name}::${member.name}>("${member.name}")`,
		});
	}

	return bindings;
}
