// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_STRING_NAME_CACHE_POOL_H
#define PUERTS_GODOT_PUERTS_STRING_NAME_CACHE_POOL_H

#include "puerts_eastl.h"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/string_name.hpp>

class PuertsStringNameCachePool : public godot::RefCounted {
	GDCLASS(PuertsStringNameCachePool, godot::RefCounted)

public:
	enum Policy {
		POLICY_HASH_MAP = 0,
		POLICY_FIXED_HASH_MAP = 1,
		POLICY_NO_CACHE = 2,
	};

private:
	struct StringNameHash {
		size_t operator()(const godot::StringName &p_name) const {
			return p_name.hash();
		}
	};

	struct StringNameEqual {
		bool operator()(const godot::StringName &p_left, const godot::StringName &p_right) const {
			return p_left == p_right;
		}
	};

	static constexpr int32_t FIXED_HASH_MAP_MAX_CAPACITY = 1024;
	using FixedHashMap = puerts_eastl::fixed_hash_map<godot::StringName, godot::CharString, FIXED_HASH_MAP_MAX_CAPACITY, FIXED_HASH_MAP_MAX_CAPACITY + 1, false, StringNameHash, StringNameEqual, true>;
	using HashMap = puerts_eastl::hash_map<godot::StringName, godot::CharString, StringNameHash, StringNameEqual, true>;

	Policy policy_ = POLICY_HASH_MAP;
	int32_t capacity_ = 512;
	HashMap *hash_map_cache_ = nullptr;
	FixedHashMap *fixed_hash_map_cache_ = nullptr;
	godot::CharString no_cache_scratch_utf8_;

protected:
	static void _bind_methods();

public:
	PuertsStringNameCachePool() = default;
	~PuertsStringNameCachePool() override;
	godot::Error initialize(Policy p_policy = POLICY_HASH_MAP, int32_t p_capacity = 512);
	[[nodiscard]] bool is_initialized() const;
	void clear();
	[[nodiscard]] Policy get_policy() const;
	[[nodiscard]] int32_t get_capacity() const;
	[[nodiscard]] const godot::CharString &get_cached_utf8(const godot::StringName &p_name);

private:
	bool initialized_ = false;
};

VARIANT_ENUM_CAST(PuertsStringNameCachePool::Policy);

#endif // PUERTS_GODOT_PUERTS_STRING_NAME_CACHE_POOL_H
