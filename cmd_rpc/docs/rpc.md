# rpc

Send one line-based request to a TCP service and print the response.

## Usage

```sh
rpc [-h] [--help-json] [--json] [-H HOST] [-p PORT] [-t SECONDS] [-r RETRIES] <message>
```

## Options

| Option | Description |
|---|---|
| `-h, --help` | show this help and exit |
| `--help-json` | print argument schema as JSON and exit |
| `--json` | output response and connection metadata as JSON |
| `-H, --host <host>` | target host (default: `127.0.0.1`) |
| `-p, --port <port>` | target TCP port (default: `3000`) |
| `-t, --timeout <sec>` | connect/read/write timeout in seconds (default: `3`) |
| `-r, --retries <n>` | retry count after the first attempt (default: `0`) |
| `<message>` | request payload (single line) |

## Protocol

- Transport: TCP (`SOCK_STREAM`)
- Framing: line-based (`\n` terminator)
- Client sends exactly one line (`message + "\n"`)
- Client reads one response line (up to internal limit)

## Examples

```sh
# Send a line to localhost:3000
rpc "ping"

# Custom endpoint with retries
rpc -H 127.0.0.1 -p 5555 -t 2 -r 2 "health"

# Stable JSON output
rpc --json -H 127.0.0.1 -p 3000 "status"
```
