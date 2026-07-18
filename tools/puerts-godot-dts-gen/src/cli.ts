// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import type { GeneratorOptions } from "./options.js";

export type CliArgs = {
	input: string;
	output: string;
	options: GeneratorOptions;
};

function readOptionValue(argv: string[], index: number, flag: string): string {
	const value = (argv[index + 1] ?? "").trim();
	if (!value) {
		throw new Error(`Missing value for ${flag}.`);
	}
	return value;
}

export function parseArgs(argv: string[]): CliArgs {
	let input = "";
	let output = "";
	const options: GeneratorOptions = {};

	for (let i = 0; i < argv.length; i += 1) {
		const flag = argv[i];
		switch (flag) {
			case "--input":
				input = readOptionValue(argv, i, flag);
				i += 1;
				break;
			case "--output":
				output = readOptionValue(argv, i, flag);
				i += 1;
				break;
			case "--module-name":
				options.moduleName = readOptionValue(argv, i, flag);
				i += 1;
				break;
			case "--docs-base-url":
				options.docsBaseUrl = readOptionValue(argv, i, flag);
				i += 1;
				break;
			case "--unknown-type":
				options.unknownType = readOptionValue(argv, i, flag);
				i += 1;
				break;
			case "--no-docs":
				options.emitApiDocumentation = false;
				break;
			default:
				throw new Error(`Unknown option: ${flag}`);
		}
	}

	if (!input || !output) {
		throw new Error(
			"Usage: node dist/index.js --input <extension_api.json> --output <godot.d.ts> " +
				"[--module-name <name>] [--docs-base-url <url>] [--unknown-type <type>] [--no-docs]",
		);
	}

	return { input, output, options };
}
