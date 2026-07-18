// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_bridge_registry.h"

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/object.hpp>

using namespace godot;

void *PuertsBridgeRegistry::make_handle(uint32_t p_handle_id) {
	return reinterpret_cast<void *>((static_cast<uintptr_t>(p_handle_id) << 1U) | 1U);
}

bool PuertsBridgeRegistry::parse_handle(void *p_handle, uint32_t &r_handle_id) {
	const auto raw = reinterpret_cast<uintptr_t>(p_handle);
	if ((raw & 1U) == 0 || raw > UINT32_MAX) {
		return false;
	}
	r_handle_id = static_cast<uint32_t>(raw >> 1U);
	return true;
}

bool PuertsBridgeRegistry::is_handle(void *p_handle) {
	uint32_t handle_id = 0;
	return parse_handle(p_handle, handle_id);
}

uint32_t PuertsBridgeRegistry::take_handle_id() {
	ERR_FAIL_COND_V(next_handle_id_ > (UINT32_MAX >> 1U), 0);
	return next_handle_id_++;
}

uint32_t PuertsBridgeRegistry::allocate() {
	if (!free_slots_.empty()) {
		const uint32_t index = free_slots_.back();
		free_slots_.pop_back();
		return index;
	}

	entries_.emplace_back();
	return static_cast<uint32_t>(entries_.size() - 1);
}

bool PuertsBridgeRegistry::find_index(void *p_handle, uint32_t &r_index) const {
	uint32_t handle_id = 0;
	if (!parse_handle(p_handle, handle_id)) {
		return false;
	}

	const auto found = handle_slots_.find(handle_id);
	if (found == handle_slots_.end()) {
		return false;
	}
	r_index = found->second;
	return true;
}

PuertsBridgeRegistry::Entry *PuertsBridgeRegistry::find(void *p_handle, uint32_t *r_index) {
	uint32_t index = 0;
	if (!find_index(p_handle, index)) {
		return nullptr;
	}
	if (r_index != nullptr) {
		*r_index = index;
	}
	return &entries_[index];
}

const PuertsBridgeRegistry::Entry *PuertsBridgeRegistry::find(void *p_handle, uint32_t *r_index) const {
	uint32_t index = 0;
	if (!find_index(p_handle, index)) {
		return nullptr;
	}
	if (r_index != nullptr) {
		*r_index = index;
	}
	return &entries_[index];
}

void PuertsBridgeRegistry::release_slot(uint32_t p_index) {
	Entry &entry = entries_[p_index];
	// Keep referenced values alive until all registry mutations are complete. Their
	// destructors may run user code that disposes the owning environment.
	Variant released_value = eastl::move(entry.value);
	if (entry.kind == Kind::Object) {
		object_slots_.erase(static_cast<uint64_t>(entry.object_id));
	}
	handle_slots_.erase(entry.handle_id);
	entry = Entry();
	free_slots_.push_back(p_index);
}

Object *PuertsBridgeRegistry::object_from(const Entry &p_entry) {
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

	void *handle = nullptr;
	const void *bound_type = nullptr;
	if (find_object(p_object, handle, bound_type)) {
		return handle;
	}

	const uint32_t handle_id = take_handle_id();
	ERR_FAIL_COND_V(handle_id == 0, nullptr);
	const uint32_t index = allocate();
	Entry &entry = entries_[index];
	const ObjectID object_id(p_object->get_instance_id());
	const bool is_ref_counted = object_id.is_ref_counted();
	entry.kind = Kind::Object;
	entry.handle_id = handle_id;
	entry.object_id = object_id;
	entry.script_owned = p_script_owned && !is_ref_counted;
	entry.value = is_ref_counted ? Variant(p_object) : Variant();
	entry.type_id = p_type_id;
	handle_slots_.insert({ handle_id, index });
	object_slots_.insert({ static_cast<uint64_t>(object_id), index });
	return make_handle(handle_id);
}

void PuertsBridgeRegistry::clear() {
	puerts_eastl::vector<Object *> script_objects;
	for (const Entry &entry : entries_) {
		if (entry.kind == Kind::Object && entry.script_owned) {
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
	return store_object(p_object, p_type_id, false);
}

void *PuertsBridgeRegistry::own_object(Object *p_object, const void *p_type_id) {
	return store_object(p_object, p_type_id, true);
}

void *PuertsBridgeRegistry::box_variant(const Variant &p_value, const void *p_type_id) {
	const uint32_t handle_id = take_handle_id();
	ERR_FAIL_COND_V(handle_id == 0, nullptr);
	const uint32_t index = allocate();
	Entry &entry = entries_[index];
	entry.kind = Kind::Variant;
	entry.handle_id = handle_id;
	entry.value = p_value;
	entry.type_id = p_type_id;
	handle_slots_.insert({ handle_id, index });
	return make_handle(handle_id);
}

bool PuertsBridgeRegistry::find_object(Object *p_object, void *&r_handle, const void *&r_type_id) const {
	r_handle = nullptr;
	r_type_id = nullptr;
	if (p_object == nullptr) {
		return false;
	}

	const auto found = object_slots_.find(static_cast<uint64_t>(p_object->get_instance_id()));
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

	if (entry->kind == Kind::Object && !entry->object_id.is_ref_counted()) {
		r_value = object_from(*entry);
	} else {
		r_value = entry->value;
	}
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

bool PuertsBridgeRegistry::get_object(void *p_handle, Object *&r_object) const {
	const Entry *entry = find(p_handle);
	if (entry == nullptr || entry->kind != Kind::Object) {
		return false;
	}
	r_object = object_from(*entry);
	return true;
}

bool PuertsBridgeRegistry::release(void *p_handle) {
	uint32_t index = 0;
	Entry *entry = find(p_handle, &index);
	if (entry == nullptr) {
		return false;
	}

	Object *script_object = entry->script_owned ? object_from(*entry) : nullptr;
	release_slot(index);
	if (script_object != nullptr) {
		memdelete(script_object);
	}
	return true;
}
