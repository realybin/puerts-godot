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

enum class PuertsEnvironmentState {
	Uninitialized,
	Ready,
	DisposePending,
	Disposing,
};

struct PuertsEnvironmentData {
	PuertsEnvironmentState state = PuertsEnvironmentState::Uninitialized;
	PuertsEnvironment *environment = nullptr;
	PuertsBridgeRegistry bridge;

	[[nodiscard]] bool accepts_calls() const {
		return state == PuertsEnvironmentState::Ready || state == PuertsEnvironmentState::DisposePending;
	}
};

namespace puerts::internal {

constexpr size_t kInlineArgumentCount = 8;

struct CallArguments {
	puerts_eastl::fixed_vector<godot::Variant, kInlineArgumentCount> values;
	puerts_eastl::fixed_vector<const godot::Variant *, kInlineArgumentCount> pointers;

	void resize(int p_count) {
		values.resize(static_cast<size_t>(p_count));
		pointers.resize(static_cast<size_t>(p_count));
		for (int i = 0; i < p_count; ++i) {
			pointers[i] = &values[i];
		}
	}
};

godot::String read_utf8_string(pesapi_ffi *p_apis, pesapi_env p_env, pesapi_value p_value);
godot::String format_call_error(const godot::String &p_target_name, const GDExtensionCallError &p_call_error);

class EnvironmentHandleScope {
public:
	EnvironmentHandleScope(pesapi_ffi *p_apis, pesapi_env_ref p_env_ref) :
			api_(p_apis),
			scope_(p_apis->open_scope(p_env_ref)),
			script_env_(p_apis->get_env_from_ref(p_env_ref)) {
	}

	~EnvironmentHandleScope() {
		api_->close_scope(scope_);
	}

	EnvironmentHandleScope(const EnvironmentHandleScope &) = delete;
	EnvironmentHandleScope &operator=(const EnvironmentHandleScope &) = delete;

	[[nodiscard]] pesapi_scope scope() const { return scope_; }
	[[nodiscard]] pesapi_env env() const { return script_env_; }

private:
	pesapi_ffi *api_;
	pesapi_scope scope_;
	pesapi_env script_env_;
};

class CallbackContext {
public:
	struct Argument {
		pesapi_value value = nullptr;
		void *native_handle = nullptr;
		const void *native_type_id = nullptr;
		bool value_loaded = false;
		bool native_loaded = false;
		bool variant_loaded = false;
	};

	CallbackContext(pesapi_ffi *p_apis, pesapi_callback_info p_info) :
			api_(p_apis),
			callback_info_(p_info),
			script_env_(p_apis->get_env(p_info)),
			argument_count_(p_apis->get_args_len(p_info)) {
		arguments_.resize(static_cast<size_t>(argument_count_));
		environment_data_ = const_cast<PuertsEnvironmentData *>(static_cast<const PuertsEnvironmentData *>(api_->get_env_private(script_env_)));
		puerts_environment_ = environment_data_ != nullptr ? environment_data_->environment : nullptr;
	}

	[[nodiscard]] pesapi_ffi *api() const { return api_; }
	[[nodiscard]] pesapi_callback_info callback_info() const { return callback_info_; }
	[[nodiscard]] pesapi_env script_env() const { return script_env_; }
	[[nodiscard]] int argument_count() const { return argument_count_; }
	[[nodiscard]] PuertsEnvironmentData *environment_data() const { return environment_data_; }
	[[nodiscard]] PuertsEnvironment *puerts_environment() const { return puerts_environment_; }
	[[nodiscard]] bool require_argument_count(int p_expected) const {
		if (argument_count_ == p_expected) {
			return true;
		}
		api_->throw_by_string(callback_info_, "Argument count does not match the bound signature.");
		return false;
	}

	[[nodiscard]] bool require_minimum_argument_count(int p_minimum) const {
		if (argument_count_ >= p_minimum) {
			return true;
		}
		api_->throw_by_string(callback_info_, "Argument count does not match the bound signature.");
		return false;
	}

	[[nodiscard]] Argument &argument(int p_index) {
		return arguments_[static_cast<size_t>(p_index)];
	}

	[[nodiscard]] pesapi_value argument_value(int p_index) {
		Argument &entry = argument(p_index);
		if (!entry.value_loaded) {
			entry.value = api_->get_arg(callback_info_, p_index);
			entry.value_loaded = true;
		}
		return entry.value;
	}

	[[nodiscard]] Argument &native_argument(int p_index) {
		Argument &entry = argument(p_index);
		if (!entry.native_loaded) {
			const pesapi_value value = argument_value(p_index);
			entry.native_handle = api_->get_native_object_ptr(script_env_, value);
			if (entry.native_handle != nullptr) {
				entry.native_type_id = api_->get_native_object_typeid(script_env_, value);
			}
			entry.native_loaded = true;
		}
		return entry;
	}

	[[nodiscard]] godot::Variant &variant(int p_index) {
		const size_t index = static_cast<size_t>(p_index);
		if (variants_.size() <= index) {
			variants_.resize(index + 1);
		}
		return variants_[index];
	}

	[[nodiscard]] void *holder_ptr() {
		ensure_holder_loaded();
		return holder_ptr_;
	}

	[[nodiscard]] const void *holder_type_id() {
		ensure_holder_loaded();
		return holder_type_id_;
	}

	[[nodiscard]] const godot::Variant *holder_boxed_variant() {
		ensure_holder_boxed_variant_loaded();
		return holder_boxed_variant_;
	}

	[[nodiscard]] bool require() const {
		if (environment_data_ == nullptr || !environment_data_->accepts_calls()) {
			api_->throw_by_string(callback_info_, "Puerts environment is not available.");
			return false;
		}
		return true;
	}

	[[nodiscard]] bool reject(pesapi_callback_info p_info, const char *p_message) const {
		if (p_info != nullptr) {
			api_->throw_by_string(p_info, p_message);
		}
		return false;
	}

private:
	pesapi_ffi *api_ = nullptr;
	pesapi_callback_info callback_info_ = nullptr;
	pesapi_env script_env_ = nullptr;
	int argument_count_ = 0;
	puerts_eastl::fixed_vector<Argument, kInlineArgumentCount> arguments_;
	puerts_eastl::fixed_vector<godot::Variant, kInlineArgumentCount> variants_;
	bool holder_loaded_ = false;
	void *holder_ptr_ = nullptr;
	const void *holder_type_id_ = nullptr;
	bool holder_boxed_variant_loaded_ = false;
	const godot::Variant *holder_boxed_variant_ = nullptr;
	PuertsEnvironmentData *environment_data_ = nullptr;
	PuertsEnvironment *puerts_environment_ = nullptr;

	void ensure_holder_loaded() {
		if (holder_loaded_) {
			return;
		}

		holder_ptr_ = api_->get_native_holder_ptr(callback_info_);
		holder_type_id_ = api_->get_native_holder_typeid(callback_info_);
		holder_loaded_ = true;
	}

	void ensure_holder_boxed_variant_loaded() {
		if (holder_boxed_variant_loaded_) {
			return;
		}

		ensure_holder_loaded();
		holder_boxed_variant_loaded_ = true;
		if (holder_ptr_ == nullptr) {
			return;
		}

		holder_boxed_variant_ = environment_data_->bridge.find_box(holder_ptr_);
	}
};

} // namespace puerts::internal

#endif // PUERTS_GODOT_PUERTS_RUNTIME_H
