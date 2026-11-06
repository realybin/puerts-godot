// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: Apache-2.0

export type MethodTypeOverride = {
	arguments?: Readonly<Record<string, string>>;
	returnType?: string;
	varargsType?: string;
};

export type MethodTypeOverrides = Readonly<Record<string, MethodTypeOverride>>;
