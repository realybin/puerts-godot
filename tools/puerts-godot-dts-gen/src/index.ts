// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: Apache-2.0

import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import type { ApiData } from "./api-types.js";
import { parseArgs } from "./cli.js";
import { generate } from "./generator.js";

function main() {
	const { input, output, options } = parseArgs(process.argv.slice(2));
	const inputPath = resolve(process.cwd(), input);
	const outputPath = resolve(process.cwd(), output);
	const data = JSON.parse(readFileSync(inputPath, "utf8")) as ApiData;
	const dts = generate(data, options);
	mkdirSync(dirname(outputPath), { recursive: true });
	writeFileSync(outputPath, dts, "utf8");
}

main();
