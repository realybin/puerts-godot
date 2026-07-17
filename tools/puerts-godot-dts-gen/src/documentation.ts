// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

function inlineCode(value: string): string {
	return `\`${value.trim().replaceAll("`", "\\`")}\``;
}

function resolveDocsUrl(value: string): string {
	return value.replace("$DOCS_URL/", "https://docs.godotengine.org/en/stable/");
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

function godotBbcodeToMarkdown(source: string): string {
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
		.replace(/\[url=([^\]]+)\]([\s\S]*?)\[\/url  ,ll\]/giu, (_match, url: string, label: string) =>
			protect(`[${label}](${resolveDocsUrl(url)})`),
		)
		.replace(/\[url\]([\s\S]*?)\[\/url\]/giu, (_match, url: string) => protect(`<${resolveDocsUrl(url)}>`))
		.replace(/\[img(?:=[^\]]+)?\]([\s\S]*?)\[\/img\]/giu, (_match, url: string) => protect(`![image](${url})`));

	text = text
		.replace(/\[codeblocks\]|\[\/codeblocks\]/giu, "")
		.replace(
			/\[(?:param|member|method|signal|constant|enum|annotation|theme_item|constructor|operator)\s+([^\]]+)\]/giu,
			(_match, value: string) => inlineCode(value),
		)
		.replace(/\[b\]/giu, "**")
		.replace(/\[\/b\]/giu, "**")
		.replace(/\[i\]/giu, "*")
		.replace(/\[\/i\]/giu, "*")
		.replace(/\[u\]/giu, "")
		.replace(/\[\/u\]/giu, "")
		.replace(/\[(?:ul|ol)\]/giu, "\n")
		.replace(/\[\/(?:ul|ol)\]/giu, "\n")
		.replace(/\[li\]/giu, "\n- ")
		.replace(/\[\/li\]/giu, "")
		.replace(/\[br\]/giu, "<br>")
		.replace(/\[lb\]/giu, "[")
		.replace(/\[rb\]/giu, "]")
		.replace(/\[(?:center|right|fill|indent)(?:=[^\]]+)?\]/giu, "")
		.replace(/\[\/(?:center|right|fill|indent)\]/giu, "")
		.replace(/\[color=[^\]]+\]/giu, "")
		.replace(/\[\/color\]/giu, "")
		.replace(/\[([A-Za-z_@][A-Za-z0-9_@.]*)\]/gu, (_match, value: string) => inlineCode(value));

	for (const [index, fragment] of protectedFragments.entries()) {
		text = text.replaceAll(`\u0000DOC${index}\u0000`, fragment);
	}

	text = normalizeMarkdownParagraphs(text.replace(/[ \t]+\n/g, "\n"))
		.replace(/\n{3,}/g, "\n\n")
		.replaceAll("*/", "*\\/")
		.trim();

	return text;
}

function emitJsDoc(...sections: Array<string | undefined>): string[] {
	const uniqueSections = [...new Set(sections.map((section) => section?.trim()).filter((section): section is string => Boolean(section)))];
	if (uniqueSections.length === 0) {
		return [];
	}

	const markdown = godotBbcodeToMarkdown(uniqueSections.join("\n\n"));
	if (!markdown) {
		return [];
	}

	return ["/**", ...markdown.split("\n").map((line) => (line ? ` * ${line}` : " *")), " */"];
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

export function emitDeclaration(declaration: string, documentation: Array<string | undefined> = [], indent = ""): string[] {
	return [...emitJsDoc(...documentation), declaration].map((line) => `${indent}${line}`);
}
