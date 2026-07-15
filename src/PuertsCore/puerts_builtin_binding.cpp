// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_builtin_binding.h"

#include "puerts_static_binding.h"

#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR == 5
#include "puerts_builtin_bindings.4_5.generated.inc"
#include "puerts_global_scope.4_5.generated.inc"
#include "puerts_object_profile_bindings.4_5.generated.inc"
#elif GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR == 6
#include "puerts_builtin_bindings.4_6.generated.inc"
#include "puerts_global_scope.4_6.generated.inc"
#include "puerts_object_profile_bindings.4_6.generated.inc"
#elif GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR == 7
#include "puerts_builtin_bindings.4_7.generated.inc"
#include "puerts_global_scope.4_7.generated.inc"
#include "puerts_object_profile_bindings.4_7.generated.inc"
#else
#error "Puerts static bindings support Godot API versions 4.5, 4.6, and 4.7."
#endif

void register_puerts_builtin_bindings() {
	register_puerts_builtin_bindings_generated();
	register_puerts_global_scope_generated();
	register_puerts_object_profile_bindings_generated();
}
