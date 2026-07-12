// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_SCRIPT_VALUE_H
#define PUERTS_GODOT_PUERTS_SCRIPT_VALUE_H

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "pesapi.h"

class PuertsEnvironment;

class PuertsScriptValue : public godot::RefCounted {
	GDCLASS(PuertsScriptValue, godot::RefCounted)

	PuertsEnvironment *environment_ = nullptr;
	pesapi_ffi *ffi_ = nullptr;
	pesapi_value_ref value_ref_ = nullptr;
	void *cache_token_ = nullptr;

protected:
	static void _bind_methods();

public:
	PuertsScriptValue() = default;
	~PuertsScriptValue() override;

	[[nodiscard]] bool is_valid() const;
	[[nodiscard]] bool is_null() const;
	[[nodiscard]] bool is_undefined() const;
	[[nodiscard]] bool is_bool() const;
	[[nodiscard]] bool is_int() const;
	[[nodiscard]] bool is_float() const;
	[[nodiscard]] bool is_string() const;
	[[nodiscard]] bool is_binary() const;
	[[nodiscard]] bool is_object() const;
	[[nodiscard]] bool is_function() const;

	[[nodiscard]] bool to_bool() const;
	[[nodiscard]] int64_t to_int() const;
	[[nodiscard]] double to_float() const;
	[[nodiscard]] godot::String to_string() const;
	[[nodiscard]] godot::PackedByteArray to_binary() const;
	[[nodiscard]] godot::Variant to_native() const;
	[[nodiscard]] godot::Variant unwrap_native() const;

	[[nodiscard]] godot::Ref<PuertsScriptValue> get_property(const godot::StringName &p_name) const;
	void set_property(const godot::StringName &p_name, const godot::Variant &p_value);

	[[nodiscard]] godot::Ref<PuertsScriptValue> call(const godot::Array &p_args = godot::Array()) const;
	[[nodiscard]] godot::Ref<PuertsScriptValue> call_method(const godot::StringName &p_name, const godot::Array &p_args = godot::Array()) const;

private:
	friend class PuertsEnvironment;

	void initialize(PuertsEnvironment *p_environment, pesapi_ffi *p_ffi, pesapi_value_ref p_value_ref);
	void release_value_ref();
	template <typename Result, typename Function>
	Result with_value(Result p_fallback, Function &&p_function, bool p_may_reenter = false) const;
	godot::Ref<PuertsScriptValue> call_script_function(
			PuertsEnvironment *p_environment,
			pesapi_scope p_scope,
			pesapi_env p_env,
			pesapi_value p_function,
			pesapi_value p_receiver,
			const godot::Array &p_args) const;
	bool ensure_live_native_object_receiver(PuertsEnvironment *p_environment, pesapi_env p_env, pesapi_value p_value) const;
	[[nodiscard]] PuertsEnvironment *get_environment() const;
};

#endif // PUERTS_GODOT_PUERTS_SCRIPT_VALUE_H
