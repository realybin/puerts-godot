// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_STRING_NAME_UTF8_CACHE_H
#define PUERTS_GODOT_PUERTS_STRING_NAME_UTF8_CACHE_H

#include "puerts_eastl.h"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/string_name.hpp>

class PuertsStringNameUtf8Cache : public godot::RefCounted {
	GDCLASS(PuertsStringNameUtf8Cache, godot::RefCounted)

	struct StringNameHash {
		size_t operator()(const godot::StringName &p_name) const {
			return p_name.hash();
		}
	};

	struct StringNameEqual {
		bool operator()(const godot::StringName &p_left, const godot::StringName &p_right) const {
			return p_left == p_right;
		}
	};

	using HashMap = puerts_eastl::hash_map<godot::StringName, godot::CharString, StringNameHash, StringNameEqual, true>;
	HashMap cache_;

protected:
	static void _bind_methods();

public:
	[[nodiscard]] godot::CharString get_utf8(const godot::StringName &p_name);
	void clear();
};

#endif // PUERTS_GODOT_PUERTS_STRING_NAME_UTF8_CACHE_H
