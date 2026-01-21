#!/usr/bin/env python3
import socket
import os

HOST = "0.0.0.0"
PORT = 40000
PACKAGE_ROOT = "./packages"

def http_response(body=b"", status="200 OK", content_type="text/plain"):
    hdr = (
        f"HTTP/1.0 {status}\r\n"
        f"Content-Length: {len(body)}\r\n"
        f"Content-Type: text/plain\r\n"
        f"Connection: close\r\n"
        "\r\n"
    )
    return hdr.encode("ascii") + body

def read_request(conn):
    buf = b""
    while b"\r\n\r\n" not in buf and len(buf) < 4096:
        chunk = conn.recv(512)
        if not chunk:
            break
        buf += chunk
    return buf.decode("ascii", errors="ignore")

def list_packages():
    if not os.path.isdir(PACKAGE_ROOT):
        return []
    return sorted(
        d for d in os.listdir(PACKAGE_ROOT)
        if os.path.isdir(os.path.join(PACKAGE_ROOT, d))
    )

def serve_file(path):
    if not os.path.isfile(path):
        return None
    with open(path, "rb") as f:
        return f.read()

def handle_client(conn):
    req = read_request(conn)
    if not req:
        return

    first = req.splitlines()[0]
    print("REQ:", first)

    parts = first.split()
    if len(parts) < 2:
        conn.sendall(http_response(status="400 Bad Request"))
        return

    method, path = parts[0], parts[1]

    if method != "GET":
        conn.sendall(http_response(b"GET only\n", "405 Method Not Allowed"))
        return

    if path == "/ping":
        conn.sendall(http_response(b"pong\n"))
        return

    if path.startswith("/packages"):
        body = "\n".join(list_packages()).encode()
        conn.sendall(http_response(body))
        return

    if path.startswith("/packages/"):
        rel = path[len("/packages/"):]
        fs_path = os.path.normpath(os.path.join(PACKAGE_ROOT, rel))

        if not fs_path.startswith(os.path.abspath(PACKAGE_ROOT)):
            conn.sendall(http_response(b"Forbidden\n", "403 Forbidden"))
            return

        data = serve_file(fs_path)
        if data is None:
            conn.sendall(http_response(b"Not Found\n", "404 Not Found"))
            return

        content_type = "application/json" if fs_path.endswith(".json") else "text/plain"
        conn.sendall(http_response(data, content_type=content_type))
        return

    conn.sendall(http_response(b"Not Found\n", "404 Not Found"))

def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    s.bind((HOST, PORT))
    s.listen(5)

    print(f"UwU package repo listening on {PORT}")

    while True:
        try:
            conn, addr = s.accept()
            handle_client(conn)
            conn.close()
        except KeyboardInterrupt:
            print("\nbye")
            break

if __name__ == "__main__":
    main()
