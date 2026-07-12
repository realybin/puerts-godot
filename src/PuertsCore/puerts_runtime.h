// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_RUNTIME_H
#define PUERTS_GODOT_PUERTS_RUNTIME_H

#include "pesapi.h"
#include "puerts_bridge_registry.h"
#include "puerts_eastl.h"

#include <godot_cpp/variant/variant.hpp>

class PuertsEnvironment;

struct PuertsEnvPrivate {
	bool alive = false;
	PuertsEnvironment *environment = nullptr;
	PuertsBridgeRegistry *bridge = nullptr;
};

namespace puerts::internal {

constexpr size_t INLINE_ARGUMENT_COUNT = 8;
constexpr size_t INLINE_VARIANT_ARGUMENT_COUNT = 4;
constexpr size_t INLINE_NATIVE_ARGUMENT_COUNT = 2;

class env_scope {
public:
	env_scope(pesapi_ffi *p_apis, pesapi_env_ref p_env_ref) :
			apis_(p_apis),
			scope_(p_apis->open_scope(p_env_ref)),
			env_(p_apis->get_env_from_ref(p_env_ref)) {
	}

	~env_scope() {
		apis_->close_scope(scope_);
	}

	env_scope(const env_scope &) = delete;
	env_scope &operator=(const env_scope &) = delete;

	[[nodiscard]] pesapi_scope get_scope() const { return scope_; }
	[[nodiscard]] pesapi_env get_env() const { return env_; }

private:
	pesapi_ffi *apis_;
	pesapi_scope scope_;
	pesapi_env env_;
};

struct callback_context {
	struct arg_native_state {
		int index = -1;
		void *native_handle = nullptr;
		const void *native_type_id = nullptr;
		bool native_type_info_loaded = false;
		const void *native_type_info = nullptr;
	};

	struct arg_variant_state {
		int index = -1;
		bool loaded = false;
		godot::Variant value;
	};

	pesapi_ffi *apis = nullptr;
	pesapi_callback_info info = nullptr;
	pesapi_env env = nullptr;
	int arg_count = 0;
	puerts_eastl::fixed_vector<pesapi_value, INLINE_ARGUMENT_COUNT> arg_values;
	puerts_eastl::fixed_vector<arg_variant_state, INLINE_VARIANT_ARGUMENT_COUNT> arg_variants;
	puerts_eastl::fixed_vector<arg_native_state, INLINE_NATIVE_ARGUMENT_COUNT> arg_native_states;
	bool holder_loaded = false;
	void *holder_ptr = nullptr;
	const void *holder_type_id = nullptr;
	bool holder_type_info_loaded = false;
	const void *holder_type_info = nullptr;
	bool holder_boxed_variant_loaded = false;
	const godot::Variant *holder_boxed_variant = nullptr;
	const PuertsEnvPrivate *env_private = nullptr;
	PuertsEnvironment *environment = nullptr;

	callback_context(pesapi_ffi *p_apis, pesapi_callback_info p_info) :
			apis(p_apis),
			info(p_info),
			env(p_apis->get_env(p_info)),
			arg_count(p_apis->get_args_len(p_info)) {
		env_private = static_cast<const PuertsEnvPrivate *>(apis->get_env_private(env));
		environment = env_private != nullptr ? env_private->environment : nullptr;
	}

	[[nodiscard]] pesapi_value get_arg(int p_index) {
		if (p_index < 0 || p_index >= arg_count) {
			return nullptr;
		}
		ensure_arg_loaded(p_index);
		return arg_values[p_index];
	}

	[[nodiscard]] arg_native_state *get_arg_native_state(int p_index) {
		if (p_index < 0 || p_index >= arg_count) {
			return nullptr;
		}
		for (arg_native_state &state : arg_native_states) {
			if (state.index == p_index) {
				return &state;
			}
		}

		ensure_arg_loaded(p_index);
		arg_native_states.push_back(arg_native_state{});
		arg_native_state &state = arg_native_states.back();
		state.index = p_index;
		pesapi_value value = arg_values[p_index];
		state.native_handle = apis->get_native_object_ptr(env, value);
		if (state.native_handle != nullptr) {
			state.native_type_id = apis->get_native_object_typeid(env, value);
		}
		return &state;
	}

	[[nodiscard]] arg_variant_state *get_arg_variant_state(int p_index) {
		for (arg_variant_state &state : arg_variants) {
			if (state.index == p_index) {
				return &state;
			}
		}
		arg_variants.push_back(arg_variant_state{});
		arg_variants.back().index = p_index;
		return &arg_variants.back();
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

	[[nodiscard]] bool require() const {
		if (env_private == nullptr || !env_private->alive) {
			apis->throw_by_string(info, "Puerts environment is not available.");
			return false;
		}
		return true;
	}

private:
	void ensure_arg_loaded(int p_index) {
		while (arg_values.size() <= static_cast<size_t>(p_index)) {
			arg_values.push_back(apis->get_arg(info, static_cast<int>(arg_values.size())));
		}
	}

	void ensure_holder_loaded() {
		if (holder_loaded) {
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
		if (holder_ptr == nullptr) {
			return;
		}

		holder_boxed_variant = env_private->bridge->get_box(holder_ptr);
	}
};

} // namespace puerts::internal

#endif // PUERTS_GODOT_PUERTS_RUNTIME_H
