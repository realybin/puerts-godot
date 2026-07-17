// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

type Documented = {
	description?: string;
};

export type ApiType = Documented & {
	type: string;
	meta?: string;
};

export type ApiArgument = ApiType & {
	name: string;
	default_value?: string;
};

export type ApiMethod = Documented & {
	name: string;
	is_static?: boolean;
	is_vararg?: boolean;
	return_type?: string;
	return_value?: ApiType;
	arguments?: ApiArgument[];
};

export type ApiOperator = Documented & {
	name: string;
	right_type?: string;
	return_type: string;
};

export type ApiProperty = ApiType & {
	name: string;
	setter?: string;
	getter?: string;
};

export type ApiConstant = Documented & {
	name: string;
	type?: string;
	meta?: string;
	value: number | string;
};

export type ApiEnumValue = Documented & {
	name: string;
	value: number;
};

export type ApiEnum = Documented & {
	name: string;
	is_bitfield?: boolean;
	values: ApiEnumValue[];
};

export type ApiSignal = Documented & {
	name: string;
	arguments?: ApiArgument[];
};

export type ApiClass = Documented & {
	name: string;
	brief_description?: string;
	inherits?: string;
	is_refcounted?: boolean;
	is_instantiable?: boolean;
	methods?: ApiMethod[];
	properties?: ApiProperty[];
	constants?: ApiConstant[];
	enums?: ApiEnum[];
	signals?: ApiSignal[];
};

export type ApiBuiltinClass = Documented & {
	name: string;
	brief_description?: string;
	constructors?: Array<Documented & { arguments?: ApiArgument[] }>;
	methods?: ApiMethod[];
	operators?: ApiOperator[];
	members?: Array<ApiType & { name: string }>;
	constants?: ApiConstant[];
	enums?: ApiEnum[];
};

export type ApiData = {
	header?: {
		version_major?: number;
		version_minor?: number;
		version_patch?: number;
		version_status?: string;
		version_build?: string;
		version_full_name?: string;
		precision?: string;
	};
	classes: ApiClass[];
	builtin_classes: ApiBuiltinClass[];
	global_constants: Array<Documented & { name: string; value: number }>;
	global_enums: ApiEnum[];
	utility_functions: ApiMethod[];
	singletons: ApiSingleton[];
};

export type ApiSingleton = Documented & {
	name: string;
	type: string;
};
