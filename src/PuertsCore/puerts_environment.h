// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_ENVIRONMENT_H
#define PUERTS_GODOT_PUERTS_ENVIRONMENT_H

#include "puerts_backend.h"
#include "puerts_eastl.h"
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

class PuertsScriptValue;
class PuertsBridgeRegistry;
struct PuertsEnvPrivate;
class PuertsTypeRegister;
class PuertsEnvironment;
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
	PuertsEnvPrivate *env_private_ = nullptr;
	bool disposing_ = false;
	bool dispose_requested_ = false;
	uint32_t active_operations_ = 0;
	puerts_eastl::hash_set<PuertsScriptValue *> script_values_;
	puerts_eastl::hash_map<void *, PuertsScriptValue *> cached_script_values_;
	uintptr_t next_script_value_cache_id_ = 1;
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
	class operation_scope {
	public:
		explicit operation_scope(PuertsEnvironment *p_environment);
		~operation_scope();

		operation_scope(const operation_scope &) = delete;
		operation_scope &operator=(const operation_scope &) = delete;

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
	static void script_log_error_callback(struct pesapi_ffi *apis, pesapi_callback_info info);
	static void script_log_warn_callback(struct pesapi_ffi *apis, pesapi_callback_info info);
	static void script_log_info_callback(struct pesapi_ffi *apis, pesapi_callback_info info);
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
	bool can_use_backend_function(bool p_supported, const godot::String &p_error_message);
	void end_operation();
	void dispose_internal();
	void *take_script_value_cache_token();
	bool is_script_value_cache_token(void *p_token) const;
	void register_script_value(PuertsScriptValue *p_value);
	void unregister_script_value(PuertsScriptValue *p_value);
	void invalidate_script_values();
	godot::Ref<PuertsStringNameCachePool> string_name_cache_pool_;
};

#endif // PUERTS_GODOT_PUERTS_ENVIRONMENT_H
