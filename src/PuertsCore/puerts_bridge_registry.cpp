// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_bridge_registry.h"

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/object.hpp>

using namespace godot;

void *PuertsBridgeRegistry::make_handle(uint32_t p_handle_id) {
	return reinterpret_cast<void *>(static_cast<uintptr_t>(p_handle_id));
}

bool PuertsBridgeRegistry::parse_handle(void *p_handle, uint32_t &r_handle_id) {
	const auto raw = reinterpret_cast<uintptr_t>(p_handle);
	if (raw == 0 || raw > UINT32_MAX) {
		return false;
	}
	r_handle_id = static_cast<uint32_t>(raw);
	return true;
}

PuertsBridgeRegistry::ObjectKey PuertsBridgeRegistry::key_for(const Entry &p_entry) {
	return ObjectKey{
		static_cast<uint64_t>(p_entry.object_id),
		p_entry.object_ptr,
	};
}

PuertsBridgeRegistry::ObjectKey PuertsBridgeRegistry::key_for(Object *p_object) {
	return ObjectKey{
		static_cast<uint64_t>(p_object->get_instance_id()),
		reinterpret_cast<uintptr_t>(p_object),
	};
}

uint32_t PuertsBridgeRegistry::allocate() {
	if (!free_slots_.empty()) {
		const uint32_t index = free_slots_.back();
		free_slots_.pop_back();
		return index;
	}

	entries_.push_back(Entry());
	return static_cast<uint32_t>(entries_.size() - 1);
}

PuertsBridgeRegistry::Entry *PuertsBridgeRegistry::find(void *p_handle, uint32_t *r_index) {
	uint32_t handle_id = 0;
	if (!parse_handle(p_handle, handle_id)) {
		return nullptr;
	}

	auto found = handle_slots_.find(handle_id);
	if (found == handle_slots_.end()) {
		return nullptr;
	}

	const uint32_t index = found->second;
	if (index >= entries_.size()) {
		return nullptr;
	}
	Entry &entry = entries_[index];
	if (entry.kind == Kind::Free || entry.handle_id != handle_id) {
		return nullptr;
	}

	if (r_index != nullptr) {
		*r_index = index;
	}
	return &entry;
}

const PuertsBridgeRegistry::Entry *PuertsBridgeRegistry::find(void *p_handle, uint32_t *r_index) const {
	uint32_t handle_id = 0;
	if (!parse_handle(p_handle, handle_id)) {
		return nullptr;
	}

	auto found = handle_slots_.find(handle_id);
	if (found == handle_slots_.end()) {
		return nullptr;
	}

	const uint32_t index = found->second;
	if (index >= entries_.size()) {
		return nullptr;
	}
	const Entry &entry = entries_[index];
	if (entry.kind == Kind::Free || entry.handle_id != handle_id) {
		return nullptr;
	}

	if (r_index != nullptr) {
		*r_index = index;
	}
	return &entry;
}

void PuertsBridgeRegistry::bind_index(const Entry &p_entry, uint32_t p_index) {
	if (p_entry.kind == Kind::Object) {
		object_slots_[key_for(p_entry)] = p_index;
	}
}

void PuertsBridgeRegistry::unbind_index(const Entry &p_entry) {
	if (p_entry.kind == Kind::Object) {
		object_slots_.erase(key_for(p_entry));
	}
}

void PuertsBridgeRegistry::release_slot(uint32_t p_index) {
	Entry &entry = entries_[p_index];
	unbind_index(entry);
	if (entry.handle_id != 0) {
		handle_slots_.erase(entry.handle_id);
	}
	entry.kind = Kind::Free;
	entry.ownership = Ownership::Borrowed;
	entry.handle_id = 0;
	entry.object_id = ObjectID();
	entry.object_ptr = 0;
	entry.value = Variant();
	entry.type_id = nullptr;
	free_slots_.push_back(p_index);
}

Object *PuertsBridgeRegistry::object_from(const Entry &p_entry) const {
	if (p_entry.kind != Kind::Object) {
		return nullptr;
	}

	if (p_entry.ownership == Ownership::Strong) {
		return p_entry.value;
	}

	Object *object = ObjectDB::get_instance(p_entry.object_id);
	if (reinterpret_cast<uintptr_t>(object) == p_entry.object_ptr) {
		return object;
	}
	return nullptr;
}

Variant PuertsBridgeRegistry::variant_from(const Entry &p_entry) const {
	if (p_entry.kind == Kind::Variant || p_entry.ownership == Ownership::Strong) {
		return p_entry.value;
	}

	if (p_entry.kind == Kind::Object) {
		return object_from(p_entry);
	}

	return {};
}

void *PuertsBridgeRegistry::store_object(Object *p_object, const void *p_type_id, Ownership p_ownership) {
	void *handle = nullptr;
	const void *bound_type = nullptr;
	if (find_object(p_object, handle, bound_type)) {
		return handle;
	}

	const uint32_t index = allocate();
	Entry &entry = entries_[index];
	const uint32_t handle_id = next_handle_id_++;
	ERR_FAIL_COND_V(handle_id == 0, nullptr);
	entry.kind = Kind::Object;
	entry.ownership = Object::cast_to<RefCounted>(p_object) != nullptr ? Ownership::Strong : p_ownership;
	entry.handle_id = handle_id;
	entry.object_id = p_object->get_instance_id();
	entry.object_ptr = reinterpret_cast<uintptr_t>(p_object);
	entry.value = entry.ownership == Ownership::Strong ? Variant(p_object) : Variant();
	entry.type_id = p_type_id;
	handle_slots_[handle_id] = index;
	bind_index(entry, index);
	return make_handle(handle_id);
}

void PuertsBridgeRegistry::clear() {
	puerts_eastl::vector<Object *> script_objects;
	for (const Entry &entry : entries_) {
		if (entry.kind == Kind::Object && entry.ownership == Ownership::Script) {
			if (Object *object = object_from(entry); object != nullptr) {
				script_objects.push_back(object);
			}
		}
	}

	entries_.clear();
	free_slots_.clear();
	handle_slots_.clear();
	object_slots_.clear();
	next_handle_id_ = 1;

	for (Object *object : script_objects) {
		memdelete(object);
	}
}

void *PuertsBridgeRegistry::bind_object(Object *p_object, const void *p_type_id) {
	return p_object != nullptr ? store_object(p_object, p_type_id, Ownership::Borrowed) : nullptr;
}

void *PuertsBridgeRegistry::own_object(Object *p_object, const void *p_type_id) {
	return p_object != nullptr ? store_object(p_object, p_type_id, Ownership::Script) : nullptr;
}

void *PuertsBridgeRegistry::box_variant(const Variant &p_value, const void *p_type_id) {
	const uint32_t index = allocate();
	Entry &entry = entries_[index];
	entry.kind = Kind::Variant;
	entry.ownership = Ownership::Strong;
	entry.handle_id = next_handle_id_++;
	ERR_FAIL_COND_V(entry.handle_id == 0, nullptr);
	entry.object_id = ObjectID();
	entry.object_ptr = 0;
	entry.value = p_value;
	entry.type_id = p_type_id;
	handle_slots_[entry.handle_id] = index;
	return make_handle(entry.handle_id);
}

bool PuertsBridgeRegistry::find_object(Object *p_object, void *&r_handle, const void *&r_type_id) {
	r_handle = nullptr;
	r_type_id = nullptr;
	if (p_object == nullptr) {
		return false;
	}

	auto found = object_slots_.find(key_for(p_object));
	if (found == object_slots_.end()) {
		return false;
	}

	const uint32_t index = found->second;
	const Entry &entry = entries_[index];
	r_handle = make_handle(entry.handle_id);
	r_type_id = entry.type_id;
	return true;
}

const Variant *PuertsBridgeRegistry::get_box(void *p_handle) const {
	const Entry *entry = find(p_handle);
	return entry != nullptr && entry->kind == Kind::Variant ? &entry->value : nullptr;
}

bool PuertsBridgeRegistry::get_variant(void *p_handle, const void *p_type_id, Variant &r_value) const {
	const Entry *entry = find(p_handle);
	if (entry == nullptr || entry->type_id != p_type_id) {
		return false;
	}

	r_value = variant_from(*entry);
	return true;
}

bool PuertsBridgeRegistry::set_box(void *p_handle, const Variant &p_value) {
	Entry *entry = find(p_handle);
	if (entry == nullptr || entry->kind != Kind::Variant) {
		return false;
	}

	entry->value = p_value;
	return true;
}

bool PuertsBridgeRegistry::is_object(void *p_handle) const {
	const Entry *entry = find(p_handle);
	return entry != nullptr && entry->kind == Kind::Object;
}

Object *PuertsBridgeRegistry::get_object(void *p_handle) const {
	const Entry *entry = find(p_handle);
	return entry != nullptr ? object_from(*entry) : nullptr;
}

bool PuertsBridgeRegistry::release(void *p_handle) {
	uint32_t index = 0;
	Entry *entry = find(p_handle, &index);
	if (entry == nullptr) {
		return false;
	}

	Object *script_object = entry->ownership == Ownership::Script ? object_from(*entry) : nullptr;
	release_slot(index);
	if (script_object != nullptr) {
		memdelete(script_object);
	}
	return true;
}
