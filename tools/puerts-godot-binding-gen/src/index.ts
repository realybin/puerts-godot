// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

import { HELP, parseArgs } from "./cli.js";
import { generateBindings } from "./orchestrator.js";

function main(): void {
	const argv = process.argv.slice(2);
	if (argv.length === 1 && argv[0] === "--help") {
		console.log(HELP);
		return;
	}
	const args = parseArgs(argv);
	generateBindings(args);
}

try {
	main();
} catch (error) {
	console.error(error instanceof Error ? error.message : String(error));
	process.exitCode = 1;
}
