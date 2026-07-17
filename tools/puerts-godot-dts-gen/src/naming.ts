// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import { RESERVED } from "./constants.js";

const VARIANT_TYPE_NAMES = [
	"Nil",
	"bool",
	"int",
	"float",
	"String",
	"Vector2",
	"Vector2i",
	"Rect2",
	"Rect2i",
	"Vector3",
	"Vector3i",
	"Transform2D",
	"Vector4",
	"Vector4i",
	"Plane",
	"Quaternion",
	"AABB",
	"Basis",
	"Transform3D",
	"Projection",
	"Color",
	"StringName",
	"NodePath",
	"RID",
	"Object",
	"Callable",
	"Signal",
	"Dictionary",
	"Array",
	"PackedByteArray",
	"PackedInt32Array",
	"PackedInt64Array",
	"PackedFloat32Array",
	"PackedFloat64Array",
	"PackedStringArray",
	"PackedVector2Array",
	"PackedVector3Array",
	"PackedColorArray",
	"PackedVector4Array",
];

export function cleanTypeName(input: string): string {
	const typeName = input.trim().replace(/^const\s+/, "").replace(/\s*&$/, "");
	const encoded = /^(\d+)\/\d+:(.*)$/.exec(typeName);
	if (!encoded) {
		return typeName;
	}
	return encoded[2] || VARIANT_TYPE_NAMES[Number(encoded[1])] || "Variant";
}

export function splitUnionCandidates(typeName: string): string[] {
	const raw = typeName
		.split(",")
		.map((s) => s.trim())
		.filter(Boolean);
	const includes = raw.filter((s) => !s.startsWith("-"));
	return includes.length > 0 ? includes : raw.map((s) => s.replace(/^-/, ""));
}

export function sanitizeIdentifier(name: string): string {
	let n = name.trim();
	if (!/^[$A-Za-z_][$\w]*$/.test(n)) {
		n = n.replace(/[^$A-Za-z0-9_]/g, "_");
		if (!/^[$A-Za-z_]/.test(n)) {
			n = `_${n}`;
		}
	}
	return RESERVED.has(n) ? `${n}_` : n;
}
