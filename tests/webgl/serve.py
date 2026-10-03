# SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
# SPDX-License-Identifier: BSD-3-Clause

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer


class WebHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        super().end_headers()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", default="build/webgl-smoke")
    parser.add_argument("--port", type=int, default=8000)
    args = parser.parse_args()
    handler = partial(WebHandler, directory=args.directory)
    with ThreadingHTTPServer(("127.0.0.1", args.port), handler) as server:
        server.serve_forever()
