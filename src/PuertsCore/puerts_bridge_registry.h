// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_BRIDGE_REGISTRY_H
#define PUERTS_GODOT_PUERTS_BRIDGE_REGISTRY_H

#include "puerts_eastl.h"

#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/variant.hpp>

class PuertsBridgeRegistry {
public:
	PuertsBridgeRegistry() = default;

	void clear();

	void *bind_object(godot::Object *p_object, const void *p_type_id);
	void *own_object(godot::Object *p_object, const void *p_type_id);
	void *box_variant(const godot::Variant &p_value, const void *p_type_id);

	bool find_object(godot::Object *p_object, void *&r_handle, const void *&r_type_id);
	[[nodiscard]] const godot::Variant *get_box(void *p_handle) const;
	bool get_variant(void *p_handle, const void *p_type_id, godot::Variant &r_value) const;
	bool set_box(void *p_handle, const godot::Variant &p_value);
	[[nodiscard]] bool is_object(void *p_handle) const;
	[[nodiscard]] godot::Object *get_object(void *p_handle) const;
	bool release(void *p_handle);

private:
	enum class Kind {
		Free,
		Object,
		Variant,
	};

	enum class Ownership {
		Borrowed,
		Script,
		Strong,
	};

	struct Entry {
		Kind kind = Kind::Free;
		Ownership ownership = Ownership::Borrowed;
		uint32_t handle_id = 0;
		godot::ObjectID object_id;
		uintptr_t object_ptr = 0;
		godot::Variant value;
		const void *type_id = nullptr;
	};

	struct ObjectKey {
		uint64_t id = 0;
		uintptr_t ptr = 0;

		bool operator==(const ObjectKey &p_other) const {
			return id == p_other.id && ptr == p_other.ptr;
		}
	};

	struct ObjectKeyHash {
		size_t operator()(const ObjectKey &p_key) const {
			const uint64_t mixed = p_key.id ^ (static_cast<uint64_t>(p_key.ptr) * 0x9e3779b97f4a7c15ULL);
			return static_cast<size_t>(mixed);
		}
	};

	puerts_eastl::vector<Entry> entries_;
	puerts_eastl::vector<uint32_t> free_slots_;
	puerts_eastl::hash_map<uint32_t, uint32_t> handle_slots_;
	puerts_eastl::hash_map<ObjectKey, uint32_t, ObjectKeyHash> object_slots_;
	uint32_t next_handle_id_ = 1;

	[[nodiscard]] static void *make_handle(uint32_t p_handle_id);
	static bool parse_handle(void *p_handle, uint32_t &r_handle_id);
	[[nodiscard]] static ObjectKey key_for(const Entry &p_entry);
	[[nodiscard]] static ObjectKey key_for(godot::Object *p_object);

	uint32_t allocate();
	Entry *find(void *p_handle, uint32_t *r_index = nullptr);
	const Entry *find(void *p_handle, uint32_t *r_index = nullptr) const;
	void bind_index(const Entry &p_entry, uint32_t p_index);
	void unbind_index(const Entry &p_entry);
	void release_slot(uint32_t p_index);
	void *store_object(godot::Object *p_object, const void *p_type_id, Ownership p_ownership);
	[[nodiscard]] godot::Object *object_from(const Entry &p_entry) const;
	[[nodiscard]] godot::Variant variant_from(const Entry &p_entry) const;
};

#endif // PUERTS_GODOT_PUERTS_BRIDGE_REGISTRY_H
