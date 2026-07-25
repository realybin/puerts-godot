// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_BRIDGE_REGISTRY_H
#define PUERTS_GODOT_PUERTS_BRIDGE_REGISTRY_H

#include "puerts_eastl.h"

#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/variant.hpp>

class PuertsBridgeRegistry {
public:
	struct ObjectBinding {
		void *handle = nullptr;
		const void *type_id = nullptr;
	};

	PuertsBridgeRegistry() = default;
	~PuertsBridgeRegistry();

	PuertsBridgeRegistry(const PuertsBridgeRegistry &) = delete;
	PuertsBridgeRegistry &operator=(const PuertsBridgeRegistry &) = delete;

	void clear();

	void *bind_object(godot::Object *p_object, const void *p_type_id);
	void *own_object(godot::Object *p_object, const void *p_type_id);
	void *box_variant(const godot::Variant &p_value, const void *p_type_id);

	[[nodiscard]] ObjectBinding find_object(godot::Object *p_object) const;
	[[nodiscard]] static bool is_handle(void *p_handle);
	[[nodiscard]] const godot::Variant *find_box(void *p_handle) const;
	bool try_get_variant(void *p_handle, const void *p_type_id, godot::Variant &r_value) const;
	bool update_box(void *p_handle, const godot::Variant &p_value);
	bool try_get_object(void *p_handle, godot::Object *&r_object) const;
	bool release(void *p_handle);

private:
	enum class Kind {
		Object,
		Variant,
	};

	struct Entry {
		Kind kind = Kind::Variant;
		bool script_owned = false;
		godot::ObjectID object_id;
		godot::Variant value;
		const void *type_id = nullptr;
	};

	using HandleId = uintptr_t;
	using EntryMap = puerts_eastl::hash_map<HandleId, Entry>;

	static constexpr uintptr_t HANDLE_TAG = 1;
	static constexpr HandleId MAX_HANDLE_ID = UINTPTR_MAX >> 1U;

	EntryMap entries_;
	puerts_eastl::hash_map<uint64_t, HandleId> object_entries_;
	// Handle IDs are never reused. Exhaustion is terminal, so stale tokens cannot become valid again.
	HandleId next_handle_id_ = 1;

	[[nodiscard]] static void *encode_handle(HandleId p_handle_id);
	static bool decode_handle(void *p_handle, HandleId &r_handle_id);
	HandleId take_handle_id();
	[[nodiscard]] Entry *find(void *p_handle);
	[[nodiscard]] const Entry *find(void *p_handle) const;
	void *store_object(godot::Object *p_object, const void *p_type_id, bool p_script_owned);
	[[nodiscard]] static godot::Object *resolve_object(const Entry &p_entry);
};

#endif // PUERTS_GODOT_PUERTS_BRIDGE_REGISTRY_H
