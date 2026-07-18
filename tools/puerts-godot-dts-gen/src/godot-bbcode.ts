// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import type { DocumentationLinks } from "./documentation-links.js";
import { resolveDocumentationLink } from "./documentation-links.js";
import { sanitizeIdentifier } from "./naming.js";

export type BbcodeConversionOptions = {
	readonly links: DocumentationLinks;
	readonly docsBaseUrl: string;
	readonly owner?: string;
};

function inlineCode(value: string): string {
	return `\`${value.trim().replaceAll("`", "\\`")}\``;
}

function resolveDocsUrl(value: string, docsBaseUrl: string): string {
	return value.startsWith("$DOCS_URL/") ? `${docsBaseUrl}${value.slice("$DOCS_URL/".length)}` : value;
}

function jsDocUrlLink(urlValue: string, docsBaseUrl: string, label?: string): string | undefined {
	const url = resolveDocsUrl(urlValue.trim(), docsBaseUrl);
	if (!/^https?:\/\/[^\s{}]+$/iu.test(url)) {
		return undefined;
	}
	if (label === undefined) {
		return `{@link ${url}}`;
	}
	const text = label.trim();
	return text && !/[\r\n}]/u.test(text) ? `{@link ${url} ${text}}` : undefined;
}

function jsDocSymbolLink(target: string, label: string): string | undefined {
	const text = label.trim();
	if (!/^[$A-Za-z_][$\w]*(?:\.[$A-Za-z_][$\w]*)*$/u.test(target) || !text || /[\r\n}]/u.test(text)) {
		return undefined;
	}
	return target === text ? `{@link ${target}}` : `{@link ${target} ${text}}`;
}

function normalizeMarkdownParagraphs(source: string): string {
	const output: string[] = [];
	let inCodeFence = false;
	let previousKind: "block" | "list" | "tag" | "text" | undefined;

	for (const line of source.split("\n")) {
		const trimmed = line.trim();
		if (trimmed.startsWith("```")) {
			if (!inCodeFence && output.length > 0 && output.at(-1) !== "") {
				output.push("");
			}
			output.push(line);
			inCodeFence = !inCodeFence;
			previousKind = inCodeFence ? undefined : "block";
			continue;
		}
		if (inCodeFence) {
			output.push(line);
			continue;
		}
		if (!trimmed) {
			if (output.length > 0 && output.at(-1) !== "") {
				output.push("");
			}
			previousKind = undefined;
			continue;
		}

		const kind = /^@[A-Za-z]/.test(trimmed) ? "tag" : /^(?:[-+*]\s+|\d+\.\s+)/.test(trimmed) ? "list" : "text";
		const continuesBlock = (kind === "list" && previousKind === "list") || (kind === "tag" && previousKind === "tag");
		if (output.length > 0 && output.at(-1) !== "" && !continuesBlock) {
			output.push("");
		}
		output.push(line);
		previousKind = kind;
	}

	return output.join("\n");
}

export function godotBbcodeToMarkdown(source: string, options: BbcodeConversionOptions): string {
	let text = source.replace(/\r\n?/g, "\n").trim();
	if (!text) {
		return "";
	}

	const protectedFragments: string[] = [];
	const protect = (fragment: string): string => {
		const index = protectedFragments.push(fragment) - 1;
		return `\u0000DOC${index}\u0000`;
	};

	text = text
		.replace(/\[codeblock(?:\s+[^\]]*)?\]([\s\S]*?)\[\/codeblock\]/giu, (_match, value: string) =>
			protect(`\n\`\`\`gdscript\n${value.trim()}\n\`\`\`\n`),
		)
		.replace(/\[gdscript(?:\s+[^\]]*)?\]([\s\S]*?)\[\/gdscript\]/giu, (_match, value: string) =>
			protect(`\n\`\`\`gdscript\n${value.trim()}\n\`\`\`\n`),
		)
		.replace(/\[csharp(?:\s+[^\]]*)?\]([\s\S]*?)\[\/csharp\]/giu, (_match, value: string) =>
			protect(`\n\`\`\`csharp\n${value.trim()}\n\`\`\`\n`),
		)
		.replace(/\[code(?:\s+[^\]]*)?\]([\s\S]*?)\[\/code\]/giu, (_match, value: string) => protect(inlineCode(value)))
		.replace(/\[kbd\]([\s\S]*?)\[\/kbd\]/giu, (_match, value: string) => protect(inlineCode(value)))
		.replace(/\[url=([^\]]+)\]([\s\S]*?)\[\/url\]/giu, (match, url: string, label: string) => {
			const link = jsDocUrlLink(url, options.docsBaseUrl, label);
			return link ? protect(link) : match;
		})
		.replace(/\[url\]([\s\S]*?)\[\/url\]/giu, (match, url: string) => {
			const link = jsDocUrlLink(url, options.docsBaseUrl);
			return link ? protect(link) : match;
		})
		.replace(/\[img(?:=[^\]]+)?\]([\s\S]*?)\[\/img\]/giu, (_match, url: string) => protect(`![image](${url})`));

	text = text
		.replace(/\[codeblocks\]|\[\/codeblocks\]/giu, "")
		.replace(
			/\[(param|member|method|signal|constant|enum|annotation|theme_item|constructor|operator)\s+([^\]]+)\]/giu,
			(_match, tag: string, value: string) => {
				if (tag.toLowerCase() === "param") {
					if (!/^[A-Za-z_][A-Za-z0-9_]*$/u.test(value.trim())) {
						return inlineCode(value);
					}
					const link = jsDocSymbolLink(sanitizeIdentifier(value), value);
					return link ? protect(link) : inlineCode(value);
				}

				const target = resolveDocumentationLink(options.links, value, options.owner);
				if (target) {
					const link = jsDocSymbolLink(target, value);
					return link ? protect(link) : inlineCode(value);
				}

				const isLocalMember =
					/^[A-Za-z_][A-Za-z0-9_]*$/u.test(value.trim()) && /^(?:member|method|signal)$/iu.test(tag);
				const link = isLocalMember && jsDocSymbolLink(sanitizeIdentifier(value), value);
				return link ? protect(link) : inlineCode(value);
			},
		)
		.replace(/\[b\]/giu, "**")
		.replace(/\[\/b\]/giu, "**")
		.replace(/\[i\]/giu, "*")
		.replace(/\[\/i\]/giu, "*")
		.replace(/\[u\]|\[\/u\]/giu, "")
		.replace(/\[(?:ul|ol)\]|\[\/(?:ul|ol)\]/giu, "\n")
		.replace(/\[li\]/giu, "\n- ")
		.replace(/\[\/li\]/giu, "")
		.replace(/\[br\]/giu, "<br>")
		.replace(/\[lb\]/giu, "[")
		.replace(/\[rb\]/giu, "]")
		.replace(/\[(?:center|right|fill|indent)(?:=[^\]]+)?\]|\[\/(?:center|right|fill|indent)\]/giu, "")
		.replace(/\[color=[^\]]+\]|\[\/color\]/giu, "")
		.replace(/\[([A-Za-z_@][A-Za-z0-9_@.]*)\]/gu, (_match, value: string) => {
			const target = resolveDocumentationLink(options.links, value, options.owner);
			const link = target && jsDocSymbolLink(target, value);
			return link ? protect(link) : inlineCode(value);
		});

	for (const [index, fragment] of protectedFragments.entries()) {
		text = text.replaceAll(`\u0000DOC${index}\u0000`, fragment);
	}

	return normalizeMarkdownParagraphs(text.replace(/[ \t]+\n/g, "\n"))
		.replace(/\n{3,}/g, "\n\n")
		.replaceAll("*/", "*\\/")
		.trim();
}
