#!/usr/bin/env python3
import socket

HOST = "0.0.0.0"
PORT = 40000

PACKAGES = {
    "test": "Package: test\nStatus: Working\nDescription: This is a test package to verify the system.\n",
    "demo": "Package: demo\nStatus: Working\nDescription: This is a demo package showing off the OS capabilities.\n",
    "neofetch": "   .      .     UwU OS v1.0.0\n  ( `\\--/' )    -------------\n   ) O  O (     OS: UwU OS x86\n  (  \\  /  )    Kernel: 1.0.0-uwu\n   \\  `'  /     Uptime: 1 min\n    `----'      Packages: 5 (pkg)\n                Shell: UwU Shell\n                CPU: x86 Virtual CPU\n                Memory: 128MB\n",
    "hello": "Hello from the UwU OS package system!\nEverything is working perfectly.\n",
    "sysinfo": "System Information:\n-------------------\nOS: UwU OS\nArch: x86\nFS: Custom RAMFS\nNet: RTL8139 / E1000\nStatus: All systems nominal.\n",
    "cowsay": " _______\n< moo!! >\n -------\n        \\   ^__^\n         \\  (oo)\\_______\n            (__)\\       )\\/\\\n                ||----w |\n                ||     ||\n",
    "fortune": "You will successfully implement TCP/IP today.\n...or maybe tomorrow.\n...actually probably next week.\n",
    "panic": "KERNEL PANIC - not syncing: VFS: Unable to mount root fs\n\nRelax. Everything is fine.\n",
}

def reply(body, status="200 OK"):
    data = body.encode("utf-8")
    hdr = (
        f"HTTP/1.0 {status}\r\n"
        f"Content-Length: {len(data)}\r\n"
        f"Content-Type: text/plain\r\n"
        f"Connection: close\r\n"
        "\r\n"
    )
    return hdr.encode("ascii") + data

def read_request(conn):
    buf = b""
    while b"\r\n\r\n" not in buf and len(buf) < 4096:
        chunk = conn.recv(512)
        if not chunk:
            break
        buf += chunk
    return buf.decode("ascii", errors="ignore")

def handle_client(conn):
    req = read_request(conn)
    if not req:
        print("empty request?")
        return

    first = req.splitlines()[0]
    print("REQ:", first)

    parts = first.split()
    if len(parts) < 2:
        conn.sendall(reply("", "400 Bad Request"))
        return

    method, path = parts[0], parts[1]

    if method != "GET":
        conn.sendall(reply("GET only\n", "405 Method Not Allowed"))
        return

    if path == "/packages/list":
        body = "Available Packages:\n"
        for name in PACKAGES:
            body += f"- {name}\n"
        conn.sendall(reply(body))
        return

    if path.startswith("/packages/"):
        name = path[len("/packages/"):]
        pkg = PACKAGES.get(name)
        if not pkg:
            conn.sendall(reply("Package not found\n", "404 Not Found"))
            return
        conn.sendall(reply(pkg))
        return

    conn.sendall(reply("Not Found\n", "404 Not Found"))

def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    try:
        s.bind((HOST, PORT))
        s.listen(5)
    except OSError as e:
        print("bind failed:", e)
        return

    print(f"ez")
    print("hm")

    while True:
        try:
            conn, addr = s.accept()
            print("connect:", addr)
            handle_client(conn)
            conn.close()
        except KeyboardInterrupt:
            print("\nbye")
            break
        except Exception as e:
            print("oops:", e)

if __name__ == "__main__":
    main()
