// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_BINDING_INVOCATION_H
#define PUERTS_GODOT_PUERTS_BINDING_INVOCATION_H

#include "puerts_binding_conversion.h"
#include "puerts_type_record.h"

#include <godot_cpp/core/version.hpp>
#include <godot_cpp/godot.hpp>

#include <array>
#include <climits>

#include <EASTL/tuple.h>
#include <EASTL/utility.h>

namespace puerts::internal {

using TypeRecord = PuertsTypeRegistry::TypeRecord;

template <typename... Args>
int match_arguments(CallbackContext &context) {
	int argument_index = 0;
	int total_cost = 0;
	auto accumulate = [&](int cost) {
		if (cost < 0) {
			return false;
		}
		total_cost += cost;
		return true;
	};
	return (accumulate(match_argument<Args>(context, argument_index++)) && ...) ? total_cost : -1;
}

template <size_t I, typename Arguments, typename Consumer, typename... Converted>
bool convert_arguments_at(
		CallbackContext &context,
		Consumer &p_consumer,
		Converted &&...p_converted) {
	if constexpr (I == eastl::tuple_size<Arguments>::value) {
		p_consumer(eastl::forward<Converted>(p_converted)...);
		return true;
	} else {
		using argument_type = typename eastl::tuple_element<I, Arguments>::type;
		bool converted = false;
		if (!convert_script_argument<argument_type>(context, static_cast<int>(I), [&](auto &&p_value) {
				converted = convert_arguments_at<I + 1, Arguments>(
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

template <typename... Args, typename Consumer>
bool convert_script_arguments(
		CallbackContext &context,
		Consumer &&p_consumer) {
	using argument_types = eastl::tuple<Args...>;
	return convert_arguments_at<0, argument_types>(context, p_consumer);
}

template <typename R, typename Callable>
void invoke_and_return_value(
		CallbackContext &context,
		Callable &&callable) {
	if constexpr (eastl::is_void_v<R>) {
		(void)eastl::forward<Callable>(callable)();
	} else {
		write_return_value<R>(context, eastl::forward<Callable>(callable)());
	}
}

template <typename C, bool WriteBack, typename... Args, typename InvokeFn>
bool invoke_receiver(
		CallbackContext &context,
		InvokeFn &&p_invoke) {
	BoundReceiver<C> instance = resolve_receiver<C>(context);
	if (!instance.is_valid()) {
		return false;
	}

	if (!convert_script_arguments<Args...>(context, [&](auto &&...p_args) {
			p_invoke(instance.get(), eastl::forward<decltype(p_args)>(p_args)...);
		})) {
		return false;
	}

	if constexpr (WriteBack) {
		instance.write_back();
	}
	return true;
}

template <auto Function>
struct StaticMethodInvoker;

template <typename R, typename... Args, R (*Function)(Args...)>
struct StaticMethodInvoker<Function> {
	static bool validate_receiver(CallbackContext &) { return true; }
	static constexpr int kArity = static_cast<int>(sizeof...(Args));
	static int match(CallbackContext &context) {
		return match_arguments<Args...>(context);
	}

	static bool invoke(CallbackContext &context) {
		return convert_script_arguments<Args...>(context, [&](auto &&...p_args) {
			invoke_and_return_value<R>(context, [&]() -> decltype(auto) {
				return Function(eastl::forward<decltype(p_args)>(p_args)...);
			});
		});
	}
};

template <auto Callable, typename C, typename R, bool WriteBack, typename... Args>
struct ReceiverInvoker {
	using receiver_type = C;
	static bool validate_receiver(CallbackContext &context) {
		return resolve_receiver<C>(context).is_valid();
	}
	static constexpr int kArity = static_cast<int>(sizeof...(Args));
	static int match(CallbackContext &context) {
		return match_arguments<Args...>(context);
	}

	static bool invoke(CallbackContext &context) {
		return invoke_receiver<C, WriteBack, Args...>(context, [&](C *instance, auto &&...p_args) {
			invoke_and_return_value<R>(context, [&]() -> decltype(auto) {
				if constexpr (eastl::is_member_function_pointer_v<decltype(Callable)>) {
					return (instance->*Callable)(eastl::forward<decltype(p_args)>(p_args)...);
				} else {
					return Callable(*instance, eastl::forward<decltype(p_args)>(p_args)...);
				}
			});
		});
	}
};

template <auto Callable, typename Enable = void>
struct InstanceMethodInvoker;

template <typename C, typename R, typename... Args, R (C::*Method)(Args...)>
struct InstanceMethodInvoker<Method, void> : ReceiverInvoker<Method, C, R, true, Args...> {};

template <typename C, typename R, typename... Args, R (C::*Method)(Args...) const>
struct InstanceMethodInvoker<Method, void> : ReceiverInvoker<Method, C, R, false, Args...> {};

template <typename C, typename R, typename... Args, R (*Method)(C &, Args...)>
struct InstanceMethodInvoker<Method, eastl::enable_if_t<!eastl::is_const_v<C>>> : ReceiverInvoker<Method, C, R, true, Args...> {};

template <typename C, typename R, typename... Args, R (*Method)(const C &, Args...)>
struct InstanceMethodInvoker<Method, void> : ReceiverInvoker<Method, C, R, false, Args...> {};

template <typename T, typename... Args>
struct ConstructorInvoker {
	static constexpr int kArity = static_cast<int>(sizeof...(Args));
	static int match(CallbackContext &context) {
		return match_arguments<Args...>(context);
	}

	static void *invoke(CallbackContext &context) {
		void *instance = nullptr;
		if (!convert_script_arguments<Args...>(context, [&](auto &&...arguments) {
				instance = construct(context, eastl::forward<decltype(arguments)>(arguments)...);
			})) {
			return nullptr;
		}
		if (instance == nullptr) {
			(void)context.reject("Failed to construct native instance.");
		}
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
	static void *construct(CallbackContext &context, Converted &&...p_args) {
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
			if (void *handle = context.environment_context()->bridge.adopt_object(object, static_type_id<T>::get()); handle != nullptr) {
				return handle;
			}
			godot::memdelete(object);
			return nullptr;
		} else if constexpr (is_builtin_value_v<T>) {
			return context.environment_context()->bridge.box_variant(
					godot::Variant(T(eastl::forward<Converted>(p_args)...)), static_type_id<T>::get());
		} else {
			return memnew(T(eastl::forward<Converted>(p_args)...));
		}
	}
};

template <typename C, typename R, bool WriteBack>
struct VarargMethodInvoker {
	static bool invoke(CallbackContext &context) {
		BoundReceiver<C> instance = resolve_receiver<C>(context);
		if (!instance.is_valid()) {
			return false;
		}

		CallArguments arguments;
		if (!convert_call_arguments(context, arguments)) {
			return false;
		}

		auto *apis = context.api();
		const auto info = context.callback_info();
		const auto *method = static_cast<const TypeRecord::Method *>(apis->get_userdata(info));
		godot::Variant result;
		GDExtensionCallError call_error{ GDEXTENSION_CALL_OK, 0, 0 };
		godot::Variant instance_variant;
		if constexpr (eastl::is_base_of_v<godot::Object, C>) {
			instance_variant = godot::Variant(static_cast<godot::Object *>(instance.get()));
		} else {
			instance_variant = godot::Variant(*instance.get());
		}
		instance_variant.callp(method->name, arguments.pointers.empty() ? nullptr : arguments.pointers.data(), static_cast<int>(arguments.pointers.size()), result, call_error);

		if (call_error.error != GDEXTENSION_CALL_OK) {
			const godot::String target_name = godot::String(script_type_name<C>::value()) + "." + godot::String(method->name);
			const godot::String message = format_call_error(target_name, call_error);
			const godot::CharString utf8 = message.utf8();
			apis->throw_by_string(info, utf8.get_data());
			return false;
		}

		if constexpr (!eastl::is_void_v<R>) {
			if constexpr (eastl::is_same_v<unqualified_t<R>, godot::Variant>) {
				write_return_value<R>(context, result);
			} else {
				write_return_value<R>(context, godot::VariantCaster<R>::cast(result));
			}
		}

		if constexpr (WriteBack && !eastl::is_base_of_v<godot::Object, C>) {
			*instance.get() = godot::VariantCaster<C>::cast(instance_variant);
			instance.write_back();
		}
		return true;
	}
};

template <auto Getter, auto Setter>
struct PropertyInvoker {
	using receiver_type = typename InstanceMethodInvoker<Getter>::receiver_type;
	static constexpr bool kWritable = Setter != nullptr;
	static_assert(InstanceMethodInvoker<Getter>::kArity == 0, "A property getter must take no script arguments.");

	static void getter(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackContext context(apis, info);
		if (!context.require() || !context.require_argument_count(0)) {
			return;
		}
		(void)InstanceMethodInvoker<Getter>::invoke(context);
	}

	static void setter(pesapi_ffi *apis, pesapi_callback_info info) {
		static_assert(InstanceMethodInvoker<Setter>::kArity == 1, "A property setter must take one script argument.");
		CallbackContext context(apis, info);
		if (!context.require() || !context.require_argument_count(1)) {
			return;
		}
		(void)InstanceMethodInvoker<Setter>::invoke(context);
	}
};

template <typename C, typename V, V C::*Member>
struct PropertyInvoker<Member, nullptr> {
	using receiver_type = C;
	static constexpr bool kWritable = !eastl::is_const_v<V>;
	static void getter(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackContext context(apis, info);
		if (!context.require()) {
			return;
		}

		BoundReceiver<C> instance = resolve_receiver<C>(context);
		if (!instance.is_valid()) {
			return;
		}

		write_return_value<V>(context, instance.get()->*Member);
	}

	static void setter(pesapi_ffi *apis, pesapi_callback_info info) {
		CallbackContext context(apis, info);
		if (!context.require() || !context.require_argument_count(1)) {
			return;
		}

		BoundReceiver<C> instance = resolve_receiver<C>(context);
		if (!instance.is_valid()) {
			return;
		}

		if (!convert_script_argument<V>(context, 0, [&](auto &&p_value) {
				instance.get()->*Member = eastl::forward<decltype(p_value)>(p_value);
			})) {
			return;
		}

		instance.write_back();
	}
};

template <uint32_t Hash, typename R>
struct UtilityFunctionInvoker {
	static bool invoke(CallbackContext &context) {
		auto *apis = context.api();
		const auto info = context.callback_info();
		const auto *method = static_cast<const TypeRecord::Method *>(apis->get_userdata(info));
		const auto utility_function = godot::gdextension_interface::variant_get_ptr_utility_function(method->name._native_ptr(), Hash);
		if (utility_function == nullptr) {
			apis->throw_by_string(info, "Failed to bind utility function.");
			return false;
		}

		CallArguments arguments;
		if (!convert_call_arguments(context, arguments)) {
			return false;
		}
		if constexpr (eastl::is_void_v<R>) {
			utility_function(nullptr, reinterpret_cast<GDExtensionConstVariantPtr *>(arguments.pointers.data()), context.argument_count());
		} else {
			typename godot::PtrToArg<unqualified_t<R>>::EncodeT result{};
			utility_function(&result, reinterpret_cast<GDExtensionConstVariantPtr *>(arguments.pointers.data()), context.argument_count());
			write_return_value<R>(context, static_cast<unqualified_t<R>>(result));
		}
		return true;
	}
};

// Each bound method owns one callback. The registry does not interpret signatures.
template <typename... Invokers>
struct MethodCallback {
	static void callback(pesapi_ffi *api, pesapi_callback_info info) {
		CallbackContext context(api, info);
		if (!context.require()) {
			return;
		}
		struct Candidate {
			int arity;
			int (*match)(CallbackContext &);
			bool (*invoke)(CallbackContext &);
		};
		static constexpr std::array<Candidate, sizeof...(Invokers)> candidates = { { { Invokers::kArity, &Invokers::match, &Invokers::invoke }... } };
		if constexpr (sizeof...(Invokers) == 1) {
			if (context.require_argument_count(candidates[0].arity)) {
				(void)candidates[0].invoke(context);
			}
			return;
		}
		using first_invoker = typename eastl::tuple_element<0, eastl::tuple<Invokers...>>::type;
		if (!first_invoker::validate_receiver(context)) {
			return;
		}
		const Candidate *selected = nullptr;
		int best_cost = INT_MAX;
		bool ambiguous = false;
		bool matched_arity = false;
		for (const auto &candidate : candidates) {
			if (candidate.arity != context.argument_count()) {
				continue;
			}
			matched_arity = true;
			const int cost = candidate.match(context);
			if (cost >= 0 && cost < best_cost) {
				selected = &candidate;
				best_cost = cost;
				ambiguous = false;
			} else if (cost >= 0 && cost == best_cost) {
				ambiguous = true;
			}
		}
		if (ambiguous) {
			(void)context.reject("Ambiguous bound method overload.");
			return;
		}
		if (selected == nullptr) {
			(void)context.reject(matched_arity ? "Argument type does not match the bound signature." : "Argument count does not match the bound signature.");
			return;
		}

		(void)selected->invoke(context);
	}
};

template <typename C, typename R, bool WriteBack, int MinimumArity>
struct VarargMethodCallback {
	static void callback(pesapi_ffi *api, pesapi_callback_info info) {
		CallbackContext context(api, info);
		if (context.require() && context.require_minimum_argument_count(MinimumArity)) {
			(void)VarargMethodInvoker<C, R, WriteBack>::invoke(context);
		}
	}
};

template <uint32_t Hash, typename R, int Arity>
struct UtilityFunctionCallback {
	static void callback(pesapi_ffi *api, pesapi_callback_info info) {
		CallbackContext context(api, info);
		if (!context.require()) {
			return;
		}
		if (Arity < 0 ? context.require_minimum_argument_count(0) : context.require_argument_count(Arity)) {
			(void)UtilityFunctionInvoker<Hash, R>::invoke(context);
		}
	}
};

} // namespace puerts::internal

#endif // PUERTS_GODOT_PUERTS_BINDING_INVOCATION_H
