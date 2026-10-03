// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

// Evaluated by the Emscripten bridge and called with its WASM accessors.
(function (api) {
	"use strict";

	const environments = new Map();
	// Match Unity WebGL by using one host realm for the lifetime of the page.
	// Keep this explicit so accidental recreation cannot reuse native state.
	let activeEnvironment = null;
	const encoder = new TextEncoder();
	let nextId = 1;

	function id() {
		if (nextId > 0xffffffff) throw new Error("Puerts WebGL handle space exhausted");
		return nextId++;
	}

	function isObject(value) {
		return value !== null && (typeof value === "object" || typeof value === "function");
	}

	function rememberError(env, error) {
		const scope = env.stack[env.stack.length - 1];
		if (scope) {
			if (!scope.caught) {
				scope.caught = true;
				scope.error = error;
			}
		} else {
			env.lastError = error;
			env.lastCaught = true;
			env.exceptionStrings.clear();
		}
	}

	function exceptionText(target, error, withStack) {
		withStack = !!withStack;
		if (!target.exceptionStrings.has(withStack)) {
			let stack;
			// Accessors and coercion can have side effects.  Capture them once for
			// each formatting mode so the native size query and copy agree.
			try { if (withStack && error != null) stack = error.stack; } catch (_) { }
			let text;
			try { text = stack ? String(stack) : String(error); }
			catch (_) { text = "Unable to format JavaScript exception"; }
			target.exceptionStrings.set(withStack, text);
		}
		return target.exceptionStrings.get(withStack);
	}

	function openScope(env) {
		const scope = { id: id(), handles: [], caught: false, error: undefined, exceptionStrings: new Map() };
		env.scopes.set(scope.id, scope);
		env.stack.push(scope);
		return scope;
	}

	function closeScope(env, scope) {
		if (env.stack[env.stack.length - 1] !== scope) {
			throw new Error("Puerts WebGL scopes must close in reverse order");
		}
		env.stack.pop();
		for (const handle of scope.handles) env.values.delete(handle);
		env.scopes.delete(scope.id);
	}

	function addValue(env, value) {
		const handle = id();
		env.values.set(handle, value);
		const scope = env.stack[env.stack.length - 1];
		if (scope) scope.handles.push(handle);
		return handle;
	}

	function value(env, handle) {
		if (!env.values.has(handle)) throw new Error("Invalid or expired Puerts WebGL value handle");
		return env.values.get(handle);
	}

	function active(env) {
		if (env.disposed) throw new Error("Puerts WebGL environment has been disposed");
		if (env !== activeEnvironment) throw new Error("Puerts WebGL environment was not initialized");
	}

	function memory(ptr, size) {
		const heap = api.heap();
		ptr >>>= 0;
		size >>>= 0;
		if (ptr > heap.byteLength || size > heap.byteLength - ptr) {
			throw new RangeError("Puerts WebGL memory access is out of bounds");
		}
		return heap.subarray(ptr, ptr + size);
	}

	function writeUtf8(text, ptr, capacity) {
		const bytes = encoder.encode(text);
		if (ptr && capacity) memory(ptr, Math.min(capacity >>> 0, bytes.length)).set(bytes.subarray(0, capacity >>> 0));
		return bytes.length;
	}

	function binary(value) {
		if (ArrayBuffer.isView(value)) return new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
		const tag = Object.prototype.toString.call(value);
		if (tag === "[object ArrayBuffer]" || tag === "[object SharedArrayBuffer]") return new Uint8Array(value);
		throw new TypeError("Expected an ArrayBuffer or an ArrayBuffer view");
	}

	function releaseResource(record) {
		if (record.released) return;
		record.released = true;
		const env = record.env;
		env.resources.delete(record.token);
		env.finalizers.unregister(record.token);
		if (record.kind === "native") {
			if (env.nativeCache.get(record.ptr) === record) env.nativeCache.delete(record.ptr);
			// Exit must run even for borrowed objects; ownership only controls finalize.
			try {
				if (record.description.exit) {
					api.call(record.description.exit, record.ptr, record.description.data || 0, env.private, record.userdata);
				}
			} finally {
				if (record.owned && record.description.finalize) {
					api.finalize(env.id, record.description.finalize, record.ptr, record.description.data || 0, env.private);
				}
			}
		} else if (record.finalize) {
			api.call(record.finalize, env.ffi, record.data, env.private);
		}
	}

	function track(env, target, record) {
		record.env = env;
		record.token = {};
		record.released = false;
		record.generation = 0;
		env.resources.set(record.token, record);
		registerTarget(env, target, record);
		return record;
	}

	function registerTarget(env, target, record) {
		++record.generation;
		record.weak = new WeakRef(target);
		env.finalizers.register(target, { record, generation: record.generation }, record.token);
	}

	function invoke(env, callback, data, receiver, args, construct) {
		active(env);
		const scope = openScope(env);
		const infoId = id();
		const holder = isObject(receiver) ? env.nativeObjects.get(receiver) : undefined;
		const info = { args, holder, data, result: undefined };
		env.infos.set(infoId, info);
		try {
			let result;
			if (construct) result = api.construct(env.id, callback, infoId);
			else {
				api.callback(env.id, callback, infoId);
				result = info.result;
			}
			if (scope.caught) throw scope.error;
			active(env);
			return result;
		} finally {
			env.infos.delete(infoId);
			// Native code can dispose an environment during its own callback.
			if (!env.disposed) closeScope(env, scope);
		}
	}

	function makeFunction(env, callback, data, finalize, holderType) {
		const fn = function (...args) {
			if (holderType) {
				active(env);
				const holder = isObject(this) ? env.nativeObjects.get(this) : undefined;
				if (!holder || holder.released || !instanceOf(env, holderType, this)) {
					throw new TypeError("Invalid receiver for native Puerts method");
				}
			}
			return invoke(env, callback, data || 0, this, args, false);
		};
		Object.setPrototypeOf(fn, env.realm.Function.prototype);
		if (finalize) track(env, fn, { kind: "function", data: data || 0, finalize });
		return fn;
	}

	function getDescription(env, type) {
		const ptr = api.describe(env.id, type);
		if (!ptr) throw new Error("Puerts native type is not registered: " + type);
		const description = JSON.parse(api.readUTF8(ptr));
		if (description.type !== type) throw new Error("Invalid Puerts native class description");
		return description;
	}

	function createClass(env, type) {
		const cached = env.classes.get(type);
		if (cached) return cached;
		if (env.buildingClasses.has(type)) throw new Error("Circular native class inheritance");
		env.buildingClasses.add(type);
		try {
			const description = getDescription(env, type);
			const base = description.super ? createClass(env, description.super) : undefined;
			const ctor = function (...args) {
				active(env);
				if (!new.target) throw new TypeError("Native Puerts constructors require new");
				if (!description.constructor) throw new TypeError("Native Puerts type cannot be constructed: " + description.name);
				const ptr = invoke(env, description.constructor, description.data || 0, undefined, args, true);
				if (!ptr) throw new Error("Native Puerts constructor returned a null pointer");
				return wrapNative(env, type, ptr, true, new.target.prototype);
			};
			Object.setPrototypeOf(ctor, base || env.realm.Function.prototype);
			Object.defineProperty(ctor, "name", { value: description.name || "NativeClass", configurable: true });
			ctor.prototype = env.realm.Object.create(base ? base.prototype : env.realm.Object.prototype);
			Object.defineProperty(ctor.prototype, "constructor", { value: ctor, writable: true, configurable: true });
			const defineMethods = (target, methods, holderType) => {
				for (const method of methods || []) {
					Object.defineProperty(target, method.name, {
						value: makeFunction(env, method.callback, method.data, 0, holderType),
						writable: true, configurable: true,
					});
				}
			};
			const defineProperties = (target, properties, holderType) => {
				for (const property of properties || []) {
					Object.defineProperty(target, property.name, {
						get: property.getter ? makeFunction(env, property.getter, property.getter_data, 0, holderType) : undefined,
						set: property.setter ? makeFunction(env, property.setter, property.setter_data, 0, holderType) : undefined,
						enumerable: true, configurable: true,
					});
				}
			};
			defineMethods(ctor.prototype, description.methods, type);
			defineMethods(ctor, description.functions, 0);
			defineProperties(ctor.prototype, description.properties, type);
			defineProperties(ctor, description.variables, 0);
			env.descriptions.set(type, description);
			env.classes.set(type, ctor);
			return ctor;
		} finally {
			env.buildingClasses.delete(type);
		}
	}

	function wrapNative(env, type, ptr, owned, prototype) {
		if (!ptr) return null;
		const cached = env.nativeCache.get(ptr);
		if (cached && !cached.released) {
			const wrapper = cached.weak.deref();
			cached.owned = cached.owned || !!owned;
			if (wrapper) return wrapper;
			// A WeakRef can clear before its FinalizationRegistry callback runs.
			// Transfer the live native binding to the replacement wrapper instead
			// of calling OnExit and freeing the pointer that we are about to use.
			const ctor = createClass(env, cached.type);
			const replacement = env.realm.Object.create(prototype || ctor.prototype);
			env.finalizers.unregister(cached.token);
			registerTarget(env, replacement, cached);
			env.nativeObjects.set(replacement, cached);
			return replacement;
		}
		const ctor = createClass(env, type);
		const description = env.descriptions.get(type);
		const wrapper = env.realm.Object.create(prototype || ctor.prototype);
		const record = track(env, wrapper, { kind: "native", ptr, type, owned: !!owned, description, userdata: 0 });
		env.nativeObjects.set(wrapper, record);
		env.nativeCache.set(ptr, record);
		try {
			if (description.enter) record.userdata = api.call(description.enter, ptr, description.data || 0, env.private) || 0;
		} catch (error) {
			releaseResource(record);
			throw error;
		}
		return wrapper;
	}

	function instanceOf(env, type, object) {
		const native = isObject(object) ? env.nativeObjects.get(object) : undefined;
		if (!native || native.released) return false;
		let current = native.type;
		while (current) {
			if (current === type) return true;
			current = env.descriptions.get(current).super || 0;
		}
		return false;
	}

	function initialize(env) {
		if (activeEnvironment) {
			throw new Error("Puerts WebGL supports one JavaScript environment per page; it cannot be recreated while active");
		}
		if (typeof WeakRef !== "function" || typeof FinalizationRegistry !== "function") {
			throw new Error("Puerts WebGL requires browser support for WeakRef and FinalizationRegistry");
		}
		// globalThis also works in Web Workers.  Emscripten's module and its
		// browser APIs remain in this same realm, so native objects can be
		// handed directly to browser code.
		env.realm = globalThis;
		env.evaluate = env.realm.eval;
		try {
			env.evaluate("0");
		} catch (error) {
			throw new Error("Puerts WebGL cannot evaluate JavaScript in its browser realm. Check the page Content Security Policy and its unsafe-eval permission. " + String(error));
		}
		env.finalizers = new FinalizationRegistry(({ record, generation }) => {
			// An older wrapper's queued finalizer must not release its replacement.
			if (record.released || generation !== record.generation) return;
			try { releaseResource(record); }
			catch (error) { rememberError(env, error); }
		});
		activeEnvironment = env;
	}

	function dispose(env) {
		if (env === activeEnvironment) {
			throw new Error("The active Puerts WebGL environment cannot be destroyed; reload the page to restart it");
		}
		if (env.disposed) return 1;
		env.disposed = true;
		for (const record of Array.from(env.resources.values())) {
			try { releaseResource(record); }
			catch (error) { rememberError(env, error); }
		}
		env.values.clear();
		env.refs.clear();
		env.infos.clear();
		env.stack.length = 0;
		env.scopes.clear();
		env.nativeCache.clear();
		env.classes.clear();
		env.descriptions.clear();
		// Only failed initialization reaches here.  The active singleton and
		// its host realm remain alive for the lifetime of the page.
		environments.delete(env.id);
		return 1;
	}

	function dispatch(envId, op, a = 0, b = 0, c = 0, d = 0) {
		let env = environments.get(envId);
		if (op === "init") {
			if (env) {
				rememberError(env, new Error("Puerts WebGL environment cannot be initialized more than once"));
				return 0;
			}
			env = {
				id: envId, ffi: a, private: b, disposed: false, lastError: undefined, lastCaught: false,
				exceptionStrings: new Map(),
				values: new Map(), refs: new Map(), scopes: new Map(), stack: [], infos: new Map(),
				classes: new Map(), descriptions: new Map(), buildingClasses: new Set(),
				nativeObjects: new WeakMap(), nativeCache: new Map(), resources: new Map(),
				privateData: new WeakMap(), owners: new WeakMap(),
			};
			environments.set(envId, env);
			try { initialize(env); return 1; }
			catch (error) { rememberError(env, error); return 0; }
		}
		if (!env) return 0;
		try {
			if (op === "dispose") return dispose(env);
			if (op !== "has_caught" && op !== "exception") active(env);
			switch (op) {
				case "open_scope": return openScope(env).id;
				case "close_scope": {
					const scope = env.scopes.get(a);
					if (!scope) throw new Error("Invalid Puerts WebGL scope");
					closeScope(env, scope);
					return 1;
				}
				case "has_caught": return +(a ? !!env.scopes.get(a)?.caught : env.lastCaught);
				case "exception": {
					const scope = a ? env.scopes.get(a) : undefined;
					const caught = a ? scope && scope.caught : env.lastCaught;
					const error = a ? scope && scope.error : env.lastError;
					return writeUtf8(caught ? exceptionText(scope || env, error, b) : "", c, d);
				}
				case "create_null": return addValue(env, null);
				case "create_undefined": return addValue(env, undefined);
				case "create_boolean": return addValue(env, !!a);
				case "create_int32": return addValue(env, a | 0);
				case "create_uint32": return addValue(env, a >>> 0);
				case "create_double": return addValue(env, a);
				case "create_int64": return addValue(env, BigInt.asIntN(64, (BigInt(b >>> 0) << 32n) | BigInt(a >>> 0)));
				case "create_uint64": return addValue(env, (BigInt(b >>> 0) << 32n) | BigInt(a >>> 0));
				case "create_string_utf8": return addValue(env, api.readUTF8(a, b));
				case "create_string_utf16": {
					const data = memory(a, b * 2);
					const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
					let text = "";
					for (let i = 0; i < b; i++) text += String.fromCharCode(view.getUint16(i * 2, true));
					return addValue(env, text);
				}
				case "create_binary": {
					const copy = new env.realm.Uint8Array(b);
					copy.set(memory(a, b));
					return addValue(env, copy);
				}
				case "create_array": return addValue(env, new env.realm.Array());
				case "create_object": return addValue(env, new env.realm.Object());
				case "create_function": return addValue(env, makeFunction(env, a, b, c, 0));
				case "create_class": return addValue(env, createClass(env, a));
				case "native_object_to_value": return addValue(env, wrapNative(env, a, b, c));
				case "get_value_bool": return +!!value(env, a);
				case "get_value_int32": return Number(value(env, a)) | 0;
				case "get_value_uint32": return Number(value(env, a)) >>> 0;
				case "get_value_double": return Number(value(env, a));
				case "get_int64_low": case "get_uint64_low": return Number(BigInt.asUintN(64, BigInt(value(env, a))) & 0xffffffffn);
				case "get_int64_high": case "get_uint64_high": return Number(BigInt.asUintN(64, BigInt(value(env, a))) >> 32n);
				case "string_utf8": return writeUtf8(String(value(env, a)), b, c);
				case "string_snapshot": return addValue(env, String(value(env, a)));
				case "string_utf16": {
					const text = String(value(env, a));
					if (b && c) {
						const count = Math.min(c >>> 0, text.length);
						const data = memory(b, count * 2);
						const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
						for (let i = 0; i < count; i++) view.setUint16(i * 2, text.charCodeAt(i), true);
					}
					return text.length;
				}
				case "binary_size": return binary(value(env, a)).byteLength;
				case "binary_copy": { const bytes = binary(value(env, a)); memory(b, bytes.byteLength).set(bytes); return bytes.byteLength; }
				case "get_array_length": return value(env, a).length >>> 0;
				case "is_null": return +(value(env, a) === null);
				case "is_undefined": return +(value(env, a) === undefined);
				case "is_boolean": return +(typeof value(env, a) === "boolean");
				case "is_int32": { const v = value(env, a); return +(typeof v === "number" && (v | 0) === v); }
				case "is_uint32": { const v = value(env, a); return +(typeof v === "number" && (v >>> 0) === v); }
				case "is_int64": { const v = value(env, a); return +(typeof v === "bigint" && v === BigInt.asIntN(64, v)); }
				case "is_uint64": { const v = value(env, a); return +(typeof v === "bigint" && v === BigInt.asUintN(64, v)); }
				case "is_double": return +(typeof value(env, a) === "number");
				case "is_string": return +(typeof value(env, a) === "string");
				case "is_object": return +isObject(value(env, a));
				case "is_function": return +(typeof value(env, a) === "function");
				case "is_binary": { try { binary(value(env, a)); return 1; } catch (_) { return 0; } }
				case "is_array": return +Array.isArray(value(env, a));
				case "get_native_object_ptr": return env.nativeObjects.get(value(env, a))?.ptr || 0;
				case "get_native_object_typeid": return env.nativeObjects.get(value(env, a))?.type || 0;
				case "is_instance_of": return +instanceOf(env, a, value(env, b));
				case "boxing": { const box = new env.realm.Object(); box[0] = value(env, a); return addValue(env, box); }
				case "unboxing": return addValue(env, value(env, a)[0]);
				case "is_boxed_value": return +isObject(value(env, a));
				case "update_boxed_value": value(env, a)[0] = value(env, b); return 1;
				case "get_args_len": return env.infos.get(a).args.length;
				case "get_arg": return addValue(env, env.infos.get(a).args[b]);
				case "get_native_holder_ptr": return env.infos.get(a).holder?.ptr || 0;
				case "get_native_holder_typeid": return env.infos.get(a).holder?.type || 0;
				case "get_userdata": return env.infos.get(a).data;
				case "add_return": env.infos.get(a).result = value(env, b); return 1;
				case "throw_by_string": {
					if (!env.infos.has(a)) throw new Error("Invalid native callback info");
					throw new env.realm.Error(api.readUTF8(b));
				}
				case "create_value_ref": { const ref = id(); env.refs.set(ref, { value: value(env, a), weak: undefined }); return ref; }
				case "get_value_from_ref": {
					const ref = env.refs.get(a);
					if (!ref) throw new Error("Invalid Puerts WebGL value reference");
					return addValue(env, ref.weak ? ref.weak.deref() : ref.value);
				}
				case "release_value_ref": return +env.refs.delete(a);
				case "set_ref_weak": {
					const ref = env.refs.get(a);
					if (!ref) throw new Error("Invalid Puerts WebGL value reference");
					if (!ref.weak && isObject(ref.value)) { ref.weak = new WeakRef(ref.value); ref.value = undefined; }
					return 1;
				}
				case "set_owner": {
					const child = value(env, a), owner = value(env, b);
					if (!isObject(child) || !isObject(owner)) return 0;
					let owned = env.owners.get(owner);
					if (!owned) { owned = new Set(); env.owners.set(owner, owned); }
					owned.add(child);
					return 1;
				}
				case "get_property": return addValue(env, value(env, a)[api.readUTF8(b)]);
				case "set_property": return +Reflect.set(value(env, a), api.readUTF8(b), value(env, c));
				case "get_private": return env.privateData.get(value(env, a)) || 0;
				case "set_private": env.privateData.set(value(env, a), b); return 1;
				case "get_property_uint32": return addValue(env, value(env, a)[b >>> 0]);
				case "set_property_uint32": return +Reflect.set(value(env, a), b >>> 0, value(env, c));
				case "call_function": {
					const bytes = memory(d, c * 4);
					const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
					const args = [];
					for (let i = 0; i < c; i++) args.push(value(env, view.getUint32(i * 4, true)));
					return addValue(env, Reflect.apply(value(env, a), b ? value(env, b) : undefined, args));
				}
				case "eval": {
					const path = c ? api.readUTF8(c).replace(/[\r\n\u2028\u2029]/g, "") : "puerts-webgl.js";
					return addValue(env, env.evaluate(api.readUTF8(a, b) + "\n//# sourceURL=" + path));
				}
				case "global": return addValue(env, env.realm);
				case "set_env_private": env.private = a; return 1;
				default: throw new Error("Unknown Puerts WebGL bridge operation: " + op);
			}
		} catch (error) {
			rememberError(env, error);
			return 0;
		}
	}

	return { dispatch };
})
