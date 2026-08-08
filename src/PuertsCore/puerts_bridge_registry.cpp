// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_bridge_registry.h"

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>

using namespace godot;

PuertsBridgeRegistry::~PuertsBridgeRegistry() {
	clear();
}

bool PuertsBridgeRegistry::is_handle(void *p_handle) {
	return (reinterpret_cast<uintptr_t>(p_handle) & HANDLE_TAG) != 0;
}

void *PuertsBridgeRegistry::encode_handle(HandleId p_handle_id) {
	return reinterpret_cast<void *>((p_handle_id << 1U) | HANDLE_TAG);
}

bool PuertsBridgeRegistry::decode_handle(void *p_handle, HandleId &r_handle_id) {
	const uintptr_t raw = reinterpret_cast<uintptr_t>(p_handle);
	if ((raw & HANDLE_TAG) == 0) {
		return false;
	}

	r_handle_id = raw >> 1U;
	return r_handle_id != 0;
}

PuertsBridgeRegistry::HandleId PuertsBridgeRegistry::take_handle_id() {
	if (next_handle_id_ > MAX_HANDLE_ID) {
		return 0;
	}
	return next_handle_id_++;
}

PuertsBridgeRegistry::Entry *PuertsBridgeRegistry::find_entry(void *p_handle) {
	return const_cast<Entry *>(static_cast<const PuertsBridgeRegistry *>(this)->find_entry(p_handle));
}

const PuertsBridgeRegistry::Entry *PuertsBridgeRegistry::find_entry(void *p_handle) const {
	HandleId handle_id = 0;
	if (!decode_handle(p_handle, handle_id)) {
		return nullptr;
	}
	const auto found = entries_.find(handle_id);
	return found == entries_.end() ? nullptr : &found->second;
}

Object *PuertsBridgeRegistry::resolve_object(const Entry &p_entry) {
	if (p_entry.kind != Kind::Object) {
		return nullptr;
	}
	if (p_entry.object_id.is_ref_counted()) {
		return p_entry.value;
	}
	return ObjectDB::get_instance(p_entry.object_id);
}

void *PuertsBridgeRegistry::store_object(Object *p_object, const void *p_type_id, bool p_script_owned) {
	if (p_object == nullptr) {
		return nullptr;
	}

	const uint64_t object_id_value = p_object->get_instance_id();
	if (const auto existing = object_entries_.find(object_id_value); existing != object_entries_.end()) {
		const HandleId existing_handle_id = existing->second.handle_id;
		Entry *existing_entry = find_entry(encode_handle(existing_handle_id));
		if (existing_entry != nullptr && resolve_object(*existing_entry) == p_object) {
			return encode_handle(existing_handle_id);
		}

		// ObjectDB instance IDs can be reused after an object is freed. Do not
		// let a stale index make the new object inherit an unrelated handle.
		if (existing_entry != nullptr) {
			entries_.erase(existing_handle_id);
		}
		object_entries_.erase(existing);
	}

	const HandleId handle_id = take_handle_id();
	if (handle_id == 0) {
		return nullptr;
	}
	Entry &entry = entries_[handle_id];
	entry.kind = Kind::Object;
	entry.object_id = ObjectID(object_id_value);
	entry.script_owned = p_script_owned && !entry.object_id.is_ref_counted();
	entry.value = entry.object_id.is_ref_counted() ? Variant(p_object) : Variant();
	entry.type_id = p_type_id;
	object_entries_.insert({ object_id_value, { handle_id, p_type_id } });
	return encode_handle(handle_id);
}

void PuertsBridgeRegistry::remove_object_entry(HandleId p_handle_id, const Entry &p_entry) {
	if (p_entry.kind == Kind::Object) {
		const auto object_entry = object_entries_.find(static_cast<uint64_t>(p_entry.object_id));
		if (object_entry != object_entries_.end() && object_entry->second.handle_id == p_handle_id) {
			object_entries_.erase(object_entry);
		}
	}
}

void PuertsBridgeRegistry::clear() {
	puerts_eastl::vector<Object *> script_owned_objects;
	for (const auto &item : entries_) {
		const Entry &entry = item.second;
		if (entry.kind == Kind::Object && entry.script_owned) {
			if (Object *object = resolve_object(entry); object != nullptr) {
				script_owned_objects.push_back(object);
			}
		}
	}

	EntryMap released_entries;
	released_entries.swap(entries_);
	object_entries_.clear();
	released_entries.clear();

	for (Object *object : script_owned_objects) {
		memdelete(object);
	}
}

void *PuertsBridgeRegistry::bind_object(Object *p_object, const void *p_type_id) {
	return store_object(p_object, p_type_id, false);
}

void *PuertsBridgeRegistry::own_object(Object *p_object, const void *p_type_id) {
	return store_object(p_object, p_type_id, true);
}

void *PuertsBridgeRegistry::box_variant(const Variant &p_value, const void *p_type_id) {
	const HandleId handle_id = take_handle_id();
	if (handle_id == 0) {
		return nullptr;
	}
	Entry &entry = entries_[handle_id];
	entry.kind = Kind::Variant;
	entry.value = p_value;
	entry.type_id = p_type_id;
	return encode_handle(handle_id);
}

PuertsBridgeRegistry::ObjectBinding PuertsBridgeRegistry::find_object(Object *p_object) const {
	if (p_object == nullptr) {
		return {};
	}
	const auto found = object_entries_.find(static_cast<uint64_t>(p_object->get_instance_id()));
	if (found == object_entries_.end()) {
		return {};
	}
	return { encode_handle(found->second.handle_id), found->second.type_id };
}

const Variant *PuertsBridgeRegistry::find_box(void *p_handle) const {
	const Entry *entry = find_entry(p_handle);
	return entry != nullptr && entry->kind == Kind::Variant ? &entry->value : nullptr;
}

bool PuertsBridgeRegistry::try_get_variant(void *p_handle, const void *p_type_id, Variant &r_value) const {
	const Entry *entry = find_entry(p_handle);
	if (entry == nullptr || entry->type_id != p_type_id) {
		return false;
	}
	if (entry->kind == Kind::Object && !entry->object_id.is_ref_counted()) {
		r_value = resolve_object(*entry);
	} else {
		r_value = entry->value;
	}
	return true;
}

bool PuertsBridgeRegistry::update_box(void *p_handle, const Variant &p_value) {
	Entry *entry = find_entry(p_handle);
	if (entry == nullptr || entry->kind != Kind::Variant) {
		return false;
	}
	entry->value = p_value;
	return true;
}

bool PuertsBridgeRegistry::try_get_object(void *p_handle, Object *&r_object) const {
	const Entry *entry = find_entry(p_handle);
	if (entry == nullptr || entry->kind != Kind::Object) {
		return false;
	}
	r_object = resolve_object(*entry);
	return true;
}

bool PuertsBridgeRegistry::release(void *p_handle) {
	HandleId handle_id = 0;
	if (!decode_handle(p_handle, handle_id)) {
		return false;
	}
	const auto found = entries_.find(handle_id);
	if (found == entries_.end()) {
		return false;
	}

	Entry &entry = found->second;
	Object *script_owned_object = entry.script_owned ? resolve_object(entry) : nullptr;
	remove_object_entry(handle_id, entry);
	entries_.erase(found);
	if (script_owned_object != nullptr) {
		memdelete(script_owned_object);
	}
	return true;
}
