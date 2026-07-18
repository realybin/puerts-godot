// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_environment.h"

#include "puerts_bridge_registry.h"
#include "puerts_runtime.h"
#include "puerts_script_value.h"
#include "puerts_type_register.h"

#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/object.hpp>

using namespace godot;

Ref<PuertsScriptValue> PuertsEnvironment::eval(const String &p_code, const StringName &p_chunk_name) {
	if (!is_alive()) {
		log_error("Puerts environment is not initialized.");
		return {};
	}

	operation_scope operation(this);
	puerts::internal::EnvironmentScope scope(ffi_, env_ref_);
	pesapi_env env = scope.get_env();
	CharString code_utf8 = p_code.utf8();
	const CharString &chunk_name_utf8 = get_cached_utf8(p_chunk_name);
	pesapi_value result = ffi_->eval(env, reinterpret_cast<const uint8_t *>(code_utf8.get_data()), code_utf8.length(), chunk_name_utf8.get_data());

	if (ffi_->has_caught(scope.get_scope())) {
		log_error(read_exception(scope.get_scope()));
		return {};
	}

	Ref<PuertsScriptValue> value = create_script_value(env, result);
	return value;
}

void PuertsEnvironment::set_global(const StringName &p_name, const Variant &p_value) {
	if (!is_alive()) {
		log_error("Puerts environment is not initialized.");
		return;
	}

	operation_scope operation(this);
	puerts::internal::EnvironmentScope scope(ffi_, env_ref_);
	pesapi_env env = scope.get_env();
	const CharString &name_utf8 = get_cached_utf8(p_name);
	bool converted = false;
	pesapi_value script_value = variant_to_script(env, p_value, &converted, nullptr);
	if (!converted) {
		return;
	}
	ffi_->set_property(env, ffi_->global(env), name_utf8.get_data(), script_value);
	if (ffi_->has_caught(scope.get_scope())) {
		log_error(read_exception(scope.get_scope()));
		return;
	}
}

Ref<PuertsScriptValue> PuertsEnvironment::get_global(const StringName &p_name) {
	if (!is_alive()) {
		log_error("Puerts environment is not initialized.");
		return {};
	}

	operation_scope operation(this);
	puerts::internal::EnvironmentScope scope(ffi_, env_ref_);
	pesapi_env env = scope.get_env();
	const CharString &name_utf8 = get_cached_utf8(p_name);
	pesapi_value value = ffi_->get_property(env, ffi_->global(env), name_utf8.get_data());
	// Backends differ here:
	// - some report property getter failures via `has_caught(scope)`
	// - some simply return a null `pesapi_value`
	// - some may leave a stale caught flag while still returning a usable value
	// Treat only the "no value" case as a hard global read failure.
	if (value == nullptr) {
		log_error(read_exception(scope.get_scope()));
		return {};
	}

	Ref<PuertsScriptValue> result = create_script_value(env, value);
	return result;
}

pesapi_value PuertsEnvironment::variant_to_script(
		pesapi_env p_env,
		const Variant &p_value,
		bool *r_success,
		String *r_error_message) {
	auto fail = [&](const String &p_message) -> pesapi_value {
		if (r_success != nullptr) {
			*r_success = false;
		}
		if (r_error_message != nullptr) {
			*r_error_message = p_message;
		}
		log_error(p_message);
		return ffi_->create_null(p_env);
	};
	if (r_success != nullptr) {
		*r_success = true;
	}
	if (r_error_message != nullptr) {
		*r_error_message = String();
	}

	PuertsBridgeRegistry &bridge = runtime_.bridge;
	PuertsTypeRegister &type_register = PuertsTypeRegister::get_singleton();

	switch (p_value.get_type()) {
		case Variant::NIL:
			return ffi_->create_null(p_env);
		case Variant::BOOL:
			return ffi_->create_boolean(p_env, static_cast<bool>(p_value));
		case Variant::INT: {
			const auto int_value = static_cast<int64_t>(p_value);
			if (int_value >= -2147483648LL && int_value <= 2147483647LL) {
				return ffi_->create_int32(p_env, static_cast<int32_t>(int_value));
			}
			return ffi_->create_int64(p_env, int_value);
		}
		case Variant::FLOAT:
			return ffi_->create_double(p_env, static_cast<double>(p_value));
		case Variant::STRING: {
			CharString text = String(p_value).utf8();
			return ffi_->create_string_utf8(p_env, text.get_data(), text.length());
		}
		case Variant::STRING_NAME: {
			CharString text = String(StringName(p_value)).utf8();
			return ffi_->create_string_utf8(p_env, text.get_data(), text.length());
		}
#if VARIANT_TO_SCRIPT_PACKED_BYTE_ARRAY_CAST
		case Variant::PACKED_BYTE_ARRAY: {
			PackedByteArray bytes = p_value;
			return ffi_->create_binary_by_value(p_env, bytes.ptrw(), bytes.size());
		}
#endif
		case Variant::OBJECT: {
			Object *object = p_value;
			if (object == nullptr) {
				return ffi_->create_null(p_env);
			}
			auto *script_value = Object::cast_to<PuertsScriptValue>(object);
			if (script_value != nullptr) {
				if (!script_value->is_valid()) {
					return fail("Puerts script value is no longer valid.");
				}
				if (script_value->get_environment() != this) {
					return fail("Puerts script value belongs to another PuertsEnvironment.");
				}
				return ffi_->get_value_from_ref(p_env, script_value->value_ref_);
			}

			void *handle = nullptr;
			if (const void *type_id = nullptr; bridge.find_object(object, handle, type_id)) {
				return ffi_->native_object_to_value(p_env, type_id, handle, false);
			}

			const void *type_id = type_register.find_or_add_object_type(object->get_class());
			if (type_register.ensure_registered(type_id)) {
				handle = bridge.bind_object(object, type_id);
				return ffi_->native_object_to_value(p_env, type_id, handle, false);
			}

			return fail("Failed to register object type for script conversion.");
		}
		default:
			const void *type_id = type_register.get_builtin_type_id(p_value.get_type());
			if (type_register.ensure_registered(type_id)) {
				void *handle = bridge.box_variant(p_value, type_id);
				return ffi_->native_object_to_value(p_env, type_id, handle, false);
			}
			return fail("Failed to register builtin type for script conversion.");
	}
}

Variant PuertsEnvironment::script_to_variant(pesapi_env p_env, pesapi_value p_value) {
	if (ffi_->is_null(p_env, p_value) || ffi_->is_undefined(p_env, p_value)) {
		return {};
	}
	if (ffi_->is_boolean(p_env, p_value)) {
		return ffi_->get_value_bool(p_env, p_value) != 0;
	}
	if (ffi_->is_int32(p_env, p_value)) {
		return ffi_->get_value_int32(p_env, p_value);
	}
	if (ffi_->is_uint32(p_env, p_value)) {
		return ffi_->get_value_uint32(p_env, p_value);
	}
	if (ffi_->is_int64(p_env, p_value)) {
		return ffi_->get_value_int64(p_env, p_value);
	}
	if (ffi_->is_uint64(p_env, p_value)) {
		return ffi_->get_value_uint64(p_env, p_value);
	}
	if (ffi_->is_double(p_env, p_value)) {
		return ffi_->get_value_double(p_env, p_value);
	}
	if (ffi_->is_string(p_env, p_value)) {
		return read_string(p_env, p_value);
	}
#if SCRIPT_TO_VARIANT_PACKED_BYTE_ARRAY_CAST
	if (ffi_->is_binary(p_env, p_value)) {
		return read_binary(p_env, p_value);
	}
#endif
	void *handle = ffi_->get_native_object_ptr(p_env, p_value);
	if (handle != nullptr) {
		if (const void *type_id = ffi_->get_native_object_typeid(p_env, p_value); type_id != nullptr) {
			Variant native_value;
			if (native_to_variant(handle, type_id, native_value)) {
				return native_value;
			}
		}
	}

	return create_script_value(p_env, p_value);
}

bool PuertsEnvironment::native_to_variant(void *p_handle, const void *p_type_id, Variant &r_value) {
	if (runtime_.bridge.get_variant(p_handle, p_type_id, r_value)) {
		return true;
	}
	if (PuertsBridgeRegistry::is_handle(p_handle)) {
		return false;
	}
	return PuertsTypeRegister::get_singleton().native_to_variant(p_handle, p_type_id, r_value);
}

bool puerts::native_to_variant(PuertsEnvironment *p_environment, void *p_handle, const void *p_type_id, Variant &r_value) {
	return p_environment->native_to_variant(p_handle, p_type_id, r_value);
}

Variant puerts::script_to_variant(PuertsEnvironment *p_environment, pesapi_env p_env, pesapi_value p_value) {
	return p_environment->script_to_variant(p_env, p_value);
}

bool puerts::return_variant(
		pesapi_ffi *p_apis,
		pesapi_callback_info p_info,
		pesapi_env p_env,
		PuertsEnvironment *p_environment,
		const Variant &p_value) {
	bool converted = false;
	String error;
	pesapi_value value = p_environment->variant_to_script(p_env, p_value, &converted, &error);
	if (!converted) {
		const CharString message = (error.is_empty() ? String("Failed to convert Variant to script value.") : error).utf8();
		p_apis->throw_by_string(p_info, message.get_data());
		return false;
	}
	p_apis->add_return(p_info, value);
	return true;
}

Ref<PuertsScriptValue> PuertsEnvironment::create_script_value(pesapi_env p_env, pesapi_value p_value) {
	bool cacheable = p_value != nullptr && ffi_->is_object(p_env, p_value);
	if (!cacheable && p_value != nullptr) {
		cacheable = ffi_->get_native_object_ptr(p_env, p_value) != nullptr &&
				ffi_->get_native_object_typeid(p_env, p_value) != nullptr;
	}
	void *cache_token = nullptr;
	if (cacheable) {
		void *private_ptr = nullptr;
		if (ffi_->get_private(p_env, p_value, &private_ptr)) {
			auto cached = cached_script_values_.find(private_ptr);
			if (cached != cached_script_values_.end()) {
				return Ref<PuertsScriptValue>(cached->second);
			}
			if (is_script_value_cache_token(private_ptr)) {
				cache_token = private_ptr;
			}
		}
	}

	pesapi_value_ref value_ref = ffi_->create_value_ref(p_env, p_value, 0);
	if (value_ref == nullptr) {
		return {};
	}

	Ref<PuertsScriptValue> script_value;
	script_value.instantiate();
	script_value->initialize(this, ffi_, value_ref);
	register_script_value(script_value.ptr());
	if (cacheable) {
		const bool attach_token = cache_token == nullptr;
		if (attach_token) {
			cache_token = take_script_value_cache_token();
		}
		if (cache_token != nullptr) {
			cached_script_values_.insert({ cache_token, script_value.ptr() });
			void *attached_token = nullptr;
			const bool attached = !attach_token ||
					(ffi_->set_private(p_env, p_value, cache_token) &&
							ffi_->get_private(p_env, p_value, &attached_token) && attached_token == cache_token);
			if (attached) {
				script_value->cache_token_ = cache_token;
			} else {
				cached_script_values_.erase(cache_token);
			}
		}
	}
	return script_value;
}

String PuertsEnvironment::read_exception(pesapi_scope p_scope) const {
	const char *message = ffi_->get_exception_as_string(p_scope, true);
	if (message == nullptr) {
		return "Unknown script exception.";
	}
	return String::utf8(message);
}

String PuertsEnvironment::read_string(pesapi_env p_env, pesapi_value p_value) const {
	return puerts::internal::read_utf8_string(ffi_, p_env, p_value);
}

PackedByteArray PuertsEnvironment::read_binary(pesapi_env p_env, pesapi_value p_value) const {
	size_t size = 0;
	void *buffer = ffi_->get_value_binary(p_env, p_value, &size);
	PackedByteArray bytes;
	bytes.resize(static_cast<int>(size));
	if (size > 0 && buffer != nullptr) {
		memcpy(bytes.ptrw(), buffer, size);
	}
	return bytes;
}
