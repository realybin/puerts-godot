// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

export type CliArgs = {
	input: string;
	output: string;
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

	for (let i = 0; i < argv.length; i += 1) {
		switch (argv[i]) {
			case "--input":
				input = readOptionValue(argv, i, "--input");
				i += 1;
				break;
			case "--output":
				output = readOptionValue(argv, i, "--output");
				i += 1;
				break;
		}
	}

	if (!input || !output) {
		throw new Error("Usage: node dist/index.js --input <extension_api.json> --output <godot.d.ts>");
	}

	return { input, output };
}
