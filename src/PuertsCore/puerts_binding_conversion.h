// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_BINDING_CONVERSION_H
#define PUERTS_GODOT_PUERTS_BINDING_CONVERSION_H

#include "puerts_binding_types.h"
#include "puerts_bridge_registry.h"
#include "puerts_environment.h"
#include "puerts_runtime.h"
#include "puerts_type_register.h"

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
inline constexpr bool is_string_like_v = eastl::is_same_v<unqualified_t<T>, godot::String> || eastl::is_same_v<unqualified_t<T>, godot::StringName>;

template <typename T>
inline constexpr bool is_binary_like_v = eastl::is_same_v<unqualified_t<T>, godot::PackedByteArray>;

template <typename T>
inline bool can_cast_variant(const godot::Variant &p_value) {
	return godot::gdextension_interface::variant_can_convert_strict(
				   static_cast<GDExtensionVariantType>(p_value.get_type()),
				   variant_type_v<T>) &&
			godot::VariantObjectClassChecker<T>::check(p_value);
}

template <typename T>
struct finalizer {
	static void finalize(pesapi_ffi *, void *ptr, void *, void *) {
		godot::memdelete(static_cast<T *>(ptr));
	}

	static pesapi_finalize get() {
		if constexpr (eastl::is_base_of_v<godot::Object, T>) {
			return nullptr;
		} else {
			return &finalize;
		}
	}
};

template <typename T, typename = void>
struct variant_converter {
	static godot::Variant convert(void *) {
		return {};
	}

	static godot::Variant (*get())(void *) {
		return nullptr;
	}
};

template <typename T>
struct variant_converter<T, eastl::enable_if_t<has_variant_type_v<T>>> {
	static godot::Variant convert(void *ptr) {
		return *static_cast<T *>(ptr);
	}

	static godot::Variant (*get())(void *) {
		if constexpr (variant_type_v<T> == GDEXTENSION_VARIANT_TYPE_OBJECT ||
				variant_type_v<T> == GDEXTENSION_VARIANT_TYPE_NIL) {
			return nullptr;
		} else {
			return &convert;
		}
	}
};

inline const godot::Variant &load_variant_argument(
		CallbackFrame &p_frame,
		int p_index,
		CallbackFrame::Argument *p_native_argument = nullptr) {
	CallbackFrame::Argument &argument = p_frame.get_argument(p_index);
	if (!argument.variant_loaded) {
		if (p_native_argument == nullptr ||
				!puerts::native_to_variant(
						p_frame.environment,
						p_native_argument->native_handle,
						p_native_argument->native_type_id,
						argument.variant)) {
			argument.variant = puerts::script_to_variant(p_frame.environment, p_frame.env, p_frame.get_argument_value(p_index));
		}
		argument.variant_loaded = true;
	}
	return argument.variant;
}

template <typename T, typename Consumer>
bool with_converted_variant(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		CallbackFrame &frame,
		int p_index,
		Consumer &&p_consumer) {
	static_assert(!is_nonconst_lvalue_ref_v<T>, "Non-const reference arguments are not supported.");
	auto reject_argument_type = [&]() {
		if (info != nullptr) {
			apis->throw_by_string(info, "Argument type does not match the bound signature.");
		}
		return false;
	};
	auto cast_variant = [&](const godot::Variant &p_variant) {
		if constexpr (variant_type_v<T> != GDEXTENSION_VARIANT_TYPE_NIL) {
			if (!can_cast_variant<T>(p_variant)) {
				return reject_argument_type();
			}
		}

		p_consumer(godot::VariantCaster<T>::cast(p_variant));
		return true;
	};

	CallbackFrame::Argument *native_argument = nullptr;
	if constexpr (variant_type_v<T> != GDEXTENSION_VARIANT_TYPE_OBJECT &&
			variant_type_v<T> != GDEXTENSION_VARIANT_TYPE_NIL) {
		pesapi_value value = frame.get_argument_value(p_index);
		if (apis->is_object(frame.env, value)) {
			native_argument = &frame.get_native_argument(p_index);
			if (native_argument->native_handle == nullptr) {
				return reject_argument_type();
			}
		}
	}
	return cast_variant(load_variant_argument(frame, p_index, native_argument));
}

template <typename Target>
Target *resolve_native_pointer(
		pesapi_ffi *apis,
		CallbackFrame::Argument &p_argument,
		pesapi_callback_info info = nullptr) {
	if (p_argument.native_handle == nullptr || PuertsBridgeRegistry::is_handle(p_argument.native_handle)) {
		if (info != nullptr) {
			apis->throw_by_string(info, "Native object is no longer valid.");
		}
		return nullptr;
	}
	const void *type_id = p_argument.native_type_id;
	if (type_id == static_type_id<Target>::get()) {
		return static_cast<Target *>(p_argument.native_handle);
	}
	if (!PuertsTypeRegister::get_singleton().is_assignable(type_id, static_type_id<Target>::get())) {
		if (info != nullptr) {
			apis->throw_by_string(info, "Native object type does not match the bound signature.");
		}
		return nullptr;
	}

	return static_cast<Target *>(p_argument.native_handle);
}

template <typename T, typename Consumer>
bool with_converted_native(
		pesapi_ffi *apis,
		CallbackFrame::Argument &p_argument,
		pesapi_callback_info info,
		Consumer &&p_consumer) {
	static_assert(!is_nonconst_lvalue_ref_v<T>, "Non-const reference arguments are not supported.");
	using argument_type = unqualified_t<T>;
	using target_type = eastl::conditional_t<
			eastl::is_pointer_v<argument_type>,
			eastl::remove_cv_t<eastl::remove_pointer_t<argument_type>>,
			argument_type>;
	target_type *ptr = resolve_native_pointer<target_type>(apis, p_argument, info);
	if (ptr == nullptr) {
		return false;
	}
	if constexpr (eastl::is_pointer_v<argument_type>) {
		p_consumer(static_cast<argument_type>(ptr));
	} else {
		p_consumer(argument_type(*ptr));
	}
	return true;
}

template <bool Probe, typename T, typename Consumer>
bool with_converted_argument(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		CallbackFrame &frame,
		int p_index,
		Consumer &&p_consumer) {
	pesapi_callback_info error_info = Probe ? nullptr : info;
	if constexpr (has_variant_type_v<T>) {
		return with_converted_variant<T>(apis, error_info, frame, p_index, eastl::forward<Consumer>(p_consumer));
	}
	return with_converted_native<T>(apis, frame.get_native_argument(p_index), error_info, eastl::forward<Consumer>(p_consumer));
}

template <bool Probe, typename T>
bool convert_argument(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		CallbackFrame &frame,
		int p_index,
		unqualified_t<T> &r_value) {
	return with_converted_argument<Probe, T>(apis, info, frame, p_index, [&](auto &&p_value) {
		r_value = eastl::forward<decltype(p_value)>(p_value);
	});
}

template <typename T>
inline void add_variant_return_value(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		pesapi_env env,
		PuertsEnvironment *environment,
		T &&value) {
	puerts::return_variant(apis, info, env, environment, godot::Variant(eastl::forward<T>(value)));
}

template <typename T>
inline void add_native_owned_return(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		pesapi_env env,
		const T &value) {
	using target_type = unqualified_t<T>;
	apis->add_return(info, apis->native_object_to_value(env, static_type_id<target_type>::get(), memnew(target_type(value)), true));
}

template <typename R>
void write_return_value(pesapi_ffi *apis, pesapi_callback_info info, pesapi_env env, PuertsEnvironment *environment, const R &value) {
	using target_type = unqualified_t<R>;
	if constexpr (eastl::is_same_v<target_type, bool>) {
		apis->add_return(info, apis->create_boolean(env, value));
		return;
	}

	if constexpr (eastl::is_integral_v<target_type>) {
		if constexpr (eastl::is_signed_v<target_type>) {
			const auto int_value = static_cast<int64_t>(value);
			if (int_value >= INT32_MIN && int_value <= INT32_MAX) {
				apis->add_return(info, apis->create_int32(env, static_cast<int32_t>(int_value)));
			} else {
				apis->add_return(info, apis->create_int64(env, int_value));
			}
		} else {
			const auto int_value = static_cast<uint64_t>(value);
			if (int_value <= static_cast<uint64_t>(INT32_MAX)) {
				apis->add_return(info, apis->create_int32(env, static_cast<int32_t>(int_value)));
			} else if (int_value <= static_cast<uint64_t>(INT64_MAX)) {
				apis->add_return(info, apis->create_int64(env, static_cast<int64_t>(int_value)));
			} else {
				apis->add_return(info, apis->create_uint64(env, int_value));
			}
		}
		return;
	}

	if constexpr (eastl::is_floating_point_v<target_type>) {
		apis->add_return(info, apis->create_double(env, static_cast<double>(value)));
		return;
	}

	if constexpr (is_string_like_v<R> || is_binary_like_v<R>) {
		add_variant_return_value(apis, info, env, environment, value);
		return;
	}

	if constexpr (has_variant_type_v<R>) {
		if constexpr (variant_type_v<R> == GDEXTENSION_VARIANT_TYPE_OBJECT ||
				variant_type_v<R> == GDEXTENSION_VARIANT_TYPE_NIL) {
			add_variant_return_value(apis, info, env, environment, value);
		} else if constexpr (!eastl::is_arithmetic_v<target_type>) {
			// Registration is immutable after module startup. Cache this template-
			// specific decision so hot return paths do not hash the type ID.
			static const bool has_static_type_registration =
					PuertsTypeRegister::get_singleton().has_type(static_type_id<target_type>::get());
			if (has_static_type_registration) {
				add_native_owned_return(apis, info, env, value);
			} else {
				// Generic value types like TypedArray<T> may not have dedicated static bindings.
				// Return them through Variant to preserve engine-backed behavior in script.
				add_variant_return_value(apis, info, env, environment, value);
			}
		}
		return;
	}

	if constexpr (eastl::is_pointer_v<R>) {
		if (value == nullptr) {
			apis->add_return(info, apis->create_null(env));
			return;
		}
		using pointee_type = eastl::remove_cv_t<eastl::remove_pointer_t<R>>;
		apis->add_return(info, apis->native_object_to_value(env, static_type_id<pointee_type>::get(), const_cast<pointee_type *>(value), false));
		return;
	}

	if constexpr (eastl::is_lvalue_reference_v<R>) {
		apis->add_return(info, apis->native_object_to_value(env, static_type_id<target_type>::get(), const_cast<target_type *>(&value), false));
		return;
	}

	add_native_owned_return(apis, info, env, value);
}

template <typename T, typename = void>
struct is_boxed_receiver : eastl::false_type {};

template <typename T>
struct is_boxed_receiver<T, eastl::enable_if_t<has_variant_type_v<unqualified_t<T>>>> : eastl::bool_constant<
																								variant_type_v<unqualified_t<T>> != GDEXTENSION_VARIANT_TYPE_OBJECT &&
																								variant_type_v<unqualified_t<T>> != GDEXTENSION_VARIANT_TYPE_NIL> {};

template <typename T, bool UseStorage>
struct ReceiverStorage;

template <typename T>
struct ReceiverStorage<T, true> {
	using target_type = unqualified_t<T>;

	// Builtin variants are copied into local storage and written back after mutation.
	target_type storage{};
	target_type *raw_ptr = nullptr;
	CallbackFrame *frame = nullptr;
	void *boxed_handle = nullptr;

	ReceiverStorage() = default;

	[[nodiscard]] target_type *get() {
		return boxed_handle != nullptr ? &storage : raw_ptr;
	}

	[[nodiscard]] bool is_valid() const {
		return boxed_handle != nullptr || raw_ptr != nullptr;
	}

	void write_back() const {
		if (boxed_handle != nullptr) {
			frame->env_private->bridge.set_box(boxed_handle, godot::Variant(storage));
		}
	}
};

template <typename T>
struct ReceiverStorage<T, false> {
	using target_type = unqualified_t<T>;

	target_type *raw_ptr = nullptr;

	ReceiverStorage() = default;

	[[nodiscard]] target_type *get() {
		return raw_ptr;
	}

	[[nodiscard]] bool is_valid() const {
		return raw_ptr != nullptr;
	}

	void write_back() const {}
};

template <typename T>
using BoundReceiver = ReceiverStorage<T, is_boxed_receiver<T>::value>;

template <typename T>
BoundReceiver<T> resolve_receiver(pesapi_ffi *apis, pesapi_callback_info info, CallbackFrame &frame) {
	BoundReceiver<T> instance;
	if constexpr (is_boxed_receiver<T>::value) {
		instance.frame = &frame;
	}

	void *holder = frame.get_holder_ptr();
	if (holder == nullptr) {
		apis->throw_by_string(info, "Native object is not available.");
		return instance;
	}
	if (const void *holder_type_id = frame.get_holder_typeid(); holder_type_id != nullptr) {
		const void *expected_type_id = static_type_id<typename BoundReceiver<T>::target_type>::get();
		if (holder_type_id != expected_type_id &&
				!PuertsTypeRegister::get_singleton().is_assignable(holder_type_id, expected_type_id)) {
			apis->throw_by_string(info, "Native object type does not match the bound signature.");
			return instance;
		}
	}

	if constexpr (has_variant_type_v<typename BoundReceiver<T>::target_type>) {
		if constexpr (variant_type_v<typename BoundReceiver<T>::target_type> != GDEXTENSION_VARIANT_TYPE_OBJECT &&
				variant_type_v<typename BoundReceiver<T>::target_type> != GDEXTENSION_VARIANT_TYPE_NIL) {
			// Boxed builtin receivers come from the bridge; direct native returns use the raw pointer path below.
			if (const godot::Variant *boxed_variant = frame.get_holder_boxed_variant(); boxed_variant != nullptr) {
				if (boxed_variant->get_type() != godot::Variant::NIL &&
						can_cast_variant<typename BoundReceiver<T>::target_type>(*boxed_variant)) {
					instance.storage = godot::VariantCaster<typename BoundReceiver<T>::target_type>::cast(*boxed_variant);
					instance.boxed_handle = holder;
					return instance;
				}

				apis->throw_by_string(info, "Native object type does not match the bound signature.");
				return instance;
			}
		}
	}

	if constexpr (eastl::is_base_of_v<godot::Object, typename BoundReceiver<T>::target_type>) {
		godot::Object *resolved = nullptr;
		if (frame.env_private->bridge.get_object(holder, resolved)) {
			if (resolved == nullptr) {
				apis->throw_by_string(info, "Native object is no longer valid.");
				return instance;
			}
			auto *typed = godot::Object::cast_to<typename BoundReceiver<T>::target_type>(resolved);
			if (typed == nullptr) {
				apis->throw_by_string(info, "Native object type does not match the bound signature.");
				return instance;
			}
			instance.raw_ptr = typed;
			return instance;
		}
	}
	if (PuertsBridgeRegistry::is_handle(holder)) {
		apis->throw_by_string(info, "Native object is no longer valid.");
		return instance;
	}

	instance.raw_ptr = static_cast<typename BoundReceiver<T>::target_type *>(holder);
	return instance;
}

} // namespace puerts::internal

#endif // PUERTS_GODOT_PUERTS_BINDING_CONVERSION_H
