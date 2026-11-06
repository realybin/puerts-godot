// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: Apache-2.0

#ifndef PUERTS_GODOT_PUERTS_SCRIPT_CALLABLE_H
#define PUERTS_GODOT_PUERTS_SCRIPT_CALLABLE_H

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/callable.hpp>

class PuertsScriptValue;

namespace puerts::internal {

godot::Callable make_script_callable(const godot::Ref<PuertsScriptValue> &p_function);

} // namespace puerts::internal

#endif // PUERTS_GODOT_PUERTS_SCRIPT_CALLABLE_H
