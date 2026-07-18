// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import type { DocumentationLinks } from "./documentation-links.js";
import { godotBbcodeToMarkdown } from "./godot-bbcode.js";

function emitJsDoc(
	sections: Array<string | undefined>,
	links: DocumentationLinks,
	docsBaseUrl: string,
	owner?: string,
): string[] {
	const uniqueSections = [...new Set(sections.map((section) => section?.trim()).filter((section): section is string => Boolean(section)))];
	if (uniqueSections.length === 0) {
		return [];
	}

	const markdown = godotBbcodeToMarkdown(uniqueSections.join("\n\n"), { links, docsBaseUrl, owner });
	return markdown ? ["/**", ...markdown.split("\n").map((line) => (line ? ` * ${line}` : " *")), " */"] : [];
}

export function godotClassMetadata(isRefcounted: boolean | undefined, isInstantiable: boolean | undefined): string | undefined {
	const tags: string[] = [];
	if (isRefcounted !== undefined) {
		tags.push(`@refCounted ${isRefcounted}`);
	}
	if (isInstantiable !== undefined) {
		tags.push(`@instantiable ${isInstantiable}`);
	}
	return tags.length > 0 ? tags.join("\n") : undefined;
}

export type DocumentationRendererOptions = {
	readonly links: DocumentationLinks;
	readonly docsBaseUrl: string;
	readonly enabled: boolean;
};

export class DocumentationRenderer {
	readonly #options: DocumentationRendererOptions;

	constructor(options: DocumentationRendererOptions) {
		this.#options = options;
	}

	forOwner(owner?: string): DocumentationScope {
		return new DocumentationScope(this, owner);
	}

	emitDeclaration(
		declaration: string,
		documentation: Array<string | undefined> = [],
		indent = "",
		owner?: string,
	): string[] {
		const jsDoc = this.#options.enabled
			? emitJsDoc(documentation, this.#options.links, this.#options.docsBaseUrl, owner)
			: [];
		return [...jsDoc, declaration].map((line) => `${indent}${line}`);
	}
}

export class DocumentationScope {
	readonly #renderer: DocumentationRenderer;
	readonly #owner: string | undefined;

	constructor(renderer: DocumentationRenderer, owner?: string) {
		this.#renderer = renderer;
		this.#owner = owner;
	}

	emit(declaration: string, documentation: Array<string | undefined> = [], indent = ""): string[] {
		return this.#renderer.emitDeclaration(declaration, documentation, indent, this.#owner);
	}
}
