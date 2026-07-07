// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_SCRIPT_VALUE_CACHE_H
#define PUERTS_GODOT_PUERTS_SCRIPT_VALUE_CACHE_H

#include "pesapi.h"
#include "puerts_eastl.h"

#include <godot_cpp/core/object_id.hpp>

namespace puerts::script_value_cache {

struct Entry {
	pesapi_value_ref value_ref = nullptr;
	godot::ObjectID wrapper_id;
};

inline bool can_attach_cache_entry(
		pesapi_ffi *p_ffi,
		pesapi_env p_env,
		pesapi_value p_value) {
	if (p_ffi == nullptr || p_env == nullptr || p_value == nullptr) {
		return false;
	}

	if (p_ffi->is_object(p_env, p_value)) {
		return true;
	}

	return p_ffi->get_native_object_ptr(p_env, p_value) != nullptr &&
			p_ffi->get_native_object_typeid(p_env, p_value) != nullptr;
}

inline puerts_eastl::hash_set<uintptr_t> &active_entry_ptrs() {
	static puerts_eastl::hash_set<uintptr_t> s_active_entries;
	return s_active_entries;
}

inline void register_entry(Entry *p_entry) {
	if (p_entry != nullptr) {
		active_entry_ptrs().insert(reinterpret_cast<uintptr_t>(p_entry));
	}
}

inline void unregister_entry(Entry *p_entry) {
	if (p_entry != nullptr) {
		active_entry_ptrs().erase(reinterpret_cast<uintptr_t>(p_entry));
	}
}

inline Entry *entry_from_raw_ptr(void *p_ptr) {
	if (p_ptr == nullptr) {
		return nullptr;
	}

	const auto raw = reinterpret_cast<uintptr_t>(p_ptr);
	return active_entry_ptrs().find(raw) != active_entry_ptrs().end() ? static_cast<Entry *>(p_ptr) : nullptr;
}

inline Entry *entry_from_value_ref(pesapi_ffi *p_ffi, pesapi_value_ref p_value_ref) {
	if (p_value_ref == nullptr) {
		return nullptr;
	}

	uint32_t field_count = 0;
	void **fields = p_ffi->get_ref_internal_fields(p_value_ref, &field_count);
	if (fields == nullptr || field_count < 1 || fields[0] == nullptr) {
		return nullptr;
	}

	return entry_from_raw_ptr(fields[0]);
}

inline void clear_value_ref_link(pesapi_ffi *p_ffi, pesapi_value_ref p_value_ref) {
	if (p_value_ref == nullptr) {
		return;
	}

	uint32_t field_count = 0;
	void **fields = p_ffi->get_ref_internal_fields(p_value_ref, &field_count);
	if (fields != nullptr && field_count >= 1) {
		fields[0] = nullptr;
	}
}

} // namespace puerts::script_value_cache

#endif // PUERTS_GODOT_PUERTS_SCRIPT_VALUE_CACHE_H
