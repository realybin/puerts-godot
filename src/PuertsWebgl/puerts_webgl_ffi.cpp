// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_webgl_ffi.h"

#include "ScriptClassRegistry.h"
#include "puerts_webgl_runtime.generated.h"

#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <emscripten.h>

#include <algorithm>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

namespace {

// EM_JS is carried in the side module and installed by Emscripten's dynamic
// loader. Pass function pointers explicitly: main-module exports belong to Godot.
// clang-format off
EM_JS(int, webgl_initialize_bridge, (const char *source, uintptr_t describe, uintptr_t callback, uintptr_t construct, uintptr_t finalize), {
	if (typeof document === 'undefined') {
		console.error('Puerts WebGL requires the browser main thread.');
		return 0;
	}
	if (Module['puertsGodotWebgl'])
		return 1;
	const heap = () => {
		if (HEAPU8.buffer !== wasmMemory.buffer)
			updateMemoryViews();
		return HEAPU8;
	};
	const readUTF8 = (ptr, length) => {
		if (!ptr)
			return "";
		const bytes = heap();
		if (length === undefined) {
			length = 0;
			while (bytes[ptr + length])
				++length;
		}
		const text = bytes.subarray(ptr, ptr + length);
		return new TextDecoder('utf-8', { ignoreBOM: true }).decode(text.buffer instanceof ArrayBuffer ? text : text.slice());
	};
	const call = (ptr, ...args) => wasmTable.get(ptr)(...args);
	try {
		const factory = (0, eval)('(' + readUTF8(source) + ')');
		Module['puertsGodotWebgl'] = factory({
			heap,
			readUTF8,
			call,
			describe: (env, type) => call(describe, env, type),
			callback: (env, fn, info) => call(callback, env, fn, info),
			construct: (env, fn, info) => call(construct, env, fn, info),
			finalize: (env, fn, ptr, data, privateData) => call(finalize, env, fn, ptr, data, privateData)
		});
		return 1;
	} catch (error) {
		console.error('Puerts WebGL bridge initialization failed:', error);
		return 0;
	}
});

EM_JS(double, webgl_dispatch, (uintptr_t env, const char *op, double a, double b, double c, double d), {
	if (HEAPU8.buffer !== wasmMemory.buffer)
		updateMemoryViews();
	let end = op;
	while (HEAPU8[end])
		++end;
	const bytes = HEAPU8.subarray(op, end);
	const name = new TextDecoder().decode(bytes.buffer instanceof ArrayBuffer ? bytes : bytes.slice());
	return Module['puertsGodotWebgl'].dispatch(env, name, a, b, c, d);
});
// clang-format on

struct WebScope;

struct WebEnvironment {
	bool alive = true;
	uint32_t references = 1;
	pesapi_registry registry = nullptr;
	const void *private_data = nullptr;
	std::vector<WebScope *> scopes;
};

struct ScopeData {
	std::deque<std::string> strings;
	std::deque<std::u16string> strings16;
	std::deque<std::vector<uint8_t>> buffers;
};

struct WebScope {
	WebEnvironment *environment;
	ScopeData *data;
	uint32_t handle;
	bool allocated;
};

static_assert(sizeof(WebScope) <= sizeof(pesapi_scope_memory));

struct WebValueReference {
	WebEnvironment *environment;
	uint32_t handle;
	uint32_t references = 1;
	std::vector<void *> fields;
};

struct WebCallbackInfo {
	WebEnvironment *environment;
	uint32_t handle;
};

pesapi_ffi ffi;
WebEnvironment *browser_environment = nullptr;

template <typename T>
uintptr_t address(T p_pointer) {
	return reinterpret_cast<uintptr_t>(p_pointer);
}

WebEnvironment *environment(pesapi_env p_env) {
	return reinterpret_cast<WebEnvironment *>(p_env);
}

double dispatch(WebEnvironment *p_env, const char *p_operation, double p_a = 0, double p_b = 0, double p_c = 0, double p_d = 0) {
	return p_env != nullptr && p_env->alive ? webgl_dispatch(address(p_env), p_operation, p_a, p_b, p_c, p_d) : 0;
}

pesapi_value value(double p_handle) {
	return reinterpret_cast<pesapi_value>(static_cast<uintptr_t>(p_handle));
}

ScopeData &scope_data(WebEnvironment *p_env) {
	return *p_env->scopes.back()->data;
}

void retain(WebEnvironment *p_env) {
	++p_env->references;
}

void release(WebEnvironment *p_env) {
	if (--p_env->references == 0) {
		delete p_env;
	}
}

// JavaScript opens its own handle scope before entering a native callback. This
// native scratch scope must not discard the return handle before JS reads it.
struct CallbackScratchScope {
	WebScope scope;
	explicit CallbackScratchScope(WebEnvironment *p_env) : scope{ p_env, new ScopeData, 0, false } {
		retain(p_env);
		p_env->scopes.push_back(&scope);
	}
	~CallbackScratchScope() {
		scope.environment->scopes.pop_back();
		delete scope.data;
		release(scope.environment);
	}
};

extern "C" void webgl_native_callback(WebEnvironment *p_env, pesapi_callback p_callback, uint32_t p_info) {
	if (!p_env->alive) {
		return;
	}
	CallbackScratchScope scope(p_env);
	WebCallbackInfo info{ p_env, p_info };
	p_callback(&ffi, reinterpret_cast<pesapi_callback_info>(&info));
}

extern "C" void *webgl_native_construct(WebEnvironment *p_env, pesapi_constructor p_constructor, uint32_t p_info) {
	if (!p_env->alive || p_constructor == nullptr) {
		return nullptr;
	}
	CallbackScratchScope scope(p_env);
	WebCallbackInfo info{ p_env, p_info };
	return p_constructor(&ffi, reinterpret_cast<pesapi_callback_info>(&info));
}

extern "C" void webgl_native_finalize(WebEnvironment *p_env, pesapi_finalize p_finalize, void *p_ptr, void *p_data, void *p_private) {
	if (p_env->alive && p_finalize != nullptr) {
		CallbackScratchScope scope(p_env);
		p_finalize(&ffi, p_ptr, p_data, p_private);
	}
}

template <typename T>
godot::Array describe_functions(T *p_functions) {
	godot::Array result;
	if (p_functions != nullptr) {
		for (auto *item = p_functions; item->Name != nullptr; ++item) {
			godot::Dictionary function;
			function["name"] = godot::String::utf8(item->Name);
			function["callback"] = int64_t(address(item->Callback));
			function["data"] = int64_t(address(item->Data));
			result.append(function);
		}
	}
	return result;
}

godot::Array describe_properties(puerts::ScriptPropertyInfo *p_properties) {
	godot::Array result;
	if (p_properties != nullptr) {
		for (auto *item = p_properties; item->Name != nullptr; ++item) {
			godot::Dictionary property;
			property["name"] = godot::String::utf8(item->Name);
			property["getter"] = int64_t(address(item->Getter));
			property["setter"] = int64_t(address(item->Setter));
			property["getter_data"] = int64_t(address(item->GetterData));
			property["setter_data"] = int64_t(address(item->SetterData));
			result.append(property);
		}
	}
	return result;
}

extern "C" const char *webgl_describe_class(WebEnvironment *p_env, const void *p_type) {
	const auto *definition = puerts::LoadClassByID(reinterpret_cast<puerts::ScriptClassRegistry *>(p_env->registry), p_type);
	if (definition == nullptr) {
		return nullptr;
	}
	godot::Dictionary result;
	result["type"] = int64_t(address(definition->TypeId));
	result["super"] = int64_t(address(definition->SuperTypeId));
	result["name"] = godot::String::utf8(definition->ScriptName);
	result["constructor"] = int64_t(address(definition->Initialize));
	result["finalize"] = int64_t(address(definition->Finalize));
	result["data"] = int64_t(address(definition->Data));
	result["enter"] = int64_t(address(definition->OnEnter));
	result["exit"] = int64_t(address(definition->OnExit));
	result["methods"] = describe_functions(definition->Methods);
	result["functions"] = describe_functions(definition->Functions);
	result["properties"] = describe_properties(definition->Properties);
	result["variables"] = describe_properties(definition->Variables);
	const godot::CharString json = godot::JSON::stringify(result).utf8();
	auto &strings = scope_data(p_env).strings;
	strings.emplace_back(json.get_data(), json.length());
	return strings.back().c_str();
}

pesapi_scope open_scope_placement(pesapi_env_ref p_ref, pesapi_scope_memory *p_memory) {
	auto *env = reinterpret_cast<WebEnvironment *>(p_ref);
	auto *scope = new (p_memory) WebScope{ env, new ScopeData, static_cast<uint32_t>(dispatch(env, "open_scope")), false };
	retain(env);
	env->scopes.push_back(scope);
	return reinterpret_cast<pesapi_scope>(scope);
}

pesapi_scope open_scope(pesapi_env_ref p_ref) {
	auto *memory = new pesapi_scope_memory;
	auto *scope = reinterpret_cast<WebScope *>(open_scope_placement(p_ref, memory));
	scope->allocated = true;
	return reinterpret_cast<pesapi_scope>(scope);
}

void close_scope_placement(pesapi_scope p_scope) {
	auto *scope = reinterpret_cast<WebScope *>(p_scope);
	auto *env = scope->environment;
	dispatch(env, "close_scope", scope->handle);
	env->scopes.pop_back();
	delete scope->data;
	scope->~WebScope();
	release(env);
}

void close_scope(pesapi_scope p_scope) {
	const bool allocated = reinterpret_cast<WebScope *>(p_scope)->allocated;
	close_scope_placement(p_scope);
	if (allocated) {
		delete reinterpret_cast<pesapi_scope_memory *>(p_scope);
	}
}

int has_caught(pesapi_scope p_scope) {
	auto *scope = reinterpret_cast<WebScope *>(p_scope);
	return dispatch(scope->environment, "has_caught", scope->handle);
}

const char *get_exception_as_string(pesapi_scope p_scope, int p_stack) {
	auto *scope = reinterpret_cast<WebScope *>(p_scope);
	const size_t size = dispatch(scope->environment, "exception", scope->handle, p_stack);
	scope->data->strings.emplace_back(size, '\0');
	auto &text = scope->data->strings.back();
	dispatch(scope->environment, "exception", scope->handle, p_stack, address(text.data()), size);
	return text.c_str();
}

#define WEB_CREATE0(name) \
	pesapi_value name(pesapi_env p_env) { return value(dispatch(environment(p_env), #name)); }
#define WEB_CREATE1(name, type) \
	pesapi_value name(pesapi_env p_env, type p_value) { return value(dispatch(environment(p_env), #name, p_value)); }
#define WEB_GET(name, type) \
	type name(pesapi_env p_env, pesapi_value p_value) { return static_cast<type>(dispatch(environment(p_env), #name, address(p_value))); }

WEB_CREATE0(create_null)
WEB_CREATE0(create_undefined)
WEB_CREATE0(create_array)
WEB_CREATE0(create_object)
WEB_CREATE0(global)
WEB_CREATE1(create_boolean, int)
WEB_CREATE1(create_int32, int32_t)
WEB_CREATE1(create_uint32, uint32_t)
WEB_CREATE1(create_double, double)
WEB_GET(get_value_bool, int)
WEB_GET(get_value_int32, int32_t)
WEB_GET(get_value_uint32, uint32_t)
WEB_GET(get_value_double, double)
WEB_GET(get_array_length, uint32_t)
WEB_GET(is_null, int)
WEB_GET(is_undefined, int)
WEB_GET(is_boolean, int)
WEB_GET(is_int32, int)
WEB_GET(is_uint32, int)
WEB_GET(is_int64, int)
WEB_GET(is_uint64, int)
WEB_GET(is_double, int)
WEB_GET(is_string, int)
WEB_GET(is_object, int)
WEB_GET(is_function, int)
WEB_GET(is_binary, int)
WEB_GET(is_array, int)
WEB_GET(is_boxed_value, int)

pesapi_value create_int64(pesapi_env p_env, int64_t p_value) {
	const auto bits = static_cast<uint64_t>(p_value);
	return value(dispatch(environment(p_env), "create_int64", uint32_t(bits), uint32_t(bits >> 32)));
}

pesapi_value create_uint64(pesapi_env p_env, uint64_t p_value) {
	return value(dispatch(environment(p_env), "create_uint64", uint32_t(p_value), uint32_t(p_value >> 32)));
}

uint64_t get_value_uint64(pesapi_env p_env, pesapi_value p_value) {
	const uint64_t low = static_cast<uint32_t>(dispatch(environment(p_env), "get_uint64_low", address(p_value)));
	const uint64_t high = static_cast<uint32_t>(dispatch(environment(p_env), "get_uint64_high", address(p_value)));
	return low | (high << 32);
}

int64_t get_value_int64(pesapi_env p_env, pesapi_value p_value) {
	const uint64_t bits = get_value_uint64(p_env, p_value);
	int64_t result;
	std::memcpy(&result, &bits, sizeof(result));
	return result;
}

pesapi_value create_string_utf8(pesapi_env p_env, const char *p_text, size_t p_size) {
	return value(dispatch(environment(p_env), "create_string_utf8", address(p_text), p_size));
}

pesapi_value create_string_utf16(pesapi_env p_env, const uint16_t *p_text, size_t p_size) {
	return value(dispatch(environment(p_env), "create_string_utf16", address(p_text), p_size));
}

const char *get_value_string_utf8(pesapi_env p_env, pesapi_value p_value, char *p_buffer, size_t *p_size) {
	auto *env = environment(p_env);
	if (p_buffer != nullptr) {
		*p_size = dispatch(env, "string_utf8", address(p_value), address(p_buffer), *p_size);
		return p_buffer;
	}
	// Coercion can invoke user code. Query and copy the same string snapshot.
	const double snapshot = dispatch(env, "string_snapshot", address(p_value));
	const size_t size = dispatch(env, "string_utf8", snapshot);
	auto &strings = scope_data(env).strings;
	strings.emplace_back(size, '\0');
	dispatch(env, "string_utf8", snapshot, address(strings.back().data()), size);
	*p_size = size;
	return strings.back().c_str();
}

const uint16_t *get_value_string_utf16(pesapi_env p_env, pesapi_value p_value, uint16_t *p_buffer, size_t *p_size) {
	auto *env = environment(p_env);
	if (p_buffer != nullptr) {
		*p_size = dispatch(env, "string_utf16", address(p_value), address(p_buffer), *p_size);
		return p_buffer;
	}
	const double snapshot = dispatch(env, "string_snapshot", address(p_value));
	const size_t size = dispatch(env, "string_utf16", snapshot);
	auto &strings = scope_data(env).strings16;
	strings.emplace_back(size, u'\0');
	dispatch(env, "string_utf16", snapshot, address(strings.back().data()), size);
	*p_size = size;
	return reinterpret_cast<const uint16_t *>(strings.back().c_str());
}

pesapi_value create_binary(pesapi_env p_env, void *p_buffer, size_t p_size) {
	return value(dispatch(environment(p_env), "create_binary", address(p_buffer), p_size));
}

void *get_value_binary(pesapi_env p_env, pesapi_value p_value, size_t *p_size) {
	auto *env = environment(p_env);
	*p_size = dispatch(env, "binary_size", address(p_value));
	auto &buffers = scope_data(env).buffers;
	buffers.emplace_back(*p_size);
	dispatch(env, "binary_copy", address(p_value), address(buffers.back().data()));
	return buffers.back().data();
}

pesapi_value create_function(pesapi_env p_env, pesapi_callback p_callback, void *p_data, pesapi_function_finalize p_finalize) {
	return value(dispatch(environment(p_env), "create_function", address(p_callback), address(p_data), address(p_finalize)));
}

pesapi_value create_class(pesapi_env p_env, const void *p_type) {
	return value(dispatch(environment(p_env), "create_class", address(p_type)));
}

pesapi_value native_object_to_value(pesapi_env p_env, const void *p_type, void *p_ptr, int p_finalize) {
	return value(dispatch(environment(p_env), "native_object_to_value", address(p_type), address(p_ptr), p_finalize));
}

void *get_native_object_ptr(pesapi_env p_env, pesapi_value p_value) {
	return reinterpret_cast<void *>(static_cast<uintptr_t>(dispatch(environment(p_env), "get_native_object_ptr", address(p_value))));
}

const void *get_native_object_typeid(pesapi_env p_env, pesapi_value p_value) {
	return reinterpret_cast<const void *>(static_cast<uintptr_t>(dispatch(environment(p_env), "get_native_object_typeid", address(p_value))));
}

int is_instance_of(pesapi_env p_env, const void *p_type, pesapi_value p_value) {
	return dispatch(environment(p_env), "is_instance_of", address(p_type), address(p_value));
}

pesapi_value boxing(pesapi_env p_env, pesapi_value p_value) {
	return value(dispatch(environment(p_env), "boxing", address(p_value)));
}

pesapi_value unboxing(pesapi_env p_env, pesapi_value p_value) {
	return value(dispatch(environment(p_env), "unboxing", address(p_value)));
}

void update_boxed_value(pesapi_env p_env, pesapi_value p_box, pesapi_value p_value) {
	dispatch(environment(p_env), "update_boxed_value", address(p_box), address(p_value));
}

#define WEB_INFO(name, type)                                                        \
	type name(pesapi_callback_info p_info) {                                        \
		auto *info = reinterpret_cast<WebCallbackInfo *>(p_info);                   \
		return static_cast<type>(dispatch(info->environment, #name, info->handle)); \
	}
WEB_INFO(get_args_len, int)

pesapi_value get_arg(pesapi_callback_info p_info, int p_index) {
	auto *info = reinterpret_cast<WebCallbackInfo *>(p_info);
	return value(dispatch(info->environment, "get_arg", info->handle, p_index));
}

pesapi_env get_env(pesapi_callback_info p_info) {
	return reinterpret_cast<pesapi_env>(reinterpret_cast<WebCallbackInfo *>(p_info)->environment);
}

#define WEB_INFO_PTR(name, type)                                                                                 \
	type name(pesapi_callback_info p_info) {                                                                     \
		auto *info = reinterpret_cast<WebCallbackInfo *>(p_info);                                                \
		return reinterpret_cast<type>(static_cast<uintptr_t>(dispatch(info->environment, #name, info->handle))); \
	}
WEB_INFO_PTR(get_native_holder_ptr, void *)
WEB_INFO_PTR(get_native_holder_typeid, const void *)
WEB_INFO_PTR(get_userdata, void *)

void add_return(pesapi_callback_info p_info, pesapi_value p_value) {
	auto *info = reinterpret_cast<WebCallbackInfo *>(p_info);
	dispatch(info->environment, "add_return", info->handle, address(p_value));
}

void throw_by_string(pesapi_callback_info p_info, const char *p_message) {
	auto *info = reinterpret_cast<WebCallbackInfo *>(p_info);
	dispatch(info->environment, "throw_by_string", info->handle, address(p_message));
}

pesapi_env_ref create_env_ref(pesapi_env p_env) {
	retain(environment(p_env));
	return reinterpret_cast<pesapi_env_ref>(p_env);
}

int env_ref_is_valid(pesapi_env_ref p_ref) {
	return p_ref != nullptr && reinterpret_cast<WebEnvironment *>(p_ref)->alive;
}

pesapi_env get_env_from_ref(pesapi_env_ref p_ref) {
	return env_ref_is_valid(p_ref) ? reinterpret_cast<pesapi_env>(p_ref) : nullptr;
}

pesapi_env_ref duplicate_env_ref(pesapi_env_ref p_ref) {
	retain(reinterpret_cast<WebEnvironment *>(p_ref));
	return p_ref;
}

void release_env_ref(pesapi_env_ref p_ref) {
	release(reinterpret_cast<WebEnvironment *>(p_ref));
}

pesapi_value_ref create_value_ref(pesapi_env p_env, pesapi_value p_value, uint32_t p_field_count) {
	auto *env = environment(p_env);
	const uint32_t handle = dispatch(env, "create_value_ref", address(p_value));
	if (handle == 0) {
		return nullptr;
	}
	retain(env);
	auto *reference = new WebValueReference{ env, handle, 1, std::vector<void *>(p_field_count, nullptr) };
	return reinterpret_cast<pesapi_value_ref>(reference);
}

pesapi_value_ref duplicate_value_ref(pesapi_value_ref p_ref) {
	++reinterpret_cast<WebValueReference *>(p_ref)->references;
	return p_ref;
}

void release_value_ref(pesapi_value_ref p_ref) {
	auto *reference = reinterpret_cast<WebValueReference *>(p_ref);
	if (--reference->references == 0) {
		dispatch(reference->environment, "release_value_ref", reference->handle);
		release(reference->environment);
		delete reference;
	}
}

pesapi_value get_value_from_ref(pesapi_env p_env, pesapi_value_ref p_ref) {
	auto *reference = reinterpret_cast<WebValueReference *>(p_ref);
	return reference->environment == environment(p_env) ? value(dispatch(reference->environment, "get_value_from_ref", reference->handle)) : nullptr;
}

void set_ref_weak(pesapi_env p_env, pesapi_value_ref p_ref) {
	auto *reference = reinterpret_cast<WebValueReference *>(p_ref);
	if (reference->environment == environment(p_env)) {
		dispatch(reference->environment, "set_ref_weak", reference->handle);
	}
}

int set_owner(pesapi_env p_env, pesapi_value p_value, pesapi_value p_owner) {
	return dispatch(environment(p_env), "set_owner", address(p_value), address(p_owner));
}

pesapi_env_ref get_ref_associated_env(pesapi_value_ref p_ref) {
	return reinterpret_cast<pesapi_env_ref>(reinterpret_cast<WebValueReference *>(p_ref)->environment);
}

void **get_ref_internal_fields(pesapi_value_ref p_ref, uint32_t *p_count) {
	auto *reference = reinterpret_cast<WebValueReference *>(p_ref);
	if (p_count != nullptr) {
		*p_count = reference->fields.size();
	}
	return reference->fields.data();
}

pesapi_value get_property(pesapi_env p_env, pesapi_value p_object, const char *p_key) {
	return value(dispatch(environment(p_env), "get_property", address(p_object), address(p_key)));
}

int set_property(pesapi_env p_env, pesapi_value p_object, const char *p_key, pesapi_value p_value) {
	return dispatch(environment(p_env), "set_property", address(p_object), address(p_key), address(p_value));
}

int get_private(pesapi_env p_env, pesapi_value p_object, void **p_ptr) {
	if (!is_object(p_env, p_object) && !is_function(p_env, p_object)) {
		return false;
	}
	*p_ptr = reinterpret_cast<void *>(static_cast<uintptr_t>(dispatch(environment(p_env), "get_private", address(p_object))));
	return true;
}

int set_private(pesapi_env p_env, pesapi_value p_object, void *p_ptr) {
	return dispatch(environment(p_env), "set_private", address(p_object), address(p_ptr));
}

pesapi_value get_property_uint32(pesapi_env p_env, pesapi_value p_object, uint32_t p_key) {
	return value(dispatch(environment(p_env), "get_property_uint32", address(p_object), p_key));
}

int set_property_uint32(pesapi_env p_env, pesapi_value p_object, uint32_t p_key, pesapi_value p_value) {
	return dispatch(environment(p_env), "set_property_uint32", address(p_object), p_key, address(p_value));
}

pesapi_value call_function(pesapi_env p_env, pesapi_value p_function, pesapi_value p_this, int p_argc, const pesapi_value p_argv[]) {
	return value(dispatch(environment(p_env), "call_function", address(p_function), address(p_this), p_argc, address(p_argv)));
}

pesapi_value eval(pesapi_env p_env, const uint8_t *p_code, size_t p_size, const char *p_path) {
	return value(dispatch(environment(p_env), "eval", address(p_code), p_size, address(p_path)));
}

const void *get_env_private(pesapi_env p_env) {
	return environment(p_env)->private_data;
}

void set_env_private(pesapi_env p_env, const void *p_ptr) {
	auto *env = environment(p_env);
	env->private_data = p_ptr;
	dispatch(env, "set_env_private", address(p_ptr));
}

void set_registry(pesapi_env p_env, pesapi_registry p_registry) {
	environment(p_env)->registry = p_registry;
}

} // namespace

int puerts_webgl_get_api_version() {
	return PESAPI_VERSION;
}

pesapi_ffi *puerts_webgl_get_ffi() {
	static bool initialized = false;
	if (initialized) {
		return &ffi;
	}
#define WEB_API(name) ffi.name = &name;
	WEB_API(create_null)
	WEB_API(create_undefined)
	WEB_API(create_boolean)
	WEB_API(create_int32)
	WEB_API(create_uint32)
	WEB_API(create_int64)
	WEB_API(create_uint64)
	WEB_API(create_double)
	WEB_API(create_string_utf8)
	WEB_API(create_string_utf16)
	WEB_API(create_binary)
	ffi.create_binary_by_value = &create_binary;
	WEB_API(create_array)
	WEB_API(create_object)
	WEB_API(create_function)
	WEB_API(create_class)
	WEB_API(get_value_bool)
	WEB_API(get_value_int32)
	WEB_API(get_value_uint32)
	WEB_API(get_value_int64)
	WEB_API(get_value_uint64)
	WEB_API(get_value_double)
	WEB_API(get_value_string_utf8)
	WEB_API(get_value_string_utf16)
	WEB_API(get_value_binary)
	WEB_API(get_array_length)
	WEB_API(is_null)
	WEB_API(is_undefined)
	WEB_API(is_boolean)
	WEB_API(is_int32)
	WEB_API(is_uint32)
	WEB_API(is_int64)
	WEB_API(is_uint64)
	WEB_API(is_double)
	WEB_API(is_string)
	WEB_API(is_object)
	WEB_API(is_function)
	WEB_API(is_binary)
	WEB_API(is_array)
	WEB_API(native_object_to_value)
	WEB_API(get_native_object_ptr)
	WEB_API(get_native_object_typeid)
	WEB_API(is_instance_of)
	WEB_API(boxing)
	WEB_API(unboxing)
	WEB_API(update_boxed_value)
	WEB_API(is_boxed_value)
	WEB_API(get_args_len)
	WEB_API(get_arg)
	WEB_API(get_env)
	WEB_API(get_native_holder_ptr)
	WEB_API(get_native_holder_typeid)
	WEB_API(get_userdata)
	WEB_API(add_return)
	WEB_API(throw_by_string)
	WEB_API(create_env_ref)
	WEB_API(env_ref_is_valid)
	WEB_API(get_env_from_ref)
	WEB_API(duplicate_env_ref)
	WEB_API(release_env_ref)
	WEB_API(open_scope)
	WEB_API(open_scope_placement)
	WEB_API(has_caught)
	WEB_API(get_exception_as_string)
	WEB_API(close_scope)
	WEB_API(close_scope_placement)
	WEB_API(create_value_ref)
	WEB_API(duplicate_value_ref)
	WEB_API(release_value_ref)
	WEB_API(get_value_from_ref)
	WEB_API(set_ref_weak)
	WEB_API(set_owner)
	WEB_API(get_ref_associated_env)
	WEB_API(get_ref_internal_fields)
	WEB_API(get_property)
	WEB_API(set_property)
	WEB_API(get_private)
	WEB_API(set_private)
	WEB_API(get_property_uint32)
	WEB_API(set_property_uint32)
	WEB_API(call_function)
	WEB_API(eval)
	WEB_API(global)
	WEB_API(get_env_private)
	WEB_API(set_env_private)
	WEB_API(set_registry)
#undef WEB_API
	initialized = true;
	return &ffi;
}

pesapi_env_ref puerts_webgl_create_env_ref() {
	if (browser_environment != nullptr) {
		ERR_PRINT("The browser JavaScript backend supports only one environment per page.");
		return nullptr;
	}
	if (!webgl_initialize_bridge(puerts_webgl_runtime_source, address(&webgl_describe_class), address(&webgl_native_callback), address(&webgl_native_construct), address(&webgl_native_finalize))) {
		return nullptr;
	}
	auto *env = new WebEnvironment;
	if (!dispatch(env, "init", address(&ffi))) {
		const size_t size = dispatch(env, "exception", 0, true);
		std::string error(size, '\0');
		dispatch(env, "exception", 0, true, address(error.data()), size);
		ERR_PRINT(godot::String::utf8(error.c_str()));
		dispatch(env, "dispose");
		delete env;
		return nullptr;
	}
	browser_environment = env;
	return reinterpret_cast<pesapi_env_ref>(env);
}

void puerts_webgl_destroy_env_ref(pesapi_env_ref p_ref) {
	// The host realm and its callbacks remain live until the page is unloaded.
	// PuertsEnvironment rejects disposal for this backend before reaching here.
	(void)p_ref;
}
