// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import { normalizeOperators } from "../../puerts-godot-operator-model/index.js";
import type { ApiData, ApiEnum } from "./api-types.js";
import { NON_CLASS_BUILTIN_NAMES } from "./constants.js";
import { sanitizeIdentifier } from "./naming.js";

export type DocumentationLinks = ReadonlyMap<string, string>;

class DocumentationLinkRegistry {
	readonly #ambiguous = new Set<string>();
	readonly #targets = new Map<string, string>();

	add(source: string, target: string): void {
		if (!source || this.#ambiguous.has(source)) {
			return;
		}
		const existing = this.#targets.get(source);
		if (existing !== undefined && existing !== target) {
			this.#targets.delete(source);
			this.#ambiguous.add(source);
			return;
		}
		this.#targets.set(source, target);
	}

	build(): DocumentationLinks {
		return this.#targets;
	}
}

function enumShortName(enumDef: ApiEnum): string {
	return enumDef.name.split(".").at(-1)!;
}

function addOwnedEnums(registry: DocumentationLinkRegistry, owner: string, enums: ApiEnum[] | undefined): Set<string> {
	const enumConstants = new Set<string>();
	for (const enumDef of enums ?? []) {
		const shortName = enumShortName(enumDef);
		const enumTarget = `${sanitizeIdentifier(owner)}.${sanitizeIdentifier(shortName)}`;
		registry.add(`${owner}.${shortName}`, enumTarget);
		registry.add(enumDef.name, enumTarget);
		for (const value of enumDef.values) {
			enumConstants.add(value.name);
			registry.add(`${owner}.${value.name}`, `${enumTarget}.${sanitizeIdentifier(value.name)}`);
		}
	}
	return enumConstants;
}

function globalEnumTarget(name: string): string {
	const parts = name.split(".");
	if (parts.length === 1) {
		return `GlobalScope.${sanitizeIdentifier(parts[0])}`;
	}
	return `GlobalScope.${sanitizeIdentifier(parts[0])}.${sanitizeIdentifier(parts.slice(1).join("_"))}`;
}

export function buildDocumentationLinks(api: ApiData): DocumentationLinks {
	const registry = new DocumentationLinkRegistry();
	registry.add("@GlobalScope", "GlobalScope");
	registry.add("Variant", "Variant");
	registry.add("bool", "Bool");
	registry.add("int", "Int");
	registry.add("float", "Float");
	registry.add("String", "String");

	for (const enumDef of api.global_enums) {
		const enumTarget = globalEnumTarget(enumDef.name);
		registry.add(enumDef.name, enumTarget);
		registry.add(`@GlobalScope.${enumShortName(enumDef)}`, enumTarget);
		for (const value of enumDef.values) {
			const target = `${enumTarget}.${sanitizeIdentifier(value.name)}`;
			registry.add(value.name, target);
			registry.add(`@GlobalScope.${value.name}`, target);
		}
	}

	for (const method of api.utility_functions) {
		registry.add(`@GlobalScope.${method.name}`, `GlobalScope.${sanitizeIdentifier(method.name)}`);
	}
	for (const singleton of api.singletons) {
		registry.add(`@GlobalScope.${singleton.name}`, `GlobalScope.${sanitizeIdentifier(singleton.name)}`);
	}

	for (const builtin of api.builtin_classes) {
		if (NON_CLASS_BUILTIN_NAMES.has(builtin.name)) {
			continue;
		}
		const ownerTarget = sanitizeIdentifier(builtin.name);
		registry.add(builtin.name, ownerTarget);
		registry.add(`${builtin.name}.${builtin.name}`, ownerTarget);
		const enumConstants = addOwnedEnums(registry, builtin.name, builtin.enums);
		for (const member of builtin.members ?? []) {
			registry.add(`${builtin.name}.${member.name}`, `${ownerTarget}.${sanitizeIdentifier(member.name)}`);
		}
		for (const method of builtin.methods ?? []) {
			registry.add(`${builtin.name}.${method.name}`, `${ownerTarget}.${sanitizeIdentifier(method.name)}`);
		}
		for (const constant of builtin.constants ?? []) {
			if (!enumConstants.has(constant.name)) {
				registry.add(`${builtin.name}.${constant.name}`, `${ownerTarget}.${sanitizeIdentifier(constant.name)}`);
			}
		}
		for (const operator of normalizeOperators(builtin.name, builtin.operators)) {
			registry.add(`${builtin.name}.operator ${operator.name}`, `${ownerTarget}.${operator.scriptName}`);
		}
	}

	for (const classDef of api.classes) {
		const ownerTarget = sanitizeIdentifier(classDef.name);
		registry.add(classDef.name, ownerTarget);
		registry.add(`${classDef.name}.${classDef.name}`, ownerTarget);
		const enumConstants = addOwnedEnums(registry, classDef.name, classDef.enums);
		for (const property of classDef.properties ?? []) {
			registry.add(`${classDef.name}.${property.name}`, `${ownerTarget}.${sanitizeIdentifier(property.name)}`);
		}
		for (const signal of classDef.signals ?? []) {
			registry.add(`${classDef.name}.${signal.name}`, `${ownerTarget}.${sanitizeIdentifier(signal.name)}`);
		}
		for (const method of classDef.methods ?? []) {
			registry.add(`${classDef.name}.${method.name}`, `${ownerTarget}.${sanitizeIdentifier(method.name)}`);
		}
		for (const constant of classDef.constants ?? []) {
			if (!enumConstants.has(constant.name)) {
				registry.add(`${classDef.name}.${constant.name}`, `${ownerTarget}.${sanitizeIdentifier(constant.name)}`);
			}
		}
	}

	return registry.build();
}

export function resolveDocumentationLink(
	links: DocumentationLinks,
	value: string,
	owner: string | undefined,
): string | undefined {
	const source = value.trim();
	if (owner && !source.includes(".")) {
		const owned = links.get(`${owner}.${source}`);
		if (owned) {
			return owned;
		}
	}
	return links.get(source);
}
