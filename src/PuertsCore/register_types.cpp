// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/version.hpp>
#include <godot_cpp/godot.hpp>

#include "puerts_builtin_binding.h"
#include "puerts_environment.h"
#include "puerts_script_value.h"
#include "puerts_string_name_cache_pool.h"

void initialize_puerts_core_module(godot::ModuleInitializationLevel p_level) {
	if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(PuertsEnvironment);
	GDREGISTER_CLASS(PuertsScriptValue);
	GDREGISTER_CLASS(PuertsStringNameCachePool);
	register_puerts_builtin_bindings();
}

void uninitialize_puerts_core_module(godot::ModuleInitializationLevel p_level) {
	if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

extern "C" {
GDExtensionBool GDE_EXPORT puerts_core_library_init(
		GDExtensionInterfaceGetProcAddress p_get_proc_address,
		GDExtensionClassLibraryPtr p_library,
		GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
	init_obj.register_initializer(initialize_puerts_core_module);
	init_obj.register_terminator(uninitialize_puerts_core_module);
	init_obj.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);

#if GODOT_VERSION_MAJOR < 4 || (GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR < 5)
#error "Older versions are no longer supported. Please upgrade to a newer version of Godot."
#endif
	return init_obj.init();
}
}
