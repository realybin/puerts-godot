// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

export type ApiType = {
	type: string;
	meta?: string;
};

export type ApiArgument = ApiType & {
	name: string;
	default_value?: string;
};

export type ApiMethod = {
	name: string;
	is_static?: boolean;
	is_vararg?: boolean;
	return_type?: string;
	return_value?: ApiType;
	arguments?: ApiArgument[];
};

export type ApiOperator = {
	name: string;
	right_type?: string;
	return_type: string;
};

export type ApiProperty = ApiType & {
	name: string;
	setter?: string;
	getter?: string;
};

export type ApiConstant = {
	name: string;
	type?: string;
	meta?: string;
	value: number | string;
};

export type ApiEnumValue = {
	name: string;
	value: number;
};

export type ApiEnum = {
	name: string;
	is_bitfield?: boolean;
	values: ApiEnumValue[];
};

export type ApiSignal = {
	name: string;
	arguments?: ApiArgument[];
};

export type ApiClass = {
	name: string;
	inherits?: string;
	is_instantiable?: boolean;
	methods?: ApiMethod[];
	properties?: ApiProperty[];
	constants?: ApiConstant[];
	enums?: ApiEnum[];
	signals?: ApiSignal[];
};

export type ApiBuiltinClass = {
	name: string;
	constructors?: Array<{ arguments?: ApiArgument[] }>;
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
	global_constants: Array<{ name: string; value: number }>;
	global_enums: ApiEnum[];
	utility_functions: ApiMethod[];
	singletons: ApiSingleton[];
};

export type ApiSingleton = {
	name: string;
	type: string;
};
