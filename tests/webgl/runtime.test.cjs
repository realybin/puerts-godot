// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

// Exercise the browser bridge independently of Godot/Emscripten.
// Run with: node --expose-gc --test tests/webgl/runtime.test.cjs
const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");
const test = require("node:test");
const path = require("node:path");

const source = fs.readFileSync(path.join(__dirname, "../../src/PuertsWebgl/puerts_webgl_runtime.js"), "utf8");

function fixture(options = {}) {
	let heap = new Uint8Array(64 * 1024);
	let cursor = 1024;
	const functions = new Map(), descriptions = new Map(), events = [];
	const context = vm.createContext({ console, TextEncoder, TextDecoder, WeakRef, FinalizationRegistry, ...options });
	const readUTF8 = (ptr, length) => {
		if (!ptr) return "";
		if (length === undefined) {
			length = 0;
			while (heap[ptr + length] && ptr + length < heap.length) ++length;
		}
		return Buffer.from(heap.subarray(ptr, ptr + length)).toString("utf8");
	};
	const text = value => {
		const bytes = Buffer.from(value, "utf8");
		const ptr = cursor;
		cursor += bytes.length + 1;
		heap.set(bytes, ptr);
		heap[ptr + bytes.length] = 0;
		return [ptr, bytes.length];
	};
	const api = {
		heap: () => heap, readUTF8,
		call(ptr, ...args) {
			if (!functions.has(ptr)) throw new Error("Unexpected native function " + ptr);
			return functions.get(ptr)(...args);
		},
		describe(env, type) {
			const description = descriptions.get(type);
			return description ? text(JSON.stringify(description))[0] : 0;
		},
		callback(env, fn, info) { return api.call(fn, env, info); },
		construct(env, fn, info) { return api.call(fn, env, info); },
		finalize(env, fn, ptr, data, privateData) { return api.call(fn, 900, ptr, data, privateData); },
	};
	const runtime = vm.runInContext(source, context)(api);
	const d = (op, ...args) => runtime.dispatch(1, op, ...args);
	const evaluate = (code, filename = "bridge-test.js") => {
		const [ptr, size] = text(code);
		return d("eval", ptr, size, text(filename)[0]);
	};
	const string = handle => {
		const size = d("string_utf8", handle);
		d("string_utf8", handle, 128, size);
		return readUTF8(128, size);
	};
	const exception = (scope = 0, stack = 0) => {
		const size = d("exception", scope, stack);
		d("exception", scope, stack, 128, size);
		return readUTF8(128, size);
	};
	const addClass = (type, extra = {}) => descriptions.set(type, {
		type, super: 0, name: "Native" + type, constructor: 0, finalize: 0, data: 0, enter: 0, exit: 0,
		methods: [], functions: [], properties: [], variables: [], ...extra,
	});
	return { api, d, runtime, evaluate, string, exception, text, readUTF8, context, functions, descriptions, events, addClass,
		get heap() { return heap; }, grow() { const next = new Uint8Array(heap.length * 2); next.set(heap); heap = next; } };
}

test("uses the host global, rejects another environment and refuses active teardown", () => {
	const f = fixture();
	assert.equal(f.d("init", 900), 1);
	const global = f.d("global");
	const key = f.text("host_state")[0];
	const num = f.d("create_int32", 73);
	assert.equal(f.d("set_property", global, key, num), 1);
	assert.equal(f.d("get_value_int32", f.evaluate("globalThis.host_state")), 73);
	assert.equal(vm.runInContext("host_state", f.context), 73);
	assert.equal(f.runtime.dispatch(2, "init", 900), 0);
	assert.equal(f.runtime.dispatch(2, "has_caught", 0), 1);
	const errorSize = f.runtime.dispatch(2, "exception", 0, 0, 0, 0);
	f.runtime.dispatch(2, "exception", 0, 0, 128, errorSize);
	assert.match(f.readUTF8(128, errorSize), /one JavaScript environment per page/);
	assert.equal(f.runtime.dispatch(2, "create_int32", 1), 0);
	assert.equal(f.runtime.dispatch(2, "dispose"), 1); // failed initialization cleanup
	assert.equal(f.d("dispose"), 0);
	assert.match(f.exception(), /cannot be destroyed/);
	assert.equal(f.d("get_value_int32", f.evaluate("host_state + 1")), 74);
	assert.equal(f.runtime.dispatch(3, "init", 900), 0);
	assert.equal(f.runtime.dispatch(3, "dispose"), 1);
	assert.equal(f.d("init", 900), 0);
	assert.match(f.exception(), /more than once/);
});

test("captures the host evaluator before scripts change the global eval property", () => {
	const f = fixture(); f.d("init", 900);
	assert.ok(f.evaluate("globalThis.eval = () => 100") > 0);
	assert.equal(f.d("get_value_int32", f.evaluate("12 + 13")), 25);
	assert.equal(vm.runInContext("eval('1')", f.context), 100);
});

test("reports CSP and missing WeakRef failures before creating a singleton", () => {
	const f = fixture({ eval() { throw new EvalError("unsafe-eval blocked"); } });
	assert.equal(f.d("init", 900), 0);
	assert.match(f.exception(), /Content Security Policy/);
	assert.equal(f.d("dispose"), 1);
	delete f.context.eval;
	assert.equal(f.d("init", 900), 1);
	const noWeak = fixture({ WeakRef: undefined });
	assert.equal(noWeak.d("init", 900), 0);
	assert.match(noWeak.exception(), /WeakRef and FinalizationRegistry/);
});

test("handles undefined separately from failure and enforces nested scope lifetimes", () => {
	const f = fixture(); f.d("init", 900);
	const outer = f.d("open_scope"), zero = f.d("create_int32", 0);
	const inner = f.d("open_scope"), undef = f.d("create_undefined");
	assert.ok(undef > 0);
	assert.equal(f.d("is_undefined", undef), 1);
	assert.equal(f.evaluate("throw new Error('inner failure')"), 0);
	assert.equal(f.d("has_caught", inner), 1);
	assert.equal(f.d("has_caught", outer), 0);
	assert.match(f.exception(inner, 1), /bridge-test.js/);
	f.d("close_scope", inner);
	assert.equal(f.d("is_int32", zero), 1);
	assert.equal(f.d("is_undefined", undef), 0);
	assert.equal(f.d("has_caught", outer), 1);
	assert.match(f.exception(outer), /expired.*handle/);
	f.d("close_scope", outer);
	assert.equal(f.d("has_caught", 0), 0);
});

test("preserves 64-bit integer boundaries and numeric predicates", () => {
	const f = fixture(); f.d("init", 900);
	for (const [op, low, high, signed, unsigned] of [
		["create_int64", 0, 0x80000000, 1, 0],
		["create_int64", 0xffffffff, 0x7fffffff, 1, 1],
		["create_uint64", 0xffffffff, 0xffffffff, 0, 1],
	]) {
		const handle = f.d(op, low, high);
		assert.equal(f.d("get_uint64_low", handle), low);
		assert.equal(f.d("get_uint64_high", handle), high);
		assert.equal(f.d("is_int64", handle), signed);
		assert.equal(f.d("is_uint64", handle), unsigned);
	}
	assert.equal(f.d("is_int64", f.evaluate("1n << 80n")), 0);
	assert.equal(f.d("is_uint64", f.evaluate("-1n")), 0);
	assert.equal(f.d("is_int32", f.d("create_double", 2147483648)), 0);
	assert.equal(f.d("is_uint32", f.d("create_int32", -1)), 0);
	assert.equal(f.d("is_int32", f.d("create_double", 1.5)), 0);
	assert.equal(f.d("is_double", f.d("create_double", NaN)), 1);
});

test("copies UTF-8/UTF-16 within capacity, including embedded NUL and surrogates", () => {
	const f = fixture(); f.d("init", 900);
	const original = "汉字\0😀\ud800";
	const [ptr, length] = f.text(original);
	const handle = f.d("create_string_utf8", ptr, length);
	const expected = Buffer.from(original, "utf8");
	assert.equal(f.d("string_utf8", handle), expected.length);
	f.heap.fill(0xaa, 128, 160);
	f.d("string_utf8", handle, 128, 2);
	assert.deepEqual([...f.heap.subarray(128, 130)], [...expected.subarray(0, 2)]);
	assert.equal(f.heap[130], 0xaa);
	const utf16 = "A\0😀\ud800";
	const offset = 300;
	const view = new DataView(f.heap.buffer);
	for (let i = 0; i < utf16.length; i++) view.setUint16(offset + 2 * i, utf16.charCodeAt(i), true);
	const wide = f.d("create_string_utf16", offset, utf16.length);
	assert.equal(f.d("string_utf16", wide), utf16.length);
	f.heap.fill(0xaa, 400, 420);
	f.d("string_utf16", wide, 400, utf16.length);
	for (let i = 0; i < utf16.length; i++) assert.equal(view.getUint16(400 + i * 2, true), utf16.charCodeAt(i));
	assert.equal(f.heap[400 + utf16.length * 2], 0xaa);
	assert.ok(f.d("create_string_utf8", 0, 0) > 0);
});

test("string snapshots coerce once and provide stable UTF-8 and UTF-16 query and copy results", () => {
	const f = fixture(); f.d("init", 900);
	const scope = f.d("open_scope");
	const object = f.evaluate("({ calls: 0, toString() { return ++this.calls === 1 ? '汉😀' : 'a much longer result'; } })");
	const snapshot = f.d("string_snapshot", object);
	assert.ok(snapshot > 0);
	const utf8Length = f.d("string_utf8", snapshot);
	assert.equal(utf8Length, Buffer.byteLength("汉😀"));
	assert.equal(f.d("string_utf8", snapshot, 128, utf8Length), utf8Length);
	assert.equal(f.readUTF8(128, utf8Length), "汉😀");
	const utf16Length = f.d("string_utf16", snapshot);
	assert.equal(utf16Length, "汉😀".length);
	assert.equal(f.d("string_utf16", snapshot, 300, utf16Length), utf16Length);
	const view = new DataView(f.heap.buffer);
	for (let i = 0; i < utf16Length; i++) assert.equal(view.getUint16(300 + i * 2, true), "汉😀".charCodeAt(i));
	assert.equal(f.d("get_value_int32", f.d("get_property", object, f.text("calls")[0])), 1);
	f.d("close_scope", scope);
});

test("exception formatting caches each mode and reads a changing stack getter only once", () => {
	const f = fixture(); f.d("init", 900);
	const scope = f.d("open_scope");
	assert.equal(f.evaluate("globalThis.stackReads = 0; globalThis.messageReads = 0; throw { get stack() { return ++stackReads === 1 ? 'first stack' : 'unexpected longer stack'; }, toString() { return ++messageReads === 1 ? 'first message' : 'unexpected longer message'; } }"), 0);
	const stackLength = f.d("exception", scope, 1);
	assert.equal(f.d("exception", scope, 1, 128, stackLength), stackLength);
	assert.equal(f.readUTF8(128, stackLength), "first stack");
	assert.equal(f.exception(scope, 1), "first stack");
	assert.equal(f.d("get_value_int32", f.evaluate("stackReads")), 1);
	assert.equal(f.d("get_value_int32", f.evaluate("messageReads")), 0);
	const messageLength = f.d("exception", scope, 0);
	assert.equal(f.d("exception", scope, 0, 128, messageLength), messageLength);
	assert.equal(f.readUTF8(128, messageLength), "first message");
	assert.equal(f.d("get_value_int32", f.evaluate("messageReads")), 1);
	f.d("close_scope", scope);
	assert.equal(f.evaluate("throw 'another exception'"), 0);
	assert.equal(f.exception(), "another exception");
	assert.equal(f.evaluate("throw 'replaced exception'"), 0);
	assert.equal(f.exception(), "replaced exception");
});

test("exception formatting tolerates throwing stack and string accessors", () => {
	const f = fixture(); f.d("init", 900);
	const scope = f.d("open_scope");
	assert.equal(f.evaluate("throw { get stack() { throw new Error('stack getter failed'); }, toString() { throw new Error('coercion failed'); } }"), 0);
	assert.equal(f.exception(scope, 1), "Unable to format JavaScript exception");
	assert.equal(f.d("has_caught", scope), 1);
	f.d("close_scope", scope);
});

test("copies binary without aliasing WASM and handles views and memory growth", () => {
	const f = fixture(); f.d("init", 900);
	f.heap.set([1, 2, 255], 300);
	const buffer = f.d("create_binary", 300, 3);
	f.heap.fill(99, 300, 303);
	f.grow();
	assert.equal(f.d("binary_size", buffer), 3);
	assert.equal(f.d("binary_copy", buffer, 500), 3);
	assert.deepEqual([...f.heap.subarray(500, 503)], [1, 2, 255]);
	const view = f.evaluate("new Uint8Array([10, 20, 30, 40]).subarray(1, 3)");
	assert.equal(f.d("is_binary", view), 1);
	assert.equal(f.d("binary_copy", view, 500), 2);
	assert.deepEqual([...f.heap.subarray(500, 502)], [20, 30]);
	const arrayBuffer = f.evaluate("new Uint8Array([11, 22]).buffer");
	assert.equal(f.d("is_binary", arrayBuffer), 1);
	assert.equal(f.d("binary_copy", arrayBuffer, 500), 2);
	assert.deepEqual([...f.heap.subarray(500, 502)], [11, 22]);
	assert.equal(f.d("is_binary", f.d("create_object")), 0);
	assert.equal(f.d("create_binary", f.heap.length - 1, 2), 0);
	assert.match(f.exception(), /out of bounds/);
});

test("properties, private data, boxing and value references survive scope exit", () => {
	const f = fixture(); f.d("init", 900);
	const scope = f.d("open_scope");
	const object = f.d("create_object"), num = f.d("create_int32", 25), key = f.text("answer")[0];
	f.d("set_property", object, key, num);
	assert.equal(f.d("get_value_int32", f.d("get_property", object, key)), 25);
	f.d("set_private", object, 777);
	assert.equal(f.d("get_private", object), 777);
	const box = f.d("boxing", num);
	assert.equal(f.d("get_value_int32", f.d("unboxing", box)), 25);
	f.d("update_boxed_value", box, f.d("create_int32", 42));
	assert.equal(f.d("get_value_int32", f.d("get_property_uint32", box, 0)), 42);
	const ref = f.d("create_value_ref", object);
	const primitiveRef = f.d("create_value_ref", f.d("create_undefined"));
	f.d("set_ref_weak", primitiveRef);
	f.d("close_scope", scope);
	const objectAgain = f.d("get_value_from_ref", ref);
	assert.equal(f.d("get_private", objectAgain), 777);
	assert.equal(f.d("is_undefined", f.d("get_value_from_ref", primitiveRef)), 1);
	assert.equal(f.d("release_value_ref", ref), 1);
	assert.equal(f.d("get_value_from_ref", ref), 0);
});

test("callbacks return real values and propagate native errors with nested reentry", () => {
	const f = fixture(); f.d("init", 900);
	f.functions.set(100, (env, info) => {
		assert.equal(env, 1);
		assert.equal(f.d("get_userdata", info), 64);
		assert.equal(f.d("get_args_len", info), 1);
		const arg = f.d("get_arg", info, 0);
		const scope = f.d("open_scope");
		assert.equal(f.evaluate("throw new Error('handled reentry')"), 0);
		assert.equal(f.d("has_caught", scope), 1);
		f.d("close_scope", scope);
		const result = f.d("create_int32", f.d("get_value_int32", arg) + 1);
		f.d("add_return", info, result);
	});
	const fn = f.d("create_function", 100, 64, 0), arg = f.d("create_int32", 9);
	new DataView(f.heap.buffer).setUint32(300, arg, true);
	const result = f.d("call_function", fn, 0, 1, 300);
	assert.equal(f.d("get_value_int32", result), 10);
	assert.equal(f.d("has_caught", 0), 0);
	f.functions.set(101, (env, info) => f.d("throw_by_string", info, f.text("native boom")[0]));
	const thrower = f.d("create_function", 101, 0, 0);
	const scope = f.d("open_scope");
	assert.equal(f.d("call_function", thrower, 0, 0, 0), 0);
	assert.equal(f.d("has_caught", scope), 1);
	assert.match(f.exception(scope), /native boom/);
	f.d("close_scope", scope);
	assert.equal(f.d("has_caught", 0), 0);
	f.functions.set(102, () => {});
	const empty = f.d("call_function", f.d("create_function", 102, 0, 0), 0, 0, 0);
	assert.ok(empty > 0);
	assert.equal(f.d("is_undefined", empty), 1);
});

test("native classes expose inheritance, identity, instance methods and static accessors", () => {
	const f = fixture(); f.d("init", 900);
	let value = 12;
	f.addClass(11, { methods: [{ name: "identify", callback: 201, data: 77 }] });
	f.addClass(12, {
		super: 11, name: "Derived", constructor: 200, data: 88,
		methods: [{ name: "increment", callback: 202, data: 3 }],
		functions: [{ name: "staticCall", callback: 203, data: 4 }],
		properties: [{ name: "value", getter: 204, setter: 205, getter_data: 5, setter_data: 6 }],
		variables: [{ name: "constant", getter: 206, setter: 0, getter_data: 7 }],
	});
	f.functions.set(200, (_, info) => { assert.equal(f.d("get_userdata", info), 88); return 1234; });
	f.functions.set(201, (_, info) => {
		assert.equal(f.d("get_native_holder_ptr", info), 1234);
		assert.equal(f.d("get_native_holder_typeid", info), 12);
		assert.equal(f.d("get_userdata", info), 77);
		f.d("add_return", info, f.d("create_int32", 1234));
	});
	f.functions.set(202, (_, info) => f.d("add_return", info, f.d("create_int32", ++value)));
	f.functions.set(203, (_, info) => f.d("add_return", info, f.d("create_int32", 40)));
	f.functions.set(204, (_, info) => f.d("add_return", info, f.d("create_int32", value)));
	f.functions.set(205, (_, info) => { value = f.d("get_value_int32", f.d("get_arg", info, 0)); });
	f.functions.set(206, (_, info) => f.d("add_return", info, f.d("create_int32", 42)));
	const global = f.d("global"), ctor = f.d("create_class", 12);
	f.d("set_property", global, f.text("Derived")[0], ctor);
	assert.equal(f.d("get_value_int32", f.evaluate("globalThis.instance = new Derived(); instance.identify()")), 1234);
	assert.equal(f.d("get_value_int32", f.evaluate("instance.value = 24; instance.increment()")), 25);
	assert.equal(f.d("get_value_int32", f.evaluate("Derived.staticCall() + Derived.constant")), 82);
	const instance = f.evaluate("instance");
	assert.equal(f.d("is_instance_of", 11, instance), 1);
	assert.equal(f.d("is_instance_of", 12, instance), 1);
	assert.equal(f.d("is_instance_of", 99, instance), 0);
	const wrapper = f.d("native_object_to_value", 12, 1234, 0);
	f.d("set_property", global, f.text("wrapped")[0], wrapper);
	assert.equal(f.d("get_value_bool", f.evaluate("wrapped === instance")), 1);
	assert.equal(f.evaluate("Derived()"), 0);
	assert.match(f.exception(), /require new/);
	const scope = f.d("open_scope");
	assert.equal(f.evaluate("Derived.prototype.increment.call({})"), 0);
	assert.match(f.exception(scope), /Invalid receiver/);
	f.d("close_scope", scope);
});

async function eventually(check) {
	for (let i = 0; i < 60; i++) {
		await new Promise(resolve => setImmediate(resolve));
		global.gc();
		await new Promise(resolve => setImmediate(resolve));
		if (check()) return;
	}
	assert.fail("Object was retained or its finalizer did not run after 60 garbage collections");
}

test("rewrapping a collected native wrapper keeps the binding until its replacement is collected", () => {
	const weakRefs = [];
	let registry;
	class ControlledWeakRef {
		constructor(target) { this.target = target; weakRefs.push(this); }
		deref() { return this.target; }
	}
	class ControlledFinalizationRegistry {
		constructor(callback) { this.callback = callback; this.cells = new Map(); registry = this; }
		register(target, held, token) { this.cells.set(token, { target, held }); }
		unregister(token) { return this.cells.delete(token); }
		collectNext() {
			const [token, { target, held }] = this.cells.entries().next().value;
			this.cells.delete(token);
			for (const ref of weakRefs) if (ref.target === target) ref.target = undefined;
			// Delay the callback to reproduce the window after WeakRef clearing.
			return () => this.callback(held);
		}
	}
	const f = fixture({ WeakRef: ControlledWeakRef, FinalizationRegistry: ControlledFinalizationRegistry });
	f.d("init", 900, 444);
	f.addClass(20, { data: 33, enter: 301, exit: 302, finalize: 303,
		methods: [{ name: "read", callback: 304, data: 0 }] });
	const nativeBindings = new Set();
	f.functions.set(301, ptr => { f.events.push(["enter", ptr]); return 77; });
	f.functions.set(302, (ptr, data, envPrivate, userData) => {
		f.events.push(["exit", ptr, userData]);
		nativeBindings.delete(ptr);
	});
	f.functions.set(303, (ffi, ptr) => f.events.push(["finalize", ptr]));
	f.functions.set(304, (_, info) => {
		const ptr = f.d("get_native_holder_ptr", info);
		assert.ok(nativeBindings.has(ptr), "the replacement must have a valid Godot native binding");
		f.d("add_return", info, f.d("create_int32", ptr));
	});
	for (const owned of [0, 1]) {
		const ptr = 555 + owned;
		nativeBindings.add(ptr);
		let scope = f.d("open_scope");
		f.d("native_object_to_value", 20, ptr, owned);
		f.d("close_scope", scope);
		const delayedOldFinalizer = registry.collectNext();
		scope = f.d("open_scope");
		const replacement = f.d("native_object_to_value", 20, ptr, 0);
		assert.ok(replacement > 0);
		assert.ok(nativeBindings.has(ptr));
		assert.deepEqual(f.events.filter(event => event[1] === ptr), [["enter", ptr]]);
		delayedOldFinalizer();
		assert.ok(nativeBindings.has(ptr), "the old wrapper's pending finalizer must not release the replacement");
		const method = f.d("get_property", replacement, f.text("read")[0]);
		assert.equal(f.d("get_value_int32", f.d("call_function", method, replacement, 0, 0)), ptr);
		f.d("close_scope", scope);
		const currentFinalizer = registry.collectNext();
		currentFinalizer();
		assert.equal(nativeBindings.has(ptr), false);
		const expected = [["enter", ptr], ["exit", ptr, 77]];
		if (owned) expected.push(["finalize", ptr]);
		assert.deepEqual(f.events.filter(event => event[1] === ptr), expected);
		delayedOldFinalizer();
		currentFinalizer();
		assert.deepEqual(f.events.filter(event => event[1] === ptr), expected);
	}
});

test("GC releases native owned and borrowed objects and function data exactly once", { skip: !global.gc }, async () => {
	const f = fixture(); f.d("init", 900, 444);
	f.addClass(20, { data: 33, enter: 301, exit: 302, finalize: 303 });
	f.functions.set(301, (ptr, data, envPrivate) => { f.events.push(["enter", ptr, data, envPrivate]); return ptr + 100; });
	f.functions.set(302, (ptr, data, envPrivate, userData) => f.events.push(["exit", ptr, data, envPrivate, userData]));
	f.functions.set(303, (ffi, ptr, data, envPrivate) => f.events.push(["finalize", ffi, ptr, data, envPrivate]));
	f.functions.set(304, () => {});
	f.functions.set(305, (ffi, data, envPrivate) => f.events.push(["function", ffi, data, envPrivate]));
	const scope = f.d("open_scope");
	f.d("native_object_to_value", 20, 555, 1);
	f.d("native_object_to_value", 20, 556, 0);
	f.d("create_function", 304, 66, 305);
	f.d("close_scope", scope);
	await eventually(() => f.events.filter(x => x[0] === "exit").length === 2 && f.events.some(x => x[0] === "function"));
	assert.deepEqual(f.events.filter(x => x[0] === "finalize"), [["finalize", 900, 555, 33, 444]]);
	assert.deepEqual(f.events.filter(x => x[0] === "function"), [["function", 900, 66, 444]]);
	assert.deepEqual(f.events.filter(x => x[0] === "exit").sort((a, b) => a[1] - b[1]), [
		["exit", 555, 33, 444, 655], ["exit", 556, 33, 444, 656],
	]);
	const snapshot = JSON.stringify(f.events);
	for (let i = 0; i < 3; i++) { await new Promise(resolve => setImmediate(resolve)); global.gc(); }
	assert.equal(JSON.stringify(f.events), snapshot);
});

test("weak value references do not pin objects and owners retain their children", { skip: !global.gc }, async () => {
	const f = fixture(); f.d("init", 900);
	let scope = f.d("open_scope");
	const child = f.d("create_object"), owner = f.d("create_object");
	const childRef = f.d("create_value_ref", child), ownerRef = f.d("create_value_ref", owner);
	f.d("set_ref_weak", childRef);
	assert.equal(f.d("set_owner", child, owner), 1);
	f.d("close_scope", scope);
	for (let i = 0; i < 3; i++) { await new Promise(resolve => setImmediate(resolve)); global.gc(); }
	scope = f.d("open_scope");
	assert.equal(f.d("is_object", f.d("get_value_from_ref", childRef)), 1);
	f.d("close_scope", scope);
	f.d("release_value_ref", ownerRef);
	await eventually(() => {
		const scope = f.d("open_scope");
		const missing = f.d("is_undefined", f.d("get_value_from_ref", childRef));
		f.d("close_scope", scope);
		return missing === 1;
	});
});
