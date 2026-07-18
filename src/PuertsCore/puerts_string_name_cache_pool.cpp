// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_string_name_cache_pool.h"

#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

namespace {

template <typename Cache>
const CharString &find_or_cache_utf8(Cache &p_cache, const StringName &p_name, size_t p_capacity) {
	if (const auto existing = p_cache.find(p_name); existing != p_cache.end()) {
		return existing->second;
	}
	if (p_cache.size() >= p_capacity) {
		p_cache.clear();
	}
	return p_cache.insert({ p_name, String(p_name).utf8() }).first->second;
}

} // namespace

void PuertsStringNameCachePool::_bind_methods() {
	ClassDB::bind_method(D_METHOD("initialize", "policy", "capacity"), &PuertsStringNameCachePool::initialize, DEFVAL(POLICY_HASH_MAP), DEFVAL(512));
	ClassDB::bind_method(D_METHOD("is_initialized"), &PuertsStringNameCachePool::is_initialized);
	ClassDB::bind_method(D_METHOD("clear"), &PuertsStringNameCachePool::clear);
	ClassDB::bind_method(D_METHOD("get_policy"), &PuertsStringNameCachePool::get_policy);
	ClassDB::bind_method(D_METHOD("get_capacity"), &PuertsStringNameCachePool::get_capacity);

	BIND_ENUM_CONSTANT(POLICY_HASH_MAP);
	BIND_ENUM_CONSTANT(POLICY_FIXED_HASH_MAP);
	BIND_ENUM_CONSTANT(POLICY_NO_CACHE);
}

Error PuertsStringNameCachePool::initialize(Policy p_policy, int32_t p_capacity) {
	if (p_policy < POLICY_HASH_MAP || p_policy > POLICY_NO_CACHE) {
		return ERR_INVALID_PARAMETER;
	}
	reset_storage();
	policy_ = p_policy;
	capacity_ = MAX(1, p_capacity);
	if (policy_ == POLICY_FIXED_HASH_MAP) {
		capacity_ = FIXED_HASH_MAP_MAX_CAPACITY;
		fixed_hash_map_cache_ = memnew(FixedHashMap);
	}
	initialized_ = true;
	return OK;
}

PuertsStringNameCachePool::~PuertsStringNameCachePool() {
	reset_storage();
}

void PuertsStringNameCachePool::reset_storage() {
	if (hash_map_cache_ != nullptr) {
		memdelete(hash_map_cache_);
		hash_map_cache_ = nullptr;
	}
	if (fixed_hash_map_cache_ != nullptr) {
		memdelete(fixed_hash_map_cache_);
		fixed_hash_map_cache_ = nullptr;
	}
	no_cache_scratch_utf8_ = CharString();
}

bool PuertsStringNameCachePool::is_initialized() const {
	return initialized_;
}

void PuertsStringNameCachePool::clear() {
	if (hash_map_cache_ != nullptr) {
		hash_map_cache_->clear();
	}
	if (fixed_hash_map_cache_ != nullptr) {
		fixed_hash_map_cache_->clear();
	}
	no_cache_scratch_utf8_ = CharString();
}

PuertsStringNameCachePool::Policy PuertsStringNameCachePool::get_policy() const {
	return policy_;
}

int32_t PuertsStringNameCachePool::get_capacity() const {
	return capacity_;
}

const CharString &PuertsStringNameCachePool::get_cached_utf8(const StringName &p_name) {
	static const CharString empty_utf8;
	if (!initialized_ || p_name.is_empty()) {
		return empty_utf8;
	}

	switch (policy_) {
		case POLICY_HASH_MAP:
			if (hash_map_cache_ == nullptr) {
				hash_map_cache_ = memnew(HashMap);
				hash_map_cache_->reserve(static_cast<size_t>(capacity_));
			}
			return find_or_cache_utf8(*hash_map_cache_, p_name, static_cast<size_t>(capacity_));
		case POLICY_FIXED_HASH_MAP:
			return find_or_cache_utf8(*fixed_hash_map_cache_, p_name, static_cast<size_t>(capacity_));
		case POLICY_NO_CACHE:
			no_cache_scratch_utf8_ = String(p_name).utf8();
			return no_cache_scratch_utf8_;
	}

	return empty_utf8;
}
