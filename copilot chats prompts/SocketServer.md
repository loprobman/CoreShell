## Socket Server Status: CoreShell MCP Compatibility

This note summarizes what is implemented now in the native C MCP socket server, what still remains for strict full parity with the LSP8_ShellMCP teaching narrative, and how to test the current implementation step by step.

## Architecture Graphic (Updated)

The corrected diagram below reflects the actual runtime shape in code.

```
┌───────────────────────────────────────────────────────────────┐
│ CoreShell REPL (built-ins dispatched via pthread)            │
│                                                               │
│ rpc command (TCP line client):                               │
│ - default target 127.0.0.1:3000                              │
│ - custom host/port via -H/-p (for example port 9000)         │
└───────────────┬───────────────────────────────────────────────┘
		    │
		    │ line-based JSON over TCP
		    ▼
┌───────────────────────────────────────────────────────────────┐
│ MCP service deployment (one server process at a time)        │
│                                                               │
│ Option A: Node bridge server.js                              │
│ - package registry HTTP on :3000                             │
│ - MCP line server on :9000                                   │
│ - single-request-per-connection behavior                     │
│                                                               │
│ Option B: Native C server ./mcp_server                       │
│ - MCP line server on :9000                                   │
│ - persistent multi-message socket session                    │
│ - legacy methods: initialize/list_tools/call_tool            │
│ - native methods: tools/list, tools/call                     │
│ - jsmn-based structured JSON parsing                          │
│ - service config: socket_server.conf + CORESH_* env toggles  │
│ - FTP-style text commands: USER/MKD/STOR/RETR/LIST/PWD/QUIT │
└───────────────┬───────────────────────────────────────────────┘
		    │
		    ▼
	JSONL logs appended to artifacts/mcp_calls.log
```

## 2) Summary of Implemented Changes

### A. Legacy notebook method compatibility was added to the native C server

Implemented in:
- [mcp_server.c](mcp_server.c)

Supported legacy methods:
- initialize
- list_tools
- call_tool

Behavior added:
- Legacy request id is echoed in responses.
- Legacy envelope is returned with type=response.

### B. Legacy tool names were added and mapped in native C mode

Implemented tools:
- list_files
- get_time
- delete_older_than_days

Implemented in:
- [mcp_server.c](mcp_server.c)

Notes:
- delete_older_than_days supports dryRun and workspace safety checks.
- list_files is workspace-scoped through path resolution safeguards.

### C. Existing CoreShell MCP protocol remains available in native C

Still supported:
- type=tools/list
- type=tools/call

Documented in:
- [README.md](README.md)

This preserves compatibility with current CoreShell tests and tooling.

### D. Persistent multi-message socket sessions are now implemented

Implemented in:
- [mcp_server.c](mcp_server.c)

Behavior:
- One client TCP connection can send multiple newline-delimited JSON requests.
- Server responds line-by-line and keeps the connection open until client disconnect.

### E. Structured request parsing (jsmn) is now implemented

Implemented in:
- [jsmn.h](jsmn.h)
- [mcp_server.c](mcp_server.c)

Behavior:
- Request method/type/tool/id/arguments are decoded from parsed JSON tokens.
- This replaced fragile method detection by raw string matching for request routing.

### F. Legacy progress notifications are implemented for selected tools

Implemented in:
- [mcp_server.c](mcp_server.c)

Behavior:
- In legacy `call_tool`, server can emit notification lines (`type=notification`) before the final response.
- Current notification coverage is tool-specific and emitted for `list_files` and `delete_older_than_days`.

### G. Regression tests were added for legacy methods

Added tests:
- legacy initialize
- legacy list_tools
- legacy call_tool get_time
- legacy call_tool list_files
- strict legacy persistent session + notifications

Implemented in:
- [tests/mcp_server_c_test.c](tests/mcp_server_c_test.c)

### H. A classroom-friendly legacy client was added

Added script:
- [legacy_mcp_client.py](legacy_mcp_client.py)

Purpose:
- Demonstrates legacy initialize/list_tools/call_tool sequence in notebook style.

### I. Service configuration mechanism is implemented

Implemented in:
- [mcp_server.c](mcp_server.c)

Supported controls:
- Config file path supported by implementation: `socket_server.conf` in workspace root
- Environment overrides:
	- `CORESH_ENABLE_NATIVE`
	- `CORESH_ENABLE_LEGACY`
	- `CORESH_ENABLE_FTP`
	- `CORESH_DISABLED_TOOLS` (comma-separated tool names)

Examples:
- `disable_tool=rag.docs.search`
- `enable_legacy=false`

### J. FTP-style protocol surface is implemented

Implemented in:
- [mcp_server.c](mcp_server.c)
- [tests/mcp_server_c_test.c](tests/mcp_server_c_test.c)

Supported commands:
- `USER <name>`
- `MKD <path>`
- `STOR <path> <base64-data>`
- `RETR <path>` (returns base64)
- `LIST [path]`
- `PWD`
- `QUIT`

Notes:
- This is a protocol surface, not a full RFC FTP server.
- `STOR`/`RETR` use base64 payloads so NUL-containing data can be tested safely.

## 3) What Is Still Missing for Full LSP8 Narrative Parity

These are the remaining gaps if strict parity with LSP8_ShellMCP behavior is required.

1. Exact wire-level semantics across all edge cases
- Core behavior is functionally compatible for core lab tasks.
- Strict message schema and sequencing parity still needs additional refinements.

2. Notification coverage breadth
- Notifications are currently implemented for selected legacy tools.
- Full parity may require notifications for all long-running operations and richer event taxonomy.

3. Advanced schema validation depth
- Structured parsing is now implemented.
- Additional strict schema validation rules can be added for stronger rejection behavior on malformed payloads.

4. Full FTP RFC interoperability
- The protocol uses FTP-style commands but is not wired to the standard FTP control/data channel protocol.
- Standard `ftp` client interoperability would require a dedicated RFC-compliant server implementation.

## 4) Step-by-Step Test Cases to Exercise Implemented Features

The steps below validate both legacy compatibility and native CoreShell protocol.

### Test Set A: Build and Start

Step 1: Build server and tests

	make mcp_server
	make test-mcp-c

Expected:
- Build succeeds.

Step 2: Start native server in Terminal A

	./mcp_server

Expected:
- Server prints listening message on 127.0.0.1:9000.

### Test Set B: Legacy Compatibility Client

Step 3: Run legacy client in Terminal B

	python3 legacy_mcp_client.py --path artifacts --days 30

Expected:
- initialize returns id and server/version.
- list_tools returns list_files, get_time, delete_older_than_days.
- call_tool list_files returns output string.
- call_tool get_time returns time field.
- call_tool delete_older_than_days returns dryRun result payload.

### Test Set C: Manual Legacy Requests (one request per connection)

Step 4: Legacy initialize

	printf '{"id":1,"method":"initialize","params":{}}\n' | nc -N 127.0.0.1 9000

Expected:
- Response includes id=1, type=response, result.server.

Step 5: Legacy list_tools

    printf '{"id":2,"method":"list_tools","params":{}}\n' | nc -N 127.0.0.1 9000

Expected:
- Response includes tool names list_files/get_time/delete_older_than_days.

Step 6: Legacy call_tool get_time

    printf '{"id":3,"method":"call_tool","params":{"tool":"get_time","args":{}}}\n' | nc -N 127.0.0.1 9000

Expected:
- Response result.tool is get_time and includes time.

Step 7: Legacy call_tool list_files

	printf '{"id":4,"method":"call_tool","params":{"tool":"list_files","args":{"path":"."}}}\n' | nc -N 127.0.0.1 9000

Expected:
- Response result.tool is list_files and includes output.

Step 8: Legacy call_tool delete_older_than_days dry run

	printf '{"id":5,"method":"call_tool","params":{"tool":"delete_older_than_days","args":{"path":"artifacts","days":30,"dryRun":true}}}\n' | nc 127.0.0.1 9000

Expected:
- Response result includes matchedCount/deletedCount/output.
- dryRun is true.

### Test Set D: Strict Legacy Persistent Session + Notifications (single socket)

Step 9: Open a persistent netcat session (terminal B run ./mcp_server in Termnal A before)

	nc 127.0.0.1 9000

Step 10: Send these lines in the same open session (terminal B)

	{"id":11,"method":"initialize","params":{}}
	{"id":12,"method":"list_tools","params":{}}
	{"id":13,"method":"call_tool","params":{"tool":"list_files","args":{"path":"."}}}

Expected:
- First response is initialize result for id 11.
- Second response is list_tools result for id 12.
- Third line includes `type":"notification"` for id 13.
- Next line includes final `type":"response"` with `tool":"list_files"` for id 13.

### Test Set E: CoreShell Native MCP Protocol (unchanged behavior)

Step 11: Native tools/list

	printf '{"type":"tools/list"}\n' | nc 127.0.0.1 9000

Expected:
- Response ok=true and tools array includes registry, shell, filesystem, rag, and agent tools.

Step 12: Native tools/call lookup example

	printf '{"type":"tools/call","tool":"registry.package.lookup","arguments":{"name":"echo"}}\n' | nc 127.0.0.1 9000

Expected:
- Response ok=true with package metadata for echo.

### Test Set F: Automated C Test Suite

Step 13: Run MCP C tests

	make test-mcp-c

Expected:
- Pass lines for legacy, strict persistent-session notification test, and native tests.
- Final line indicates all native C MCP server tests passed.

### Test Set G: Negative and Safety Cases

Step 14: Unknown method

	printf '{"type":"ping"}\n' | nc 127.0.0.1 9000

Expected:
- Error response with UNKNOWN_METHOD.

Step 15: Legacy unknown tool

	printf '{"id":9,"method":"call_tool","params":{"tool":"no_such_tool","args":{}}}\n' | nc 127.0.0.1 9000

Expected:
- Legacy response with result.error = unknown tool.

Step 16: Path safety check

	printf '{"id":10,"method":"call_tool","params":{"tool":"delete_older_than_days","args":{"path":"/etc","days":1,"dryRun":true}}}\n' | nc 127.0.0.1 9000

Expected:
- Error that path must be inside workspace.

### Test Set H: OpenAI Provider Verification

These steps verify whether agent.command.plan is using local fallback, mocked OpenAI, or real OpenAI.

Step 17: Start Node server for planner endpoint and MCP bridge

	# Terminal A
	pkill -f "node server.js" || true
	pkill -f "./mcp_server" || true
	export OPENAI_API_KEY=test-key
	export CORESH_OPENAI_MOCK_RESPONSE='{"command":"pwd","rationale":"mock","trace":["mock path"]}'
	npm start

Expected:
- Node server is running with MCP on 9000 and HTTP endpoint on 3000.
- If `./mcp_server` is running, stop it first so Node owns port 9000.
- Keep this terminal open; run the next commands from a different terminal.
- Important: these exports must be in the same terminal that runs `npm start`.

Step 18: Verify fallback mode (no OpenAI key)

	# Terminal B
	# Back in Terminal A (Node terminal), remove vars and restart:
	# unset OPENAI_API_KEY CORESH_OPENAI_MOCK_RESPONSE
	# npm start
	# Then run from Terminal B:
	printf '{"type":"tools/call","tool":"agent.command.plan","arguments":{"query":"How do I print my working directory?"}}\n' | nc -N 127.0.0.1 9000

Expected:
- Response includes `"provider":"local-rag-fallback"`.
- If this hangs, your `nc` may not support `-N`; use `nc -q 1` instead.

Step 19: Verify mocked OpenAI mode

	# In Terminal A (Node terminal), set vars and restart if needed:
	# export OPENAI_API_KEY=test-key
	# export CORESH_OPENAI_MOCK_RESPONSE='{"command":"pwd","rationale":"mock","trace":["mock path"]}'
	# npm start
	# Then run from Terminal B:
	printf '{"type":"tools/call","tool":"agent.command.plan","arguments":{"query":"How do I print my working directory?"}}\n' | nc -N 127.0.0.1 9000

Expected:
- Response includes `"provider":"openai-mock"`.

Step 20: Verify real OpenAI mode

	# In Terminal A (Node terminal), set real key and restart:
	# unset CORESH_OPENAI_MOCK_RESPONSE
	# export OPENAI_API_KEY='<your real key>'
	# npm start
	# Then run from Terminal B:
	printf '{"type":"tools/call","tool":"agent.command.plan","arguments":{"query":"How do I print my working directory?"}}\n' | nc -N 127.0.0.1 9000

Expected:
- Response includes `"provider":"openai"`.
- Response contains a non-empty `model` field.

Step 21: Cleanup environment variables

	# Terminal A (Node terminal)
	unset OPENAI_API_KEY CORESH_OPENAI_MOCK_RESPONSE

Expected:
- Subsequent runs return to fallback unless key is set again.

Automated tests already covering fallback and mock modes:
- [tests/mcp-server.test.js](tests/mcp-server.test.js)
- test: fallback when OpenAI is not configured
- test: mocked OpenAI response path

### Test Set I: Service Configuration Enable/Disable

Step 22: Disable one tool using environment variable and start server

	# Terminal A
	pkill -f "./mcp_server" || true
	export CORESH_DISABLED_TOOLS='rag.docs.search'
	./mcp_server

Step 23: Call disabled tool from another terminal

	# Terminal B
	printf '{"type":"tools/call","tool":"rag.docs.search","arguments":{"query":"print working directory"}}\n' | nc -N 127.0.0.1 9000

Expected:
- Error response includes `"SERVICE_DISABLED"`.

Step 24: Re-enable default behavior

	# Terminal A
	unset CORESH_DISABLED_TOOLS
	# Restart server

Expected:
- `rag.docs.search` works again.

### Test Set J: FTP-Style Protocol

Step 25: Start native server

	# Terminal A
	pkill -f "./mcp_server" || true
	./mcp_server

Step 26: Open persistent session

	# Terminal B
	nc 127.0.0.1 9000

Step 27: Login and create directory

	USER foo
	MKD artifacts/ftp_demo_dir
	MKD artifacts/ftp_demo_dir

Expected:
- `USER` returns success code (`230`).
- First `MKD` succeeds (`257`).
- Second `MKD` reports already exists (`550`).

Step 28: Store and retrieve binary-safe payload

	STOR artifacts/ftp_demo_dir/nul.bin AAECAwQF
	RETR artifacts/ftp_demo_dir/nul.bin

Expected:
- `STOR` returns success (`226`).
- `RETR` returns success (`150`) and includes payload `AAECAwQF`.

Step 29: List, missing retrieval, and quit

	LIST artifacts/ftp_demo_dir
	RETR artifacts/ftp_demo_dir/no_such_file.bin
	QUIT

Expected:
- `LIST` returns success (`150`) and includes `nul.bin`.
- Missing file returns `550`.
- `QUIT` returns `221` and closes only the current connection.

### Test Set K: Quick Validation For Week 9 Features

Use this short checklist when you only want to verify the two newly added features.

Step 30: Feature 1 quick check (service disable)

	# Terminal A
	pkill -f "./mcp_server" || true
	export CORESH_DISABLED_TOOLS='rag.docs.search'
	./mcp_server

	# Terminal B
	printf '{"type":"tools/call","tool":"rag.docs.search","arguments":{"query":"print working directory"}}\n' | nc -N 127.0.0.1 9000

Expected:
- Response contains `"SERVICE_DISABLED"`.

Step 31: Feature 1 reset

	# Terminal A
	unset CORESH_DISABLED_TOOLS
	pkill -f "./mcp_server" || true
	./mcp_server

Step 32: Feature 2 quick check (FTP-style protocol)

	# Terminal B
	nc 127.0.0.1 9000
	USER foo
	MKD artifacts/ftp_quick
	STOR artifacts/ftp_quick/file.bin AAECAwQF
	RETR artifacts/ftp_quick/file.bin
	QUIT

Expected:
- `USER` -> `230`
- `MKD` -> `257` (or `550` if already exists)
- `STOR` -> `226`
- `RETR` -> `150 ... AAECAwQF`
- `QUIT` -> `221`

## 4) Recommended Next Implementation for Full LSP8 Parity

1. Expand notification semantics and coverage across additional long-running tools.
2. Add deeper schema-level validation and explicit typed error variants.
3. Add high-volume/session longevity tests and malformed JSON fuzz-style tests.
4. Add docs/examples for mixed legacy + native requests in one persistent session.

