# SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
# SPDX-License-Identifier: BSD-3-Clause

extends RefCounted

const TestSupport = preload("res://tests/support/puerts_support.gd")

static func collect_available_backends() -> Array:
	var available: Array = []
	for backend_info in TestSupport.iter_backends():
		var backend_name: String = backend_info["name"]
		if not TestSupport.backend_supported_on_current_platform(backend_name):
			continue
		if not ClassDB.class_exists(backend_info["class_name"]):
			continue

		var backend = TestSupport.create_backend(backend_info["class_name"])
		if backend == null:
			continue

		available.append({
			"info": backend_info,
			"backend": backend,
		})
	return available
