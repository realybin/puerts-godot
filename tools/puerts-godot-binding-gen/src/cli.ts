// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";
import type { BindingProfile, CliArgs } from "./types.js";

export const HELP = `Usage:
		node dist/index.js --api <api.json> --profile <profile.json> --output <file.inc>
		node dist/index.js --api <api.json> --kind builtin|object --classes A,B --output <file.inc>
		node dist/index.js --api <api.json> --kind builtin|object --all --output <file.inc>
		node dist/index.js --api <api.json> --kind globalscope --output <file.inc>

Options:
		--rules <path>  Override the bundled puerts_binding_rules.json
		--help          Show this help

Profiles and direct selection are mutually exclusive. Paths are relative to the current directory.`;

const IDENTIFIER = /^[_A-Za-z][_0-9A-Za-z]*$/;
const KINDS = ["builtin", "object", "globalscope"];
const DEFAULT_RULES = fileURLToPath(new URL("../config/puerts_binding_rules.json", import.meta.url));

function validateSelection(profile: BindingProfile): void {
	if (!KINDS.includes(profile.kind)) throw new Error("kind must be builtin, object, or globalscope.");
	if (profile.all !== undefined && typeof profile.all !== "boolean") throw new Error("all must be a boolean.");
	if (profile.classes !== undefined && (!Array.isArray(profile.classes) || profile.classes.length === 0 ||
			profile.classes.some(name => typeof name !== "string" || !IDENTIFIER.test(name)))) {
		throw new Error("classes must be a nonempty array of class names.");
	}
	if (profile.classes && new Set(profile.classes).size !== profile.classes.length) throw new Error("Duplicate class names.");
	if (profile.kind === "globalscope") {
		if (profile.classes !== undefined || profile.all !== undefined) throw new Error("globalscope does not accept classes or all.");
	} else if (Number(profile.classes !== undefined) + Number(profile.all === true) !== 1) {
		throw new Error("Select either classes or all.");
	}
	if (profile.register_function !== undefined &&
			(typeof profile.register_function !== "string" || !IDENTIFIER.test(profile.register_function))) {
		throw new Error("register_function must be a C++ identifier.");
	}
}

export function parseArgs(argv: string[]): CliArgs {
	const values = new Map<string, string>();
	const options = new Set(["--api", "--output", "--profile", "--kind", "--classes", "--rules", "--all"]);
	for (let index = 0; index < argv.length; index++) {
		const option = argv[index];
		if (!options.has(option)) throw new Error(`Unknown argument: ${option}. Use --help.`);
		if (values.has(option)) throw new Error(`Duplicate argument: ${option}.`);
		if (option === "--all") { values.set(option, "true"); continue; }
		const value = argv[++index];
		if (!value || value.startsWith("--") || !value.trim()) throw new Error(`Missing value for ${option}.`);
		values.set(option, value);
	}
	const input = values.get("--api");
	const output = values.get("--output");
	if (!input || !output) throw new Error("--api and --output are required. Use --help.");
	let profile: BindingProfile;
	if (values.has("--profile")) {
		if (["--kind", "--classes", "--all"].some(option => values.has(option))) {
			throw new Error("--profile cannot be combined with --kind, --classes, or --all.");
		}
		const parsed: unknown = JSON.parse(readFileSync(resolve(values.get("--profile")!), "utf8"));
		if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) throw new Error("Profile must be an object.");
		for (const key of Object.keys(parsed)) {
			if (!["kind", "classes", "all", "register_function"].includes(key)) throw new Error(`Unknown profile field: ${key}.`);
		}
		profile = parsed as BindingProfile;
	} else {
		profile = {
			kind: values.get("--kind") as BindingProfile["kind"],
			...(values.has("--classes") ? { classes: values.get("--classes")!.split(",").map(name => name.trim()) } : {}),
			...(values.has("--all") ? { all: true } : {}),
		};
	}
	validateSelection(profile);
	const registrationNames = {
		builtin: "register_puerts_builtin_bindings_generated",
		object: "register_puerts_object_profile_bindings_generated",
		globalscope: "register_puerts_global_scope_generated",
	};
	return {
		input, output,
		classNames: profile.classes ?? [],
		bindingRulesJson: values.get("--rules") ?? DEFAULT_RULES,
		classSource: profile.kind === "object" ? "classes" : "builtin",
		target: profile.kind === "globalscope" ? "globalscope" : "classes",
		registerFunction: profile.register_function ?? registrationNames[profile.kind],
	};
}
