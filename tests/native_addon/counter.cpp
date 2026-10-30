// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "pesapi.h"

namespace {

struct Counter {
	int32_t value = 0;
};

int32_t live_counter_count = 0;

void *construct_counter(pesapi_ffi *p_api, pesapi_callback_info p_info) {
	++live_counter_count;
	return new Counter;
}

void finalize_counter(pesapi_ffi *p_api, void *p_pointer, void *p_class_data, void *p_env_private) {
	delete static_cast<Counter *>(p_pointer);
	--live_counter_count;
}

void get_live_count(pesapi_ffi *p_api, pesapi_callback_info p_info) {
	p_api->add_return(p_info, p_api->create_int32(p_api->get_env(p_info), live_counter_count));
}

void increment_counter(pesapi_ffi *p_api, pesapi_callback_info p_info) {
	Counter *counter = static_cast<Counter *>(p_api->get_native_holder_ptr(p_info));
	p_api->add_return(p_info, p_api->create_int32(p_api->get_env(p_info), ++counter->value));
}

void register_counter(pesapi_registry_api *p_api, pesapi_registry p_registry, const void *p_type_id, const char *p_module_name, const char *p_type_name) {
	p_api->define_class(p_registry, p_type_id, nullptr, p_module_name, p_type_name, construct_counter, finalize_counter, nullptr, false);
	p_api->set_property_info_size(p_registry, p_type_id, 1, 1, 0, 0);
	p_api->set_method_info(p_registry, p_type_id, 0, "increment", false, increment_counter, nullptr, false);
	p_api->set_method_info(p_registry, p_type_id, 0, "live_count", true, get_live_count, nullptr, false);
}

void register_addon(pesapi_registry_api *p_api, pesapi_registry p_registry) {
	static int counter_type_id;
	static int plain_counter_type_id;
	register_counter(p_api, p_registry, &counter_type_id, "Example", "Counter");
	register_counter(p_api, p_registry, &plain_counter_type_id, nullptr, "PlainCounter");
}

} // namespace

PESAPI_MODULE(PuertsTestAddon, register_addon)
