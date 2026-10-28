// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_STATIC_BINDING_H
#define PUERTS_GODOT_PUERTS_STATIC_BINDING_H

#include "puerts_binding_invocation.h"

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
		if constexpr (internal::has_variant_type_v<T>) {
			record_.variant_type = static_cast<godot::Variant::Type>(godot::GetTypeInfo<T>::VARIANT_TYPE);
		}
		record_.to_variant = internal::variant_converter<T>::get();
	}

	TypeBuilder(const TypeBuilder &) = delete;
	TypeBuilder &operator=(const TypeBuilder &) = delete;
	TypeBuilder(TypeBuilder &&) = default;
	TypeBuilder &operator=(TypeBuilder &&) = default;

	template <typename TBase>
	TypeBuilder &extends() {
		record_.base_id = static_type_id<TBase>::get();
		record_.base_name = script_type_name<TBase>::value();
		return *this;
	}

	template <typename... Args>
	TypeBuilder &constructor() {
		using Invoker = internal::ConstructorInvoker<T, Args...>;
		record_.constructor_overloads.push_back({ Invoker::kArity, &Invoker::invoke });
		return *this;
	}

	TypeBuilder &finalizer(pesapi_finalize p_finalize) {
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
		return add_method<internal::InstanceMethodInvoker<Methods>...>(record_.instance_methods, p_name);
	}

	template <typename R, bool WriteBack = true>
	TypeBuilder &vararg_method(const char *p_name, int p_min_arity = 0) {
		TypeRecord::Method method;
		method.name = godot::StringName(p_name);
		method.overloads.push_back({ p_min_arity, -1, &internal::VarargMethodInvoker<T, R, WriteBack>::invoke });
		record_.instance_methods.push_back(puerts_eastl::move(method));
		return *this;
	}

	template <uint32_t Hash, typename R = godot::Variant>
	TypeBuilder &utility_function(const char *p_name, int p_arity = -1) {
		TypeRecord::Method method;
		method.name = godot::StringName(p_name);
		method.utility_hash = Hash;
		method.overloads.push_back({ p_arity < 0 ? 0 : p_arity, p_arity, &internal::UtilityFunctionInvoker<R>::invoke });
		record_.static_methods.push_back(puerts_eastl::move(method));
		return *this;
	}

	template <auto Getter, auto Setter = nullptr>
	TypeBuilder &property(const char *p_name) {
		TypeRecord::Property property;
		property.name = godot::StringName(p_name);
		property.getter = &internal::PropertyInvoker<Getter, Setter>::getter;
		if constexpr (Setter != nullptr || eastl::is_member_object_pointer_v<decltype(Getter)>) {
			property.setter = &internal::PropertyInvoker<Getter, Setter>::setter;
		}
		record_.instance_properties.push_back(puerts_eastl::move(property));
		return *this;
	}

	template <auto Value>
	TypeBuilder &constant(const char *p_name) {
		TypeRecord::Property property;
		property.name = godot::StringName(p_name);
		if constexpr (eastl::is_function_v<eastl::remove_pointer_t<decltype(Value)>>) {
			property.getter = &internal::ComputedConstantGetter<Value>::getter;
		} else {
			property.getter = &internal::ConstantGetter<Value>::getter;
		}
		record_.static_properties.push_back(puerts_eastl::move(property));
		return *this;
	}

	template <typename EnumTag>
	TypeBuilder &enum_type(const char *p_name) {
		TypeRecord::Property property;
		property.name = godot::StringName(p_name);
		property.getter = &internal::EnumTypeGetter<EnumTag>::getter;
		record_.static_properties.push_back(puerts_eastl::move(property));
		return *this;
	}

	template <const char *SignalName>
	TypeBuilder &signal(const char *p_name) {
		TypeRecord::Property property;
		property.name = godot::StringName(p_name);
		property.getter = &internal::SignalGetter<T, SignalName>::getter;
		record_.instance_properties.push_back(puerts_eastl::move(property));
		return *this;
	}

	TypeBuilder &singleton(const char *p_name) {
		TypeRecord::Property property;
		property.name = godot::StringName(p_name);
		property.getter = &internal::SingletonGetter::getter;
		record_.static_properties.push_back(puerts_eastl::move(property));
		return *this;
	}

	void register_type() {
		PuertsTypeRegistry::get_singleton().register_static_type(puerts_eastl::move(record_));
	}

private:
	template <typename... Invokers>
	TypeBuilder &add_method(puerts_eastl::vector<TypeRecord::Method> &r_methods, const char *p_name) {
		static_assert(sizeof...(Invokers) > 0, "A method binding requires at least one callable.");
		TypeRecord::Method method;
		method.name = godot::StringName(p_name);
		(method.overloads.push_back({ Invokers::kArity, Invokers::kArity, &Invokers::invoke }), ...);
		r_methods.push_back(puerts_eastl::move(method));
		return *this;
	}

	TypeRecord record_;
};

template <typename T>
TypeBuilder<T> define_type() {
	return TypeBuilder<T>();
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
