// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

export type Context = {
	knownClassNames: Set<string>;
	knownBuiltinNames: Set<string>;
	emittedBuiltinNames: Set<string>;
};
