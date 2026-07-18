// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

export type GeneratorOptions = {
	moduleName?: string;
	docsBaseUrl?: string;
	unknownType?: string;
	emitApiDocumentation?: boolean;
};

export type ResolvedGeneratorOptions = Readonly<Required<GeneratorOptions>>;

export const DEFAULT_GENERATOR_OPTIONS: Readonly<ResolvedGeneratorOptions> = Object.freeze({
	moduleName: "godot",
	docsBaseUrl: "https://docs.godotengine.org/en/stable/",
	unknownType: "any",
	emitApiDocumentation: true,
});

function requireValue(value: string, option: keyof GeneratorOptions): string {
	const normalized = value.trim();
	if (!normalized) {
		throw new Error(`Generator option '${option}' cannot be empty.`);
	}
	return normalized;
}

export function resolveGeneratorOptions(options: GeneratorOptions = {}): ResolvedGeneratorOptions {
	const moduleName = requireValue(options.moduleName ?? DEFAULT_GENERATOR_OPTIONS.moduleName, "moduleName");
	const docsBaseUrl = requireValue(options.docsBaseUrl ?? DEFAULT_GENERATOR_OPTIONS.docsBaseUrl, "docsBaseUrl");
	const unknownType = requireValue(options.unknownType ?? DEFAULT_GENERATOR_OPTIONS.unknownType, "unknownType");

	return {
		moduleName,
		docsBaseUrl: docsBaseUrl.endsWith("/") ? docsBaseUrl : `${docsBaseUrl}/`,
		unknownType,
		emitApiDocumentation: options.emitApiDocumentation ?? DEFAULT_GENERATOR_OPTIONS.emitApiDocumentation,
	};
}
