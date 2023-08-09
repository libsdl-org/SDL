#!/usr/bin/env python

# Based on http/server.py from Python

from argparse import ArgumentParser
from http.server import SimpleHTTPRequestHandler
from socketserver import TCPServer


def serve_forever(port: int, ServerClass):
    handler = SimpleHTTPRequestHandler
    handler.extensions_map = {
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

    addr = ("0.0.0.0", port)
    HandlerClass = SimpleHTTPRequestHandler
    with ServerClass(addr, handler) as httpd:
        host, port = httpd.socket.getsockname()[:2]
        url_host = f"[{host}]" if ":" in host else host
        print(
            f"Serving HTTP on {host} port {port} (http://{url_host}:{port}/) ..."
        )
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

    import contextlib
    import http.server
    import socket

    class DualStackServer(http.server.ThreadingHTTPServer):
        def server_bind(self):
            # suppress exception when protocol is IPv4
            with contextlib.suppress(Exception):
                self.socket.setsockopt(
                    socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
            return super().server_bind()

        def finish_request(self, request, client_address):
            self.RequestHandlerClass(request, client_address, self, directory=args.directory)

    return serve_forever(
        port=args.port,
        ServerClass=DualStackServer,
    )


if __name__ == "__main__":
    raise SystemExit(main())
