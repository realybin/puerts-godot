// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_BINDING_INVOCATION_H
#define PUERTS_GODOT_PUERTS_BINDING_INVOCATION_H

#include "puerts_binding_conversion.h"

#include <godot_cpp/core/version.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/signal.hpp>

#include <EASTL/tuple.h>
#include <EASTL/utility.h>

namespace puerts::internal {

template <bool Probe, size_t I, typename Arguments, typename Consumer, typename... Converted>
bool convert_arguments_at(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		CallbackFrame &frame,
		Consumer &p_consumer,
		Converted &&...p_converted) {
	if constexpr (I == eastl::tuple_size<Arguments>::value) {
		p_consumer(eastl::forward<Converted>(p_converted)...);
		return true;
	} else {
		using arg_type = typename eastl::tuple_element<I, Arguments>::type;
		bool converted = false;
		if (!with_converted_argument<Probe, arg_type>(apis, info, frame, static_cast<int>(I), [&](auto &&p_value) {
				converted = convert_arguments_at<Probe, I + 1, Arguments>(
						apis,
						info,
						frame,
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
bool with_converted_arguments(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		CallbackFrame &frame,
		Consumer &&p_consumer) {
	using arguments = eastl::tuple<Args...>;
	return convert_arguments_at<Probe, 0, arguments>(apis, info, frame, p_consumer);
}

template <typename... Args>
bool require_arity(CallbackFrame &frame) {
	if (frame.arg_count != static_cast<int>(sizeof...(Args))) {
		frame.apis->throw_by_string(frame.info, "Argument count does not match the bound signature.");
		return false;
	}
	return true;
}

template <auto Function, typename R, typename... Args>
void invoke_static_function(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		CallbackFrame &frame,
		Args &&...args) {
	if constexpr (eastl::is_void_v<R>) {
		(void)Function(eastl::forward<Args>(args)...);
	} else {
		write_return_value<R>(apis, info, frame.env, frame.environment, Function(eastl::forward<Args>(args)...));
	}
}

template <auto Method, typename C, typename R, typename... Args>
void invoke_member_function(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		CallbackFrame &frame,
		C *instance,
		Args &&...args) {
	if constexpr (eastl::is_void_v<R>) {
		(void)(instance->*Method)(eastl::forward<Args>(args)...);
	} else {
		write_return_value<R>(apis, info, frame.env, frame.environment, (instance->*Method)(eastl::forward<Args>(args)...));
	}
}

template <auto Function, typename C, typename R, typename... Args>
void invoke_extension_function(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		CallbackFrame &frame,
		C *instance,
		Args &&...args) {
	if constexpr (eastl::is_void_v<R>) {
		(void)Function(*instance, eastl::forward<Args>(args)...);
	} else {
		write_return_value<R>(apis, info, frame.env, frame.environment, Function(*instance, eastl::forward<Args>(args)...));
	}
}

template <typename C, bool Probe, bool WriteBack, typename... Args, typename InvokeFn>
bool invoke_receiver(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		CallbackFrame &frame,
		InvokeFn &&p_invoke) {
	if constexpr (Probe) {
		if (frame.arg_count != static_cast<int>(sizeof...(Args))) {
			return false;
		}
	}

	BoundReceiver<C> instance = resolve_receiver<C>(apis, info, frame);
	if (!instance.is_valid()) {
		if constexpr (Probe) {
			return true;
		}
		return false;
	}

	if (!with_converted_arguments<Probe, Args...>(apis, info, frame, [&](auto &&...p_args) {
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
	static bool invoke(pesapi_ffi *apis, pesapi_callback_info info, CallbackFrame &frame) {
		return invoke_receiver<C, Probe, WriteBack, Args...>(apis, info, frame, [&](C *instance, auto &&...p_args) {
			if constexpr (IsExtension) {
				invoke_extension_function<Callable, C, R>(apis, info, frame, instance, eastl::forward<decltype(p_args)>(p_args)...);
			} else {
				invoke_member_function<Callable, C, R>(apis, info, frame, instance, eastl::forward<decltype(p_args)>(p_args)...);
			}
		});
	}

	static bool try_invoke(pesapi_ffi *apis, pesapi_callback_info info, CallbackFrame &frame) {
		return invoke<true>(apis, info, frame);
	}

	static void callback(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackFrame frame(apis, info);
		if (!frame.require() || !require_arity<Args...>(frame)) {
			return;
		}
		(void)invoke<false>(apis, info, frame);
	}
};

template <auto Function>
struct static_function_wrapper;

template <typename R, typename... Args, R (*Function)(Args...)>
struct static_function_wrapper<Function> {
	static constexpr int arity = static_cast<int>(sizeof...(Args));

	template <bool Probe>
	static bool invoke(pesapi_ffi *apis, pesapi_callback_info info, CallbackFrame &frame) {
		if constexpr (Probe) {
			if (frame.arg_count != arity) {
				return false;
			}
		}
		return with_converted_arguments<Probe, Args...>(apis, info, frame, [&](auto &&...p_args) {
			invoke_static_function<Function, R>(apis, info, frame, eastl::forward<decltype(p_args)>(p_args)...);
		});
	}

	static bool try_invoke(pesapi_ffi *apis, pesapi_callback_info info, CallbackFrame &frame) {
		return invoke<true>(apis, info, frame);
	}

	static void callback(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackFrame frame(apis, info);
		if (!frame.require() || !require_arity<Args...>(frame)) {
			return;
		}
		(void)invoke<false>(apis, info, frame);
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
	static bool invoke(pesapi_ffi *apis, pesapi_callback_info info, CallbackFrame &frame, void *&r_instance) {
		if constexpr (Probe) {
			if (frame.arg_count != arity) {
				return false;
			}
		}
		return with_converted_arguments<Probe, Args...>(apis, info, frame, [&](auto &&...p_args) {
			r_instance = construct(frame, eastl::forward<decltype(p_args)>(p_args)...);
		});
	}

	static bool try_invoke(pesapi_ffi *apis, pesapi_callback_info info, CallbackFrame &frame, void *&r_instance) {
		return invoke<true>(apis, info, frame, r_instance);
	}

	static void *callback(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackFrame frame(apis, info);
		if (!frame.require() || !require_arity<Args...>(frame)) {
			return nullptr;
		}
		void *instance = nullptr;
		(void)invoke<false>(apis, info, frame, instance);
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
	static void *construct(CallbackFrame &frame, Converted &&...p_args) {
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
			if (void *handle = frame.env_private->bridge.own_object(object, static_type_id<T>::get()); handle != nullptr) {
				return handle;
			}
			godot::memdelete(object);
			return nullptr;
		}
		return memnew(T(eastl::forward<Converted>(p_args)...));
	}
};

template <typename C, typename R, const char *MethodName, int MinArity, bool WriteBack>
struct vararg_member_method_wrapper {
	static void callback(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackFrame frame(apis, info);
		if (!frame.require()) {
			return;
		}
		if (frame.arg_count < MinArity) {
			apis->throw_by_string(info, "Argument count does not match the bound signature.");
			return;
		}

		BoundReceiver<C> instance = resolve_receiver<C>(apis, info, frame);
		if (!instance.is_valid()) {
			return;
		}

		puerts_eastl::fixed_vector<godot::Variant, INLINE_ARGUMENT_COUNT> arg_values;
		puerts_eastl::fixed_vector<const godot::Variant *, INLINE_ARGUMENT_COUNT> arg_ptrs;
		arg_values.resize(static_cast<size_t>(frame.arg_count));
		arg_ptrs.resize(static_cast<size_t>(frame.arg_count));

		for (int i = 0; i < frame.arg_count; ++i) {
			if (!convert_argument<false, godot::Variant>(apis, info, frame, i, arg_values[i])) {
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
			const godot::String message = format_call_error(target_name, call_error);
			const godot::CharString utf8 = message.utf8();
			apis->throw_by_string(info, utf8.get_data());
			return;
		}

		if constexpr (!eastl::is_void_v<R>) {
			if constexpr (eastl::is_same_v<unqualified_t<R>, godot::Variant>) {
				write_return_value<R>(apis, info, frame.env, frame.environment, result);
			} else {
				write_return_value<R>(apis, info, frame.env, frame.environment, godot::VariantCaster<R>::cast(result));
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
		CallbackFrame frame(apis, info);
		if (!frame.require()) {
			return;
		}

		BoundReceiver<C> instance = resolve_receiver<C>(apis, info, frame);
		if (!instance.is_valid()) {
			return;
		}

		write_return_value<V>(apis, info, frame.env, frame.environment, instance.get()->*Member);
	}

	static void setter(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackFrame frame(apis, info);
		if (!frame.require() || !require_arity<V>(frame)) {
			return;
		}

		BoundReceiver<C> instance = resolve_receiver<C>(apis, info, frame);
		if (!instance.is_valid()) {
			return;
		}

		if (!with_converted_argument<false, V>(apis, info, frame, 0, [&](auto &&p_value) {
				instance.get()->*Member = eastl::forward<decltype(p_value)>(p_value);
			})) {
			return;
		}

		instance.write_back();
	}
};

template <auto ConstantValue>
struct enum_constant_property_wrapper {
	using constant_type = unqualified_t<decltype(ConstantValue)>;
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
		if (!registry.has_type(enum_type_id)) {
			apis->throw_by_string(info, "Enum type is not registered.");
			return;
		}
		if (!registry.ensure_registered(enum_type_id)) {
			apis->throw_by_string(info, "Failed to register enum type.");
			return;
		}

		pesapi_env env = apis->get_env(info);
		apis->add_return(info, apis->create_class(env, enum_type_id));
	}
};

template <typename C, const char *SignalName>
struct signal_property_wrapper {
	static_assert(eastl::is_base_of_v<godot::Object, C>, "Signal binding requires an Object-derived receiver type.");

	static void getter(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackFrame frame(apis, info);
		if (!frame.require()) {
			return;
		}

		BoundReceiver<C> instance = resolve_receiver<C>(apis, info, frame);
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
				frame.env,
				frame.environment,
				godot::Variant(godot::Signal(instance.get(), signal_name)));
	}
};

template <typename... Overloads>
struct overload_combiner {
	static void callback(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackFrame frame(apis, info);
		if (!frame.require()) {
			return;
		}
		if (!(Overloads::try_invoke(apis, info, frame) || ...)) {
			apis->throw_by_string(info, "No overload matches the provided arguments.");
		}
	}
};

template <typename... Overloads>
struct default_overload_combiner {
	template <typename Overload>
	static bool invoke_matching_arity(pesapi_ffi *apis, pesapi_callback_info info, CallbackFrame &frame) {
		if (frame.arg_count != Overload::arity) {
			return false;
		}
		(void)Overload::template invoke<false>(apis, info, frame);
		return true;
	}

	static void callback(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackFrame frame(apis, info);
		if (!frame.require()) {
			return;
		}
		if (!(invoke_matching_arity<Overloads>(apis, info, frame) || ...)) {
			apis->throw_by_string(info, "Argument count does not match the bound signature.");
		}
	}
};

template <typename... Overloads>
struct constructor_combiner {
	static void *callback(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackFrame frame(apis, info);
		if (!frame.require()) {
			return nullptr;
		}
		void *instance = nullptr;
		if ((Overloads::try_invoke(apis, info, frame, instance) || ...)) {
			return instance;
		}

		apis->throw_by_string(info, "No constructor overload matches the provided arguments.");
		return nullptr;
	}
};

} // namespace puerts::internal

#endif // PUERTS_GODOT_PUERTS_BINDING_INVOCATION_H
