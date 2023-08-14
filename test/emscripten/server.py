#!/usr/bin/env python

# Based on http/server.py from Python

from argparse import ArgumentParser
import contextlib
from http.server import SimpleHTTPRequestHandler
from http.server import ThreadingHTTPServer
import socket
from socketserver import TCPServer


class MyHTTPRequestHandler(SimpleHTTPRequestHandler):
    extensions_map = {
        ".manifest": "text/cache-manifest",
        ".html": "text/html",
        ".png": "image/png",
        ".jpg": "image/jpg",
        ".svg":	"image/svg+xml",
        ".css":	"text/css",
        ".js":	"application/x-javascript",
        ".wasm": "application/wasm",
        "": "application/octet-stream",
    }

    def end_headers(self):
        self.send_my_headers()
        SimpleHTTPRequestHandler.end_headers(self)

    def send_my_headers(self):
        self.send_header("Cache-Control", "no-cache, no-store, must-revalidate")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")


def serve_forever(port: int, ServerClass):
    handler = MyHTTPRequestHandler

    addr = ("0.0.0.0", port)
    HandlerClass = SimpleHTTPRequestHandler
    with ServerClass(addr, handler) as httpd:
        host, port = httpd.socket.getsockname()[:2]
        url_host = f"[{host}]" if ":" in host else host
        print(f"Serving HTTP on {host} port {port} (http://{url_host}:{port}/) ...")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nKeyboard interrupt received, exiting.")
            return 0


def main():
    parser = ArgumentParser(allow_abbrev=False)
    parser.add_argument("port", nargs="?", type=int, default=8080)
    parser.add_argument("-d", dest="directory", type=str, default=None)
    args = parser.parse_args()

    class DualStackServer(ThreadingHTTPServer):
        def server_bind(self):
            # suppress exception when protocol is IPv4
            with contextlib.suppress(Exception):
                self.socket.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
            return super().server_bind()

        def finish_request(self, request, client_address):
            self.RequestHandlerClass(request, client_address, self, directory=args.directory)

    return serve_forever(
        port=args.port,
        ServerClass=DualStackServer,
    )


if __name__ == "__main__":
    raise SystemExit(main())
