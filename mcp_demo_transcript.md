# MCP Demo Transcript (3 NL Queries, Native C Server)

Server: 127.0.0.1:9000 (./mcp_server)
Date: 2026-06-04T20:57:54-04:00

## Query 1
Prompt: Show available MCP tools.

Request:
{"type":"tools/list"}

Response:
{"ok":true,"type":"tools/list","protocol":"mcp-line-json","tools":[{"name":"registry.packages.list","description":"List the CoreShell packages known to the registry"},{"name":"registry.package.lookup","description":"Look up one package by name"},{"name":"shell.commands.list","description":"List the CoreShell commands exposed by the shell"},{"name":"shell.command.help","description":"Return the help metadata for a CoreShell command"},{"name":"shell.command.run","description":"Run a small allowlisted shell command"},{"name":"filesystem.delete_older_than_days","description":"Delete files older than N days under a workspace path"}]}

## Query 2
Prompt: List CoreShell commands and show help for rpc.

Request A:
{"type":"tools/call","tool":"shell.commands.list"}

Response A:
{"ok":true,"tool":"shell.commands.list","result":[{"name":"cat","summary":"concatenate files and print to stdout","longDescription":"Concatenate one or more files and write them to standard output.","docsPath":"cmd_cat/docs/cat.md"},{"name":"cd","summary":"change the current directory","longDescription":"Change the current working directory to the specified path, or $HOME if no path is given.","docsPath":"cmd_cd/docs/cd.md"},{"name":"cp","summary":"copy a file or directory","longDescription":"Copy SOURCE to DEST. Use -r to copy directories recursively.","docsPath":"cmd_cp/docs/cp.md"},{"name":"echo","summary":"print arguments to standard output","longDescription":"Echo the given string arguments to stdout, separated by spaces. Supports -n and -e flags similar to GNU echo.","docsPath":"cmd_echo/docs/echo.md"},{"name":"exit","summary":"exit the shell","longDescription":"Exit the CoreShell immediately.","docsPath":"cmd_exit/docs/exit.md"},{"name":"head","summary":"print the first lines of a file","longDescription":"Output the first N lines (default: 10) of the given file. Use -n to specify a different line count.","docsPath":"cmd_head/docs/head.md"},{"name":"help","summary":"show help for built-in commands","longDescription":"Display a list of all built-in commands, or detailed help for a specific command.","docsPath":"cmd_help/docs/help.md"},{"name":"jobs","summary":"list active background jobs","longDescription":"Display all active background jobs in the current shell session, including job id, state, pid, and command.","docsPath":"cmd_jobs/docs/jobs.md"},{"name":"kill","summary":"send a signal to process or job","longDescription":"Send a signal to a process by PID or a background job by %job id. Defaults to SIGTERM and supports -s SIGNAL.","docsPath":"cmd_kill/docs/kill.md"},{"name":"ls","summary":"list directory contents","longDescription":"List entries in the specified directory (default: current directory). Use -a to include hidden entries (names beginning with '.'). Use -l for a long listing showing permissions, owner, size, and modification time.","docsPath":"cmd_ls/docs/ls.md"},{"name":"mkdir","summary":"create directories","longDescription":"Create one or more directories. Use -p to create parent directories as needed.","docsPath":"cmd_mkdir/docs/mkdir.md"},{"name":"mv","summary":"move (rename) a file or directory","longDescription":"Rename SOURCE to DEST, moving it if they are on the same filesystem.","docsPath":"cmd_mv/docs/mv.md"},{"name":"pkg","summary":"manage CoreShell packages","longDescription":"Build, install, list, remove, compile, and upgrade packages for CoreShell.","docsPath":"cmd_pkg/docs/pkg.md"},{"name":"pwd","summary":"print working directory","longDescription":"Print the absolute path of the current working directory. By default uses getcwd(3) to return the physical path with symlinks resolved. Use -L to return the logical $PWD from the environment, which may contain symlinks.","docsPath":"cmd_pwd/docs/pwd.md"},{"name":"rm","summary":"remove files or directories","longDescription":"Remove one or more files. Use -r to remove directories recursively.","docsPath":"cmd_rm/docs/rm.md"},{"name":"rmdir","summary":"remove empty directories","longDescription":"Remove one or more empty directories. Use -p to also remove parent directories if they become empty.","docsPath":"cmd_rmdir/docs/rmdir.md"},{"name":"rpc","summary":"send a line request to a TCP service","longDescription":"Connect to a TCP host/port, send a single line request, and print the line-based response. Supports timeout, retries, and optional JSON output.","docsPath":"cmd_rpc/docs/rpc.md"},{"name":"stat","summary":"display file status","longDescription":"Show size, mode, link count, owner ids, and timestamps for one or more files.","docsPath":"cmd_stat/docs/stat.md"},{"name":"tail","summary":"print the last lines of a file","longDescription":"Output the last N lines (default: 10) of the given file.","docsPath":"cmd_tail/docs/tail.md"},{"name":"touch","summary":"create a file or update its timestamp","longDescription":"Update the access and modification timestamps of each file. Create the file if it does not exist, unless -c is specified.","docsPath":"cmd_touch/docs/touch.md"}],"type":"tools/call","protocol":"mcp-line-json"}

Request B:
{"type":"tools/call","tool":"shell.command.help","arguments":{"name":"rpc"}}

Response B:
{"ok":true,"tool":"shell.command.help","result":{"name":"rpc","summary":"send a line request to a TCP service","longDescription":"Connect to a TCP host/port, send a single line request, and print the line-based response. Supports timeout, retries, and optional JSON output.","docs":"# rpc\n\nSend one line-based request to a TCP service and print the response.\n\n## Usage\n\n```sh\nrpc [-h] [--help-json] [--json] [-H HOST] [-p PORT] [-t SECONDS] [-r RETRIES] <message>\n```\n\n## Options\n\n| Option | Description |\n|---|---|\n| `-h, --help` | show this help and exit |\n| `--help-json` | print argument schema as JSON and exit |\n| `--json` | output response and connection metadata as JSON |\n| `-H, --host <host>` | target host (default: `127.0.0.1`) |\n| `-p, --port <port>` | target TCP port (default: `3000`) |\n| `-t, --timeout <sec>` | connect/read/write timeout in seconds (default: `3`) |\n| `-r, --retries <n>` | retry count after the first attempt (default: `0`) |\n| `<message>` | request payload (single line) |\n\n## Protocol\n\n- Transport: TCP (`SOCK_STREAM`)\n- Framing: line-based (`\\n` terminator)\n- Client sends exactly one line (`message + \"\\n\"`)\n- Client reads one response line (up to internal limit)\n\n## Examples\n\n```sh\n# Send a line to localhost:3000\nrpc \"ping\"\n\n# Custom endpoint with retries\nrpc -H 127.0.0.1 -p 5555 -t 2 -r 2 \"health\"\n\n# Stable JSON output\nrpc --json -H 127.0.0.1 -p 3000 \"status\"\n```\n"},"type":"tools/call","protocol":"mcp-line-json"}

## Query 3
Prompt: Delete files older than 30 days in artifacts, but do a dry run first.

Request:
{"type":"tools/call","tool":"filesystem.delete_older_than_days","arguments":{"path":"artifacts","days":30,"dryRun":true}}

Response:
{"ok":true,"tool":"filesystem.delete_older_than_days","result":{"path":"artifacts","days":30,"dryRun":true,"matchedCount":0,"deletedCount":0,"files":[]},"type":"tools/call","protocol":"mcp-line-json"}
