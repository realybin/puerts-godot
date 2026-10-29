// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PUERTS_GODOT_PUERTS_NATIVE_SIGNATURE_H
#define PUERTS_GODOT_PUERTS_NATIVE_SIGNATURE_H

#include <cstddef>

#include <EASTL/tuple.h>

namespace puerts::internal {

template <typename Signature>
struct native_parameters;

template <typename R, typename... Args>
struct native_parameters<R (*)(Args...)> {
	using types = eastl::tuple<Args...>;
};

template <typename R, typename C, typename... Args>
struct native_parameters<R (C::*)(Args...)> : native_parameters<R (*)(Args...)> {};

template <typename R, typename C, typename... Args>
struct native_parameters<R (C::*)(Args...) const> : native_parameters<R (*)(Args...)> {};

template <typename Signature, std::size_t Index>
using native_parameter_t = typename eastl::tuple_element<Index, typename native_parameters<Signature>::types>::type;

} // namespace puerts::internal

#endif // PUERTS_GODOT_PUERTS_NATIVE_SIGNATURE_H
