// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
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

template <size_t I, typename Arguments, typename Consumer, typename... Converted>
bool convert_arguments_at(
		pesapi_ffi *apis,
		CallbackContext &frame,
		Consumer &p_consumer,
		Converted &&...p_converted) {
	if constexpr (I == eastl::tuple_size<Arguments>::value) {
		p_consumer(eastl::forward<Converted>(p_converted)...);
		return true;
	} else {
		using arg_type = typename eastl::tuple_element<I, Arguments>::type;
		bool converted = false;
		if (!convert_script_argument<arg_type>(apis, frame, static_cast<int>(I), [&](auto &&p_value) {
				converted = convert_arguments_at<I + 1, Arguments>(
						apis,
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

template <typename... Args, typename Consumer>
bool convert_script_arguments(
		pesapi_ffi *apis,
		CallbackContext &frame,
		Consumer &&p_consumer) {
	using arguments = eastl::tuple<Args...>;
	return convert_arguments_at<0, arguments>(apis, frame, p_consumer);
}

template <typename R, typename Callable>
void invoke_and_return_value(
		pesapi_ffi *apis,
		CallbackContext &frame,
		Callable &&callable) {
	if constexpr (eastl::is_void_v<R>) {
		(void)eastl::forward<Callable>(callable)();
	} else {
		write_return_value<R>(apis, frame, eastl::forward<Callable>(callable)());
	}
}

template <typename C, bool WriteBack, typename... Args, typename InvokeFn>
bool invoke_receiver(
		pesapi_ffi *apis,
		pesapi_callback_info info,
		CallbackContext &frame,
		InvokeFn &&p_invoke) {
	BoundReceiver<C> instance = resolve_receiver<C>(apis, info, frame);
	if (!instance.is_valid()) {
		return !frame.report_errors();
	}

	if (!convert_script_arguments<Args...>(apis, frame, [&](auto &&...p_args) {
			p_invoke(instance.get(), eastl::forward<decltype(p_args)>(p_args)...);
		})) {
		return false;
	}

	if constexpr (WriteBack) {
		instance.write_back();
	}
	return true;
}

template <typename Binding>
struct MethodEntryCallback {
	static void callback(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackContext frame(apis, info);
		if (!frame.require() || !frame.require_argument_count(Binding::kArity)) {
			return;
		}
		(void)Binding::invoke(apis, info, frame);
	}
};

template <auto Callable, typename C, typename R, bool WriteBack, typename... Args>
struct ReceiverCallback : MethodEntryCallback<ReceiverCallback<Callable, C, R, WriteBack, Args...>> {
	static constexpr int kArity = static_cast<int>(sizeof...(Args));

	static bool invoke(pesapi_ffi *apis, pesapi_callback_info info, CallbackContext &frame) {
		return invoke_receiver<C, WriteBack, Args...>(apis, info, frame, [&](C *instance, auto &&...p_args) {
			invoke_and_return_value<R>(apis, frame, [&]() -> decltype(auto) {
				if constexpr (eastl::is_member_function_pointer_v<decltype(Callable)>) {
					return (instance->*Callable)(eastl::forward<decltype(p_args)>(p_args)...);
				} else {
					return Callable(*instance, eastl::forward<decltype(p_args)>(p_args)...);
				}
			});
		});
	}
};

template <auto Function>
struct FunctionCallback;

template <typename R, typename... Args, R (*Function)(Args...)>
struct FunctionCallback<Function> : MethodEntryCallback<FunctionCallback<Function>> {
	static constexpr int kArity = static_cast<int>(sizeof...(Args));

	static bool invoke(pesapi_ffi *apis, pesapi_callback_info info, CallbackContext &frame) {
		(void)info;
		return convert_script_arguments<Args...>(apis, frame, [&](auto &&...p_args) {
			invoke_and_return_value<R>(apis, frame, [&]() -> decltype(auto) {
				return Function(eastl::forward<decltype(p_args)>(p_args)...);
			});
		});
	}
};

template <auto Method>
struct MemberMethodCallback;

template <typename C, typename R, typename... Args, R (C::*Method)(Args...)>
struct MemberMethodCallback<Method> : ReceiverCallback<Method, C, R, true, Args...> {};

template <auto Method, typename Enable = void>
struct ExtensionMethodCallback;

template <typename C, typename R, typename... Args, R (*Method)(C &, Args...)>
struct ExtensionMethodCallback<Method, eastl::enable_if_t<!eastl::is_const_v<C>>> : ReceiverCallback<Method, C, R, true, Args...> {};

template <typename C, typename R, typename... Args, R (*Method)(const C &, Args...)>
struct ExtensionMethodCallback<Method, void> : ReceiverCallback<Method, C, R, false, Args...> {};

template <typename C, typename R, typename... Args, R (C::*Method)(Args...) const>
struct MemberMethodCallback<Method> : ReceiverCallback<Method, C, R, false, Args...> {};

template <auto Callable>
struct OverloadEntry {
	using type = FunctionCallback<Callable>;
};

template <typename C, typename R, typename... Args, R (C::*Method)(Args...)>
struct OverloadEntry<Method> {
	using type = MemberMethodCallback<Method>;
};

template <typename C, typename R, typename... Args, R (C::*Method)(Args...) const>
struct OverloadEntry<Method> {
	using type = MemberMethodCallback<Method>;
};

template <typename T, typename... Args>
struct ConstructorCallback {
	using target_type = T;
	static constexpr int kArity = static_cast<int>(sizeof...(Args));

	static bool invoke(pesapi_ffi *apis, CallbackContext &frame, void *&r_instance) {
		return convert_script_arguments<Args...>(apis, frame, [&](auto &&...p_args) {
			r_instance = construct(frame, eastl::forward<decltype(p_args)>(p_args)...);
		});
	}

	static void *callback(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackContext frame(apis, info);
		if (!frame.require() || !frame.require_argument_count(static_cast<int>(sizeof...(Args)))) {
			return nullptr;
		}
		void *instance = nullptr;
		(void)invoke(apis, frame, instance);
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
	static void *construct(CallbackContext &frame, Converted &&...p_args) {
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
			if (void *handle = frame.environment_context()->bridge.adopt_object(object, static_type_id<T>::get()); handle != nullptr) {
				return handle;
			}
			godot::memdelete(object);
			return nullptr;
		}
		return memnew(T(eastl::forward<Converted>(p_args)...));
	}
};

template <typename C, typename R, const char *MethodName, int MinArity, bool WriteBack>
struct VarargMethodCallback {
	static void callback(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackContext frame(apis, info);
		if (!frame.require()) {
			return;
		}

		BoundReceiver<C> instance = resolve_receiver<C>(apis, info, frame);
		if (!instance.is_valid()) {
			return;
		}

		CallArguments arguments;
		if (!frame.require_minimum_argument_count(MinArity) ||
				!convert_call_arguments(apis, frame, arguments)) {
			return;
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
		instance_variant.callp(method_name, arguments.pointers.empty() ? nullptr : arguments.pointers.data(), static_cast<int>(arguments.pointers.size()), result, call_error);

		if (call_error.error != GDEXTENSION_CALL_OK) {
			const godot::String target_name = godot::String(script_type_name<C>::value()) + "." + godot::String(method_name);
			const godot::String message = format_call_error(target_name, call_error);
			const godot::CharString utf8 = message.utf8();
			apis->throw_by_string(info, utf8.get_data());
			return;
		}

		if constexpr (!eastl::is_void_v<R>) {
			if constexpr (eastl::is_same_v<unqualified_t<R>, godot::Variant>) {
				write_return_value<R>(apis, frame, result);
			} else {
				write_return_value<R>(apis, frame, godot::VariantCaster<R>::cast(result));
			}
		}

		if constexpr (WriteBack && !eastl::is_base_of_v<godot::Object, C>) {
			*instance.get() = godot::VariantCaster<C>::cast(instance_variant);
			instance.write_back();
		}
	}
};

template <auto Member>
struct MemberPropertyCallbacks;

template <typename C, typename V, V C::*Member>
struct MemberPropertyCallbacks<Member> {
	static void getter(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackContext frame(apis, info);
		if (!frame.require()) {
			return;
		}

		BoundReceiver<C> instance = resolve_receiver<C>(apis, info, frame);
		if (!instance.is_valid()) {
			return;
		}

		write_return_value<V>(apis, frame, instance.get()->*Member);
	}

	static void setter(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackContext frame(apis, info);
		if (!frame.require() || !frame.require_argument_count(1)) {
			return;
		}

		BoundReceiver<C> instance = resolve_receiver<C>(apis, info, frame);
		if (!instance.is_valid()) {
			return;
		}

		if (!convert_script_argument<V>(apis, frame, 0, [&](auto &&p_value) {
				instance.get()->*Member = eastl::forward<decltype(p_value)>(p_value);
			})) {
			return;
		}

		instance.write_back();
	}
};

template <auto ConstantValue>
struct EnumConstantGetter {
	using constant_type = unqualified_t<decltype(ConstantValue)>;
	static_assert(eastl::is_integral_v<constant_type> || eastl::is_enum_v<constant_type>, "Enum constant binding requires an integral or enum constant value.");
	static constexpr int64_t kValue = static_cast<int64_t>(ConstantValue);

	static void getter(pesapi_ffi *apis, pesapi_callback_info info) {
		pesapi_env env = apis->get_env(info);
		if (kValue >= INT32_MIN && kValue <= INT32_MAX) {
			apis->add_return(info, apis->create_int32(env, static_cast<int32_t>(kValue)));
			return;
		}
		apis->add_return(info, apis->create_int64(env, kValue));
	}
};

template <typename EnumTag>
struct EnumTypeGetter {
	static void getter(pesapi_ffi *apis, pesapi_callback_info info) {
		PuertsTypeRegistry &registry = PuertsTypeRegistry::get_singleton();
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
struct SignalGetter {
	static_assert(eastl::is_base_of_v<godot::Object, C>, "Signal binding requires an Object-derived receiver type.");

	static void getter(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackContext frame(apis, info);
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
		puerts::return_variant_to_script(
				apis,
				info,
				frame.script_env(),
				frame.puerts_environment(),
				godot::Variant(godot::Signal(instance.get(), signal_name)));
	}
};

template <typename... Overloads>
struct OverloadCallback {
	template <typename Overload>
	static bool try_invoke(pesapi_ffi *apis, pesapi_callback_info info, CallbackContext &frame) {
		return frame.argument_count() == Overload::kArity && Overload::invoke(apis, info, frame);
	}

	static void callback(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackContext frame(apis, info);
		if (!frame.require()) {
			return;
		}
		frame.set_report_errors(false);
		if (((frame.argument_count() == Overloads::kArity) || ...)) {
			if ((try_invoke<Overloads>(apis, info, frame) || ...)) {
				return;
			}
			const char *reason = frame.last_rejection();
			apis->throw_by_string(info, reason != nullptr ? reason : "No overload matches the provided arguments.");
			return;
		}
		apis->throw_by_string(info, "Argument count does not match the bound signature.");
	}
};

template <typename... Overloads>
struct ConstructorOverloadCallback {
	template <typename Overload>
	static bool try_construct(pesapi_ffi *apis, pesapi_callback_info info, CallbackContext &frame, void *&r_instance) {
		return frame.argument_count() == Overload::kArity && Overload::invoke(apis, frame, r_instance);
	}

	static void *callback(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackContext frame(apis, info);
		if (!frame.require()) {
			return nullptr;
		}
		frame.set_report_errors(false);
		void *instance = nullptr;
		if ((try_construct<Overloads>(apis, info, frame, instance) || ...)) {
			return instance;
		}

		apis->throw_by_string(info, "No constructor overload matches the provided arguments.");
		return nullptr;
	}
};

} // namespace puerts::internal

#endif // PUERTS_GODOT_PUERTS_BINDING_INVOCATION_H
