// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_STATIC_BINDING_H
#define PUERTS_GODOT_PUERTS_STATIC_BINDING_H

#include "puerts_binding_invocation.h"
#include "puerts_native_signature.h"

namespace puerts {

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
class TypeBuilder {
	using TypeRecord = PuertsTypeRegistry::TypeRecord;

public:
	TypeBuilder() {
		record_.type_id = static_type_id<T>::get();
		record_.kind = TypeRecord::Kind::StaticBinding;
		record_.name = script_type_name<T>::value();
		record_.finalize = internal::finalizer<T>::get();
		if constexpr (eastl::is_base_of_v<godot::Object, T>) {
			record_.variant_type = godot::Variant::OBJECT;
		} else if constexpr (internal::has_variant_type_v<T>) {
			record_.variant_type = static_cast<godot::Variant::Type>(godot::GetTypeInfo<T>::VARIANT_TYPE);
		}
	}

	// Namespaces and enum groups contain members but have no C++ instance type.
	explicit TypeBuilder(const char *p_name) {
		static_assert(eastl::is_void_v<T>, "Named scopes use define_namespace().");
		record_.kind = TypeRecord::Kind::StaticBinding;
		record_.name = p_name;
	}

	TypeBuilder(const TypeBuilder &) = delete;
	TypeBuilder &operator=(const TypeBuilder &) = delete;
	TypeBuilder(TypeBuilder &&) = delete;
	TypeBuilder &operator=(TypeBuilder &&) = delete;

	template <typename TBase>
	TypeBuilder &extends() {
		record_.base_id = static_type_id<TBase>::get();
		static_assert(eastl::is_base_of_v<TBase, T> && !eastl::is_same_v<TBase, T>, "extends requires a proper C++ base class.");
		record_.upcast = [](void *pointer) -> void * {
			return static_cast<TBase *>(static_cast<T *>(pointer));
		};
		return *this;
	}

	template <typename... Args>
	TypeBuilder &constructor() {
		using Invoker = internal::ConstructorInvoker<T, Args...>;
		record_.constructor_overloads.push_back({ Invoker::kArity, &Invoker::match, &Invoker::invoke });
		return *this;
	}

	TypeBuilder &finalizer(pesapi_finalize p_finalize) {
		static_assert(!eastl::is_base_of_v<godot::Object, T> && !internal::has_variant_type_v<T>, "Godot types manage their own lifetime.");
		record_.finalize = p_finalize;
		return *this;
	}

	TypeBuilder &no_finalize() {
		return finalizer(nullptr);
	}

	template <auto... Functions>
	TypeBuilder &static_method(const char *p_name) {
		return add_method<internal::StaticMethodInvoker<Functions>...>(record_.static_methods, p_name);
	}

	template <auto... Methods>
	TypeBuilder &method(const char *p_name) {
		static_assert((eastl::is_base_of_v<typename internal::InstanceMethodInvoker<Methods>::receiver_type, T> && ...),
				"Instance methods must belong to the bound type or one of its bases.");
		return add_method<internal::InstanceMethodInvoker<Methods>...>(record_.instance_methods, p_name);
	}

	template <typename R, bool WriteBack = true, int MinimumArity = 0>
	TypeBuilder &vararg_method(const char *p_name) {
		TypeRecord::Method method;
		method.name = godot::StringName(p_name);
		method.callback = &internal::VarargMethodCallback<T, R, WriteBack, MinimumArity>::callback;
		record_.instance_methods.push_back(puerts_eastl::move(method));
		return *this;
	}

	template <uint32_t Hash, typename R = godot::Variant, int Arity = -1>
	TypeBuilder &utility_function(const char *p_name) {
		TypeRecord::Method method;
		method.name = godot::StringName(p_name);
		method.callback = &internal::UtilityFunctionCallback<Hash, R, Arity>::callback;
		record_.static_methods.push_back(puerts_eastl::move(method));
		return *this;
	}

	template <auto Getter, auto Setter = nullptr>
	TypeBuilder &property(const char *p_name) {
		using Invoker = internal::PropertyInvoker<Getter, Setter>;
		static_assert(eastl::is_base_of_v<typename Invoker::receiver_type, T>, "Properties must belong to the bound type or one of its bases.");
		TypeRecord::Property property;
		property.name = godot::StringName(p_name);
		property.getter = &Invoker::getter;
		if constexpr (Invoker::kWritable) {
			property.setter = &Invoker::setter;
		}
		record_.instance_properties.push_back(puerts_eastl::move(property));
		return *this;
	}

	TypeBuilder &constant(const char *p_name, int64_t p_value) {
		TypeRecord::Property property;
		property.name = p_name;
		property.getter_data = record_.make_callback_data(p_value);
		property.getter = &PuertsTypeRegistry::integer_constant_getter_callback;
		record_.static_properties.push_back(puerts_eastl::move(property));
		return *this;
	}

	template <auto Value>
	TypeBuilder &constant(const char *p_name) {
		if constexpr (eastl::is_function_v<eastl::remove_pointer_t<decltype(Value)>>) {
			using Invoker = internal::StaticMethodInvoker<Value>;
			static_assert(Invoker::kArity == 0, "Computed constants must take no arguments.");
			TypeRecord::Property property;
			property.name = p_name;
			property.getter = &internal::MethodCallback<Invoker>::callback;
			record_.static_properties.push_back(puerts_eastl::move(property));
			return *this;
		} else {
			static_assert(eastl::is_integral_v<decltype(Value)> || eastl::is_enum_v<decltype(Value)>, "Constants require an integer, enum, or getter.");
			static_assert(!eastl::is_unsigned_v<decltype(Value)> || static_cast<uint64_t>(Value) <= INT64_MAX, "Godot integer constants must fit in int64_t.");
			return constant(p_name, static_cast<int64_t>(Value));
		}
	}

	TypeBuilder &nested_type(const char *p_name, const char *p_type_name) {
		TypeRecord::Property property;
		property.name = godot::StringName(p_name);
		property.getter = &PuertsTypeRegistry::nested_type_getter_callback;
		property.getter_data = record_.make_callback_data(godot::StringName(p_type_name));
		record_.static_properties.push_back(puerts_eastl::move(property));
		return *this;
	}

	TypeBuilder &signal(const char *p_name) {
		TypeRecord::Property property;
		property.name = godot::StringName(p_name);
		static_assert(eastl::is_base_of_v<godot::Object, T>, "Signals require a Godot Object type.");
		property.getter = &PuertsTypeRegistry::signal_getter_callback;
		record_.instance_properties.push_back(puerts_eastl::move(property));
		return *this;
	}

	TypeBuilder &singleton(const char *p_name) {
		TypeRecord::Property property;
		property.name = godot::StringName(p_name);
		property.getter = &PuertsTypeRegistry::singleton_getter_callback;
		record_.static_properties.push_back(puerts_eastl::move(property));
		return *this;
	}

	bool register_type() {
		ERR_FAIL_COND_V_MSG(submitted_, false, "A type builder can only be submitted once.");
		submitted_ = true;
		return PuertsTypeRegistry::get_singleton().register_static_type(puerts_eastl::move(record_));
	}

private:
	template <typename... Invokers>
	TypeBuilder &add_method(puerts_eastl::vector<TypeRecord::Method> &r_methods, const char *p_name) {
		static_assert(sizeof...(Invokers) > 0, "A method binding requires at least one callable.");
		TypeRecord::Method method;
		method.name = godot::StringName(p_name);
		method.callback = &internal::MethodCallback<Invokers...>::callback;
		r_methods.push_back(puerts_eastl::move(method));
		return *this;
	}

	TypeRecord record_;
	bool submitted_ = false;
};

template <typename T>
TypeBuilder<T> define_type() {
	return TypeBuilder<T>();
}

inline TypeBuilder<void> define_namespace(const char *p_name) {
	return TypeBuilder<void>(p_name);
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
