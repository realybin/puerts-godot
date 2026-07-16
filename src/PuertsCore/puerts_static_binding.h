// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_STATIC_BINDING_H
#define PUERTS_GODOT_PUERTS_STATIC_BINDING_H

#include "puerts_bridge_registry.h"
#include "puerts_environment.h"
#include "puerts_runtime.h"
#include "puerts_type_register.h"

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/version.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/signal.hpp>

#include <EASTL/tuple.h>
#include <EASTL/type_traits.h>
#include <EASTL/utility.h>

namespace puerts {

template <typename T, typename = void>
struct script_type_name;

template <typename T>
struct script_type_name<T, eastl::void_t<decltype(T::get_class_static())>> {
	static godot::StringName value() {
		return T::get_class_static();
	}
};

template <typename T>
struct static_type_id {
	static const void *get() {
		static int s_type_tag = 0;
		return &s_type_tag;
	}
};

struct FunctionBinding {
	godot::StringName name;
	pesapi_callback callback = nullptr;
	void *userdata = nullptr;
};

struct PropertyBinding {
	godot::StringName name;
	pesapi_callback getter = nullptr;
	pesapi_callback setter = nullptr;
	void *getter_userdata = nullptr;
	void *setter_userdata = nullptr;
};

struct StaticTypeDefinition {
	const void *type_id = nullptr;
	godot::StringName class_name;
	godot::Variant::Type variant_type = godot::Variant::NIL;
	const void *base_type_id = nullptr;
	godot::StringName base_class_name;
	pesapi_constructor constructor = nullptr;
	pesapi_finalize finalize = nullptr;
	godot::Variant (*native_to_variant)(void *ptr) = nullptr;
	puerts_eastl::vector<FunctionBinding> static_methods;
	puerts_eastl::vector<FunctionBinding> instance_methods;
	puerts_eastl::vector<PropertyBinding> instance_properties;
	puerts_eastl::vector<PropertyBinding> static_properties;
};

struct function_binding_spec {
	pesapi_callback callback = nullptr;
	void *userdata = nullptr;
};

struct property_binding_spec {
	pesapi_callback getter = nullptr;
	pesapi_callback setter = nullptr;
	void *getter_userdata = nullptr;
	void *setter_userdata = nullptr;
};

struct constructor_binding_spec {
	pesapi_constructor constructor = nullptr;
	pesapi_finalize finalize = nullptr;
};

inline godot::Variant script_to_variant(PuertsEnvironment *p_environment, pesapi_env p_env, pesapi_value p_value) {
	return p_environment->script_to_variant(p_env, p_value);
}

inline bool return_variant(
		pesapi_ffi *p_apis,
		pesapi_callback_info p_info,
		pesapi_env p_env,
		PuertsEnvironment *p_environment,
		const godot::Variant &p_value) {
	bool converted = false;
	godot::String error_message;
	pesapi_value script_value = p_environment->variant_to_script(p_env, p_value, &converted, &error_message);
	if (!converted) {
		if (error_message.is_empty()) {
			error_message = "Failed to convert Variant to script value.";
		}
		godot::CharString message = error_message.utf8();
		p_apis->throw_by_string(p_info, message.get_data());
		return false;
	}
	p_apis->add_return(p_info, script_value);
	return true;
}

namespace internal {

template <typename T>
using bare_type = eastl::remove_cv_t<eastl::remove_reference_t<T>>;

template <typename T, typename = void>
struct has_type_info : eastl::false_type {};

template <typename T>
struct has_type_info<T, eastl::void_t<decltype(godot::GetTypeInfo<T>::VARIANT_TYPE)>> : eastl::true_type {};

template <typename T>
inline constexpr bool has_type_info_v = has_type_info<T>::value;

template <typename T>
inline constexpr GDExtensionVariantType gdextension_variant_type_v =
		static_cast<GDExtensionVariantType>(godot::GetTypeInfo<T>::VARIANT_TYPE);

template <typename T>
inline constexpr bool is_nonconst_lvalue_ref_v = eastl::is_lvalue_reference_v<T> && !eastl::is_const_v<eastl::remove_reference_t<T>>;

template <typename T>
inline constexpr bool is_string_like_v = eastl::is_same_v<bare_type<T>, godot::String> || eastl::is_same_v<bare_type<T>, godot::StringName>;

template <typename T>
inline constexpr bool is_binary_like_v = eastl::is_same_v<bare_type<T>, godot::PackedByteArray>;

template <typename T>
struct default_finalize_resolver {
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
struct native_to_variant_resolver {
	static godot::Variant convert(void *) {
		return {};
	}

	static godot::Variant (*get())(void *) {
		return nullptr;
	}
};

template <typename T>
struct native_to_variant_resolver<T, eastl::enable_if_t<has_type_info_v<T>>> {
	static godot::Variant convert(void *ptr) {
		return *static_cast<T *>(ptr);
	}

	static godot::Variant (*get())(void *) {
		if constexpr (gdextension_variant_type_v<T> == GDEXTENSION_VARIANT_TYPE_OBJECT ||
				gdextension_variant_type_v<T> == GDEXTENSION_VARIANT_TYPE_NIL) {
			return nullptr;
		} else {
			return &convert;
		}
	}
};

inline const PuertsTypeRegister::TypeInfo *find_type(const void *p_type_id) {
	return PuertsTypeRegister::get_singleton().get_type_by_id(p_type_id);
}

inline bool is_assignable_from(const PuertsTypeRegister::TypeInfo *p_type_info, const void *p_target_type_id) {
	while (p_type_info != nullptr) {
		if (p_type_info->type_id == p_target_type_id) {
			return true;
		}
		p_type_info = p_type_info->base_type;
	}
	return false;
}

template <typename T>
inline bool is_assignable_from(const PuertsTypeRegister::TypeInfo *p_type_info) {
	return is_assignable_from(p_type_info, static_type_id<T>::get());
}

inline const PuertsTypeRegister::TypeInfo *arg_type(callback_context::arg_native_state &p_state) {
	if (p_state.native_type_id == nullptr) {
		return nullptr;
	}
	if (!p_state.native_type_info_loaded) {
		p_state.native_type_info = find_type(p_state.native_type_id);
		p_state.native_type_info_loaded = true;
	}
	return static_cast<const PuertsTypeRegister::TypeInfo *>(p_state.native_type_info);
}

inline const godot::Variant &arg_variant(
		callback_context &p_context,
		int p_index,
		callback_context::arg_native_state *p_native_state = nullptr) {
	callback_context::arg_variant_state *state = p_context.get_arg_variant_state(p_index);
	if (!state->loaded) {
		if (p_native_state == nullptr ||
				!puerts::native_to_variant(
						p_context.environment,
						p_native_state->native_handle,
						p_native_state->native_type_id,
						state->value)) {
			state->value = puerts::script_to_variant(p_context.environment, p_context.env, p_context.get_arg(p_index));
		}
		state->loaded = true;
	}
	return state->value;
}

template <typename T, typename Consumer>
bool convert_variant_arg_with(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		callback_context &context,
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
		if constexpr (gdextension_variant_type_v<T> != GDEXTENSION_VARIANT_TYPE_NIL) {
			const auto actual_type = static_cast<GDExtensionVariantType>(p_variant.get_type());
			const auto expected_type = gdextension_variant_type_v<T>;
			if (!godot::gdextension_interface::variant_can_convert_strict(actual_type, expected_type) ||
					!godot::VariantObjectClassChecker<T>::check(p_variant)) {
				return reject_argument_type();
			}
		}

		p_consumer(godot::VariantCaster<T>::cast(p_variant));
		return true;
	};

	callback_context::arg_native_state *native_state = nullptr;
	if constexpr (gdextension_variant_type_v<T> != GDEXTENSION_VARIANT_TYPE_OBJECT &&
			gdextension_variant_type_v<T> != GDEXTENSION_VARIANT_TYPE_NIL) {
		pesapi_value value = context.get_arg(p_index);
		if (apis->is_object(context.env, value)) {
			native_state = context.get_arg_native_state(p_index);
			if (native_state->native_handle == nullptr) {
				return reject_argument_type();
			}
		}
	}
	return cast_variant(arg_variant(context, p_index, native_state));
}

template <typename Target>
Target *native_arg_ptr(
		pesapi_ffi *apis,
		callback_context::arg_native_state &p_arg_native_state,
		pesapi_callback_info info = nullptr) {
	if (p_arg_native_state.native_handle == nullptr || PuertsBridgeRegistry::is_handle(p_arg_native_state.native_handle)) {
		if (info != nullptr) {
			apis->throw_by_string(info, "Native object is no longer valid.");
		}
		return nullptr;
	}
	const void *type_id = p_arg_native_state.native_type_id;
	if (type_id == static_type_id<Target>::get()) {
		return static_cast<Target *>(p_arg_native_state.native_handle);
	}
	const PuertsTypeRegister::TypeInfo *type_info = arg_type(p_arg_native_state);
	if (type_info == nullptr || !is_assignable_from<Target>(type_info)) {
		if (info != nullptr) {
			apis->throw_by_string(info, "Native object type does not match the bound signature.");
		}
		return nullptr;
	}

	return static_cast<Target *>(p_arg_native_state.native_handle);
}

template <typename T, typename Consumer>
bool convert_native_arg_with(
		pesapi_ffi *apis,
		callback_context::arg_native_state &p_arg_native_state,
		pesapi_callback_info info,
		Consumer &&p_consumer) {
	static_assert(!is_nonconst_lvalue_ref_v<T>, "Non-const reference arguments are not supported.");
	using argument_type = bare_type<T>;
	using target_type = eastl::conditional_t<
			eastl::is_pointer_v<argument_type>,
			eastl::remove_cv_t<eastl::remove_pointer_t<argument_type>>,
			argument_type>;
	target_type *ptr = native_arg_ptr<target_type>(apis, p_arg_native_state, info);
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
bool convert_arg_with(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		callback_context &context,
		int p_index,
		Consumer &&p_consumer) {
	pesapi_callback_info error_info = Probe ? nullptr : info;
	if constexpr (has_type_info_v<T>) {
		return convert_variant_arg_with<T>(apis, error_info, context, p_index, eastl::forward<Consumer>(p_consumer));
	}
	return convert_native_arg_with<T>(apis, *context.get_arg_native_state(p_index), error_info, eastl::forward<Consumer>(p_consumer));
}

template <bool Probe, typename T>
bool convert_arg(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		callback_context &context,
		int p_index,
		bare_type<T> &r_value) {
	return convert_arg_with<Probe, T>(apis, info, context, p_index, [&](auto &&p_value) {
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
	using target_type = bare_type<T>;
	apis->add_return(info, apis->native_object_to_value(env, static_type_id<target_type>::get(), memnew(target_type(value)), true));
}

template <typename R>
void write_return(pesapi_ffi *apis, pesapi_callback_info info, pesapi_env env, PuertsEnvironment *environment, const R &value) {
	using target_type = bare_type<R>;
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

	if constexpr (has_type_info_v<R>) {
		if constexpr (gdextension_variant_type_v<R> == GDEXTENSION_VARIANT_TYPE_OBJECT ||
				gdextension_variant_type_v<R> == GDEXTENSION_VARIANT_TYPE_NIL) {
			add_variant_return_value(apis, info, env, environment, value);
		} else if constexpr (!eastl::is_arithmetic_v<target_type>) {
			const bool has_static_type_registration =
					find_type(static_type_id<target_type>::get()) != nullptr;
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
struct is_boxed_receiver<T, eastl::enable_if_t<has_type_info_v<bare_type<T>>>> : eastl::bool_constant<
																						 gdextension_variant_type_v<bare_type<T>> != GDEXTENSION_VARIANT_TYPE_OBJECT &&
																						 gdextension_variant_type_v<bare_type<T>> != GDEXTENSION_VARIANT_TYPE_NIL> {};

template <typename T, bool UseStorage>
struct receiver_impl;

template <typename T>
struct receiver_impl<T, true> {
	using target_type = bare_type<T>;

	// Builtin variants are copied into local storage and written back after mutation.
	target_type storage{};
	target_type *raw_ptr = nullptr;
	callback_context *context = nullptr;
	void *boxed_handle = nullptr;

	receiver_impl() = default;

	[[nodiscard]] target_type *get() {
		return boxed_handle != nullptr ? &storage : raw_ptr;
	}

	[[nodiscard]] bool is_valid() const {
		return boxed_handle != nullptr || raw_ptr != nullptr;
	}

	void write_back() const {
		if (boxed_handle != nullptr) {
			context->env_private->bridge->set_box(boxed_handle, godot::Variant(storage));
		}
	}
};

template <typename T>
struct receiver_impl<T, false> {
	using target_type = bare_type<T>;

	target_type *raw_ptr = nullptr;

	receiver_impl() = default;

	[[nodiscard]] target_type *get() {
		return raw_ptr;
	}

	[[nodiscard]] bool is_valid() const {
		return raw_ptr != nullptr;
	}

	void write_back() const {}
};

template <typename T>
using receiver = receiver_impl<T, is_boxed_receiver<T>::value>;

template <typename T>
receiver<T> resolve_receiver(pesapi_ffi *apis, pesapi_callback_info info, callback_context &context) {
	receiver<T> instance;
	if constexpr (is_boxed_receiver<T>::value) {
		instance.context = &context;
	}

	void *holder = context.get_holder_ptr();
	if (holder == nullptr) {
		apis->throw_by_string(info, "Native object is not available.");
		return instance;
	}
	if (const void *holder_type_id = context.get_holder_typeid(); holder_type_id != nullptr) {
		if (holder_type_id != static_type_id<typename receiver<T>::target_type>::get()) {
			if (!context.holder_type_info_loaded) {
				context.holder_type_info = find_type(holder_type_id);
				context.holder_type_info_loaded = true;
			}

			const auto *holder_type_info = static_cast<const PuertsTypeRegister::TypeInfo *>(context.holder_type_info);
			if (holder_type_info == nullptr || !is_assignable_from<typename receiver<T>::target_type>(holder_type_info)) {
				apis->throw_by_string(info, "Native object type does not match the bound signature.");
				return instance;
			}
		}
	}

	if constexpr (has_type_info_v<typename receiver<T>::target_type>) {
		if constexpr (gdextension_variant_type_v<typename receiver<T>::target_type> != GDEXTENSION_VARIANT_TYPE_OBJECT &&
				gdextension_variant_type_v<typename receiver<T>::target_type> != GDEXTENSION_VARIANT_TYPE_NIL) {
			// Boxed builtin receivers come from the bridge; direct native returns use the raw pointer path below.
			if (const godot::Variant *boxed_variant = context.get_holder_boxed_variant(); boxed_variant != nullptr) {
				const auto actual_type = static_cast<GDExtensionVariantType>(boxed_variant->get_type());
				const auto expected_type = gdextension_variant_type_v<typename receiver<T>::target_type>;
				if (actual_type != GDEXTENSION_VARIANT_TYPE_NIL &&
						godot::gdextension_interface::variant_can_convert_strict(actual_type, expected_type) &&
						godot::VariantObjectClassChecker<typename receiver<T>::target_type>::check(*boxed_variant)) {
					instance.storage = godot::VariantCaster<typename receiver<T>::target_type>::cast(*boxed_variant);
					instance.boxed_handle = holder;
					return instance;
				}

				apis->throw_by_string(info, "Native object type does not match the bound signature.");
				return instance;
			}
		}
	}

	if constexpr (eastl::is_base_of_v<godot::Object, typename receiver<T>::target_type>) {
		godot::Object *resolved = nullptr;
		if (context.env_private->bridge->get_object(holder, resolved)) {
			if (resolved == nullptr) {
				apis->throw_by_string(info, "Native object is no longer valid.");
				return instance;
			}
			auto *typed = godot::Object::cast_to<typename receiver<T>::target_type>(resolved);
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

	instance.raw_ptr = static_cast<typename receiver<T>::target_type *>(holder);
	return instance;
}

template <bool Probe, size_t I, typename Arguments, typename Consumer, typename... Converted>
bool convert_args_at(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		callback_context &context,
		Consumer &p_consumer,
		Converted &&...p_converted) {
	if constexpr (I == eastl::tuple_size<Arguments>::value) {
		p_consumer(eastl::forward<Converted>(p_converted)...);
		return true;
	} else {
		using arg_type = typename eastl::tuple_element<I, Arguments>::type;
		bool converted = false;
		if (!convert_arg_with<Probe, arg_type>(apis, info, context, static_cast<int>(I), [&](auto &&p_value) {
				converted = convert_args_at<Probe, I + 1, Arguments>(
						apis,
						info,
						context,
						p_consumer,
						eastl::forward<Converted>(p_converted)...,
						eastl::forward<decltype(p_value)>(p_value));
			})) {
			return false;
		}
		return converted;
	}
}

template <bool Probe, typename... Args, typename Consumer>
bool convert_args_with(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		callback_context &context,
		Consumer &&p_consumer) {
	using arguments = eastl::tuple<Args...>;
	return convert_args_at<Probe, 0, arguments>(apis, info, context, p_consumer);
}

template <typename... Args>
bool check_arity(callback_context &context) {
	if (context.arg_count != static_cast<int>(sizeof...(Args))) {
		context.apis->throw_by_string(context.info, "Argument count does not match the bound signature.");
		return false;
	}
	return true;
}

template <auto Function, typename R, typename... Args>
void call_function_and_return(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		callback_context &context,
		Args &&...args) {
	if constexpr (eastl::is_void_v<R>) {
		(void)Function(eastl::forward<Args>(args)...);
	} else {
		write_return<R>(apis, info, context.env, context.environment, Function(eastl::forward<Args>(args)...));
	}
}

template <auto Method, typename C, typename R, typename... Args>
void call_member_and_return(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		callback_context &context,
		C *instance,
		Args &&...args) {
	if constexpr (eastl::is_void_v<R>) {
		(void)(instance->*Method)(eastl::forward<Args>(args)...);
	} else {
		write_return<R>(apis, info, context.env, context.environment, (instance->*Method)(eastl::forward<Args>(args)...));
	}
}

template <auto Function, typename C, typename R, typename... Args>
void call_extension_and_return(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		callback_context &context,
		C *instance,
		Args &&...args) {
	if constexpr (eastl::is_void_v<R>) {
		(void)Function(*instance, eastl::forward<Args>(args)...);
	} else {
		write_return<R>(apis, info, context.env, context.environment, Function(*instance, eastl::forward<Args>(args)...));
	}
}

template <typename C, bool Probe, bool WriteBack, typename... Args, typename InvokeFn>
bool call_receiver_method(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		callback_context &context,
		InvokeFn &&p_invoke) {
	if constexpr (Probe) {
		if (context.arg_count != static_cast<int>(sizeof...(Args))) {
			return false;
		}
	}

	receiver<C> instance = resolve_receiver<C>(apis, info, context);
	if (!instance.is_valid()) {
		if constexpr (Probe) {
			return true;
		}
		return false;
	}

	if (!convert_args_with<Probe, Args...>(apis, info, context, [&](auto &&...p_args) {
			p_invoke(instance.get(), eastl::forward<decltype(p_args)>(p_args)...);
		})) {
		return false;
	}

	if constexpr (WriteBack) {
		instance.write_back();
	}
	return true;
}

template <auto Callable, typename C, typename R, bool WriteBack, bool IsExtension, typename... Args>
struct receiver_function_wrapper_base {
	static constexpr int arity = static_cast<int>(sizeof...(Args));

	template <bool Probe>
	static bool invoke_core(pesapi_ffi *apis, pesapi_callback_info info, callback_context &context) {
		return call_receiver_method<C, Probe, WriteBack, Args...>(apis, info, context, [&](C *instance, auto &&...p_args) {
			if constexpr (IsExtension) {
				call_extension_and_return<Callable, C, R>(apis, info, context, instance, eastl::forward<decltype(p_args)>(p_args)...);
			} else {
				call_member_and_return<Callable, C, R>(apis, info, context, instance, eastl::forward<decltype(p_args)>(p_args)...);
			}
		});
	}

	static bool try_invoke_with_context(pesapi_ffi *apis, pesapi_callback_info info, callback_context &context) {
		return invoke_core<true>(apis, info, context);
	}

	static void callback(pesapi_ffi *apis, pesapi_callback_info info) {
		callback_context context(apis, info);
		if (!context.require() || !check_arity<Args...>(context)) {
			return;
		}
		(void)invoke_core<false>(apis, info, context);
	}
};

template <auto Function>
struct static_function_wrapper;

template <typename R, typename... Args, R (*Function)(Args...)>
struct static_function_wrapper<Function> {
	static constexpr int arity = static_cast<int>(sizeof...(Args));

	template <bool Probe>
	static bool invoke_core(pesapi_ffi *apis, pesapi_callback_info info, callback_context &context) {
		if constexpr (Probe) {
			if (context.arg_count != arity) {
				return false;
			}
		}
		return convert_args_with<Probe, Args...>(apis, info, context, [&](auto &&...p_args) {
			call_function_and_return<Function, R>(apis, info, context, eastl::forward<decltype(p_args)>(p_args)...);
		});
	}

	static bool try_invoke_with_context(pesapi_ffi *apis, pesapi_callback_info info, callback_context &context) {
		return invoke_core<true>(apis, info, context);
	}

	static void callback(pesapi_ffi *apis, pesapi_callback_info info) {
		callback_context context(apis, info);
		if (!context.require() || !check_arity<Args...>(context)) {
			return;
		}
		(void)invoke_core<false>(apis, info, context);
	}
};

template <auto Method>
struct member_function_wrapper;

template <typename C, typename R, typename... Args, R (C::*Method)(Args...)>
struct member_function_wrapper<Method> : receiver_function_wrapper_base<Method, C, R, true, false, Args...> {};

template <auto Method, typename Enable = void>
struct extension_method_wrapper;

template <typename C, typename R, typename... Args, R (*Method)(C &, Args...)>
struct extension_method_wrapper<Method, eastl::enable_if_t<!eastl::is_const_v<C>>> : receiver_function_wrapper_base<Method, C, R, true, true, Args...> {};

template <typename C, typename R, typename... Args, R (*Method)(const C &, Args...)>
struct extension_method_wrapper<Method, void> : receiver_function_wrapper_base<Method, C, R, false, true, Args...> {};

template <typename C, typename R, typename... Args, R (C::*Method)(Args...) const>
struct member_function_wrapper<Method> : receiver_function_wrapper_base<Method, C, R, false, false, Args...> {};

template <typename T, typename... Args>
struct constructor_wrapper {
	using target_type = T;
	static constexpr int arity = static_cast<int>(sizeof...(Args));

	template <bool Probe>
	static bool invoke_core(pesapi_ffi *apis, pesapi_callback_info info, callback_context &context, void *&r_instance) {
		if constexpr (Probe) {
			if (context.arg_count != arity) {
				return false;
			}
		}
		return convert_args_with<Probe, Args...>(apis, info, context, [&](auto &&...p_args) {
			r_instance = construct(context, eastl::forward<decltype(p_args)>(p_args)...);
		});
	}

	static bool try_invoke_with_context(pesapi_ffi *apis, pesapi_callback_info info, callback_context &context, void *&r_instance) {
		return invoke_core<true>(apis, info, context, r_instance);
	}

	static void *callback(pesapi_ffi *apis, pesapi_callback_info info) {
		callback_context context(apis, info);
		if (!context.require() || !check_arity<Args...>(context)) {
			return nullptr;
		}
		void *instance = nullptr;
		(void)invoke_core<false>(apis, info, context, instance);
		return instance;
	}

private:
	template <typename TObject>
	static TObject *construct_classdb_object() {
#if GODOT_VERSION_MAJOR > 4 || (GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR >= 4)
		GDExtensionObjectPtr native_object = godot::gdextension_interface::classdb_construct_object2(
				static_cast<GDExtensionConstStringNamePtr>(script_type_name<TObject>::value()._native_ptr()));
#else
		GDExtensionObjectPtr native_object = godot::gdextension_interface::classdb_construct_object(
				static_cast<GDExtensionConstStringNamePtr>(script_type_name<TObject>::value()._native_ptr()));
#endif
		godot::Object *object = native_object != nullptr ? godot::internal::get_object_instance_binding(native_object) : nullptr;
		return object != nullptr ? godot::Object::cast_to<TObject>(object) : nullptr;
	}

	template <typename... Converted>
	static void *construct(callback_context &context, Converted &&...p_args) {
		if constexpr (eastl::is_base_of_v<godot::Object, T>) {
			T *object = nullptr;
			if constexpr (sizeof...(Args) == 0) {
				object = construct_classdb_object<T>();
			} else {
				object = memnew(T(eastl::forward<Converted>(p_args)...));
			}
			if (object == nullptr) {
				return nullptr;
			}
			if (void *handle = context.env_private->bridge->own_object(object, static_type_id<T>::get()); handle != nullptr) {
				return handle;
			}
			godot::memdelete(object);
			return nullptr;
		}
		return memnew(T(eastl::forward<Converted>(p_args)...));
	}
};

inline godot::String format_variant_call_error(const godot::String &p_target_name, const GDExtensionCallError &p_call_error) {
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

template <typename C, typename R, const char *MethodName, int MinArity, bool WriteBack>
struct vararg_member_method_wrapper {
	static void callback(pesapi_ffi *apis, pesapi_callback_info info) {
		callback_context context(apis, info);
		if (!context.require()) {
			return;
		}
		if (context.arg_count < MinArity) {
			apis->throw_by_string(info, "Argument count does not match the bound signature.");
			return;
		}

		receiver<C> instance = resolve_receiver<C>(apis, info, context);
		if (!instance.is_valid()) {
			return;
		}

		puerts_eastl::fixed_vector<godot::Variant, INLINE_ARGUMENT_COUNT> arg_values;
		puerts_eastl::fixed_vector<const godot::Variant *, INLINE_ARGUMENT_COUNT> arg_ptrs;
		arg_values.resize(static_cast<size_t>(context.arg_count));
		arg_ptrs.resize(static_cast<size_t>(context.arg_count));

		for (int i = 0; i < context.arg_count; ++i) {
			if (!convert_arg<false, godot::Variant>(apis, info, context, i, arg_values[i])) {
				return;
			}
			arg_ptrs[i] = &arg_values[i];
		}

		static const godot::StringName method_name(MethodName);
		godot::Variant result;
		GDExtensionCallError call_error{ GDEXTENSION_CALL_OK, 0, 0 };
		godot::Variant instance_variant;
		if constexpr (eastl::is_base_of_v<godot::Object, C>) {
			instance_variant = godot::Variant(static_cast<godot::Object *>(instance.get()));
		} else {
			instance_variant = godot::Variant(*instance.get());
		}
		instance_variant.callp(method_name, arg_ptrs.empty() ? nullptr : arg_ptrs.data(), static_cast<int>(arg_ptrs.size()), result, call_error);

		if (call_error.error != GDEXTENSION_CALL_OK) {
			const godot::String target_name = godot::String(script_type_name<C>::value()) + "." + godot::String(method_name);
			const godot::String message = format_variant_call_error(target_name, call_error);
			const godot::CharString utf8 = message.utf8();
			apis->throw_by_string(info, utf8.get_data());
			return;
		}

		if constexpr (!eastl::is_void_v<R>) {
			if constexpr (eastl::is_same_v<bare_type<R>, godot::Variant>) {
				write_return<R>(apis, info, context.env, context.environment, result);
			} else {
				write_return<R>(apis, info, context.env, context.environment, godot::VariantCaster<R>::cast(result));
			}
		}

		if constexpr (WriteBack && !eastl::is_base_of_v<godot::Object, C>) {
			*instance.get() = godot::VariantCaster<C>::cast(instance_variant);
			instance.write_back();
		}
	}
};

template <auto Member>
struct property_wrapper;

template <typename C, typename V, V C::*Member>
struct property_wrapper<Member> {
	static void getter(pesapi_ffi *apis, pesapi_callback_info info) {
		callback_context context(apis, info);
		if (!context.require()) {
			return;
		}

		receiver<C> instance = resolve_receiver<C>(apis, info, context);
		if (!instance.is_valid()) {
			return;
		}

		write_return<V>(apis, info, context.env, context.environment, instance.get()->*Member);
	}

	static void setter(pesapi_ffi *apis, pesapi_callback_info info) {
		callback_context context(apis, info);
		if (!context.require() || !check_arity<V>(context)) {
			return;
		}

		receiver<C> instance = resolve_receiver<C>(apis, info, context);
		if (!instance.is_valid()) {
			return;
		}

		if (!convert_arg_with<false, V>(apis, info, context, 0, [&](auto &&p_value) {
				instance.get()->*Member = eastl::forward<decltype(p_value)>(p_value);
			})) {
			return;
		}

		instance.write_back();
	}
};

template <auto ConstantValue>
struct enum_constant_property_wrapper {
	using constant_type = bare_type<decltype(ConstantValue)>;
	static_assert(eastl::is_integral_v<constant_type> || eastl::is_enum_v<constant_type>, "Enum constant binding requires an integral or enum constant value.");
	static constexpr int64_t value = static_cast<int64_t>(ConstantValue);

	static void getter(pesapi_ffi *apis, pesapi_callback_info info) {
		pesapi_env env = apis->get_env(info);
		if (value >= INT32_MIN && value <= INT32_MAX) {
			apis->add_return(info, apis->create_int32(env, static_cast<int32_t>(value)));
			return;
		}
		apis->add_return(info, apis->create_int64(env, value));
	}
};

template <typename EnumTag>
struct enum_group_property_wrapper {
	static void getter(pesapi_ffi *apis, pesapi_callback_info info) {
		PuertsTypeRegister &registry = PuertsTypeRegister::get_singleton();
		const void *enum_type_id = static_type_id<EnumTag>::get();
		PuertsTypeRegister::TypeInfo *enum_type = registry.get_type_by_id(enum_type_id);
		if (enum_type == nullptr) {
			apis->throw_by_string(info, "Enum type is not registered.");
			return;
		}
		if (!registry.ensure_registered(enum_type)) {
			apis->throw_by_string(info, "Failed to register enum type.");
			return;
		}

		pesapi_env env = apis->get_env(info);
		apis->add_return(info, apis->create_class(env, enum_type->type_id));
	}
};

template <typename C, const char *SignalName>
struct signal_property_wrapper {
	static_assert(eastl::is_base_of_v<godot::Object, C>, "Signal binding requires an Object-derived receiver type.");

	static void getter(pesapi_ffi *apis, pesapi_callback_info info) {
		callback_context context(apis, info);
		if (!context.require()) {
			return;
		}

		receiver<C> instance = resolve_receiver<C>(apis, info, context);
		if (!instance.is_valid()) {
			return;
		}

		static const godot::StringName signal_name(SignalName);
		if (!instance.get()->has_signal(signal_name)) {
			godot::CharString message = godot::String("Signal not found: " + godot::String(signal_name)).utf8();
			apis->throw_by_string(info, message.get_data());
			return;
		}
		puerts::return_variant(
				apis,
				info,
				context.env,
				context.environment,
				godot::Variant(godot::Signal(instance.get(), signal_name)));
	}
};

template <typename... Overloads>
struct overload_combiner {
	static void callback(pesapi_ffi *apis, pesapi_callback_info info) {
		callback_context context(apis, info);
		if (!context.require()) {
			return;
		}
		if (!(Overloads::try_invoke_with_context(apis, info, context) || ...)) {
			apis->throw_by_string(info, "No overload matches the provided arguments.");
		}
	}
};

template <typename... Overloads>
struct constructor_combiner {
	static void *callback(pesapi_ffi *apis, pesapi_callback_info info) {
		callback_context context(apis, info);
		if (!context.require()) {
			return nullptr;
		}
		void *instance = nullptr;
		if ((Overloads::try_invoke_with_context(apis, info, context, instance) || ...)) {
			return instance;
		}

		apis->throw_by_string(info, "No constructor overload matches the provided arguments.");
		return nullptr;
	}
};

} // namespace internal

template <typename Overload>
struct overload_spec {};

template <typename Overload>
struct constructor_overload_spec {};

template <typename T, typename... Args>
constructor_binding_spec make_constructor();

template <godot::Variant::Operator Operator, typename R, typename T>
R evaluate_operator(const T &value) {
	godot::Variant result;
	bool valid = false;
	godot::Variant::evaluate(Operator, godot::Variant(value), godot::Variant(), result, valid);
	(void)valid;
	return godot::VariantCaster<R>::cast(result);
}

template <godot::Variant::Operator Operator, typename R, typename TLeft, typename TRight>
R evaluate_operator(const TLeft &left, const TRight &right) {
	godot::Variant result;
	bool valid = false;
	godot::Variant::evaluate(Operator, godot::Variant(left), godot::Variant(right), result, valid);
	(void)valid;
	return godot::VariantCaster<R>::cast(result);
}

template <typename T>
class class_binding_builder {
public:
	class_binding_builder() {
		definition_.type_id = static_type_id<T>::get();
		definition_.class_name = script_type_name<T>::value();
		definition_.finalize = internal::default_finalize_resolver<T>::get();
		if constexpr (internal::has_type_info_v<T>) {
			definition_.variant_type = static_cast<godot::Variant::Type>(godot::GetTypeInfo<T>::VARIANT_TYPE);
		}
		definition_.native_to_variant = internal::native_to_variant_resolver<T>::get();
	}

	template <typename TBase>
	class_binding_builder &extends() {
		definition_.base_type_id = static_type_id<TBase>::get();
		definition_.base_class_name = script_type_name<TBase>::value();
		return *this;
	}

	class_binding_builder &constructor(const constructor_binding_spec &p_binding) {
		definition_.constructor = p_binding.constructor;
		if (p_binding.finalize != nullptr) {
			definition_.finalize = p_binding.finalize;
		}
		return *this;
	}

	template <typename... Args>
	class_binding_builder &constructor() {
		return constructor(make_constructor<T, Args...>());
	}

	class_binding_builder &function(const char *p_name, const function_binding_spec &p_binding) {
		definition_.static_methods.push_back({ godot::StringName(p_name), p_binding.callback, p_binding.userdata });
		return *this;
	}

	class_binding_builder &function(const char *p_name, pesapi_callback p_callback, void *p_userdata = nullptr) {
		return function(p_name, function_binding_spec{ p_callback, p_userdata });
	}

	class_binding_builder &method(const char *p_name, const function_binding_spec &p_binding) {
		definition_.instance_methods.push_back({ godot::StringName(p_name), p_binding.callback, p_binding.userdata });
		return *this;
	}

	class_binding_builder &method(const char *p_name, pesapi_callback p_callback, void *p_userdata = nullptr) {
		return method(p_name, function_binding_spec{ p_callback, p_userdata });
	}

	class_binding_builder &property(const char *p_name, const property_binding_spec &p_binding) {
		definition_.instance_properties.push_back({ godot::StringName(p_name), p_binding.getter, p_binding.setter, p_binding.getter_userdata, p_binding.setter_userdata });
		return *this;
	}

	class_binding_builder &property(const char *p_name, pesapi_callback p_getter, pesapi_callback p_setter = nullptr, void *p_getter_userdata = nullptr, void *p_setter_userdata = nullptr) {
		return property(p_name, property_binding_spec{ p_getter, p_setter, p_getter_userdata, p_setter_userdata });
	}

	class_binding_builder &static_property(const char *p_name, const property_binding_spec &p_binding) {
		definition_.static_properties.push_back({ godot::StringName(p_name), p_binding.getter, p_binding.setter, p_binding.getter_userdata, p_binding.setter_userdata });
		return *this;
	}

	class_binding_builder &static_property(const char *p_name, pesapi_callback p_getter, pesapi_callback p_setter = nullptr, void *p_getter_userdata = nullptr, void *p_setter_userdata = nullptr) {
		return static_property(p_name, property_binding_spec{ p_getter, p_setter, p_getter_userdata, p_setter_userdata });
	}

	void register_type() const {
		PuertsTypeRegister::get_singleton().register_static_type(definition_);
	}

private:
	StaticTypeDefinition definition_;
};

template <typename T>
class_binding_builder<T> define_class() {
	return class_binding_builder<T>();
}

template <typename T, typename... Args>
constructor_binding_spec make_constructor() {
	return constructor_binding_spec{ &internal::constructor_wrapper<T, Args...>::callback, internal::default_finalize_resolver<T>::get() };
}

template <auto Function>
function_binding_spec make_function() {
	return function_binding_spec{ &internal::static_function_wrapper<Function>::callback, nullptr };
}

template <auto Method>
function_binding_spec make_method() {
	return function_binding_spec{ &internal::member_function_wrapper<Method>::callback, nullptr };
}

template <auto Method>
function_binding_spec make_extension_method() {
	return function_binding_spec{ &internal::extension_method_wrapper<Method>::callback, nullptr };
}

template <typename C, typename R, const char *MethodName, int MinArity = 0, bool WriteBack = true>
function_binding_spec make_vararg_method() {
	return function_binding_spec{ &internal::vararg_member_method_wrapper<C, R, MethodName, MinArity, WriteBack>::callback, nullptr };
}

template <auto Function>
overload_spec<internal::static_function_wrapper<Function>> make_overload() {
	return {};
}

template <auto Method>
overload_spec<internal::member_function_wrapper<Method>> make_method_overload() {
	return {};
}

template <auto Method>
overload_spec<internal::extension_method_wrapper<Method>> make_extension_method_overload() {
	return {};
}

template <typename T, typename... Args>
constructor_overload_spec<internal::constructor_wrapper<T, Args...>> make_constructor_overload() {
	return {};
}

template <typename... Overloads>
function_binding_spec combine_overloads(overload_spec<Overloads>...) {
	return function_binding_spec{ &internal::overload_combiner<Overloads...>::callback, nullptr };
}

template <typename... Overloads>
constructor_binding_spec combine_constructors(constructor_overload_spec<Overloads>...) {
	using first_overload = eastl::tuple_element_t<0, eastl::tuple<Overloads...>>;
	return constructor_binding_spec{ &internal::constructor_combiner<Overloads...>::callback, internal::default_finalize_resolver<typename first_overload::target_type>::get() };
}

template <auto Member>
property_binding_spec make_property() {
	return property_binding_spec{ &internal::property_wrapper<Member>::getter, &internal::property_wrapper<Member>::setter, nullptr, nullptr };
}

template <auto ConstantValue>
property_binding_spec make_enum_constant() {
	return property_binding_spec{ &internal::enum_constant_property_wrapper<ConstantValue>::getter, nullptr, nullptr, nullptr };
}

template <auto Getter>
property_binding_spec make_value_constant() {
	return property_binding_spec{ &internal::static_function_wrapper<Getter>::callback, nullptr, nullptr, nullptr };
}

template <typename EnumTag>
property_binding_spec make_enum_group() {
	return property_binding_spec{ &internal::enum_group_property_wrapper<EnumTag>::getter, nullptr, nullptr, nullptr };
}

template <typename C, const char *SignalName>
property_binding_spec make_signal_property() {
	return property_binding_spec{ &internal::signal_property_wrapper<C, SignalName>::getter, nullptr, nullptr, nullptr };
}

} // namespace puerts

#define PUERTS_SCRIPT_TYPE(T, NAME)         \
	namespace puerts {                      \
	template <>                             \
	struct script_type_name<T, void> {      \
		static godot::StringName value() {  \
			return godot::StringName(NAME); \
		}                                   \
	};                                      \
	}

#endif // PUERTS_GODOT_PUERTS_STATIC_BINDING_H
