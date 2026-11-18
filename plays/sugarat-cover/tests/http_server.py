#!/usr/bin/env python3
"""Expose the actual firmware HTTP handlers on an OS-assigned loopback port for browser checks."""
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
import argparse
import json
import os
import subprocess
import signal

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bridge', type=Path, required=True)
    parser.add_argument('--storage', type=Path, required=True)
    args = parser.parse_args()
    env = {key: value for key, value in os.environ.items()
           if not any(part in key.upper() for part in ('KEY', 'SECRET', 'TOKEN', 'PASSWORD'))}
    bridge = subprocess.Popen([str(args.bridge.resolve()), str(ROOT / 'main/web'), str(ROOT / 'assets/sample.jpg'),
                               str(args.storage.resolve())], stdin=subprocess.PIPE, stdout=subprocess.PIPE, env=env)
    class Handler(BaseHTTPRequestHandler):
        def handle_request(self):
            size = int(self.headers.get('Content-Length', '0'))
            if size > 160 * 1024 + 1:
                self.send_error(413)
                return
            self.connection.settimeout(10)
            body = self.rfile.read(size)
            method = {'GET': 0, 'POST': 1, 'PUT': 2, 'DELETE': 3}[self.command]
            token = self.headers.get('X-Cover-Token', '-')
            bridge.stdin.write(f'{method} {self.path} {token} {len(body)}\n'.encode() + body)
            bridge.stdin.flush()
            response = bridge.stdout.readline().decode().split()
            if len(response) != 4:
                raise RuntimeError('Firmware handler bridge exited unexpectedly')
            status, content_type, length, location = response
            payload = bridge.stdout.read(int(length))
            self.send_response(int(status))
            self.send_header('Content-Type', content_type.replace('~', ' '))
            self.send_header('Content-Length', str(len(payload)))
            self.send_header('Cache-Control', 'no-store')
            if location != '-':
                self.send_header('Location', location.replace('http://192.168.4.1', f'http://127.0.0.1:{self.server.server_port}', 1))
            self.end_headers()
            self.wfile.write(payload)
        do_GET = do_POST = do_PUT = do_DELETE = handle_request
        def log_message(self, *args):
            pass
    server = HTTPServer(('127.0.0.1', 0), Handler)
    print(json.dumps({'url': f'http://127.0.0.1:{server.server_port}/'}), flush=True)
    def stop(signum, frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, stop)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        bridge.stdin.close()
        try:
            bridge.wait(timeout=10)
        except subprocess.TimeoutExpired:
            bridge.kill()
            bridge.wait()
            raise RuntimeError('Firmware handler bridge did not exit after EOF')
        if bridge.returncode != 0:
            raise RuntimeError(f'Firmware handler bridge failed: {bridge.returncode}')

if __name__ == '__main__':
    main()
