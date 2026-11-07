// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: Apache-2.0

// Exercise the compiled Core and Web side modules through the PESAPI ABI.
#include "puerts_web_ffi.h"

#include <emscripten.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

pesapi_ffi *smoke_ffi = nullptr;
pesapi_env_ref smoke_reference = nullptr;

void require(bool p_condition, const char *p_expression, int p_line) {
	if (!p_condition) {
		std::fprintf(stderr, "[web-ffi] FAIL line=%d assertion=%s\n", p_line, p_expression);
		std::fflush(stderr);
		std::exit(1);
	}
}

#define CHECK(expression) require(bool(expression), #expression, __LINE__)

class Scope {
	pesapi_ffi *ffi_;
	alignas(std::max_align_t) pesapi_scope_memory memory_;
	pesapi_scope scope_;

public:
	Scope(pesapi_ffi *p_ffi, pesapi_env_ref p_ref) :
			ffi_(p_ffi), scope_(ffi_->open_scope_placement(p_ref, &memory_)) {
		CHECK(scope_ != nullptr && !ffi_->has_caught(scope_));
	}

	~Scope() { ffi_->close_scope_placement(scope_); }

	Scope(const Scope &) = delete;
	Scope &operator=(const Scope &) = delete;

	pesapi_scope get() const { return scope_; }
};

pesapi_value evaluate(pesapi_ffi *p_ffi, pesapi_env p_env, const char *p_code) {
	return p_ffi->eval(p_env, reinterpret_cast<const uint8_t *>(p_code), std::strlen(p_code), "web-ffi-smoke.js");
}

void test_integers(pesapi_ffi *p_ffi, pesapi_env p_env) {
	const auto global = p_ffi->global(p_env);
	constexpr auto signed_min = std::numeric_limits<int64_t>::min();
	constexpr auto unsigned_max = std::numeric_limits<uint64_t>::max();
	CHECK(p_ffi->set_property(p_env, global, "__puertsSigned", p_ffi->create_int64(p_env, signed_min)));
	CHECK(p_ffi->set_property(p_env, global, "__puertsUnsigned", p_ffi->create_uint64(p_env, unsigned_max)));
	const auto matches = evaluate(p_ffi, p_env, "__puertsSigned === -9223372036854775808n && __puertsUnsigned === 18446744073709551615n");
	CHECK(matches != nullptr && p_ffi->get_value_bool(p_env, matches));
	CHECK(p_ffi->get_value_int64(p_env, evaluate(p_ffi, p_env, "-9223372036854775808n")) == signed_min);
	CHECK(p_ffi->get_value_uint64(p_env, evaluate(p_ffi, p_env, "18446744073709551615n")) == unsigned_max);
}

void test_strings(pesapi_ffi *p_ffi, pesapi_env p_env) {
	constexpr char utf8[] = u8"A\0你好😀Z";
	const auto value = p_ffi->create_string_utf8(p_env, utf8, sizeof(utf8) - 1);
	char buffer[] = { 'X', 'X', 'X' };
	size_t length = 2;
	CHECK(p_ffi->get_value_string_utf8(p_env, value, buffer, &length) == buffer);
	CHECK(length == sizeof(utf8) - 1 && buffer[0] == 'A' && buffer[1] == 0 && buffer[2] == 'X');

	// The allocating getter queries and copies through the bridge using one coercion.
	const auto changing = evaluate(p_ffi, p_env,
			"({calls: 0, toString() { return ++this.calls === 1 ? '你'.repeat(700) + '😀' : 'S'; }})");
	const char *text = p_ffi->get_value_string_utf8(p_env, changing, nullptr, &length);
	CHECK(text != nullptr && length == 2104 && std::memcmp(text, u8"你", 3) == 0);
	CHECK(std::strcmp(text + 2100, u8"😀") == 0);
	CHECK(p_ffi->get_value_int32(p_env, p_ffi->get_property(p_env, changing, "calls")) == 1);
}

void add_callback(pesapi_ffi *p_ffi, pesapi_callback_info p_info) {
	const auto env = p_ffi->get_env(p_info);
	auto *calls = static_cast<int *>(p_ffi->get_userdata(p_info));
	CHECK(calls != nullptr && p_ffi->get_args_len(p_info) == 2);
	++*calls;
	const int left = p_ffi->get_value_int32(env, p_ffi->get_arg(p_info, 0));
	const int right = p_ffi->get_value_int32(env, p_ffi->get_arg(p_info, 1));
	p_ffi->add_return(p_info, p_ffi->create_int32(env, left + right));
}

void throwing_callback(pesapi_ffi *p_ffi, pesapi_callback_info p_info) {
	p_ffi->throw_by_string(p_info, "native callback failure");
}

void test_callbacks(pesapi_ffi *p_ffi, pesapi_env p_env, pesapi_env_ref p_ref) {
	int calls = 0;
	const auto invoke = evaluate(p_ffi, p_env, "(fn) => fn(20, 22)");
	const pesapi_value args[] = { p_ffi->create_function(p_env, add_callback, &calls, nullptr) };
	const auto global = p_ffi->global(p_env);
	const auto answer = p_ffi->call_function(p_env, invoke, global, 1, args);
	CHECK(answer != nullptr && p_ffi->get_value_int32(p_env, answer) == 42 && calls == 1);

	Scope isolated(p_ffi, p_ref);
	const pesapi_value throwing_args[] = { p_ffi->create_function(p_env, throwing_callback, nullptr, nullptr) };
	CHECK(p_ffi->call_function(p_env, invoke, global, 1, throwing_args) == nullptr);
	CHECK(p_ffi->has_caught(isolated.get()));
	const char *error = p_ffi->get_exception_as_string(isolated.get(), true);
	CHECK(error != nullptr && std::strstr(error, "native callback failure") != nullptr);
}

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE int puerts_web_ffi_smoke() {
	CHECK(puerts_web_get_api_version() == PESAPI_VERSION);
	auto *ffi = puerts_web_get_ffi();
	CHECK(ffi != nullptr);
	const auto reference = puerts_web_create_env_ref();
	CHECK(reference != nullptr && ffi->env_ref_is_valid(reference));
	const auto env = ffi->get_env_from_ref(reference);
	CHECK(env != nullptr);
	smoke_ffi = ffi;
	smoke_reference = reference;
	{
		Scope outer(ffi, reference);
		test_integers(ffi, env);
		test_strings(ffi, env);
		test_callbacks(ffi, env, reference);
		CHECK(!ffi->has_caught(outer.get()));
	}
	std::puts("[web-ffi] PASS");
	std::fflush(stdout);
	return 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE int puerts_web_ffi_memory_growth(uintptr_t p_pointer, uint32_t p_length) {
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
	std::puts("[web-ffi] MEMORY_GROWTH PASS");
	std::fflush(stdout);
	return 0;
}

int main() {
	return puerts_web_ffi_smoke();
}
