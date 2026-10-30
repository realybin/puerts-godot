// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_ADDON_H
#define PUERTS_GODOT_PUERTS_ADDON_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>

class PuertsAddon : public godot::RefCounted {
	GDCLASS(PuertsAddon, godot::RefCounted)

protected:
	static void _bind_methods();

public:
	static godot::Error load(const godot::String &p_path, const godot::String &p_module_name);
};

#endif // PUERTS_GODOT_PUERTS_ADDON_H
