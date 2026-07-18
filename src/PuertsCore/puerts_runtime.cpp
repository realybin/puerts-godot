// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "puerts_runtime.h"

namespace puerts::internal {

godot::String read_utf8_string(pesapi_ffi *p_apis, pesapi_env p_env, pesapi_value p_value) {
	constexpr size_t INLINE_UTF8_BUFFER_SIZE = 256;
	size_t size = 0;
	const char *inline_text = p_apis->get_value_string_utf8(p_env, p_value, nullptr, &size);
	if (inline_text != nullptr) {
		return godot::String::utf8(inline_text, static_cast<int>(size));
	}

	// Most identifiers and log messages fit in the inline storage. The fixed
	// vector only falls back to the heap for unusually long script strings.
	puerts_eastl::fixed_vector<char, INLINE_UTF8_BUFFER_SIZE> buffer;
	buffer.resize(size + 1);
	p_apis->get_value_string_utf8(p_env, p_value, buffer.data(), &size);
	buffer[size] = '\0';
	return godot::String::utf8(buffer.data(), static_cast<int>(size));
}

godot::String format_call_error(const godot::String &p_target_name, const GDExtensionCallError &p_call_error) {
	switch (p_call_error.error) {
		case GDEXTENSION_CALL_OK:
			return {};
		case GDEXTENSION_CALL_ERROR_INVALID_METHOD:
			return "Method not found: " + p_target_name;
		case GDEXTENSION_CALL_ERROR_INVALID_ARGUMENT: {
			godot::String expected_type = "unknown";
			if (p_call_error.expected >= 0 && p_call_error.expected < godot::Variant::VARIANT_MAX) {
				expected_type = godot::Variant::get_type_name(static_cast<godot::Variant::Type>(p_call_error.expected));
			}
			return godot::vformat("Invalid argument %d when calling %s. Expected %s.", p_call_error.argument + 1, p_target_name, expected_type);
		}
		case GDEXTENSION_CALL_ERROR_TOO_MANY_ARGUMENTS:
			return godot::vformat("Too many arguments when calling %s. Expected %d.", p_target_name, p_call_error.expected);
		case GDEXTENSION_CALL_ERROR_TOO_FEW_ARGUMENTS:
			return godot::vformat("Too few arguments when calling %s. Expected %d.", p_target_name, p_call_error.expected);
		case GDEXTENSION_CALL_ERROR_INSTANCE_IS_NULL:
			return "Instance is null when calling " + p_target_name;
		case GDEXTENSION_CALL_ERROR_METHOD_NOT_CONST:
			return "Method is not const: " + p_target_name;
		default:
			return "Call failed: " + p_target_name;
	}
}

} // namespace puerts::internal
