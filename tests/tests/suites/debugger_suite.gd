# SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
# SPDX-License-Identifier: BSD-3-Clause

extends RefCounted

const TestSupport = preload("res://tests/support/puerts_support.gd")

static func create_environment(backend: Object, backend_name: String) -> Dictionary:
	return TestSupport.create_environment(backend, backend_name)

static func skip(reason: String) -> Dictionary:
	return TestSupport.skip(reason)
const _HOST = "127.0.0.1"
const _BASE_PORT = 39080
const _CONNECT_TIMEOUT_MS = 1000
const _POLL_INTERVAL_MS = 25


static func _backend_has_debugger(backend_name: String) -> bool:
	return backend_name == "v8" or backend_name == "nodejs"


static func _debug_port_for(backend_info: Dictionary) -> int:
	var backend_index = TestSupport.BACKENDS.find(backend_info)
	if backend_index < 0:
		backend_index = 0
	return _BASE_PORT + backend_index


static func _wait_for_tcp_connect(port: int) -> String:
	var client = StreamPeerTCP.new()
	var connect_error = client.connect_to_host(_HOST, port)
	if connect_error != OK:
		return "tcp connect start failed port=%d error=%d" % [port, connect_error]

	var deadline = Time.get_ticks_msec() + _CONNECT_TIMEOUT_MS
	while Time.get_ticks_msec() <= deadline:
		var poll_error = client.poll()
		if poll_error != OK:
			client.disconnect_from_host()
			return "tcp poll failed port=%d error=%d" % [port, poll_error]

		var status = client.get_status()
		if status == StreamPeerTCP.STATUS_CONNECTED:
			client.disconnect_from_host()
			return ""
		if status == StreamPeerTCP.STATUS_ERROR:
			client.disconnect_from_host()
			return "tcp connect status error port=%d" % port

		OS.delay_msec(_POLL_INTERVAL_MS)

	client.disconnect_from_host()
	return "tcp connect timeout port=%d status=%d" % [port, client.get_status()]


static func run_debugger_open_suite(backend: Object, backend_info: Dictionary) -> Variant:
	var backend_name: String = backend_info["name"]
	if not _backend_has_debugger(backend_name):
		return skip("%s backend does not support debugger" % backend_name)

	var created = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])

	var env: Object = created["env"]
	var port = _debug_port_for(backend_info)
	env.open_debugger(port)
	env.debugger_tick()
	var result = _wait_for_tcp_connect(port)
	env.close_debugger()
	if env.is_alive():
		env.dispose()
	return result


static func run_debugger_close_suite(backend: Object, backend_info: Dictionary) -> Variant:
	var backend_name: String = backend_info["name"]
	if not _backend_has_debugger(backend_name):
		return skip("%s backend does not support debugger" % backend_name)

	var created = create_environment(backend, backend_name)
	if not bool(created["ok"]):
		return str(created["error"])

	var env: Object = created["env"]
	var port = _debug_port_for(backend_info)
	env.open_debugger(port)
	var result = _wait_for_tcp_connect(port)
	if result.is_empty():
		env.close_debugger()
		OS.delay_msec(_POLL_INTERVAL_MS)
		var reconnect_error = _wait_for_tcp_connect(port)
		if reconnect_error.is_empty():
			result = "%s debugger port still accepted tcp connections after close" % backend_name

	if env.is_alive():
		env.dispose()
	return result
