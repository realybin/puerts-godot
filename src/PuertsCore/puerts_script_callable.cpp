// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_script_callable.h"

#include "puerts_script_value.h"

#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/templates/hashfuncs.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable_custom.hpp>

using namespace godot;

namespace {

class PuertsScriptCallable final : public CallableCustom {
public:
	explicit PuertsScriptCallable(const Ref<PuertsScriptValue> &p_function) :
			function_(p_function) {
	}

	uint32_t hash() const override {
		return hash_murmur3_one_64(function_->get_instance_id());
	}

	String get_as_text() const override {
		return "<PuertsScriptCallable>";
	}

	static bool compare_equal(const CallableCustom *p_left, const CallableCustom *p_right) {
		const auto *left = static_cast<const PuertsScriptCallable *>(p_left);
		const auto *right = static_cast<const PuertsScriptCallable *>(p_right);
		return left->function_.ptr() == right->function_.ptr();
	}

	CompareEqualFunc get_compare_equal_func() const override {
		return &PuertsScriptCallable::compare_equal;
	}

	static bool compare_less(const CallableCustom *p_left, const CallableCustom *p_right) {
		const auto *left = static_cast<const PuertsScriptCallable *>(p_left);
		const auto *right = static_cast<const PuertsScriptCallable *>(p_right);
		return reinterpret_cast<uintptr_t>(left->function_.ptr()) < reinterpret_cast<uintptr_t>(right->function_.ptr());
	}

	CompareLessFunc get_compare_less_func() const override {
		return &PuertsScriptCallable::compare_less;
	}

	bool is_valid() const override {
		return function_.is_valid() && function_->is_valid();
	}

	ObjectID get_object() const override {
		return {};
	}

	int get_argument_count(bool &r_is_valid) const override {
		r_is_valid = false;
		return 0;
	}

	void call(const Variant **p_arguments, int p_argument_count, Variant &r_return_value, GDExtensionCallError &r_call_error) const override {
		r_return_value = {};
		r_call_error.error = GDEXTENSION_CALL_OK;
		if (!is_valid()) {
			r_call_error.error = GDEXTENSION_CALL_ERROR_INSTANCE_IS_NULL;
			return;
		}

		Array arguments;
		arguments.resize(p_argument_count);
		for (int i = 0; i < p_argument_count; ++i) {
			arguments[i] = *p_arguments[i];
		}

		Ref<PuertsScriptValue> result = function_->call(arguments);
		if (result.is_valid()) {
			r_return_value = result->to_native();
		}
	}

private:
	Ref<PuertsScriptValue> function_;
};

} // namespace

Callable puerts::internal::make_script_callable(const Ref<PuertsScriptValue> &p_function) {
	return Callable(memnew(PuertsScriptCallable(p_function)));
}
