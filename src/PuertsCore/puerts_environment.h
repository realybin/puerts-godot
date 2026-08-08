// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_ENVIRONMENT_H
#define PUERTS_GODOT_PUERTS_ENVIRONMENT_H

#include "puerts_backend.h"
#include "puerts_eastl.h"
#include "puerts_runtime.h"
#include "puerts_string_name_cache_pool.h"

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
class PuertsTypeRegister;
class PuertsEnvironment;

struct PuertsScriptValueCacheEntry {
	PuertsScriptValue *value = nullptr;
};

namespace puerts {
godot::Variant script_to_variant(PuertsEnvironment *p_environment, pesapi_env p_env, pesapi_value p_value);
bool native_to_variant(PuertsEnvironment *p_environment, void *p_handle, const void *p_type_id, godot::Variant &r_value);
bool return_variant(pesapi_ffi *p_apis, pesapi_callback_info p_info, pesapi_env p_env, PuertsEnvironment *p_environment, const godot::Variant &p_value);
} //namespace puerts

class PuertsEnvironment : public godot::RefCounted {
	GDCLASS(PuertsEnvironment, godot::RefCounted)

	godot::Ref<godot::RefCounted> backend_ref_;
	const PuertsBackendFunctions *backend_functions_ = nullptr;
	pesapi_ffi *ffi_ = nullptr;
	pesapi_env_ref env_ref_ = nullptr;
	PuertsEnvironmentData environment_data_;
	uint32_t active_operations_ = 0;
	PuertsScriptValue *script_values_head_ = nullptr;
	puerts_eastl::hash_map<PuertsScriptValueCacheEntry *, puerts_eastl::unique_ptr<PuertsScriptValueCacheEntry>> script_value_cache_;
	godot::Callable error_callback_;
	godot::Callable warn_callback_;
	godot::Callable info_callback_;

protected:
	static void _bind_methods();

public:
	PuertsEnvironment() = default;
	~PuertsEnvironment() override;

	godot::Error initialize(godot::Object *p_backend, const godot::Ref<PuertsStringNameCachePool> &p_string_name_cache_pool);
	void dispose();
	bool is_alive() const;
	[[nodiscard]] godot::Object *get_backend() const;
	void set_error_callback(const godot::Callable &p_callback);
	void set_warn_callback(const godot::Callable &p_callback);
	void set_info_callback(const godot::Callable &p_callback);

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
	friend class PuertsTypeRegister;
	friend godot::Variant puerts::script_to_variant(PuertsEnvironment *p_environment, pesapi_env p_env, pesapi_value p_value);
	friend bool puerts::native_to_variant(PuertsEnvironment *p_environment, void *p_handle, const void *p_type_id, godot::Variant &r_value);
	friend bool puerts::return_variant(pesapi_ffi *p_apis, pesapi_callback_info p_info, pesapi_env p_env, PuertsEnvironment *p_environment, const godot::Variant &p_value);

	void log_error(const godot::String &p_message);
	void log_warn(const godot::String &p_message);
	void log_info(const godot::String &p_message);
	void emit_log(const godot::Callable &p_callback, const godot::String &p_message);
	template <void (PuertsEnvironment::*LogFunction)(const godot::String &)>
	static void dispatch_log_callback(struct pesapi_ffi *p_apis, pesapi_callback_info p_info);
	static void script_log_error_callback(struct pesapi_ffi *apis, pesapi_callback_info info);
	static void script_log_warn_callback(struct pesapi_ffi *apis, pesapi_callback_info info);
	static void script_log_info_callback(struct pesapi_ffi *apis, pesapi_callback_info info);
	static void script_to_callable_callback(struct pesapi_ffi *apis, pesapi_callback_info info);
	[[nodiscard]] const godot::CharString &get_cached_utf8(const godot::StringName &p_name);
	pesapi_value variant_to_script(
			pesapi_env p_env,
			const godot::Variant &p_value,
			bool *r_success = nullptr,
			godot::String *r_error_message = nullptr);
	godot::Variant script_to_variant(pesapi_env p_env, pesapi_value p_value);
	bool native_to_variant(void *p_handle, const void *p_type_id, godot::Variant &r_value);
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
		if (!is_alive()) {
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

	godot::Ref<PuertsStringNameCachePool> string_name_cache_pool_;
};

#endif // PUERTS_GODOT_PUERTS_ENVIRONMENT_H
