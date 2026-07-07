// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_EASTL_H
#define PUERTS_GODOT_PUERTS_EASTL_H

#include <EASTL/allocator_malloc.h>
#include <EASTL/fixed_hash_map.h>
#include <EASTL/functional.h>
#include <EASTL/hash_map.h>
#include <EASTL/hash_set.h>
#include <EASTL/vector.h>

namespace puerts_eastl {

using allocator = eastl::allocator_malloc;

template <typename T>
using vector = eastl::vector<T, allocator>;

template <typename Key, typename Value, typename Hash = eastl::hash<Key>, typename Predicate = eastl::equal_to<Key>, bool CacheHashCode = false>
using hash_map = eastl::hash_map<Key, Value, Hash, Predicate, allocator, CacheHashCode>;

template <typename Key, typename Value, size_t node_count, size_t bucket_count = node_count + 1, bool bEnableOverflow = true, typename Hash = eastl::hash<Key>, typename Predicate = eastl::equal_to<Key>, bool CacheHashCode = false>
using fixed_hash_map = eastl::fixed_hash_map<Key, Value, node_count, bucket_count, bEnableOverflow, Hash, Predicate, CacheHashCode, allocator>;

template <typename Value, typename Hash = eastl::hash<Value>, typename Predicate = eastl::equal_to<Value>, bool CacheHashCode = false>
using hash_set = eastl::hash_set<Value, Hash, Predicate, allocator, CacheHashCode>;

} // namespace puerts_eastl

#endif // PUERTS_GODOT_PUERTS_EASTL_H
