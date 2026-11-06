// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: Apache-2.0

import type { ApiData } from "./api-types.js";

export function resolveApiVersion(api: ApiData): string {
	if (api.header?.version_full_name) {
		return api.header.version_full_name;
	}

	if (typeof api.header?.version_major === "number" && typeof api.header.version_minor === "number" && typeof api.header.version_patch === "number") {
		return `Godot Engine v${api.header.version_major}.${api.header.version_minor}.${api.header.version_patch}`;
	}

	return "unknown";
}
