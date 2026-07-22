// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import type { MethodTypeOverrides } from "./method-overrides.js";

export type BuiltinGenericDefinition = {
	typeParameters: string;
	members: string[];
	methodTypes: MethodTypeOverrides;
};

const ARRAY_METHOD_TYPES: MethodTypeOverrides = {
	assign: { arguments: { array: "Array<T>" } },
	get: { returnType: "T" },
	set: { arguments: { value: "T" } },
	push_back: { arguments: { value: "T" } },
	push_front: { arguments: { value: "T" } },
	append: { arguments: { value: "T" } },
	append_array: { arguments: { array: "Array<T>" } },
	insert: { arguments: { value: "T" } },
	fill: { arguments: { value: "T" } },
	erase: { arguments: { value: "T" } },
	front: { returnType: "T | null" },
	back: { returnType: "T | null" },
	pick_random: { returnType: "T | null" },
	find: { arguments: { what: "T" } },
	rfind: { arguments: { what: "T" } },
	count: { arguments: { value: "T" } },
	has: { arguments: { value: "T" } },
	pop_back: { returnType: "T | null" },
	pop_front: { returnType: "T | null" },
	pop_at: { returnType: "T | null" },
	bsearch: { arguments: { value: "T" } },
	bsearch_custom: { arguments: { value: "T" } },
	duplicate: { returnType: "Array<T>" },
	duplicate_deep: { returnType: "Array<T>" },
	slice: { returnType: "Array<T>" },
	filter: { returnType: "Array<T>" },
	map: { returnType: "Array<Variant>" },
	max: { returnType: "T | null" },
	min: { returnType: "T | null" },
};

const DICTIONARY_METHOD_TYPES: MethodTypeOverrides = {
	assign: { arguments: { dictionary: "Dictionary<K, V>" } },
	merge: { arguments: { dictionary: "Dictionary<K, V>" } },
	merged: { arguments: { dictionary: "Dictionary<K, V>" }, returnType: "Dictionary<K, V>" },
	has: { arguments: { key: "K" } },
	has_all: { arguments: { keys: "Array<K>" } },
	find_key: { arguments: { value: "V" }, returnType: "K | null" },
	erase: { arguments: { key: "K" } },
	keys: { returnType: "Array<K>" },
	values: { returnType: "Array<V>" },
	duplicate: { returnType: "Dictionary<K, V>" },
	duplicate_deep: { returnType: "Dictionary<K, V>" },
	get: { arguments: { key: "K", default: "V | null" }, returnType: "V | null" },
	get_or_add: { arguments: { key: "K", default: "V | null" }, returnType: "V | null" },
	set: { arguments: { key: "K", value: "V" } },
};

const CALLABLE_METHOD_TYPES: MethodTypeOverrides = {
	callv: { returnType: "ReturnType<T>" },
	call: { returnType: "ReturnType<T>", varargsType: "Parameters<T>" },
	call_deferred: { varargsType: "Parameters<T>" },
	rpc: { varargsType: "Parameters<T>" },
	rpc_id: { varargsType: "Parameters<T>" },
};

const SIGNAL_METHOD_TYPES: MethodTypeOverrides = {
	connect: { arguments: { callable: "Callable<T>" } },
	disconnect: { arguments: { callable: "Callable<T>" } },
	is_connected: { arguments: { callable: "Callable<T>" } },
	emit: { varargsType: "Parameters<T>" },
};

const BUILTIN_GENERICS: Record<string, BuiltinGenericDefinition> = {
	Array: {
		typeParameters: "<T extends Variant = Variant>",
		members: [
			"/** Type-only invariant element marker; no corresponding runtime property exists. */",
			"private readonly __element_type__: (value: T) => T;",
		],
		methodTypes: ARRAY_METHOD_TYPES,
	},
	Dictionary: {
		typeParameters: "<K extends Variant = Variant, V extends Variant = Variant>",
		members: [
			"/** Type-only invariant key marker; no corresponding runtime property exists. */",
			"private readonly __key_type__: (value: K) => K;",
			"/** Type-only invariant value marker; no corresponding runtime property exists. */",
			"private readonly __value_type__: (value: V) => V;",
		],
		methodTypes: DICTIONARY_METHOD_TYPES,
	},
	Callable: {
		typeParameters: "<T extends (...args: any[]) => any = (...args: any[]) => any>",
		members: [
			"/** Type-only callable signature marker; no corresponding runtime property exists. */",
			"private readonly __function_type__: T;",
		],
		methodTypes: CALLABLE_METHOD_TYPES,
	},
	Signal: {
		typeParameters: "<T extends (...args: any[]) => any = (...args: any[]) => any>",
		members: [
			"/** Type-only invariant signal signature marker; no corresponding runtime property exists. */",
			"private readonly __function_type__: (value: T) => T;",
		],
		methodTypes: SIGNAL_METHOD_TYPES,
	},
};

export function getBuiltinGenericDefinition(name: string): BuiltinGenericDefinition | undefined {
	return BUILTIN_GENERICS[name];
}
