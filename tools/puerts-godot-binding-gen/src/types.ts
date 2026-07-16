// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

export type ApiArgument = {
	name: string;
	type: string;
	meta?: string;
	default_value?: string;
};

export type ApiMethod = {
	name: string;
	is_const?: boolean;
	is_static?: boolean;
	is_vararg?: boolean;
	hash?: number;
	return_type?: string;
	return_value?: {
		type: string;
		meta?: string;
	};
	arguments?: ApiArgument[];
};

export type ApiOperator = {
	name: string;
	right_type?: string;
	return_type: string;
};

export type ApiEnumValue = {
	name: string;
	value: number;
};

export type ApiEnum = {
	name: string;
	is_bitfield?: boolean;
	values?: ApiEnumValue[];
};

export type ApiConstant = {
	name: string;
	type?: string;
	value: number | string;
};

export type ApiSignal = {
	name: string;
	arguments?: ApiArgument[];
};

export type ApiBuiltinClass = {
	name: string;
	is_instantiable?: boolean;
	inherits?: string;
	constructors?: Array<{ arguments?: ApiArgument[] }>;
	methods?: ApiMethod[];
	operators?: ApiOperator[];
	members?: ApiArgument[];
	constants?: ApiConstant[];
	enums?: ApiEnum[];
	signals?: ApiSignal[];
};

export type ApiClass = {
	name: string;
	is_instantiable?: boolean;
	inherits?: string;
	methods?: ApiMethod[];
	constants?: ApiConstant[];
	enums?: ApiEnum[];
	signals?: ApiSignal[];
	members?: ApiArgument[];
	constructors?: Array<{ arguments?: ApiArgument[] }>;
};

export type ApiData = {
	header?: {
		version_major?: number;
		version_minor?: number;
		version_patch?: number;
		version_status?: string;
		version_build?: string;
		version_full_name?: string;
	};
	builtin_classes?: ApiBuiltinClass[];
	classes?: ApiClass[];
	global_enums?: ApiEnum[];
	utility_functions?: ApiMethod[];
	singletons?: ApiSingleton[];
};

export type ApiSingleton = {
	name: string;
	type: string;
};

export type ClassSource = "builtin" | "classes";
export type GenerateTarget = "classes" | "globalscope";

export type CliArgs = {
	input: string;
	output: string;
	godotVersionMacro: string;
	classNames: string[];
	propertyRulesJson: string;
	classSource: ClassSource;
	registerFunction: string;
	target: GenerateTarget;
};

export type ClassesConfig = {
	classes?: string[];
	source?: ClassSource;
	register_function?: string;
};

export type PropertyRuleKind = "member_expr" | "method" | "indexed_method";

export type PropertyRule = {
	class: string;
	name: string;
	type: string;
	kind: PropertyRuleKind;
	getter?: string;
	setter?: string;
	member_expr?: string;
	index?: number;
};

export type PropertyRulesConfig = {
	custom_properties?: PropertyRule[];
};

export type BoundPropertyBinding = {
	name: string;
	bindingLine: string;
	helperCode?: string;
};

export type HeaderAnalysis = {
	methodNameCounts: Map<string, number>;
	topLevelMembers: Set<string>;
	source: string;
};

export type ApiClassLike = ApiBuiltinClass | ApiClass;

export type ParsedApiType =
	| { kind: "void" }
	| { kind: "int" }
	| { kind: "float" }
	| { kind: "bool" }
	| { kind: "object" }
	| { kind: "enum_like" }
	| { kind: "typed_array"; base: string }
	| { kind: "typed_dictionary"; body: string }
	| { kind: "pointer"; value: string }
	| { kind: "named"; value: string };

export type GeneratedEnumBinding = {
	className: string;
	enumName: string;
	tagStructName: string;
	tagType: string;
	functionName: string;
	scriptTypeLine: string;
	registerCall: string;
	code: string;
};

export type GeneratedClassBinding = {
	className: string;
	classSource: ClassSource;
	functionName: string;
	code: string;
};
