// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_addon.h"

#include "puerts_eastl.h"
#include "puerts_type_registry.h"

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/thread.hpp>
#include <godot_cpp/core/class_db.hpp>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif (defined(__linux__) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)) || (defined(__APPLE__) && TARGET_OS_OSX)
#define PUERTS_ADDON_POSIX
#include <dlfcn.h>
#endif

using namespace godot;

namespace {

struct LoadedAddon {
	String path;
	String module_name;
	void *handle;
	bool registered;
};

// Handles intentionally remain loaded: the shared registry retains code and data pointers.
puerts_eastl::vector<LoadedAddon> loaded_addons;

} // namespace

void PuertsAddon::_bind_methods() {
	ClassDB::bind_static_method("PuertsAddon", D_METHOD("load", "path", "module_name"), &PuertsAddon::load);
}

Error PuertsAddon::load(const String &p_path, const String &p_module_name) {
	ERR_FAIL_COND_V_MSG(!Thread::is_main_thread(), ERR_INVALID_PARAMETER, "Load Puerts addons on the main thread before running scripts.");
	ERR_FAIL_COND_V_MSG(p_path.is_empty() || p_module_name.is_empty() || !p_module_name.is_valid_ascii_identifier(), ERR_INVALID_PARAMETER, "An addon needs a path and an ASCII identifier as its module name.");

#if defined(_WIN32) || defined(PUERTS_ADDON_POSIX)
	const String path = ProjectSettings::get_singleton()->globalize_path(p_path).simplify_path();
	ERR_FAIL_COND_V_MSG(!path.is_absolute_path(), ERR_INVALID_PARAMETER, "Addon paths must be absolute, res://, or user:// paths.");
	for (const LoadedAddon &addon : loaded_addons) {
		if (addon.module_name == p_module_name) {
			ERR_FAIL_COND_V_MSG(addon.path != path, ERR_ALREADY_IN_USE, "The addon module name is already associated with another library.");
			return addon.registered ? OK : ERR_BUSY;
		}
	}
	const String entry = "pesapi_register_" + p_module_name + "_v" + String::num_int64(PESAPI_VERSION);
#if defined(_WIN32)
	const auto wide_path = path.utf16();
	HMODULE library = LoadLibraryExW(reinterpret_cast<LPCWSTR>(wide_path.get_data()), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	if (library == nullptr) {
		const DWORD error = GetLastError();
		ERR_FAIL_V_MSG(ERR_CANT_OPEN, vformat("Cannot load addon %s (Windows error %d).", path, static_cast<int64_t>(error)));
	}
	const auto register_function = reinterpret_cast<PuertsTypeRegistry::AddonRegisterFunction>(GetProcAddress(library, entry.utf8().get_data()));
	if (register_function == nullptr) {
		FreeLibrary(library);
		ERR_FAIL_V_MSG(ERR_METHOD_NOT_FOUND, "Addon entry not found: " + entry);
	}
#else
	void *library = dlopen(path.utf8().get_data(), RTLD_NOW | RTLD_LOCAL);
	if (library == nullptr) {
		ERR_FAIL_V_MSG(ERR_CANT_OPEN, "Cannot load addon " + path + ": " + String::utf8(dlerror()));
	}
	const auto register_function = reinterpret_cast<PuertsTypeRegistry::AddonRegisterFunction>(dlsym(library, entry.utf8().get_data()));
	if (register_function == nullptr) {
		dlclose(library);
		ERR_FAIL_V_MSG(ERR_METHOD_NOT_FOUND, "Addon entry not found: " + entry);
	}
#endif
	const size_t index = loaded_addons.size();
	loaded_addons.push_back({ path, p_module_name, library, false });
	PuertsTypeRegistry::get_singleton().register_addon(register_function);
	loaded_addons[index].registered = true;
	return OK;
#else
	ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Dynamic Puerts addons are currently supported on desktop platforms only.");
#endif
}
