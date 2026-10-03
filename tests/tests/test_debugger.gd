# SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
# SPDX-License-Identifier: BSD-3-Clause

extends "res://tests/support/test_case.gd"


func test_debugger_open_and_close() -> bool:
	if backend_name not in ["v8", "nodejs"]:
		return skip("backend has no debugger")
	var server := TCPServer.new()
	if not equal(server.listen(0, "127.0.0.1"), OK, "reserve debugger port"):
		return false
	var port := server.get_local_port()
	server.stop()
	env.open_debugger(port)
	env.debugger_tick()
	check(connects(port), "debugger accepts TCP connections")
	env.close_debugger()
	check(not connects(port), "debugger stops accepting connections")
	return true


func connects(port: int) -> bool:
	var client := StreamPeerTCP.new()
	if client.connect_to_host("127.0.0.1", port) != OK:
		return false
	var deadline := Time.get_ticks_msec() + 1000
	while Time.get_ticks_msec() < deadline:
		if client.poll() != OK or client.get_status() == StreamPeerTCP.STATUS_ERROR:
			break
		if client.get_status() == StreamPeerTCP.STATUS_CONNECTED:
			client.disconnect_from_host()
			return true
		OS.delay_msec(10)
	client.disconnect_from_host()
	return false
