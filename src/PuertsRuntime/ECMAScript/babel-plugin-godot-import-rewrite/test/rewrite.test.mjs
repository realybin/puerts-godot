// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

import assert from "node:assert/strict";
import test from "node:test";
import vm from "node:vm";
import { transformSync } from "@babel/core";
import stripTypes from "@babel/plugin-transform-typescript";
import rewrite from "../index.mjs";

function transform(code, typescript = false) {
  return transformSync(code, {
    configFile: false, babelrc: false, filename: "example.ts",
    plugins: typescript ? [rewrite, stripTypes] : [rewrite],
  }).code;
}

test("addon namespace, named imports and global aliases execute without module objects", () => {
  const code = transform(`
    import * as Addon from "PAddon";
    import { WebSocketPP as Client } from "PAddon";
    import { "Net.Client" as Namespaced } from "PAddon";
    import { to_callable as wrap } from "puerts";
    import * as puerts from "puerts";
    result = [Addon.WebSocketPP, Addon["Net.Client"], Client, Namespaced, wrap(42), puerts.log_info];
  `);
  assert.doesNotMatch(code, /import |Proxy/);
  const context = {
    load_addon_type: (name) => name,
    to_callable: (value) => value,
    log_info() {},
  };
  vm.runInNewContext(code, context);
  assert.deepEqual(Array.from(context.result), ["WebSocketPP", "Net.Client", "WebSocketPP", "Net.Client", 42, context.log_info]);
});

test("binding identity preserves shadowing and object shorthand keys", () => {
  const code = transform(`
    import { to_callable as wrap } from "puerts";
    import * as Addon from "PAddon";
    function local(Addon, wrap) { return [Addon.Client, wrap]; }
    result = [{wrap}, local({Client: 7}, 8), Addon.Client];
  `);
  const context = { to_callable: () => {}, load_addon_type: (name) => name };
  vm.runInNewContext(code, context);
  assert.equal(context.result[0].wrap, context.to_callable);
  assert.deepEqual(Array.from(context.result[1]), [7, 8]);
  assert.equal(context.result[2], "Client");
});

test("puerts imports can appear after addon imports", () => {
  const code = transform('import * as A from "PAddon"; import {load_addon_type} from "puerts"; result = A.Client;');
  assert.match(code, /load_addon_type\("Client"\)/);
});

test("type imports disappear and TypeScript type references remain valid until stripping", () => {
  const code = transform(`
    import type { Client } from "PAddon";
    import { type Other, WebSocketPP } from "PAddon";
    import * as Addon from "PAddon";
    let item: Addon.WebSocketPP;
    result = WebSocketPP;
  `, true);
  assert.doesNotMatch(code, /import |Addon\./);
  assert.match(code, /load_addon_type\("WebSocketPP"\)/);
});

test("godot imports keep existing behavior and unrelated imports remain", () => {
  const code = transform('import Godot, {Node as N} from "godot"; import x from "other"; result = [Godot.Node, N, x];');
  assert.match(code, /new Proxy/);
  assert.match(code, /load_type\("Node"\)/);
  assert.match(code, /from "other"/);
});

test("unsupported virtual module operations fail during transformation", () => {
  for (const code of [
    'import A from "PAddon";',
    'import {missing} from "puerts";',
    'import * as A from "PAddon"; consume(A);',
    'import * as A from "PAddon"; A[name];',
    'import * as A from "PAddon"; A.Client = 1;',
    'import * as A from "puerts"; A.missing();',
    'import {to_callable as wrap} from "puerts"; function f(to_callable) { wrap(f); }',
    'export * from "PAddon";',
    'import {Client} from "PAddon"; export {Client};',
  ]) assert.throws(() => transform(code), { name: "SyntaxError" });
});
