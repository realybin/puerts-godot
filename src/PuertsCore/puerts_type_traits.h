// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_TYPE_TRAITS_H
#define PUERTS_GODOT_PUERTS_TYPE_TRAITS_H

#include "puerts_eastl.h"

#include <godot_cpp/variant/string_name.hpp>

namespace puerts {

template <typename T, typename = void>
struct script_type_name;

template <typename T>
struct script_type_name<T, eastl::void_t<decltype(T::get_class_static())>> {
	static godot::StringName value() {
		return T::get_class_static();
	}
};

template <typename T>
struct static_type_id {
	static const void *get() {
		static int type_tag = 0;
		return &type_tag;
	}
};

} // namespace puerts

#endif // PUERTS_GODOT_PUERTS_TYPE_TRAITS_H
