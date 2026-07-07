// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_builtin_binding.h"

#include "puerts_static_binding.h"

#include "puerts_builtin_bindings.generated.inc"
#include "puerts_global_scope.generated.inc"
#include "puerts_object_profile_bindings.generated.inc"

void register_puerts_builtin_bindings() {
	register_puerts_builtin_bindings_generated();
	register_puerts_global_scope_generated();
	register_puerts_object_profile_bindings_generated();
}
