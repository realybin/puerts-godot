// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_string_name_utf8_cache.h"

#include <godot_cpp/variant/string.hpp>

using namespace godot;

void PuertsStringNameUtf8Cache::_bind_methods() {
	ClassDB::bind_method(D_METHOD("clear"), &PuertsStringNameUtf8Cache::clear);
}

CharString PuertsStringNameUtf8Cache::get_utf8(const StringName &p_name) {
	if (p_name.is_empty()) {
		return {};
	}
	if (const auto existing = cache_.find(p_name); existing != cache_.end()) {
		return existing->second;
	}
	return cache_.insert({ p_name, String(p_name).utf8() }).first->second;
}

void PuertsStringNameUtf8Cache::clear() {
	cache_.clear();
}
