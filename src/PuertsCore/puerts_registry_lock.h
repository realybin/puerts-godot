// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_REGISTRY_LOCK_H
#define PUERTS_GODOT_PUERTS_REGISTRY_LOCK_H

#include "pesapi.h"

extern "C" {
void PuertsLockRegistry(pesapi_registry p_registry);
void PuertsUnlockRegistry(pesapi_registry p_registry);
}

namespace puerts::internal {

// Use the native registry's mutex for metadata as well, so lazy registration
// cannot expose a partially initialized class to another environment.
class RegistryLock {
	pesapi_registry registry_;

public:
	explicit RegistryLock(pesapi_registry p_registry) :
			registry_(p_registry) {
#ifdef PUERTS_THREAD_SAFE
		PuertsLockRegistry(registry_);
#endif
	}

	~RegistryLock() {
#ifdef PUERTS_THREAD_SAFE
		PuertsUnlockRegistry(registry_);
#endif
	}

	RegistryLock(const RegistryLock &) = delete;
	RegistryLock &operator=(const RegistryLock &) = delete;
};

} // namespace puerts::internal

#endif // PUERTS_GODOT_PUERTS_REGISTRY_LOCK_H
