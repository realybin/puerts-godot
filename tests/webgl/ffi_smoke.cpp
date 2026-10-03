// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

// Link the production WebGL and Core side modules into an Emscripten main
// module. These tests use PESAPI directly and never initialize the Godot engine.
#include "puerts_webgl_ffi.h"

#include <emscripten.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

namespace {

pesapi_ffi *smoke_ffi = nullptr;
pesapi_env_ref smoke_reference = nullptr;

void require(bool p_condition, const char *p_expression, int p_line) {
	if (!p_condition) {
		std::fprintf(stderr, "[webgl-ffi] FAIL line=%d assertion=%s\n", p_line, p_expression);
		std::fflush(stderr);
		std::exit(1);
	}
}

#define CHECK(expression) require(bool(expression), #expression, __LINE__)

class Scope {
	pesapi_ffi *ffi_;
	pesapi_scope scope_;
	bool placement_;
	alignas(std::max_align_t) pesapi_scope_memory memory_;

public:
	Scope(pesapi_ffi *p_ffi, pesapi_env_ref p_ref, bool p_placement = true) :
			ffi_(p_ffi), placement_(p_placement) {
		scope_ = placement_ ? ffi_->open_scope_placement(p_ref, &memory_) : ffi_->open_scope(p_ref);
		CHECK(scope_ != nullptr);
		CHECK(!ffi_->has_caught(scope_));
	}

	~Scope() {
		if (placement_) {
			ffi_->close_scope_placement(scope_);
		} else {
			ffi_->close_scope(scope_);
		}
	}

	Scope(const Scope &) = delete;
	Scope &operator=(const Scope &) = delete;

	pesapi_scope get() const { return scope_; }
};

pesapi_value evaluate(pesapi_ffi *p_ffi, pesapi_env p_env, const char *p_code) {
	return p_ffi->eval(p_env, reinterpret_cast<const uint8_t *>(p_code), std::strlen(p_code), "webgl-ffi-smoke.js");
}

void validate_api(pesapi_ffi *p_ffi) {
	CHECK(p_ffi != nullptr);
#define CHECK_API(name) CHECK(p_ffi->name != nullptr)
	CHECK_API(create_null);
	CHECK_API(create_undefined);
	CHECK_API(create_boolean);
	CHECK_API(create_int32);
	CHECK_API(create_uint32);
	CHECK_API(create_int64);
	CHECK_API(create_uint64);
	CHECK_API(create_double);
	CHECK_API(create_string_utf8);
	CHECK_API(create_string_utf16);
	CHECK_API(create_binary);
	CHECK_API(create_binary_by_value);
	CHECK_API(create_array);
	CHECK_API(create_object);
	CHECK_API(create_function);
	CHECK_API(create_class);
	CHECK_API(get_value_bool);
	CHECK_API(get_value_int32);
	CHECK_API(get_value_uint32);
	CHECK_API(get_value_int64);
	CHECK_API(get_value_uint64);
	CHECK_API(get_value_double);
	CHECK_API(get_value_string_utf8);
	CHECK_API(get_value_string_utf16);
	CHECK_API(get_value_binary);
	CHECK_API(get_array_length);
	CHECK_API(is_null);
	CHECK_API(is_undefined);
	CHECK_API(is_boolean);
	CHECK_API(is_int32);
	CHECK_API(is_uint32);
	CHECK_API(is_int64);
	CHECK_API(is_uint64);
	CHECK_API(is_double);
	CHECK_API(is_string);
	CHECK_API(is_object);
	CHECK_API(is_function);
	CHECK_API(is_binary);
	CHECK_API(is_array);
	CHECK_API(native_object_to_value);
	CHECK_API(get_native_object_ptr);
	CHECK_API(get_native_object_typeid);
	CHECK_API(is_instance_of);
	CHECK_API(boxing);
	CHECK_API(unboxing);
	CHECK_API(update_boxed_value);
	CHECK_API(is_boxed_value);
	CHECK_API(get_args_len);
	CHECK_API(get_arg);
	CHECK_API(get_env);
	CHECK_API(get_native_holder_ptr);
	CHECK_API(get_native_holder_typeid);
	CHECK_API(get_userdata);
	CHECK_API(add_return);
	CHECK_API(throw_by_string);
	CHECK_API(create_env_ref);
	CHECK_API(env_ref_is_valid);
	CHECK_API(get_env_from_ref);
	CHECK_API(duplicate_env_ref);
	CHECK_API(release_env_ref);
	CHECK_API(open_scope);
	CHECK_API(open_scope_placement);
	CHECK_API(has_caught);
	CHECK_API(get_exception_as_string);
	CHECK_API(close_scope);
	CHECK_API(close_scope_placement);
	CHECK_API(create_value_ref);
	CHECK_API(duplicate_value_ref);
	CHECK_API(release_value_ref);
	CHECK_API(get_value_from_ref);
	CHECK_API(set_ref_weak);
	CHECK_API(set_owner);
	CHECK_API(get_ref_associated_env);
	CHECK_API(get_ref_internal_fields);
	CHECK_API(get_property);
	CHECK_API(set_property);
	CHECK_API(get_private);
	CHECK_API(set_private);
	CHECK_API(get_property_uint32);
	CHECK_API(set_property_uint32);
	CHECK_API(call_function);
	CHECK_API(eval);
	CHECK_API(global);
	CHECK_API(get_env_private);
	CHECK_API(set_env_private);
	CHECK_API(set_registry);
#undef CHECK_API
}

void test_numbers(pesapi_ffi *p_ffi, pesapi_env p_env) {
	const int64_t signed_values[] = { std::numeric_limits<int64_t>::min(), -9007199254740993LL, -1, 0, 9007199254740993LL, std::numeric_limits<int64_t>::max() };
	for (int64_t number : signed_values) {
		const auto value = p_ffi->create_int64(p_env, number);
		CHECK(p_ffi->is_int64(p_env, value));
		CHECK(p_ffi->get_value_int64(p_env, value) == number);
	}
	const uint64_t unsigned_values[] = { 0, 1, 9007199254740993ULL, uint64_t(1) << 63, std::numeric_limits<uint64_t>::max() };
	for (uint64_t number : unsigned_values) {
		const auto value = p_ffi->create_uint64(p_env, number);
		CHECK(p_ffi->is_uint64(p_env, value));
		CHECK(p_ffi->get_value_uint64(p_env, value) == number);
	}
	CHECK(p_ffi->get_value_int64(p_env, evaluate(p_ffi, p_env, "-9223372036854775808n")) == std::numeric_limits<int64_t>::min());
	CHECK(p_ffi->get_value_uint64(p_env, evaluate(p_ffi, p_env, "18446744073709551615n")) == std::numeric_limits<uint64_t>::max());
	const auto signed32 = p_ffi->create_int32(p_env, std::numeric_limits<int32_t>::min());
	CHECK(p_ffi->is_int32(p_env, signed32));
	CHECK(p_ffi->get_value_int32(p_env, signed32) == std::numeric_limits<int32_t>::min());
	const auto unsigned32 = p_ffi->create_uint32(p_env, std::numeric_limits<uint32_t>::max());
	CHECK(p_ffi->is_uint32(p_env, unsigned32));
	CHECK(p_ffi->get_value_uint32(p_env, unsigned32) == std::numeric_limits<uint32_t>::max());
	const auto decimal = p_ffi->create_double(p_env, -7.5);
	CHECK(p_ffi->is_double(p_env, decimal) && !p_ffi->is_int32(p_env, decimal));
	CHECK(p_ffi->get_value_double(p_env, decimal) == -7.5);
	const auto boolean = p_ffi->create_boolean(p_env, true);
	CHECK(p_ffi->is_boolean(p_env, boolean) && p_ffi->get_value_bool(p_env, boolean));
	CHECK(p_ffi->is_null(p_env, p_ffi->create_null(p_env)));
	CHECK(p_ffi->is_undefined(p_env, p_ffi->create_undefined(p_env)));
}

void test_strings(pesapi_ffi *p_ffi, pesapi_env p_env) {
	constexpr char utf8[] = u8"A\0你好😀Z";
	constexpr uint16_t utf16[] = { 'A', 0, 0x4f60, 0x597d, 0xd83d, 0xde00, 'Z' };
	const auto value = p_ffi->create_string_utf8(p_env, utf8, sizeof(utf8) - 1);
	CHECK(p_ffi->is_string(p_env, value));
	size_t length = 0;
	const char *text = p_ffi->get_value_string_utf8(p_env, value, nullptr, &length);
	CHECK(length == sizeof(utf8) - 1 && text != nullptr);
	CHECK(std::memcmp(text, utf8, length) == 0 && text[length] == '\0');
	const uint16_t *wide = p_ffi->get_value_string_utf16(p_env, value, nullptr, &length);
	CHECK(length == sizeof(utf16) / sizeof(utf16[0]) && wide != nullptr);
	CHECK(std::memcmp(wide, utf16, sizeof(utf16)) == 0 && wide[length] == 0);
	const auto wide_value = p_ffi->create_string_utf16(p_env, utf16, sizeof(utf16) / sizeof(utf16[0]));
	text = p_ffi->get_value_string_utf8(p_env, wide_value, nullptr, &length);
	CHECK(length == sizeof(utf8) - 1 && std::memcmp(text, utf8, length) == 0);

	char short_buffer[] = { 'X', 'X', 'X', 'X' };
	length = 2;
	CHECK(p_ffi->get_value_string_utf8(p_env, value, short_buffer, &length) == short_buffer);
	CHECK(length == sizeof(utf8) - 1 && short_buffer[0] == 'A' && short_buffer[1] == 0 && short_buffer[2] == 'X');
	uint16_t short_wide[] = { 0xffff, 0xffff, 0xffff };
	length = 2;
	CHECK(p_ffi->get_value_string_utf16(p_env, value, short_wide, &length) == short_wide);
	CHECK(length == sizeof(utf16) / sizeof(utf16[0]) && short_wide[0] == 'A' && short_wide[1] == 0 && short_wide[2] == 0xffff);
}

pesapi_value changing_string(pesapi_ffi *p_ffi, pesapi_env p_env, const char *p_first, const char *p_second) {
	const std::string code = std::string("globalThis.__puertsFfiStringCalls = 0; ({toString() { return ++globalThis.__puertsFfiStringCalls === 1 ? (") + p_first + ") : (" + p_second + "); }})";
	return evaluate(p_ffi, p_env, code.c_str());
}

void check_string_calls(pesapi_ffi *p_ffi, pesapi_env p_env) {
	CHECK(p_ffi->get_value_int32(p_env, evaluate(p_ffi, p_env, "__puertsFfiStringCalls")) == 1);
}

void test_string_coercion(pesapi_ffi *p_ffi, pesapi_env p_env) {
	// Querying the length and then writing must use one snapshot. A second
	// toString call can change the size, corrupt the copy, or run user code twice.
	size_t length = 0;
	const auto utf8_short = changing_string(p_ffi, p_env, "'A'", "'Z'.repeat(1024)");
	const char *text = p_ffi->get_value_string_utf8(p_env, utf8_short, nullptr, &length);
	CHECK(text != nullptr && length == 1 && text[0] == 'A' && text[1] == '\0');
	check_string_calls(p_ffi, p_env);

	constexpr char utf8_tail[] = u8"\0你好😀Z";
	std::string expected_utf8(700, 'L');
	expected_utf8.append(utf8_tail, sizeof(utf8_tail) - 1);
	const auto utf8_long = changing_string(p_ffi, p_env, "'L'.repeat(700) + '\\u0000你好😀Z'", "'S'");
	text = p_ffi->get_value_string_utf8(p_env, utf8_long, nullptr, &length);
	CHECK(text != nullptr && length == expected_utf8.size());
	CHECK(std::memcmp(text, expected_utf8.data(), length) == 0 && text[length] == '\0');
	check_string_calls(p_ffi, p_env);

	const auto utf16_short = changing_string(p_ffi, p_env, "'你'", "'Z'.repeat(1024)");
	const uint16_t *wide = p_ffi->get_value_string_utf16(p_env, utf16_short, nullptr, &length);
	CHECK(wide != nullptr && length == 1 && wide[0] == 0x4f60 && wide[1] == 0);
	check_string_calls(p_ffi, p_env);

	constexpr char16_t utf16_tail[] = u"\0😀Z";
	std::u16string expected_utf16(700, u'你');
	expected_utf16.append(utf16_tail, sizeof(utf16_tail) / sizeof(utf16_tail[0]) - 1);
	const auto utf16_long = changing_string(p_ffi, p_env, "'你'.repeat(700) + '\\u0000😀Z'", "'S'");
	wide = p_ffi->get_value_string_utf16(p_env, utf16_long, nullptr, &length);
	CHECK(wide != nullptr && length == expected_utf16.size());
	CHECK(std::memcmp(wide, expected_utf16.data(), length * sizeof(uint16_t)) == 0 && wide[length] == 0);
	check_string_calls(p_ffi, p_env);

	char buffer[] = { 'X', 'X', 'X', 'X' };
	length = 2;
	const auto utf8_buffer = changing_string(p_ffi, p_env, "'AB' + 'L'.repeat(1024)", "'S'");
	CHECK(p_ffi->get_value_string_utf8(p_env, utf8_buffer, buffer, &length) == buffer);
	CHECK(length == 1026 && buffer[0] == 'A' && buffer[1] == 'B' && buffer[2] == 'X' && buffer[3] == 'X');
	check_string_calls(p_ffi, p_env);

	uint16_t wide_buffer[] = { 0xffff, 0xffff, 0xffff, 0xffff };
	length = 2;
	const auto utf16_buffer = changing_string(p_ffi, p_env, "'你'.repeat(700)", "'S'");
	CHECK(p_ffi->get_value_string_utf16(p_env, utf16_buffer, wide_buffer, &length) == wide_buffer);
	CHECK(length == 700 && wide_buffer[0] == 0x4f60 && wide_buffer[1] == 0x4f60 && wide_buffer[2] == 0xffff && wide_buffer[3] == 0xffff);
	check_string_calls(p_ffi, p_env);
	CHECK(evaluate(p_ffi, p_env, "delete globalThis.__puertsFfiStringCalls; true") != nullptr);
}

void test_exception_coercion(pesapi_ffi *p_ffi, pesapi_env p_env, pesapi_env_ref p_ref, Scope &p_outer) {
	{
		Scope isolated(p_ffi, p_ref);
		CHECK(evaluate(p_ffi, p_env, "globalThis.__puertsFfiStackCalls = 0; throw Object.defineProperty(new Error('changing stack'), 'stack', {get() { return ++globalThis.__puertsFfiStackCalls === 1 ? 'first stack' : 'Z'.repeat(1024); }})") == nullptr);
		CHECK(p_ffi->has_caught(isolated.get()));
		const char *text = p_ffi->get_exception_as_string(isolated.get(), true);
		CHECK(text != nullptr && std::strcmp(text, "first stack") == 0);
		CHECK(p_ffi->get_value_int32(p_env, evaluate(p_ffi, p_env, "__puertsFfiStackCalls")) == 1);
	}
	CHECK(!p_ffi->has_caught(p_outer.get()));
	{
		Scope isolated(p_ffi, p_ref, false);
		CHECK(evaluate(p_ffi, p_env, "globalThis.__puertsFfiStackCalls = 0; throw Object.defineProperty(new Error('changing stack'), 'stack', {get() { return ++globalThis.__puertsFfiStackCalls === 1 ? 'first:' + 'L'.repeat(1024) : 'S'; }})") == nullptr);
		CHECK(p_ffi->has_caught(isolated.get()));
		const char *text = p_ffi->get_exception_as_string(isolated.get(), true);
		const std::string expected = "first:" + std::string(1024, 'L');
		CHECK(text != nullptr && std::strlen(text) == expected.size() && std::memcmp(text, expected.data(), expected.size()) == 0);
		CHECK(p_ffi->get_value_int32(p_env, evaluate(p_ffi, p_env, "__puertsFfiStackCalls")) == 1);
	}
	CHECK(!p_ffi->has_caught(p_outer.get()));
	CHECK(evaluate(p_ffi, p_env, "delete globalThis.__puertsFfiStackCalls; true") != nullptr);
}

void test_binary(pesapi_ffi *p_ffi, pesapi_env p_env) {
	uint8_t source[] = { 0, 1, 128, 255 };
	const uint8_t expected[] = { 0, 1, 128, 255 };
	const auto value = p_ffi->create_binary_by_value(p_env, source, sizeof(source));
	source[1] = 99;
	CHECK(p_ffi->is_binary(p_env, value));
	size_t size = 0;
	auto *bytes = static_cast<uint8_t *>(p_ffi->get_value_binary(p_env, value, &size));
	CHECK(size == sizeof(expected) && bytes != nullptr && std::memcmp(bytes, expected, size) == 0);
	bytes[2] = 7;
	bytes = static_cast<uint8_t *>(p_ffi->get_value_binary(p_env, value, &size));
	CHECK(bytes[2] == 128);
	const auto view = evaluate(p_ffi, p_env, "new Uint8Array([9, 1, 255, 8]).subarray(1, 3)");
	bytes = static_cast<uint8_t *>(p_ffi->get_value_binary(p_env, view, &size));
	CHECK(size == 2 && bytes[0] == 1 && bytes[1] == 255);
	const auto empty = p_ffi->create_binary(p_env, nullptr, 0);
	p_ffi->get_value_binary(p_env, empty, &size);
	CHECK(size == 0);
}

void test_objects(pesapi_ffi *p_ffi, pesapi_env p_env) {
	const auto object = p_ffi->create_object(p_env);
	CHECK(p_ffi->is_object(p_env, object) && !p_ffi->is_array(p_env, object));
	CHECK(p_ffi->set_property(p_env, object, "answer", p_ffi->create_int32(p_env, 42)));
	CHECK(p_ffi->get_value_int32(p_env, p_ffi->get_property(p_env, object, "answer")) == 42);
	const auto array = p_ffi->create_array(p_env);
	CHECK(p_ffi->is_array(p_env, array));
	CHECK(p_ffi->set_property_uint32(p_env, array, 0, object));
	CHECK(p_ffi->get_array_length(p_env, array) == 1);
	CHECK(p_ffi->get_value_int32(p_env, p_ffi->get_property(p_env, p_ffi->get_property_uint32(p_env, array, 0), "answer")) == 42);
	CHECK(p_ffi->get_native_object_ptr(p_env, object) == nullptr && p_ffi->get_native_object_typeid(p_env, object) == nullptr);
	int private_marker = 17;
	CHECK(p_ffi->set_private(p_env, object, &private_marker));
	void *private_data = nullptr;
	CHECK(p_ffi->get_private(p_env, object, &private_data) && private_data == &private_marker);
	CHECK(p_ffi->set_private(p_env, object, nullptr));
	CHECK(p_ffi->get_private(p_env, object, &private_data) && private_data == nullptr);
	const auto frozen = evaluate(p_ffi, p_env, "Object.freeze({answer: 42})");
	CHECK(p_ffi->set_private(p_env, frozen, &private_marker));
	CHECK(p_ffi->get_private(p_env, frozen, &private_data) && private_data == &private_marker);
	const auto box = p_ffi->boxing(p_env, object);
	CHECK(p_ffi->is_boxed_value(p_env, box));
	CHECK(p_ffi->is_object(p_env, p_ffi->unboxing(p_env, box)));
	p_ffi->update_boxed_value(p_env, box, p_ffi->create_int32(p_env, 7));
	CHECK(p_ffi->get_value_int32(p_env, p_ffi->unboxing(p_env, box)) == 7);
	CHECK(p_ffi->set_owner(p_env, object, array));
}

struct CallbackState {
	pesapi_env_ref reference;
	void *private_data;
	int sum_calls = 0;
	int echo_calls = 0;
};

void echo_callback(pesapi_ffi *p_ffi, pesapi_callback_info p_info) {
	auto *state = static_cast<CallbackState *>(p_ffi->get_userdata(p_info));
	CHECK(p_ffi->get_args_len(p_info) == 1);
	++state->echo_calls;
	p_ffi->add_return(p_info, p_ffi->get_arg(p_info, 0));
}

void sum_callback(pesapi_ffi *p_ffi, pesapi_callback_info p_info) {
	auto *state = static_cast<CallbackState *>(p_ffi->get_userdata(p_info));
	const auto env = p_ffi->get_env(p_info);
	CHECK(env == p_ffi->get_env_from_ref(state->reference));
	CHECK(p_ffi->get_env_private(env) == state->private_data);
	CHECK(p_ffi->get_args_len(p_info) == 2);
	CHECK(p_ffi->get_native_holder_ptr(p_info) == nullptr && p_ffi->get_native_holder_typeid(p_info) == nullptr);
	++state->sum_calls;
	const int sum = p_ffi->get_value_int32(env, p_ffi->get_arg(p_info, 0)) + p_ffi->get_value_int32(env, p_ffi->get_arg(p_info, 1));
	{
		Scope isolated(p_ffi, state->reference);
		CHECK(evaluate(p_ffi, env, "throw new Error('nested isolated')") == nullptr);
		CHECK(p_ffi->has_caught(isolated.get()));
		CHECK(std::strstr(p_ffi->get_exception_as_string(isolated.get(), false), "nested isolated") != nullptr);
	}
	Scope nested(p_ffi, state->reference, false);
	const auto fn = evaluate(p_ffi, env, "(value) => __puertsFfiEcho(value) + 1");
	const pesapi_value arguments[] = { p_ffi->create_int32(env, sum) };
	const auto result = p_ffi->call_function(env, fn, p_ffi->global(env), 1, arguments);
	CHECK(!p_ffi->has_caught(nested.get()));
	p_ffi->add_return(p_info, result);
}

void throwing_callback(pesapi_ffi *p_ffi, pesapi_callback_info p_info) {
	p_ffi->throw_by_string(p_info, "native callback failure");
}

void test_callbacks(pesapi_ffi *p_ffi, pesapi_env p_env, pesapi_env_ref p_ref, Scope &p_outer, void *p_private) {
	CallbackState state{ p_ref, p_private };
	const auto global = p_ffi->global(p_env);
	CHECK(p_ffi->set_property(p_env, global, "__puertsFfiEcho", p_ffi->create_function(p_env, echo_callback, &state, nullptr)));
	const auto sum = p_ffi->create_function(p_env, sum_callback, &state, nullptr);
	CHECK(p_ffi->is_function(p_env, sum));
	CHECK(p_ffi->set_property(p_env, global, "__puertsFfiSum", sum));
	CHECK(p_ffi->get_value_int32(p_env, evaluate(p_ffi, p_env, "__puertsFfiSum(20, 22)")) == 43);
	CHECK(state.sum_calls == 1 && state.echo_calls == 1 && !p_ffi->has_caught(p_outer.get()));
	const auto receiver = evaluate(p_ffi, p_env, "({base: 5, add(a, b) { return this.base + a + b; }})");
	const auto fn = p_ffi->get_property(p_env, receiver, "add");
	const pesapi_value arguments[] = { p_ffi->create_int32(p_env, 2), p_ffi->create_int32(p_env, 3) };
	CHECK(p_ffi->get_value_int32(p_env, p_ffi->call_function(p_env, fn, receiver, 2, arguments)) == 10);
	CHECK(p_ffi->set_property(p_env, global, "__puertsFfiThrow", p_ffi->create_function(p_env, throwing_callback, nullptr, nullptr)));
	{
		Scope isolated(p_ffi, p_ref);
		CHECK(evaluate(p_ffi, p_env, "__puertsFfiThrow()") == nullptr);
		CHECK(p_ffi->has_caught(isolated.get()));
		CHECK(std::strstr(p_ffi->get_exception_as_string(isolated.get(), true), "native callback failure") != nullptr);
	}
	CHECK(!p_ffi->has_caught(p_outer.get()));
	CHECK(p_ffi->get_value_int32(p_env, evaluate(p_ffi, p_env, "6 * 7")) == 42);
	// The native state lives on the C++ stack; remove browser references before returning.
	CHECK(evaluate(p_ffi, p_env, "delete globalThis.__puertsFfiEcho; delete globalThis.__puertsFfiSum; delete globalThis.__puertsFfiThrow; true") != nullptr);
}

void test_references(pesapi_ffi *p_ffi, pesapi_env p_env, pesapi_env_ref p_ref, Scope &p_outer) {
	pesapi_value_ref held = nullptr;
	{
		Scope source(p_ffi, p_ref, false);
		const auto object = evaluate(p_ffi, p_env, "({answer: 42})");
		held = p_ffi->create_value_ref(p_env, object, 2);
		CHECK(held != nullptr && p_ffi->get_ref_associated_env(held) == p_ref);
		CHECK(p_ffi->duplicate_value_ref(held) == held);
		uint32_t count = 0;
		auto **fields = p_ffi->get_ref_internal_fields(held, &count);
		CHECK(count == 2 && fields != nullptr && fields[0] == nullptr && fields[1] == nullptr);
		fields[1] = held;
	}
	CHECK(!p_ffi->has_caught(p_outer.get()));
	const auto object = p_ffi->get_value_from_ref(p_env, held);
	CHECK(p_ffi->get_value_int32(p_env, p_ffi->get_property(p_env, object, "answer")) == 42);
	p_ffi->release_value_ref(held);
	uint32_t count = 0;
	CHECK(p_ffi->get_ref_internal_fields(held, &count)[1] == held && count == 2);
	p_ffi->set_ref_weak(p_env, held);
	CHECK(p_ffi->is_object(p_env, p_ffi->get_value_from_ref(p_env, held)));
	p_ffi->release_value_ref(held);
	// A value in the active scope remains usable after its persistent ref is released.
	CHECK(p_ffi->get_value_int32(p_env, p_ffi->get_property(p_env, object, "answer")) == 42);

	pesapi_value expired = nullptr;
	{
		Scope temporary(p_ffi, p_ref);
		expired = p_ffi->create_object(p_env);
	}
	{
		Scope rejected(p_ffi, p_ref);
		CHECK(!p_ffi->is_object(p_env, expired));
		CHECK(p_ffi->has_caught(rejected.get()));
		CHECK(std::strstr(p_ffi->get_exception_as_string(rejected.get(), false), "expired") != nullptr);
	}
	CHECK(!p_ffi->has_caught(p_outer.get()));
}

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE int puerts_webgl_ffi_smoke() {
	CHECK(puerts_webgl_get_api_version() == PESAPI_VERSION);
	auto *ffi = puerts_webgl_get_ffi();
	validate_api(ffi);
	const auto reference = puerts_webgl_create_env_ref();
	CHECK(reference != nullptr && ffi->env_ref_is_valid(reference));
	smoke_ffi = ffi;
	smoke_reference = reference;
	const auto env = ffi->get_env_from_ref(reference);
	CHECK(env != nullptr);
	int private_marker = 19;
	ffi->set_registry(env, nullptr);
	ffi->set_env_private(env, &private_marker);
	CHECK(ffi->get_env_private(env) == &private_marker);
	{
		Scope outer(ffi, reference);
		const auto extra = ffi->create_env_ref(env);
		CHECK(extra == reference && ffi->duplicate_env_ref(extra) == reference);
		ffi->release_env_ref(extra);
		ffi->release_env_ref(extra);
		CHECK(ffi->env_ref_is_valid(reference));
		test_numbers(ffi, env);
		test_strings(ffi, env);
		test_string_coercion(ffi, env);
		test_exception_coercion(ffi, env, reference, outer);
		test_binary(ffi, env);
		test_objects(ffi, env);
		test_callbacks(ffi, env, reference, outer, &private_marker);
		test_references(ffi, env, reference, outer);
		CHECK(!ffi->has_caught(outer.get()));
	}
	ffi->set_env_private(env, nullptr);
	CHECK(ffi->get_env_private(env) == nullptr && ffi->env_ref_is_valid(reference));
	// The production singleton remains allocated until this page exits.
	std::puts("[webgl-ffi] PASS");
	std::fflush(stdout);
	return 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE int puerts_webgl_ffi_memory_growth(uintptr_t p_pointer, uint32_t p_length) {
	CHECK(smoke_ffi != nullptr && smoke_reference != nullptr);
	CHECK(smoke_ffi->env_ref_is_valid(smoke_reference));
	Scope scope(smoke_ffi, smoke_reference);
	const auto env = smoke_ffi->get_env_from_ref(smoke_reference);
	constexpr char expected_utf8[] = u8"grown-\0你好😀-tail";
	constexpr uint16_t expected_utf16[] = { 'g', 'r', 'o', 'w', 'n', '-', 0, 0x4f60, 0x597d, 0xd83d, 0xde00, '-', 't', 'a', 'i', 'l' };
	constexpr uint8_t expected_binary[] = { 0, 1, 128, 255, 7, 9 };
	CHECK(p_length == sizeof(expected_utf8) - 1);
	const auto text = smoke_ffi->create_string_utf8(env, reinterpret_cast<const char *>(p_pointer), p_length);
	CHECK(text != nullptr && smoke_ffi->is_string(env, text));
	size_t length = 0;
	const char *utf8 = smoke_ffi->get_value_string_utf8(env, text, nullptr, &length);
	CHECK(utf8 != nullptr && length == p_length && std::memcmp(utf8, expected_utf8, length) == 0);
	const auto wide = smoke_ffi->create_string_utf16(env, reinterpret_cast<const uint16_t *>(p_pointer + 64), sizeof(expected_utf16) / sizeof(expected_utf16[0]));
	CHECK(wide != nullptr);
	const uint16_t *utf16 = smoke_ffi->get_value_string_utf16(env, wide, nullptr, &length);
	CHECK(utf16 != nullptr && length == sizeof(expected_utf16) / sizeof(expected_utf16[0]) && std::memcmp(utf16, expected_utf16, sizeof(expected_utf16)) == 0);
	const auto binary = smoke_ffi->create_binary_by_value(env, reinterpret_cast<void *>(p_pointer + 128), sizeof(expected_binary));
	CHECK(binary != nullptr && smoke_ffi->is_binary(env, binary));
	const void *bytes = smoke_ffi->get_value_binary(env, binary, &length);
	CHECK(bytes != nullptr && length == sizeof(expected_binary) && std::memcmp(bytes, expected_binary, length) == 0);
	CHECK(!smoke_ffi->has_caught(scope.get()));
	std::puts("[webgl-ffi] MEMORY_GROWTH PASS");
	std::fflush(stdout);
	return 0;
}

int main() {
	return puerts_webgl_ffi_smoke();
}
