// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_environment.h"

#include "puerts_bridge_registry.h"
#include "puerts_runtime.h"
#include "puerts_script_value.h"
#include "puerts_script_value_cache.h"
#include "puerts_type_register.h"

#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>

using namespace godot;

namespace {

Ref<PuertsScriptValue> reuse_cached_script_value(puerts::script_value_cache::Entry *p_cache_entry) {
	if (p_cache_entry == nullptr || !p_cache_entry->wrapper_id.is_valid()) {
		return {};
	}

	Object *object = ObjectDB::get_instance(p_cache_entry->wrapper_id);
	auto *script_value = Object::cast_to<PuertsScriptValue>(object);
	if (script_value == nullptr || !script_value->is_valid()) {
		return {};
	}

	Ref<PuertsScriptValue> cached_value = Variant(script_value);
	return cached_value;
}

void release_stale_cached_script_value(
		pesapi_ffi *p_ffi,
		pesapi_env p_env,
		pesapi_value p_value,
		puerts::script_value_cache::Entry *p_cache_entry) {
	if (p_cache_entry == nullptr) {
		return;
	}

	p_ffi->set_private(p_env, p_value, nullptr);
	if (p_cache_entry->value_ref != nullptr) {
		puerts::script_value_cache::clear_value_ref_link(p_ffi, p_cache_entry->value_ref);
		p_ffi->release_value_ref(p_cache_entry->value_ref);
	}
	puerts::script_value_cache::unregister_entry(p_cache_entry);
	memdelete(p_cache_entry);
}

} // namespace

Ref<PuertsScriptValue> PuertsEnvironment::eval(const String &p_code, const StringName &p_chunk_name) {
	if (!is_alive()) {
		log_error("Puerts environment is not initialized.");
		return {};
	}

	pesapi_scope scope = ffi_->open_scope(env_ref_);
	pesapi_env env = ffi_->get_env_from_ref(env_ref_);
	CharString code_utf8 = p_code.utf8();
	const CharString &chunk_name_utf8 = get_cached_utf8(p_chunk_name);
	pesapi_value result = ffi_->eval(env, reinterpret_cast<const uint8_t *>(code_utf8.get_data()), code_utf8.length(), chunk_name_utf8.get_data());

	if (ffi_->has_caught(scope)) {
		log_error(read_exception(scope));
		ffi_->close_scope(scope);
		return {};
	}

	Ref<PuertsScriptValue> value = create_script_value(env, result);
	ffi_->close_scope(scope);
	return value;
}

void PuertsEnvironment::set_global(const StringName &p_name, const Variant &p_value) {
	if (!is_alive()) {
		log_error("Puerts environment is not initialized.");
		return;
	}

	pesapi_scope scope = ffi_->open_scope(env_ref_);
	pesapi_env env = ffi_->get_env_from_ref(env_ref_);
	const CharString &name_utf8 = get_cached_utf8(p_name);
	bool converted = false;
	pesapi_value script_value = variant_to_script(env, p_value, &converted, nullptr);
	if (!converted) {
		ffi_->close_scope(scope);
		return;
	}
	ffi_->set_property(env, ffi_->global(env), name_utf8.get_data(), script_value);
	if (ffi_->has_caught(scope)) {
		log_error(read_exception(scope));
		ffi_->close_scope(scope);
		return;
	}
	ffi_->close_scope(scope);
}

Ref<PuertsScriptValue> PuertsEnvironment::get_global(const StringName &p_name) {
	if (!is_alive()) {
		log_error("Puerts environment is not initialized.");
		return {};
	}

	pesapi_scope scope = ffi_->open_scope(env_ref_);
	pesapi_env env = ffi_->get_env_from_ref(env_ref_);
	const CharString &name_utf8 = get_cached_utf8(p_name);
	pesapi_value value = ffi_->get_property(env, ffi_->global(env), name_utf8.get_data());
	// Backends differ here:
	// - some report property getter failures via `has_caught(scope)`
	// - some simply return a null `pesapi_value`
	// - some may leave a stale caught flag while still returning a usable value
	// Treat only the "no value" case as a hard global read failure.
	if (value == nullptr) {
		log_error(read_exception(scope));
		ffi_->close_scope(scope);
		return {};
	}

	Ref<PuertsScriptValue> result = create_script_value(env, value);
	ffi_->close_scope(scope);
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

	PuertsBridgeRegistry *bridge = env_private_->bridge;
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
			if (const void *type_id = nullptr; bridge->find_object(object, handle, type_id)) {
				return ffi_->native_object_to_value(p_env, type_id, handle, false);
			}

			auto *type_info = type_register.find_or_add_object_type(object->get_class());
			if (type_info != nullptr && type_register.ensure_registered(type_info)) {
				handle = bridge->bind_object(object, type_info->type_id);
				return ffi_->native_object_to_value(p_env, type_info->type_id, handle, false);
			}

			return fail("Failed to register object type for script conversion.");
		}
		default:
			auto *type_info = type_register.get_builtin_type(p_value.get_type());
			if (type_info != nullptr && type_register.ensure_registered(type_info)) {
				void *handle = bridge->box_variant(p_value, type_info->type_id);
				return ffi_->native_object_to_value(p_env, type_info->type_id, handle, false);
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
	PuertsBridgeRegistry *bridge = env_private_->bridge;
	PuertsTypeRegister &type_register = PuertsTypeRegister::get_singleton();
	void *handle = ffi_->get_native_object_ptr(p_env, p_value);
	if (handle != nullptr) {
		if (const void *type_id = ffi_->get_native_object_typeid(p_env, p_value); type_id != nullptr) {
			Variant native_value;
			if (bridge->get_variant(handle, type_id, native_value)) {
				return native_value;
			}
			auto *type_info = type_register.get_type_by_id(type_id);
			if (type_info != nullptr && type_info->kind == PuertsTypeRegister::TypeInfo::Kind::STATIC_BOUND && type_info->native_to_variant != nullptr) {
				return type_info->native_to_variant(handle);
			}
		}
	}

	return create_script_value(p_env, p_value);
}

Ref<PuertsScriptValue> PuertsEnvironment::create_script_value(pesapi_env p_env, pesapi_value p_value) {
	auto make_script_value_wrapper = [this](pesapi_value_ref p_value_ref, puerts::script_value_cache::Entry *p_cache_entry = nullptr) -> Ref<PuertsScriptValue> {
		if (p_value_ref == nullptr) {
			return {};
		}

		Ref<PuertsScriptValue> script_value;
		script_value.instantiate();
		Ref environment_ref(this);
		script_value->initialize(environment_ref, ffi_, p_value_ref);
		if (p_cache_entry != nullptr) {
			p_cache_entry->wrapper_id = ObjectID(script_value->get_instance_id());
		}
		register_script_value(ObjectID(script_value->get_instance_id()));
		return script_value;
	};

	if (puerts::script_value_cache::can_attach_cache_entry(ffi_, p_env, p_value)) {
		void *private_ptr = nullptr;
		if (ffi_->get_private(p_env, p_value, &private_ptr)) {
			if (auto *cache_entry = puerts::script_value_cache::entry_from_raw_ptr(private_ptr); cache_entry != nullptr) {
				Ref<PuertsScriptValue> cached_value = reuse_cached_script_value(cache_entry);
				if (cached_value.is_valid()) {
					return cached_value;
				}

				release_stale_cached_script_value(ffi_, p_env, p_value, cache_entry);
			}
		}

		pesapi_value_ref value_ref = ffi_->create_value_ref(p_env, p_value, 1);
		if (value_ref == nullptr) {
			return {};
		}

		auto *cache_entry = memnew(puerts::script_value_cache::Entry);
		cache_entry->value_ref = value_ref;
		puerts::script_value_cache::register_entry(cache_entry);

		uint32_t field_count = 0;
		void **fields = ffi_->get_ref_internal_fields(value_ref, &field_count);
		(void)field_count;
		fields[0] = cache_entry;

		if (!ffi_->set_private(p_env, p_value, cache_entry)) {
			fields[0] = nullptr;
			puerts::script_value_cache::unregister_entry(cache_entry);
			memdelete(cache_entry);
			ffi_->release_value_ref(value_ref);
			return make_script_value_wrapper(ffi_->create_value_ref(p_env, p_value, 0));
		}

		return make_script_value_wrapper(value_ref, cache_entry);
	}

	return make_script_value_wrapper(ffi_->create_value_ref(p_env, p_value, 0));
}

String PuertsEnvironment::read_exception(pesapi_scope p_scope) const {
	const char *message = ffi_->get_exception_as_string(p_scope, true);
	if (message == nullptr) {
		return "Unknown script exception.";
	}
	return String::utf8(message);
}

String PuertsEnvironment::read_string(pesapi_env p_env, pesapi_value p_value) const {
	size_t size = 0;
	const char *inline_text = ffi_->get_value_string_utf8(p_env, p_value, nullptr, &size);
	if (inline_text != nullptr) {
		return String::utf8(inline_text, static_cast<int>(size));
	}

	char *buffer = memnew_arr(char, size + 1);
	ffi_->get_value_string_utf8(p_env, p_value, buffer, &size);
	buffer[size] = 0;
	String result = String::utf8(buffer, static_cast<int>(size));
	memdelete_arr(buffer);
	return result;
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
