import assert from "node:assert/strict";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, isAbsolute } from "node:path";
import test from "node:test";
import { parseArgs } from "../dist/cli.js";

const required = ["--api", "api.json", "--output", "output.inc"];

test("direct selection is explicit and default rules do not depend on cwd", () => {
	const args = parseArgs([...required, "--kind", "object", "--classes", "Object,Node"]);
	assert.equal(args.classSource, "classes");
	assert.deepEqual(args.classNames, ["Object", "Node"]);
	assert.ok(isAbsolute(args.bindingRulesJson));
	assert.equal(parseArgs([...required, "--kind", "builtin", "--all"]).classNames.length, 0);
	assert.equal(parseArgs([...required, "--kind", "globalscope"]).target, "globalscope");
});

test("unknown, repeated, missing, and conflicting options fail", () => {
	for (const options of [
		["--unknown"], ["--api", "other.json"], ["--rules"],
		["--kind", "builtin"], ["--kind", "globalscope", "--all"],
		["--kind", "builtin", "--classes", "Color", "--all"],
		["--profile", "unused.json", "--kind", "builtin"],
		["--kind", "builtin", "--classes", "Color,Color"],
	]) assert.throws(() => parseArgs([...required, ...options]));
});

test("profiles define selection and registration, and reject obsolete fields", () => {
	const directory = mkdtempSync(join(tmpdir(), "puerts-binding-cli-"));
	const path = join(directory, "profile.json");
	try {
		writeFileSync(path, JSON.stringify({ kind: "object", classes: ["Object"], register_function: "register_custom" }));
		assert.equal(parseArgs([...required, "--profile", path]).registerFunction, "register_custom");
		writeFileSync(path, JSON.stringify({ kind: "builtin", all: true, source: "builtin" }));
		assert.throws(() => parseArgs([...required, "--profile", path]), /Unknown profile field/);
	} finally {
		assert.ok(directory.startsWith(join(tmpdir(), "puerts-binding-cli-")));
		rmSync(directory, { recursive: true });
	}
});
