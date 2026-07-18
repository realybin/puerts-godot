// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

export type MethodTypeOverride = {
	arguments?: Readonly<Record<string, string>>;
	returnType?: string;
};

export type MethodTypeOverrides = Readonly<Record<string, MethodTypeOverride>>;
