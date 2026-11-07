# SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
# SPDX-License-Identifier: Apache-2.0

extends RefCounted


static func connects(port: int, expected: bool) -> bool:
	var client := StreamPeerTCP.new()
	var deadline := Time.get_ticks_msec() + 1000
	var result := client.connect_to_host("127.0.0.1", port)
	var connected := false
	var saw_connected := false
	if result == OK:
		while Time.get_ticks_msec() < deadline:
			result = client.poll()
			var status := client.get_status()
			connected = result == OK and status == StreamPeerTCP.STATUS_CONNECTED
			if result != OK or status in [StreamPeerTCP.STATUS_NONE, StreamPeerTCP.STATUS_ERROR]:
				break
			if connected:
				if expected:
					break
				if not saw_connected:
					# On macOS a closed port can briefly report CONNECTED before NONE.
					# Require the connection to survive a full second before failing close.
					deadline = Time.get_ticks_msec() + 1000
					saw_connected = true
			OS.delay_msec(10)
	client.disconnect_from_host()
	return connected
