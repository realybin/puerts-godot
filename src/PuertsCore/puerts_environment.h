// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_ENVIRONMENT_H
#define PUERTS_GODOT_PUERTS_ENVIRONMENT_H

#include "puerts_backend.h"
#include "puerts_eastl.h"
#include "puerts_runtime.h"
#include "puerts_string_name_utf8_cache.h"

#include <cstdint>

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <EASTL/type_traits.h>
#include <EASTL/utility.h>

class PuertsScriptValue;
class PuertsTypeRegistry;
class PuertsEnvironment;

namespace puerts {
godot::Variant script_value_to_variant(PuertsEnvironment *p_environment, pesapi_env p_env, pesapi_value p_value);
bool return_variant_to_script(pesapi_ffi *p_api, pesapi_callback_info p_info, pesapi_env p_env, PuertsEnvironment *p_environment, const godot::Variant &p_value);
} //namespace puerts

class PuertsEnvironment : public godot::RefCounted {
	GDCLASS(PuertsEnvironment, godot::RefCounted)

public:
	enum LogLevel {
		LOG_LEVEL_TRACE,
		LOG_LEVEL_DEBUG,
		LOG_LEVEL_INFO,
		LOG_LEVEL_WARNING,
		LOG_LEVEL_ERROR,
	};

private:
	godot::Ref<godot::RefCounted> backend_ref_;
	// Non-disposable browser runtimes keep their owner alive for the page lifetime.
	godot::Ref<PuertsEnvironment> page_lifetime_ref_;
	const PuertsBackendFunctionTable *backend_functions_ = nullptr;
	pesapi_ffi *api_ = nullptr;
	pesapi_env_ref env_ref_ = nullptr;
	PuertsEnvironmentContext environment_context_;
	uint32_t active_operations_ = 0;
	bool reporting_lifecycle_rejection_ = false;
	puerts_eastl::vector<PuertsScriptValue *> script_values_;
	puerts_eastl::hash_map<void *, PuertsScriptValue *> script_value_cache_;
	// Keys occupy the upper address range and are never reused.
	uintptr_t next_cache_key_ = (UINTPTR_MAX >> 2U) + 1;
	godot::Callable log_callback_;
	LogLevel log_level_ = LOG_LEVEL_INFO;
	puerts_eastl::hash_map<int32_t, LogLevel> category_log_levels_;
	bool reporting_log_ = false;

protected:
	static void _bind_methods();

public:
	PuertsEnvironment() = default;
	~PuertsEnvironment() override;

	godot::Error initialize(godot::Object *p_backend, const godot::Ref<PuertsStringNameUtf8Cache> &p_string_name_utf8_cache);
	void dispose();
	bool is_ready() const;
	[[nodiscard]] godot::Object *get_backend() const;
	void set_log_callback(const godot::Callable &p_callback);
	void set_log_level(LogLevel p_level);
	void set_category_log_level(int32_t p_category, LogLevel p_level);
	void clear_category_log_level(int32_t p_category);

	godot::Ref<PuertsScriptValue> eval(const godot::String &p_code, const godot::StringName &p_chunk_name = godot::StringName("chunk"));

	void set_global(const godot::StringName &p_name, const godot::Variant &p_value);
	[[nodiscard]] godot::Ref<PuertsScriptValue> get_global(const godot::StringName &p_name);

	void tick();
	void low_memory_notification();
	void open_debugger(int32_t p_port);
	bool debugger_tick();
	void close_debugger();
	void terminate_execution();

private:
	class OperationScope {
	public:
		explicit OperationScope(PuertsEnvironment *p_environment);
		~OperationScope();

		OperationScope(const OperationScope &) = delete;
		OperationScope &operator=(const OperationScope &) = delete;

	private:
		PuertsEnvironment *environment_ = nullptr;
		godot::Ref<PuertsEnvironment> keep_alive_;
	};

	friend class PuertsScriptValue;
	friend class PuertsTypeRegistry;
	friend godot::Variant puerts::script_value_to_variant(PuertsEnvironment *p_environment, pesapi_env p_env, pesapi_value p_value);
	friend bool puerts::return_variant_to_script(pesapi_ffi *p_api, pesapi_callback_info p_info, pesapi_env p_env, PuertsEnvironment *p_environment, const godot::Variant &p_value);

	void log_error(const godot::String &p_message);
	void log_lifecycle_rejection(const godot::String &p_message);
	void emit_log(int32_t p_category, LogLevel p_level, const godot::String &p_message);
	template <LogLevel Level>
	static void script_log_callback(pesapi_ffi *p_api, pesapi_callback_info p_info);
	static void script_to_callable_callback(struct pesapi_ffi *apis, pesapi_callback_info info);
	[[nodiscard]] godot::CharString get_utf8(const godot::StringName &p_name);
	pesapi_value variant_to_script(
			pesapi_env p_env,
			const godot::Variant &p_value,
			bool *r_success = nullptr,
			godot::String *r_error_message = nullptr);
	godot::Variant script_value_to_variant(pesapi_env p_env, pesapi_value p_value);
	bool native_handle_to_variant(void *p_handle, const void *p_type_id, godot::Variant &r_value);
	godot::Ref<PuertsScriptValue> create_script_value(pesapi_env p_env, pesapi_value p_value);
	godot::String read_exception(pesapi_scope p_scope) const;
	godot::String read_string(pesapi_env p_env, pesapi_value p_value) const;
	godot::PackedByteArray read_binary(pesapi_env p_env, pesapi_value p_value) const;
	void end_operation();
	void dispose_internal();
	void register_script_value(PuertsScriptValue *p_value);
	void unregister_script_value(PuertsScriptValue *p_value);
	void invalidate_script_values();

	template <typename Function, typename... Args>
	bool call_backend(Function p_function, const godot::String &p_error_message, Args &&...p_args) {
		if (!is_ready()) {
			log_error("Puerts environment is not initialized.");
			return false;
		}
		if (p_function == nullptr) {
			log_error(p_error_message);
			return false;
		}

		OperationScope operation(this);
		if constexpr (eastl::is_void_v<decltype(p_function(env_ref_, eastl::forward<Args>(p_args)...))>) {
			p_function(env_ref_, eastl::forward<Args>(p_args)...);
			return true;
		} else {
			return p_function(env_ref_, eastl::forward<Args>(p_args)...);
		}
	}

	godot::Ref<PuertsStringNameUtf8Cache> string_name_utf8_cache_;
};

VARIANT_ENUM_CAST(PuertsEnvironment::LogLevel);

#endif // PUERTS_GODOT_PUERTS_ENVIRONMENT_H
