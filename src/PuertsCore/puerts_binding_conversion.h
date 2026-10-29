// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_BINDING_CONVERSION_H
#define PUERTS_GODOT_PUERTS_BINDING_CONVERSION_H

#include "puerts_bridge_registry.h"
#include "puerts_environment.h"
#include "puerts_runtime.h"
#include "puerts_type_registry.h"
#include "puerts_type_traits.h"

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>

#include <EASTL/type_traits.h>

namespace puerts::internal {

template <typename T>
using unqualified_t = eastl::remove_cv_t<eastl::remove_reference_t<T>>;

template <typename T, typename = void>
struct has_variant_type : eastl::false_type {};

template <typename T>
struct has_variant_type<T, eastl::void_t<decltype(godot::GetTypeInfo<T>::VARIANT_TYPE)>> : eastl::true_type {};

template <typename T>
inline constexpr bool has_variant_type_v = has_variant_type<T>::value;

template <typename T>
inline constexpr GDExtensionVariantType variant_type_v =
		static_cast<GDExtensionVariantType>(godot::GetTypeInfo<T>::VARIANT_TYPE);

template <typename T>
inline constexpr bool is_nonconst_lvalue_ref_v = eastl::is_lvalue_reference_v<T> && !eastl::is_const_v<eastl::remove_reference_t<T>>;

template <typename T>
inline bool can_cast_variant(const godot::Variant &p_value) {
	const auto source_type = static_cast<GDExtensionVariantType>(p_value.get_type());
	return (source_type == variant_type_v<T> || godot::gdextension_interface::variant_can_convert_strict(source_type, variant_type_v<T>)) &&
			godot::VariantObjectClassChecker<T>::check(p_value);
}

template <typename T>
struct finalizer {
	static void finalize(pesapi_ffi *, void *ptr, void *, void *) {
		godot::memdelete(static_cast<T *>(ptr));
	}

	static pesapi_finalize get() {
		if constexpr (eastl::is_base_of_v<godot::Object, T> || has_variant_type_v<T>) {
			return nullptr;
		} else {
			return &finalize;
		}
	}
};

inline const godot::Variant &load_variant_argument(CallbackContext &context, int index) {
	auto &argument = context.argument(index);
	auto &variant = context.variant(index);
	if (!argument.variant_loaded) {
		variant = puerts::script_value_to_variant(context.puerts_environment(), context.script_env(), context.argument_value(index));
		argument.variant_loaded = true;
	}
	return variant;
}

template <typename T, typename Consumer, typename Value>
void consume_typed_argument(Consumer &p_consumer, Value &&p_value) {
	if constexpr (eastl::is_lvalue_reference_v<T>) {
		p_consumer(static_cast<const unqualified_t<T> &>(p_value));
	} else {
		p_consumer(unqualified_t<T>(eastl::forward<Value>(p_value)));
	}
}

template <typename T, typename Consumer>
bool convert_variant_argument(
		CallbackContext &context,
		int argument_index,
		Consumer &&p_consumer) {
	static_assert(!is_nonconst_lvalue_ref_v<T>, "Non-const reference arguments are not supported.");
	auto *apis = context.api();
	using value_type = unqualified_t<T>;
	const pesapi_env env = context.script_env();
	const pesapi_value value = context.argument_value(argument_index);
	if constexpr (eastl::is_arithmetic_v<value_type>) {
		if constexpr (eastl::is_same_v<value_type, bool>) {
			if (apis->is_boolean(env, value)) {
				consume_typed_argument<T>(p_consumer, apis->get_value_bool(env, value) != 0);
				return true;
			}
		} else {
			if (apis->is_int32(env, value)) {
				consume_typed_argument<T>(p_consumer, static_cast<value_type>(apis->get_value_int32(env, value)));
				return true;
			}
			if constexpr (eastl::is_floating_point_v<value_type>) {
				if (apis->is_double(env, value)) {
					consume_typed_argument<T>(p_consumer, static_cast<value_type>(apis->get_value_double(env, value)));
					return true;
				}
			}
		}
	}

	if constexpr (variant_type_v<T> != GDEXTENSION_VARIANT_TYPE_OBJECT && variant_type_v<T> != GDEXTENSION_VARIANT_TYPE_NIL) {
		if (context.native_argument(argument_index).native_handle == nullptr && apis->is_object(env, value)) {
			return context.reject("Argument type does not match the bound signature.");
		}
	}
	const godot::Variant &variant = load_variant_argument(context, argument_index);
	if constexpr (variant_type_v<T> != GDEXTENSION_VARIANT_TYPE_NIL) {
		if (!can_cast_variant<T>(variant)) {
			return context.reject("Argument type does not match the bound signature.");
		}
	}
	consume_typed_argument<T>(p_consumer, godot::VariantCaster<T>::cast(variant));
	return true;
}

template <typename Target>
Target *resolve_native_pointer(
		CallbackContext &context,
		CallbackContext::Argument &p_argument) {
	if (p_argument.native_handle == nullptr) {
		(void)context.reject("Native object is no longer valid.");
		return nullptr;
	}
	void *pointer = PuertsTypeRegistry::get_singleton().cast_native(
			p_argument.native_handle, p_argument.native_type_id, static_type_id<Target>::get());
	if (pointer == nullptr) {
		(void)context.reject("Native object type does not match the bound signature.");
	}
	return static_cast<Target *>(pointer);
}

template <typename T, typename Consumer>
bool convert_native_argument(
		CallbackContext &context,
		int argument_index,
		Consumer &&p_consumer) {
	static_assert(!is_nonconst_lvalue_ref_v<T>, "Non-const reference arguments are not supported.");
	using argument_type = unqualified_t<T>;
	using target_type = eastl::conditional_t<
			eastl::is_pointer_v<argument_type>,
			eastl::remove_cv_t<eastl::remove_pointer_t<argument_type>>,
			argument_type>;
	if constexpr (eastl::is_pointer_v<argument_type>) {
		if (context.api()->is_null(context.script_env(), context.argument_value(argument_index))) {
			p_consumer(static_cast<argument_type>(nullptr));
			return true;
		}
	}
	static_assert(!has_variant_type_v<target_type>, "Use a value or const reference for Godot builtin arguments, not a pointer.");
	CallbackContext::Argument &argument = context.native_argument(argument_index);
	target_type *ptr = resolve_native_pointer<target_type>(context, argument);
	if (ptr == nullptr) {
		return false;
	}
	if constexpr (eastl::is_pointer_v<argument_type>) {
		p_consumer(static_cast<argument_type>(ptr));
	} else {
		consume_typed_argument<T>(p_consumer, *ptr);
	}
	return true;
}

template <typename T, typename Consumer>
bool convert_script_argument(
		CallbackContext &context,
		int argument_index,
		Consumer &&p_consumer) {
	if constexpr (has_variant_type_v<T>) {
		return convert_variant_argument<T>(context, argument_index, eastl::forward<Consumer>(p_consumer));
	} else {
		return convert_native_argument<T>(context, argument_index, eastl::forward<Consumer>(p_consumer));
	}
}

template <typename T>
bool assign_converted_argument(
		CallbackContext &context,
		int argument_index,
		unqualified_t<T> &r_value) {
	return convert_script_argument<T>(context, argument_index, [&](auto &&p_value) {
		r_value = eastl::forward<decltype(p_value)>(p_value);
	});
}

inline bool convert_call_arguments(
		CallbackContext &context,
		CallArguments &r_arguments) {
	const int argument_count = context.argument_count();
	r_arguments.resize(argument_count);
	for (int i = 0; i < argument_count; ++i) {
		if (!assign_converted_argument<godot::Variant>(context, i, r_arguments.values[i])) {
			return false;
		}
	}
	return true;
}

template <typename T>
inline void add_variant_return_value(
		CallbackContext &context,
		T &&value) {
	puerts::return_variant_to_script(context.api(), context.callback_info(), context.script_env(), context.puerts_environment(), godot::Variant(eastl::forward<T>(value)));
}

template <typename T>
inline void add_native_owned_return(
		CallbackContext &context,
		T &&value) {
	auto *apis = context.api();
	using target_type = unqualified_t<T>;
	if (!PuertsTypeRegistry::get_singleton().can_own_native(static_type_id<target_type>::get())) {
		(void)context.reject("Native value returns require a registered type with a finalizer.");
		return;
	}
	target_type *owned_value = memnew(target_type(eastl::forward<T>(value)));
	pesapi_value script_value = apis->native_object_to_value(context.script_env(), static_type_id<target_type>::get(), owned_value, true);
	if (script_value == nullptr) {
		godot::memdelete(owned_value);
		apis->throw_by_string(context.callback_info(), "Failed to wrap native return value.");
		return;
	}
	apis->add_return(context.callback_info(), script_value);
}

template <typename R, typename Value>
void write_return_value(CallbackContext &context, Value &&value) {
	auto *apis = context.api();
	using target_type = unqualified_t<R>;
	if constexpr (eastl::is_same_v<target_type, bool>) {
		apis->add_return(context.callback_info(), apis->create_boolean(context.script_env(), value));
	} else if constexpr (eastl::is_integral_v<target_type>) {
		if constexpr (eastl::is_signed_v<target_type>) {
			const auto int_value = static_cast<int64_t>(value);
			if (int_value >= INT32_MIN && int_value <= INT32_MAX) {
				apis->add_return(context.callback_info(), apis->create_int32(context.script_env(), static_cast<int32_t>(int_value)));
			} else {
				apis->add_return(context.callback_info(), apis->create_int64(context.script_env(), int_value));
			}
		} else {
			const auto int_value = static_cast<uint64_t>(value);
			if (int_value <= static_cast<uint64_t>(INT32_MAX)) {
				apis->add_return(context.callback_info(), apis->create_int32(context.script_env(), static_cast<int32_t>(int_value)));
			} else if (int_value <= static_cast<uint64_t>(INT64_MAX)) {
				apis->add_return(context.callback_info(), apis->create_int64(context.script_env(), static_cast<int64_t>(int_value)));
			} else {
				apis->add_return(context.callback_info(), apis->create_uint64(context.script_env(), int_value));
			}
		}
	} else if constexpr (eastl::is_floating_point_v<target_type>) {
		apis->add_return(context.callback_info(), apis->create_double(context.script_env(), static_cast<double>(value)));
	} else if constexpr (has_variant_type_v<R>) {
		add_variant_return_value(context, eastl::forward<Value>(value));
	} else if constexpr (eastl::is_pointer_v<target_type>) {
		if (value == nullptr) {
			apis->add_return(context.callback_info(), apis->create_null(context.script_env()));
			return;
		}
		using pointee_type = eastl::remove_cv_t<eastl::remove_pointer_t<target_type>>;
		static_assert(!has_variant_type_v<pointee_type>, "Return Godot builtin values by value or const reference, not by pointer.");
		if constexpr (eastl::is_const_v<eastl::remove_pointer_t<target_type>>) {
			add_native_owned_return(context, *value);
			return;
		}
		const auto script_value = apis->native_object_to_value(context.script_env(), static_type_id<pointee_type>::get(), const_cast<pointee_type *>(value), false);
		if (script_value == nullptr) {
			(void)context.reject("Failed to wrap borrowed native return value.");
			return;
		}
		apis->add_return(context.callback_info(), script_value);
	} else {
		add_native_owned_return(context, eastl::forward<Value>(value));
	}
}

template <typename T, typename = void>
struct is_builtin_value : eastl::false_type {};

template <typename T>
struct is_builtin_value<T, eastl::enable_if_t<has_variant_type_v<unqualified_t<T>>>> {
	static constexpr auto kVariantType = variant_type_v<unqualified_t<T>>;
	static constexpr bool value = kVariantType != GDEXTENSION_VARIANT_TYPE_OBJECT && kVariantType != GDEXTENSION_VARIANT_TYPE_NIL;
};

template <typename T>
inline constexpr bool is_builtin_value_v = is_builtin_value<T>::value;

template <typename T, bool Boxed = is_builtin_value_v<T>>
struct BoundReceiver;

template <typename T>
struct BoundReceiver<T, true> {
	using target_type = unqualified_t<T>;

	// Builtin variants are copied into local storage and written back after mutation.
	target_type storage{};
	CallbackContext *context = nullptr;
	void *variant_handle = nullptr;

	[[nodiscard]] target_type *get() {
		return &storage;
	}

	[[nodiscard]] bool is_valid() const {
		return variant_handle != nullptr;
	}

	void write_back() const {
		if (variant_handle != nullptr) {
			context->environment_context()->bridge.update_boxed_variant(variant_handle, godot::Variant(storage));
		}
	}
};

template <typename T>
struct BoundReceiver<T, false> {
	using target_type = unqualified_t<T>;

	target_type *native_pointer = nullptr;

	[[nodiscard]] target_type *get() {
		return native_pointer;
	}

	[[nodiscard]] bool is_valid() const {
		return native_pointer != nullptr;
	}

	void write_back() const {}
};

template <typename T>
BoundReceiver<T> resolve_receiver(CallbackContext &context) {
	using target_type = typename BoundReceiver<T>::target_type;
	BoundReceiver<T> instance;
	void *holder = context.holder_ptr();
	if (holder == nullptr) {
		(void)context.reject("Native object is not available.");
		return instance;
	}
	if constexpr (is_builtin_value_v<T>) {
		// Receivers require the exact builtin type; argument conversions are separate.
		const godot::Variant *value = context.holder_boxed_variant();
		if (value == nullptr || value->get_type() != static_cast<godot::Variant::Type>(variant_type_v<target_type>)) {
			(void)context.reject("Native object type does not match the bound signature.");
			return instance;
		}
		instance.storage = godot::VariantCaster<target_type>::cast(*value);
		instance.context = &context;
		instance.variant_handle = holder;
	} else if constexpr (eastl::is_base_of_v<godot::Object, target_type>) {
		godot::Object *object = nullptr;
		if (!context.environment_context()->bridge.try_get_object(holder, object) || object == nullptr) {
			(void)context.reject("Native object is no longer valid.");
			return instance;
		}
		instance.native_pointer = godot::Object::cast_to<target_type>(object);
		if (instance.native_pointer == nullptr) {
			(void)context.reject("Native object type does not match the bound signature.");
		}
	} else {
		CallbackContext::Argument argument;
		argument.native_handle = holder;
		argument.native_type_id = context.holder_type_id();
		instance.native_pointer = resolve_native_pointer<target_type>(context, argument);
	}
	return instance;
}

// Matching never invokes user code or creates a copy of a native C++ object.
// Exact matches precede conversions; Variant is the least specific fallback.
template <typename T>
int match_argument(CallbackContext &context, int index) {
	using value_type = unqualified_t<T>;
	if constexpr (has_variant_type_v<T>) {
		if constexpr (variant_type_v<T> == GDEXTENSION_VARIANT_TYPE_NIL) {
			return 2;
		} else {
			const auto value = context.argument_value(index);
			if constexpr (variant_type_v<T> != GDEXTENSION_VARIANT_TYPE_OBJECT) {
				if (context.native_argument(index).native_handle == nullptr && context.api()->is_object(context.script_env(), value)) {
					return -1;
				}
			}
			const auto &variant = load_variant_argument(context, index);
			if (!can_cast_variant<T>(variant)) {
				return -1;
			}
			return variant.get_type() == static_cast<godot::Variant::Type>(variant_type_v<T>) ? 0 : 1;
		}
	} else {
		using target_type = eastl::remove_cv_t<eastl::remove_pointer_t<value_type>>;
		static_assert(!has_variant_type_v<target_type>, "Use a value or const reference for Godot builtin arguments, not a pointer.");
		if constexpr (eastl::is_pointer_v<value_type>) {
			if (context.api()->is_null(context.script_env(), context.argument_value(index))) {
				return 1;
			}
		}
		const auto &argument = context.native_argument(index);
		if (argument.native_handle == nullptr) {
			return -1;
		}
		const void *expected = static_type_id<target_type>::get();
		if (argument.native_type_id == expected) {
			return 0;
		}
		return PuertsTypeRegistry::get_singleton().cast_native(argument.native_handle, argument.native_type_id, expected) != nullptr ? 1 : -1;
	}
}

} // namespace puerts::internal

#endif // PUERTS_GODOT_PUERTS_BINDING_CONVERSION_H
