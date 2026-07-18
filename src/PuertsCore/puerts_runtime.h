// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_RUNTIME_H
#define PUERTS_GODOT_PUERTS_RUNTIME_H

#include "pesapi.h"
#include "puerts_bridge_registry.h"
#include "puerts_eastl.h"

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

class PuertsEnvironment;

struct PuertsEnvPrivate {
	bool alive = false;
	PuertsEnvironment *environment = nullptr;
	PuertsBridgeRegistry bridge;
};

namespace puerts::internal {

constexpr size_t INLINE_ARGUMENT_COUNT = 8;

godot::String read_utf8_string(pesapi_ffi *p_apis, pesapi_env p_env, pesapi_value p_value);
godot::String format_call_error(const godot::String &p_target_name, const GDExtensionCallError &p_call_error);

class EnvironmentScope {
public:
	EnvironmentScope(pesapi_ffi *p_apis, pesapi_env_ref p_env_ref) :
			apis_(p_apis),
			scope_(p_apis->open_scope(p_env_ref)),
			env_(p_apis->get_env_from_ref(p_env_ref)) {
	}

	~EnvironmentScope() {
		apis_->close_scope(scope_);
	}

	EnvironmentScope(const EnvironmentScope &) = delete;
	EnvironmentScope &operator=(const EnvironmentScope &) = delete;

	[[nodiscard]] pesapi_scope get_scope() const { return scope_; }
	[[nodiscard]] pesapi_env get_env() const { return env_; }

private:
	pesapi_ffi *apis_;
	pesapi_scope scope_;
	pesapi_env env_;
};

struct CallbackFrame {
	struct Argument {
		pesapi_value value = nullptr;
		void *native_handle = nullptr;
		const void *native_type_id = nullptr;
		godot::Variant variant;
		bool value_loaded = false;
		bool native_loaded = false;
		bool variant_loaded = false;
	};

	pesapi_ffi *apis = nullptr;
	pesapi_callback_info info = nullptr;
	pesapi_env env = nullptr;
	int arg_count = 0;
	puerts_eastl::fixed_vector<Argument, INLINE_ARGUMENT_COUNT> arguments;
	bool holder_loaded = false;
	void *holder_ptr = nullptr;
	const void *holder_type_id = nullptr;
	bool holder_boxed_variant_loaded = false;
	const godot::Variant *holder_boxed_variant = nullptr;
	PuertsEnvPrivate *env_private = nullptr;
	PuertsEnvironment *environment = nullptr;

	CallbackFrame(pesapi_ffi *p_apis, pesapi_callback_info p_info) :
			apis(p_apis),
			info(p_info),
			env(p_apis->get_env(p_info)),
			arg_count(p_apis->get_args_len(p_info)) {
		arguments.resize(static_cast<size_t>(arg_count));
		env_private = const_cast<PuertsEnvPrivate *>(static_cast<const PuertsEnvPrivate *>(apis->get_env_private(env)));
		environment = env_private != nullptr ? env_private->environment : nullptr;
	}

	[[nodiscard]] Argument &get_argument(int p_index) {
		return arguments[static_cast<size_t>(p_index)];
	}

	[[nodiscard]] pesapi_value get_argument_value(int p_index) {
		Argument &argument = get_argument(p_index);
		if (!argument.value_loaded) {
			argument.value = apis->get_arg(info, p_index);
			argument.value_loaded = true;
		}
		return argument.value;
	}

	[[nodiscard]] Argument &get_native_argument(int p_index) {
		Argument &argument = get_argument(p_index);
		if (!argument.native_loaded) {
			const pesapi_value value = get_argument_value(p_index);
			argument.native_handle = apis->get_native_object_ptr(env, value);
			if (argument.native_handle != nullptr) {
				argument.native_type_id = apis->get_native_object_typeid(env, value);
			}
			argument.native_loaded = true;
		}
		return argument;
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

		holder_boxed_variant = env_private->bridge.get_box(holder_ptr);
	}
};

} // namespace puerts::internal

#endif // PUERTS_GODOT_PUERTS_RUNTIME_H
