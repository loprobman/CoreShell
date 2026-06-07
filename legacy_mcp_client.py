#!/usr/bin/env python3
"""legacy_mcp_client.py

Notebook-style MCP compatibility client for CoreShell native C server.

This script uses newline-delimited JSON over TCP and exercises:
- initialize
- list_tools
- call_tool (list_files)
- call_tool (get_time)
- call_tool (delete_older_than_days, dry run)

Usage:
  python3 legacy_mcp_client.py
  python3 legacy_mcp_client.py --host 127.0.0.1 --port 9000 --path artifacts --days 30
"""

import argparse
import json
import socket
from typing import Any, Dict, Optional


def send_msg(sock: socket.socket, obj: Dict[str, Any]) -> None:
    wire = json.dumps(obj, separators=(",", ":")) + "\n"
    sock.sendall(wire.encode("utf-8"))


def recv_msg(sock: socket.socket, timeout: float = 5.0) -> Optional[Any]:
    sock.settimeout(timeout)
    data = b""

    try:
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
            if b"\n" in chunk:
                break
    except socket.timeout:
        pass

    if not data:
        return None

    line = data.decode("utf-8", errors="replace").split("\n", 1)[0].strip()
    if not line:
        return None

    try:
        return json.loads(line)
    except json.JSONDecodeError:
        return line


def request_once(host: str, port: int, payload: Dict[str, Any]) -> Optional[Any]:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((host, port))
        send_msg(sock, payload)
        return recv_msg(sock)


def pretty(label: str, payload: Dict[str, Any], response: Optional[Any]) -> None:
    print(f"\n=== {label} ===")
    print("request:")
    print(json.dumps(payload, indent=2))
    print("response:")
    if response is None:
        print("<no response>")
    elif isinstance(response, (dict, list)):
        print(json.dumps(response, indent=2))
    else:
        print(str(response))


def main() -> int:
    parser = argparse.ArgumentParser(description="CoreShell legacy MCP compatibility client")
    parser.add_argument("--host", default="127.0.0.1", help="Server host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=9000, help="Server port (default: 9000)")
    parser.add_argument("--path", default=".", help="Path for list_files and delete_older_than_days")
    parser.add_argument("--days", type=int, default=30, help="Days threshold for delete_older_than_days")
    parser.add_argument(
        "--delete",
        action="store_true",
        help="Perform deletion instead of dry-run for delete_older_than_days",
    )
    args = parser.parse_args()

    calls = [
        (
            "initialize",
            {"id": 1, "method": "initialize", "params": {}},
        ),
        (
            "list_tools",
            {"id": 2, "method": "list_tools", "params": {}},
        ),
        (
            "call_tool:list_files",
            {
                "id": 3,
                "method": "call_tool",
                "params": {
                    "tool": "list_files",
                    "args": {"path": args.path},
                },
            },
        ),
        (
            "call_tool:get_time",
            {
                "id": 4,
                "method": "call_tool",
                "params": {
                    "tool": "get_time",
                    "args": {},
                },
            },
        ),
        (
            "call_tool:delete_older_than_days",
            {
                "id": 5,
                "method": "call_tool",
                "params": {
                    "tool": "delete_older_than_days",
                    "args": {
                        "path": args.path,
                        "days": args.days,
                        "dryRun": (not args.delete),
                    },
                },
            },
        ),
    ]

    for label, payload in calls:
        try:
            response = request_once(args.host, args.port, payload)
        except OSError as exc:
            print(f"\n=== {label} ===")
            print(f"connection error: {exc}")
            return 1
        pretty(label, payload, response)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
