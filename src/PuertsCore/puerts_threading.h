// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: Apache-2.0

#ifndef PUERTS_GODOT_PUERTS_THREADING_H
#define PUERTS_GODOT_PUERTS_THREADING_H

#include "puerts_eastl.h"

#ifdef __EMSCRIPTEN__
#include <memory>
#else
#include <EASTL/shared_ptr.h>
#endif
#include <mutex>

namespace puerts::internal {

#ifdef PUERTS_THREAD_SAFE
using RecursiveMutex = std::recursive_mutex;
#else
class RecursiveMutex {
public:
	void lock() {}
	void unlock() {}
};
#endif

using MutexLock = std::lock_guard<RecursiveMutex>;
// EASTL's atomic acquire fence is not implemented for Wasm.
#ifdef __EMSCRIPTEN__
using EnvironmentMutex = std::shared_ptr<RecursiveMutex>;
inline EnvironmentMutex make_environment_mutex() {
	return std::make_shared<RecursiveMutex>();
}
#else
using EnvironmentMutex = eastl::shared_ptr<RecursiveMutex>;
inline EnvironmentMutex make_environment_mutex() {
	return eastl::allocate_shared<RecursiveMutex>(puerts_eastl::allocator{});
}
#endif

// A wrapper can outlive its environment. Retain the mutex until after unlocking,
// including when a callback releases the last reference to the environment.
class EnvironmentLock {
#ifdef PUERTS_THREAD_SAFE
	EnvironmentMutex mutex_;
	MutexLock lock_;
#endif

public:
	explicit EnvironmentLock([[maybe_unused]] const EnvironmentMutex &p_mutex)
#ifdef PUERTS_THREAD_SAFE
			: mutex_(p_mutex),
			  lock_(*mutex_)
#endif
	{
	}

	EnvironmentLock(const EnvironmentLock &) = delete;
	EnvironmentLock &operator=(const EnvironmentLock &) = delete;
};

} // namespace puerts::internal

#endif // PUERTS_GODOT_PUERTS_THREADING_H
