// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_TYPE_RECORD_H
#define PUERTS_GODOT_PUERTS_TYPE_RECORD_H

#include "puerts_type_registry.h"

#include <godot_cpp/core/memory.hpp>

namespace puerts::internal {
class CallbackContext;
}

struct PuertsReflectedType;

struct PuertsTypeRegistry::TypeRecord {
	enum class Kind {
		ReflectedObject,
		StaticBinding,
	};

	enum class RegistrationState {
		Unregistered,
		Registering,
		Registered,
	};

	struct ConstructorOverload {
		int arity = 0;
		int (*match)(puerts::internal::CallbackContext &context) = nullptr;
		void *(*invoke)(puerts::internal::CallbackContext &context) = nullptr;
	};

	struct Method {
		godot::StringName name;
		pesapi_callback callback = nullptr;
		void *callback_data = nullptr;
	};

	struct Property {
		godot::StringName name;
		pesapi_callback getter = nullptr;
		pesapi_callback setter = nullptr;
		void *getter_data = nullptr;
		void *setter_data = nullptr;
	};

	// Payload addresses stay stable when builders and member arrays are moved.
	template <typename T>
	T *make_callback_data(T p_value) {
		T *data = memnew(T(puerts_eastl::move(p_value)));
		callback_data_.emplace_back(data, [](void *pointer) { godot::memdelete(static_cast<T *>(pointer)); });
		return data;
	}

	const void *type_id = nullptr;
	Kind kind = Kind::ReflectedObject;
	godot::StringName name;
	godot::Variant::Type variant_type = godot::Variant::NIL;
	TypeRecord *base = nullptr;
	PuertsReflectedType *reflection = nullptr;
	const void *base_id = nullptr;
	void *(*upcast)(void *) = nullptr;
	bool constructible = false;
	RegistrationState registration_state = RegistrationState::Unregistered;
	puerts_eastl::vector<ConstructorOverload> constructor_overloads;
	pesapi_finalize finalize = nullptr;
	puerts_eastl::vector<Method> static_methods;
	puerts_eastl::vector<Method> instance_methods;
	puerts_eastl::vector<Property> instance_properties;
	puerts_eastl::vector<Property> static_properties;

private:
	puerts_eastl::vector<puerts_eastl::unique_ptr<void, void (*)(void *)>> callback_data_;
};

#endif // PUERTS_GODOT_PUERTS_TYPE_RECORD_H
