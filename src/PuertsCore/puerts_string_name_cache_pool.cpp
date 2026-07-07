// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_string_name_cache_pool.h"

#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

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
	policy_ = p_policy;
	capacity_ = MAX(1, p_capacity);
	if (policy_ == POLICY_FIXED_HASH_MAP) {
		capacity_ = FIXED_HASH_MAP_MAX_CAPACITY;
	}
	initialized_ = true;
	if (policy_ == POLICY_HASH_MAP) {
		if (hash_map_cache_ != nullptr) {
			memdelete(hash_map_cache_);
			hash_map_cache_ = nullptr;
		}
		if (fixed_hash_map_cache_ != nullptr) {
			memdelete(fixed_hash_map_cache_);
			fixed_hash_map_cache_ = nullptr;
		}
	} else if (policy_ == POLICY_FIXED_HASH_MAP) {
		if (hash_map_cache_ != nullptr) {
			memdelete(hash_map_cache_);
			hash_map_cache_ = nullptr;
		}
		if (fixed_hash_map_cache_ == nullptr) {
			fixed_hash_map_cache_ = memnew(FixedHashMap);
		}
		fixed_hash_map_cache_->clear();
	} else {
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
	return OK;
}

PuertsStringNameCachePool::~PuertsStringNameCachePool() {
	if (hash_map_cache_ != nullptr) {
		memdelete(hash_map_cache_);
		hash_map_cache_ = nullptr;
	}
	if (fixed_hash_map_cache_ != nullptr) {
		memdelete(fixed_hash_map_cache_);
		fixed_hash_map_cache_ = nullptr;
	}
	initialized_ = false;
}

bool PuertsStringNameCachePool::is_initialized() const {
	return initialized_;
}

void PuertsStringNameCachePool::clear() {
	if (!initialized_) {
		return;
	}

	if (policy_ == POLICY_HASH_MAP) {
		if (hash_map_cache_ != nullptr) {
			memdelete(hash_map_cache_);
			hash_map_cache_ = nullptr;
		}
		if (fixed_hash_map_cache_ != nullptr) {
			memdelete(fixed_hash_map_cache_);
			fixed_hash_map_cache_ = nullptr;
		}
		return;
	}
	if (policy_ == POLICY_FIXED_HASH_MAP) {
		if (hash_map_cache_ != nullptr) {
			memdelete(hash_map_cache_);
			hash_map_cache_ = nullptr;
		}
		if (fixed_hash_map_cache_ == nullptr) {
			fixed_hash_map_cache_ = memnew(FixedHashMap);
		}
		fixed_hash_map_cache_->clear();
		return;
	}
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

	if (policy_ == POLICY_HASH_MAP) {
		if (hash_map_cache_ == nullptr) {
			hash_map_cache_ = memnew(HashMap);
			hash_map_cache_->reserve(static_cast<size_t>(capacity_));
		}
		if (auto existing = hash_map_cache_->find(p_name); existing != hash_map_cache_->end()) {
			return existing->second;
		}
		if (hash_map_cache_->size() >= static_cast<size_t>(capacity_)) {
			hash_map_cache_->clear();
			hash_map_cache_->reserve(static_cast<size_t>(capacity_));
		}
		auto inserted = hash_map_cache_->insert({ p_name, String(p_name).utf8() });
		return inserted.first->second;
	}
	if (policy_ == POLICY_FIXED_HASH_MAP) {
		ERR_FAIL_COND_V_MSG(fixed_hash_map_cache_ == nullptr, empty_utf8, "Fixed-hash StringName cache storage is unexpectedly missing.");
		if (auto existing = fixed_hash_map_cache_->find(p_name); existing != fixed_hash_map_cache_->end()) {
			return existing->second;
		}
		if (fixed_hash_map_cache_->size() >= static_cast<size_t>(capacity_)) {
			fixed_hash_map_cache_->clear();
		}
		auto inserted = fixed_hash_map_cache_->insert({ p_name, String(p_name).utf8() });
		return inserted.first->second;
	}
	no_cache_scratch_utf8_ = String(p_name).utf8();
	return no_cache_scratch_utf8_;
}
