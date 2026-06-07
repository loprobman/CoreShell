## Socket Server Status: CoreShell MCP Compatibility

This note summarizes what has already been implemented for the MCP socket server, what is still missing for full parity with the LSP8_ShellMCP teaching narrative, and how to test the current implementation step by step.

## 1) Summary of Implemented Changes

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

### C. Existing CoreShell MCP protocol remains available

Still supported:
- type=tools/list
- type=tools/call

Documented in:
- [README.md](README.md)

This preserves compatibility with current CoreShell tests and tooling.

### D. Regression tests were added for legacy methods

Added tests:
- legacy initialize
- legacy list_tools
- legacy call_tool get_time
- legacy call_tool list_files

Implemented in:
- [tests/mcp_server_c_test.c](tests/mcp_server_c_test.c)

### E. A classroom-friendly legacy client was added

Added script:
- [legacy_mcp_client.py](legacy_mcp_client.py)

Purpose:
- Demonstrates legacy initialize/list_tools/call_tool sequence in notebook style.

## 2) What Is Still Missing for Full LSP8 Narrative Parity

These are the remaining gaps if strict parity with LSP8_ShellMCP behavior is required.

1. Persistent multi-message connection flow
- Current native server handles one request line and then closes the socket.
- LSP8 narrative demonstrates continuous multi-message interaction over one connection.

2. Progress notifications during long operations
- LSP8 examples include notification/progress style messages.
- Current native legacy path returns a single final response only.

3. Robust JSON parsing
- Current parser is lightweight string extraction.
- Full parity and hardening would require structured JSON parsing with strict field validation.

4. Exact wire-level semantics across all edge cases
- Core behavior is functionally compatible for core lab tasks.
- Strict message schema and sequencing parity still needs additional refinements.

## 3) Step-by-Step Test Cases to Exercise Implemented Features

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

	printf '{"id":1,"method":"initialize","params":{}}\n' | nc 127.0.0.1 9000

Expected:
- Response includes id=1, type=response, result.server.

Step 5: Legacy list_tools

	printf '{"id":2,"method":"list_tools","params":{}}\n' | nc 127.0.0.1 9000

Expected:
- Response includes tool names list_files/get_time/delete_older_than_days.

Step 6: Legacy call_tool get_time

	printf '{"id":3,"method":"call_tool","params":{"tool":"get_time","args":{}}}\n' | nc 127.0.0.1 9000

Expected:
- Response result.tool is get_time and includes time.

Step 7: Legacy call_tool list_files

	printf '{"id":4,"method":"call_tool","params":{"tool":"list_files","args":{"path":"."}}}\n' | nc 127.0.0.1 9000

Expected:
- Response result.tool is list_files and includes output.

Step 8: Legacy call_tool delete_older_than_days dry run

	printf '{"id":5,"method":"call_tool","params":{"tool":"delete_older_than_days","args":{"path":"artifacts","days":30,"dryRun":true}}}\n' | nc 127.0.0.1 9000

Expected:
- Response result includes matchedCount/deletedCount/output.
- dryRun is true.

### Test Set D: CoreShell Native MCP Protocol (unchanged behavior)

Step 9: Native tools/list

	printf '{"type":"tools/list"}\n' | nc 127.0.0.1 9000

Expected:
- Response ok=true and tools array includes registry, shell, filesystem, rag, and agent tools.

Step 10: Native tools/call lookup example

	printf '{"type":"tools/call","tool":"registry.package.lookup","arguments":{"name":"echo"}}\n' | nc 127.0.0.1 9000

Expected:
- Response ok=true with package metadata for echo.

### Test Set E: Automated C Test Suite

Step 11: Run MCP C tests

	make test-mcp-c

Expected:
- Pass lines for legacy and native tests.
- Final line indicates all native C MCP server tests passed.

### Test Set F: Negative and Safety Cases

Step 12: Unknown method

	printf '{"type":"ping"}\n' | nc 127.0.0.1 9000

Expected:
- Error response with UNKNOWN_METHOD.

Step 13: Legacy unknown tool

	printf '{"id":9,"method":"call_tool","params":{"tool":"no_such_tool","args":{}}}\n' | nc 127.0.0.1 9000

Expected:
- Legacy response with result.error = unknown tool.

Step 14: Path safety check

	printf '{"id":10,"method":"call_tool","params":{"tool":"delete_older_than_days","args":{"path":"/etc","days":1,"dryRun":true}}}\n' | nc 127.0.0.1 9000

Expected:
- Error that path must be inside workspace.

## 4) Recommended Next Implementation for Full LSP8 Parity

1. Convert connection handling to persistent read loop per client.
2. Add optional progress notification messages before final responses.
3. Replace naive JSON extraction with strict parser-backed request decoding.
4. Extend tests to verify multiple requests on one TCP session.

