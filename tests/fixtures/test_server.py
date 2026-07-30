#!/usr/bin/env python3
"""Small local HTTP server for integration tests.

It serves files from the current working directory and supports Range requests
through Python's standard SimpleHTTPRequestHandler implementation.
"""

from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
import argparse


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()

    server = ThreadingHTTPServer((args.host, args.port), SimpleHTTPRequestHandler)
    print(f"Serving test files on http://{args.host}:{args.port}", flush=True)
    server.serve_forever()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
