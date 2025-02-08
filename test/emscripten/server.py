#!/usr/bin/env python

# Based on http/server.py from Python

from argparse import ArgumentParser
import contextlib
from http.server import SimpleHTTPRequestHandler
from http.server import ThreadingHTTPServer
import os
import socket


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

    def __init__(self, *args, maps=None, **kwargs):
        self.maps = maps or []
        SimpleHTTPRequestHandler.__init__(self, *args, **kwargs)

    def end_headers(self):
        self.send_my_headers()
        SimpleHTTPRequestHandler.end_headers(self)

    def send_my_headers(self):
        self.send_header("Cache-Control", "no-cache, no-store, must-revalidate")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")

    def translate_path(self, path):
        for map_path, map_prefix in self.maps:
            if path.startswith(map_prefix):
                res = os.path.join(map_path, path.removeprefix(map_prefix).lstrip("/"))
                break
        else:
            res = super().translate_path(path)
        return res


def serve_forever(port: int, ServerClass):
    handler = MyHTTPRequestHandler

    addr = ("0.0.0.0", port)
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
    parser.add_argument("--map", dest="maps", nargs="+", type=str, help="Mappings, used as e.g. \"$HOME/projects/SDL:/sdl\"")
    args = parser.parse_args()

    maps = []
    for m in args.maps:
        try:
            path, uri  = m.split(":", 1)
        except ValueError:
            parser.error(f"Invalid mapping: \"{m}\"")
        maps.append((path, uri))

    class DualStackServer(ThreadingHTTPServer):
        def server_bind(self):
            # suppress exception when protocol is IPv4
            with contextlib.suppress(Exception):
                self.socket.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
            return super().server_bind()

        def finish_request(self, request, client_address):
            self.RequestHandlerClass(
                request,
                client_address,
                self,
                directory=args.directory,
                maps=maps,
            )

    return serve_forever(
        port=args.port,
        ServerClass=DualStackServer,
    )


if __name__ == "__main__":
    raise SystemExit(main())
