// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_QUICKJS_BACKEND_H
#define PUERTS_GODOT_PUERTS_QUICKJS_BACKEND_H

#include "PuertsCore/puerts_backend_resource.h"

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/core/class_db.hpp>

class PuertsQuickjsBackend : public godot::Resource {
	GDCLASS(PuertsQuickjsBackend, godot::Resource)

protected:
	static void _bind_methods();

public:
	[[nodiscard]] bool is_available() const;
	[[nodiscard]] godot::StringName get_backend_id() const;
	[[nodiscard]] godot::String get_backend_name() const;
	[[nodiscard]] godot::StringName get_language_id() const;
	[[nodiscard]] bool supports_tick() const;
	[[nodiscard]] bool supports_debugger() const;
	[[nodiscard]] bool supports_low_memory_notification() const;
	[[nodiscard]] bool supports_terminate_execution() const;
	[[nodiscard]] uint64_t _puerts_get_functions_ptr() const;
};

#endif // PUERTS_GODOT_PUERTS_QUICKJS_BACKEND_H
