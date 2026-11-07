# SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
# SPDX-License-Identifier: Apache-2.0

extends "res://tests/support/test_case.gd"

const TCPProbe = preload("res://tests/support/tcp_probe.gd")


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
	check(TCPProbe.connects(port, true), "debugger accepts TCP connections")
	env.close_debugger()
	check(not TCPProbe.connects(port, false), "debugger stops accepting connections")
	return true
