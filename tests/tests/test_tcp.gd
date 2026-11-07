# SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
# SPDX-License-Identifier: Apache-2.0

extends "res://tests/support/test_case.gd"

const TCPProbe = preload("res://tests/support/tcp_probe.gd")


func per_backend() -> bool:
	return false


func test_tcp_server_open_and_close() -> bool:
	# Match the debugger's TCP probe sequence without using the inspector or Asio.
	var server := TCPServer.new()
	if not equal(server.listen(0, "127.0.0.1"), OK, "listen on Godot TCP port"):
		return false
	var port := server.get_local_port()
	check(TCPProbe.connects(port, true), "Godot TCPServer accepts TCP connections")
	server.stop()
	check(not TCPProbe.connects(port, false), "Godot TCPServer stops accepting connections")
	return true
