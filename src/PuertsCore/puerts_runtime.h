// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_RUNTIME_H
#define PUERTS_GODOT_PUERTS_RUNTIME_H

#include "pesapi.h"
#include "puerts_bridge_registry.h"

#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/variant/variant.hpp>

class PuertsEnvironment;

struct PuertsEnvPrivate {
	bool alive = false;
	PuertsEnvironment *environment = nullptr;
	PuertsBridgeRegistry *bridge = nullptr;
};

namespace puerts::internal {

struct callback_context {
	struct arg_native_state {
		bool loaded = false;
		bool is_object = false;
		void *native_handle = nullptr;
		const void *native_type_id = nullptr;
		bool native_type_info_loaded = false;
		const void *native_type_info = nullptr;
		bool bridged_variant_loaded = false;
		godot::Variant bridged_variant;
	};

	pesapi_ffi *apis = nullptr;
	pesapi_callback_info info = nullptr;
	pesapi_env env = nullptr;
	int arg_count = 0;
	int loaded_arg_count = 0;
	godot::LocalVector<pesapi_value, int32_t, true> arg_values;
	godot::LocalVector<arg_native_state, int32_t> arg_native_states;
	bool holder_loaded = false;
	void *holder_ptr = nullptr;
	const void *holder_type_id = nullptr;
	bool holder_type_info_loaded = false;
	const void *holder_type_info = nullptr;
	bool holder_boxed_variant_loaded = false;
	const godot::Variant *holder_boxed_variant = nullptr;
	const PuertsEnvPrivate *env_private = nullptr;
	PuertsEnvironment *environment = nullptr;

	callback_context() = default;

	callback_context(pesapi_ffi *p_apis, pesapi_callback_info p_info) :
			apis(p_apis),
			info(p_info),
			env(p_apis != nullptr ? p_apis->get_env(p_info) : nullptr),
			arg_count(p_apis != nullptr && p_info != nullptr ? p_apis->get_args_len(p_info) : 0) {
		env_private = env != nullptr ? static_cast<const PuertsEnvPrivate *>(apis->get_env_private(env)) : nullptr;
		environment = env_private != nullptr ? env_private->environment : nullptr;
	}

	[[nodiscard]] pesapi_value get_arg(int p_index) {
		if (p_index < 0 || p_index >= arg_count || apis == nullptr || info == nullptr) {
			return nullptr;
		}
		ensure_arg_loaded(p_index);
		return arg_values[p_index];
	}

	[[nodiscard]] arg_native_state *get_arg_native_state(int p_index) {
		if (p_index < 0 || p_index >= arg_count) {
			return nullptr;
		}
		ensure_arg_native_state_loaded(p_index);
		return &arg_native_states[p_index];
	}

	[[nodiscard]] void *get_holder_ptr() {
		ensure_holder_loaded();
		return holder_ptr;
	}

	[[nodiscard]] const void *get_holder_typeid() {
		ensure_holder_loaded();
		return holder_type_id;
	}

	[[nodiscard]] const godot::Variant *get_holder_boxed_variant() {
		ensure_holder_boxed_variant_loaded();
		return holder_boxed_variant;
	}

	[[nodiscard]] bool require(bool p_require_environment = true, bool p_require_bridge = false) const {
		if (apis == nullptr || info == nullptr || env == nullptr || env_private == nullptr || !env_private->alive ||
				(p_require_environment && environment == nullptr) ||
				(p_require_bridge && env_private->bridge == nullptr)) {
			if (apis != nullptr && info != nullptr) {
				apis->throw_by_string(info, "Puerts environment is not available.");
			}
			return false;
		}
		return true;
	}

private:
	void ensure_arg_loaded(int p_index) {
		if (p_index < loaded_arg_count || arg_count <= 0 || p_index >= arg_count || apis == nullptr || info == nullptr) {
			return;
		}

		const int required_size = p_index + 1;
		if (arg_values.size() < required_size) {
			arg_values.resize(required_size);
		}
		for (int i = loaded_arg_count; i <= p_index; ++i) {
			arg_values[i] = apis->get_arg(info, i);
		}
		loaded_arg_count = p_index + 1;
	}

	void ensure_arg_native_state_loaded(int p_index) {
		if (p_index < 0 || p_index >= arg_count) {
			return;
		}

		ensure_arg_loaded(p_index);
		const int required_size = p_index + 1;
		if (arg_native_states.size() < required_size) {
			const int previous_size = arg_native_states.size();
			arg_native_states.resize(required_size);
			for (int i = previous_size; i < required_size; ++i) {
				arg_native_states[i] = arg_native_state{};
			}
		}

		arg_native_state &state = arg_native_states[p_index];
		if (state.loaded || apis == nullptr) {
			state.loaded = true;
			return;
		}

		pesapi_value value = arg_values[p_index];
		state.is_object = apis->is_object(env, value);
		state.native_handle = apis->get_native_object_ptr(env, value);
		state.native_type_id = apis->get_native_object_typeid(env, value);
		state.loaded = true;
	}

	void ensure_holder_loaded() {
		if (holder_loaded || apis == nullptr || info == nullptr) {
			holder_loaded = true;
			return;
		}

		holder_ptr = apis->get_native_holder_ptr(info);
		holder_type_id = apis->get_native_holder_typeid(info);
		holder_loaded = true;
	}

	void ensure_holder_boxed_variant_loaded() {
		if (holder_boxed_variant_loaded) {
			return;
		}

		ensure_holder_loaded();
		holder_boxed_variant_loaded = true;
		if (holder_ptr == nullptr || env_private == nullptr || env_private->bridge == nullptr) {
			return;
		}

		holder_boxed_variant = env_private->bridge->get_box(holder_ptr);
	}
};

inline bool resolve_context(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		callback_context &r_context,
		bool p_require_environment = true,
		bool p_require_bridge = false) {
	r_context = callback_context(apis, info);
	return r_context.require(p_require_environment, p_require_bridge);
}

} // namespace puerts::internal

#endif // PUERTS_GODOT_PUERTS_RUNTIME_H
