// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

import assert from "node:assert/strict";
import test from "node:test";
import ts from "typescript";
import { generate } from "../dist/generator.js";

test("runtime functions move to puerts and addon declarations merge with string exports", () => {
  const declaration = generate({
    classes: [{name: "Object"}], builtin_classes: [{name: "Callable"}],
    global_constants: [], global_enums: [], utility_functions: [], singletons: [],
  });
  const godotModule = declaration.slice(declaration.indexOf('declare module "godot"'), declaration.indexOf('declare module "puerts"'));
  assert.doesNotMatch(godotModule, /function load_type|export const to_callable/);
  assert.match(declaration, /declare function load_addon_type/);
  const files = new Map([
    ["/godot.d.ts", declaration],
    ["/addon.d.ts", `declare module "PAddon" {
      class Counter { increment(): number; }
      export { Counter as "Example.Counter" };
      export class PlainCounter { increment(): number; }
    }`],
    ["/other-addon.d.ts", 'declare module "PAddon" { export class Other {} }'],
    ["/main.ts", `
      import { to_callable, load_type, load_addon_type } from "puerts";
      import * as Addon from "PAddon";
      import { "Example.Counter" as Counter } from "PAddon";
      const a: number = new Addon.PlainCounter().increment();
      const b: number = new Counter().increment();
      const c = new Addon.Other();
      type Instance = InstanceType<typeof Addon["Example.Counter"]>;
      to_callable(() => b);
      load_type("Object"); load_addon_type("Example.Counter");
      // @ts-expect-error runtime helpers no longer belong to godot
      import { to_callable as oldHelper } from "godot";
      // @ts-expect-error addon declarations retain real method signatures
      const bad: string = new Counter().increment();
    `],
  ]);
  const options = {
    target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ESNext,
    moduleResolution: ts.ModuleResolutionKind.Bundler, strict: true, noEmit: true,
  };
  const host = ts.createCompilerHost(options);
  const originalRead = host.readFile.bind(host);
  const originalExists = host.fileExists.bind(host);
  host.readFile = (path) => files.get(path.replaceAll("\\", "/")) ?? originalRead(path);
  host.fileExists = (path) => files.has(path.replaceAll("\\", "/")) || originalExists(path);
  const program = ts.createProgram([...files.keys()], options, host);
  const diagnostics = ts.getPreEmitDiagnostics(program);
  assert.deepEqual(diagnostics.map((item) => ts.flattenDiagnosticMessageText(item.messageText, "\n")), []);
});
