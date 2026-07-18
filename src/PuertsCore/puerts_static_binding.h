// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_STATIC_BINDING_H
#define PUERTS_GODOT_PUERTS_STATIC_BINDING_H

#include "puerts_binding_invocation.h"

namespace puerts {

template <typename Overload>
struct OverloadSpec {};

template <typename Overload>
struct ConstructorOverloadSpec {};

template <typename T, typename... Args>
ConstructorSpec make_constructor();

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
public:
	TypeBuilder() {
		definition_.type_id = static_type_id<T>::get();
		definition_.name = script_type_name<T>::value();
		definition_.finalize = internal::finalizer<T>::get();
		if constexpr (internal::has_variant_type_v<T>) {
			definition_.variant_type = static_cast<godot::Variant::Type>(godot::GetTypeInfo<T>::VARIANT_TYPE);
		}
		definition_.to_variant = internal::variant_converter<T>::get();
	}

	template <typename TBase>
	TypeBuilder &extends() {
		definition_.base_id = static_type_id<TBase>::get();
		definition_.base_name = script_type_name<TBase>::value();
		return *this;
	}

	TypeBuilder &constructor(const ConstructorSpec &p_binding) {
		definition_.constructor = p_binding.constructor;
		if (p_binding.finalize != nullptr) {
			definition_.finalize = p_binding.finalize;
		}
		return *this;
	}

	template <typename... Args>
	TypeBuilder &constructor() {
		return constructor(make_constructor<T, Args...>());
	}

	TypeBuilder &static_method(const char *p_name, const MethodSpec &p_binding) {
		definition_.static_methods.push_back({ godot::StringName(p_name), p_binding.callback, p_binding.userdata });
		return *this;
	}

	TypeBuilder &static_method(const char *p_name, pesapi_callback p_callback, void *p_userdata = nullptr) {
		return static_method(p_name, MethodSpec{ p_callback, p_userdata });
	}

	TypeBuilder &method(const char *p_name, const MethodSpec &p_binding) {
		definition_.instance_methods.push_back({ godot::StringName(p_name), p_binding.callback, p_binding.userdata });
		return *this;
	}

	TypeBuilder &method(const char *p_name, pesapi_callback p_callback, void *p_userdata = nullptr) {
		return method(p_name, MethodSpec{ p_callback, p_userdata });
	}

	TypeBuilder &property(const char *p_name, const PropertySpec &p_binding) {
		definition_.instance_properties.push_back({ godot::StringName(p_name), p_binding.getter, p_binding.setter, p_binding.getter_userdata, p_binding.setter_userdata });
		return *this;
	}

	TypeBuilder &property(const char *p_name, pesapi_callback p_getter, pesapi_callback p_setter = nullptr, void *p_getter_userdata = nullptr, void *p_setter_userdata = nullptr) {
		return property(p_name, PropertySpec{ p_getter, p_setter, p_getter_userdata, p_setter_userdata });
	}

	TypeBuilder &static_property(const char *p_name, const PropertySpec &p_binding) {
		definition_.static_properties.push_back({ godot::StringName(p_name), p_binding.getter, p_binding.setter, p_binding.getter_userdata, p_binding.setter_userdata });
		return *this;
	}

	TypeBuilder &static_property(const char *p_name, pesapi_callback p_getter, pesapi_callback p_setter = nullptr, void *p_getter_userdata = nullptr, void *p_setter_userdata = nullptr) {
		return static_property(p_name, PropertySpec{ p_getter, p_setter, p_getter_userdata, p_setter_userdata });
	}

	void register_type() const {
		PuertsTypeRegister::get_singleton().register_static_type(definition_);
	}

private:
	TypeDefinition definition_;
};

template <typename T>
TypeBuilder<T> define_type() {
	return TypeBuilder<T>();
}

template <typename T, typename... Args>
ConstructorSpec make_constructor() {
	return ConstructorSpec{ &internal::constructor_wrapper<T, Args...>::callback, internal::finalizer<T>::get() };
}

template <auto Function>
MethodSpec make_function() {
	return MethodSpec{ &internal::static_function_wrapper<Function>::callback, nullptr };
}

template <auto Method>
MethodSpec make_method() {
	return MethodSpec{ &internal::member_function_wrapper<Method>::callback, nullptr };
}

template <auto Method>
MethodSpec make_extension_method() {
	return MethodSpec{ &internal::extension_method_wrapper<Method>::callback, nullptr };
}

template <typename C, typename R, const char *MethodName, int MinArity = 0, bool WriteBack = true>
MethodSpec make_vararg_method() {
	return MethodSpec{ &internal::vararg_member_method_wrapper<C, R, MethodName, MinArity, WriteBack>::callback, nullptr };
}

template <auto Function>
OverloadSpec<internal::static_function_wrapper<Function>> make_overload() {
	return {};
}

template <auto Method>
OverloadSpec<internal::member_function_wrapper<Method>> make_method_overload() {
	return {};
}

template <auto Method>
OverloadSpec<internal::extension_method_wrapper<Method>> make_extension_method_overload() {
	return {};
}

template <typename T, typename... Args>
ConstructorOverloadSpec<internal::constructor_wrapper<T, Args...>> make_constructor_overload() {
	return {};
}

template <typename... Overloads>
MethodSpec combine_overloads(OverloadSpec<Overloads>...) {
	return MethodSpec{ &internal::overload_combiner<Overloads...>::callback, nullptr };
}

template <typename... Overloads>
MethodSpec combine_default_overloads(OverloadSpec<Overloads>...) {
	return MethodSpec{ &internal::default_overload_combiner<Overloads...>::callback, nullptr };
}

template <typename... Overloads>
ConstructorSpec combine_constructors(ConstructorOverloadSpec<Overloads>...) {
	using first_overload = eastl::tuple_element_t<0, eastl::tuple<Overloads...>>;
	return ConstructorSpec{ &internal::constructor_combiner<Overloads...>::callback, internal::finalizer<typename first_overload::target_type>::get() };
}

template <auto Member>
PropertySpec make_property() {
	return PropertySpec{ &internal::property_wrapper<Member>::getter, &internal::property_wrapper<Member>::setter, nullptr, nullptr };
}

template <auto ConstantValue>
PropertySpec make_enum_constant() {
	return PropertySpec{ &internal::enum_constant_property_wrapper<ConstantValue>::getter, nullptr, nullptr, nullptr };
}

template <auto Getter>
PropertySpec make_value_constant() {
	return PropertySpec{ &internal::static_function_wrapper<Getter>::callback, nullptr, nullptr, nullptr };
}

template <typename EnumTag>
PropertySpec make_enum_group() {
	return PropertySpec{ &internal::enum_group_property_wrapper<EnumTag>::getter, nullptr, nullptr, nullptr };
}

template <typename C, const char *SignalName>
PropertySpec make_signal_property() {
	return PropertySpec{ &internal::signal_property_wrapper<C, SignalName>::getter, nullptr, nullptr, nullptr };
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
