// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import type { ApiData } from "./api-types.js";
import { NON_CLASS_BUILTIN_NAMES } from "./constants.js";
import { DocumentationRenderer } from "./documentation.js";
import { buildDocumentationLinks } from "./documentation-links.js";
import type { ResolvedGeneratorOptions } from "./options.js";

export type GenerationContext = {
	readonly knownClassNames: ReadonlySet<string>;
	readonly knownBuiltinNames: ReadonlySet<string>;
	readonly emittedBuiltinNames: ReadonlySet<string>;
	readonly globalEnumNames: ReadonlySet<string>;
	readonly documentation: DocumentationRenderer;
	readonly options: ResolvedGeneratorOptions;
	readonly precision?: string;
};

export function createContext(api: ApiData, options: ResolvedGeneratorOptions): GenerationContext {
	const documentationLinks = buildDocumentationLinks(api);
	return {
		knownClassNames: new Set(api.classes.map((classDef) => classDef.name)),
		knownBuiltinNames: new Set(api.builtin_classes.map((builtin) => builtin.name)),
		emittedBuiltinNames: new Set(
			api.builtin_classes.map((builtin) => builtin.name).filter((name) => !NON_CLASS_BUILTIN_NAMES.has(name)),
		),
		globalEnumNames: new Set(api.global_enums.map((enumDef) => enumDef.name)),
		documentation: new DocumentationRenderer({
			links: documentationLinks,
			docsBaseUrl: options.docsBaseUrl,
			enabled: options.emitApiDocumentation,
		}),
		options,
		precision: api.header?.precision,
	};
}
