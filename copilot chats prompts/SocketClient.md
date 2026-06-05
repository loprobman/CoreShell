# Week 8 — Architecture & Design: Socket Client, MCP, and RAG

This document describes every component implemented during Week 8, the relationships
between them, and step-by-step exercises for testing each layer interactively.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Component Map](#2-component-map)
3. [Layer 1 — Socket Client built-in (`rpc`)](#3-layer-1--socket-client-built-in-rpc)
4. [Layer 2 — MCP-Compatible Service Layer](#4-layer-2--mcp-compatible-service-layer)
   - 4.1 [Node.js MCP Bridge (`server.js`)](#41-nodejs-mcp-bridge-serverjs)
   - 4.2 [Native C MCP Server (`mcp_server.c`)](#42-native-c-mcp-server-mcp_serverc)
5. [Layer 3 — RAG Retrieval Engine](#5-layer-3--rag-retrieval-engine)
6. [Data Flow Diagrams](#6-data-flow-diagrams)
7. [Tool Catalogue](#7-tool-catalogue)
8. [Testing — Step-by-Step](#8-testing--step-by-step)
   - 8.1 [Prerequisites](#81-prerequisites)
   - 8.2 [Test the `rpc` built-in](#82-test-the-rpc-built-in)
   - 8.3 [Test the MCP tools/list and tools/call protocol](#83-test-the-mcp-toolslist-and-toolscall-protocol)
   - 8.4 [Test the RAG tools](#84-test-the-rag-tools)
   - 8.5 [Run the automated test suites](#85-run-the-automated-test-suites)

---

## 1. Overview

Week 8 delivers three integrated layers on top of the existing CoreShell POSIX shell:

```
┌─────────────────────────────────────────────────────────┐
│  CoreShell REPL  (built-in commands via pthread dispatch) │
│  ┌────────────┐                                          │
│  │  rpc       │  Layer 1 – TCP socket client built-in   │
│  └─────┬──────┘                                          │
└────────┼────────────────────────────────────────────────┘
         │ TCP (line-based JSON)
         ▼
┌─────────────────────────────────────────────────────────┐
│  MCP-Compatible Service  port 9000                       │
│  ┌──────────────────────┐  ┌───────────────────────────┐│
│  │  Node.js MCP Bridge  │  │  Native C MCP Server      ││
│  │  (server.js)         │  │  (mcp_server)             ││
│  └──────────┬───────────┘  └────────────┬──────────────┘│
│             │   Layer 2 – tools/list,   │               │
│             │   tools/call              │               │
│             └──────────┬────────────────┘               │
│                        │                                │
│  ┌─────────────────────▼──────────────────────────────┐ │
│  │  RAG Engine   Layer 3 – rag.docs.search,           │ │
│  │               rag.command.recommend                 │ │
│  │  Corpus: cmd_*/pkg.json + cmd_*/docs/*.md           │ │
│  └────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
         │ JSON lines logged to artifacts/mcp_calls.log
```

---

## 2. Component Map

| Component | Language | File(s) | Role |
|---|---|---|---|
| `rpc` built-in | C | `cmd_rpc/cmd_rpc.c`, `cmd_rpc/cmd_rpc.h` | Shell command that sends one line to a TCP service and prints the response |
| Node.js MCP bridge | JavaScript | `server.js` | TCP line-JSON MCP-compatible server; also hosts HTTP package registry on port 3000 |
| Native C MCP server | C | `mcp_server.c` | Standalone compiled binary; same MCP protocol, no Node.js dependency |
| RAG engine (Node) | JavaScript | `server.js` (`ragSearchDocs`, `ragRecommendCommand`) | Corpus-backed retrieval built into the Node bridge |
| RAG engine (C) | C | `mcp_server.c` (`build_rag_docs_search`, `build_rag_command_recommend`) | Same logic in C for the native server |
| MCP call log | Text | `artifacts/mcp_calls.log` | JSON-lines log of every request/response pair |
| Agent example | Python | `agent_mcp_example.py` | Demo script showing how an LLM agent calls MCP tools |
| Demo queries | Markdown | `mcp_demo_queries.md` | 5 natural-language prompts used to validate the implementation |
| Demo transcript | Markdown | `mcp_demo_transcript.md` | Captured live output from the 5 queries |

---

## 3. Layer 1 — Socket Client built-in (`rpc`)

### Design

The `rpc` command is a CoreShell built-in that:

1. Opens a TCP connection to a host/port.
2. Sends a single line (the message argument) terminated by `\n`.
3. Reads one response line.
4. Prints the response to stdout (or as JSON with `--json`).

```
CoreShell REPL
      │
      │  dispatch_builtin("rpc", ...)     pthread worker
      ├─────────────────────────────────> rpc_run()
      │                                       │
      │                                       ├── connect(host, port)
      │                                       ├── send(message + "\n")
      │                                       ├── recv(response line)
      │                                       └── print / JSON output
      │
      │  ← exit status via pipe
```

### Source files

| File | Purpose |
|---|---|
| `cmd_rpc/cmd_rpc.c` | Implementation: arg parsing, socket lifecycle, timeout, retries |
| `cmd_rpc/cmd_rpc.h` | Public spec declaration |
| `cmd_rpc/pkg.json` | Package metadata for registry |
| `cmd_rpc/docs/rpc.md` | Man-style documentation |

### Options

| Flag | Default | Description |
|---|---|---|
| `-H, --host` | `127.0.0.1` | Target hostname or IP |
| `-p, --port` | `3000` | Target TCP port |
| `-t, --timeout` | `3` s | Connect/read/write timeout |
| `-r, --retries` | `0` | Retry count after first failure |
| `--json` | off | Emit result as a JSON object |
| `--help-json` | off | Print argument schema as JSON |

---

## 4. Layer 2 — MCP-Compatible Service Layer

Both servers implement the same **line-based JSON protocol**:

- One JSON object per line (newline-terminated).
- Requests carry `"type": "tools/list"` or `"type": "tools/call"`.
- Responses carry `"ok": true/false` and either `"tools"` or `"result"`.

### 4.1 Node.js MCP Bridge (`server.js`)

```
┌──────────────────────────────────────────────────────┐
│  server.js                                           │
│                                                      │
│  HTTP  port 3000          TCP  port 9000             │
│  ┌───────────────┐        ┌──────────────────────┐   │
│  │ Express       │        │ net.createServer()   │   │
│  │ GET /packages │        │ createMcpServer()    │   │
│  │ GET /packages │        │                      │   │
│  │   /:name      │        │  ┌────────────────┐  │   │
│  └───────────────┘        │  │ buildToolCatalog│  │   │
│                           │  │ buildToolResponse│ │   │
│  loadPackagesFromModules() │  │ ragSearchDocs  │  │   │
│  loadCommandCatalog()      │  │ ragRecommend.. │  │   │
│  loadCommandHelp()         │  │ deleteOlder..  │  │   │
│  runShellCommand()         │  │ logMcpCall()   │  │   │
│                           │  └────────────────┘  │   │
│                           └──────────────────────┘   │
└──────────────────────────────────────────────────────┘
```

**Start:**
```bash
npm start        # node server.js — runs both HTTP and MCP listeners
```

### 4.2 Native C MCP Server (`mcp_server.c`)

```
main()
  │
  ├── getcwd()  → g_workspace
  ├── mkdir("artifacts", 0755)
  ├── socket() / bind(127.0.0.1:9000) / listen()
  │
  └── accept() loop
        │
        ├── recv_line(req)
        ├── handle_request(req, resp)
        │     ├── tools/list  → build_tools_list_response()
        │     └── tools/call  → handle_tools_call()
        │           ├── registry.packages.list
        │           ├── registry.package.lookup
        │           ├── shell.commands.list
        │           ├── shell.command.help
        │           ├── shell.command.run
        │           ├── filesystem.delete_older_than_days
        │           ├── rag.docs.search
        │           └── rag.command.recommend
        ├── log_call(req, resp)     → artifacts/mcp_calls.log
        └── send(resp + "\n")
```

**Build and start:**
```bash
make mcp_server
./mcp_server
```

---

## 5. Layer 3 — RAG Retrieval Engine

### Design

The RAG (Retrieval-Augmented Generation) engine provides grounded command
recommendations without requiring an external LLM. It indexes the corpus of
command docs locally and scores them against a natural-language query.

```
Query: "print working directory"
         │
         ▼
  rag_tokenize() / tokenize()
  → ["print", "working", "directory"]
         │
         ▼
  For each doc in corpus:
    score += 1.0  per matching term
    score += 0.25 if match is in first 200 chars (early-match bonus)
         │
         ▼
  Sort by score, return top-K hits
  Each hit: { command, sourcePath, score, snippet }
         │
         ▼
  rag.command.recommend:
    Apply heuristics (keyword rules override retrieval for clear cases)
    Return: { command, rationale, citations[] }
```

### Corpus construction

The corpus is built lazily on first call from:

1. `cmd_*/pkg.json` — `name`, `description`, `long_description` fields.
2. `cmd_*/docs/<name>.md` — full markdown documentation file.

Both are concatenated, normalized to lowercase, and tokenized after stop-word
removal. The same corpus is used for both RAG tools.

### Implementations

| Aspect | Node.js (`server.js`) | Native C (`mcp_server.c`) |
|---|---|---|
| Corpus build | `buildRagCorpus()`, cached in `ragCorpusCache` | `rag_build_corpus()`, stored in `g_rag_corpus[RAG_MAX_DOCS]` |
| Tokenize | `tokenize()` with `STOP_WORDS` Set | `rag_tokenize()` with `g_stop_words[]` array |
| Score | `rag_score()` inline in `.map()` | `rag_score()` function |
| Top-K sort | `.sort()` + `.slice()` | `qsort()` with `rag_hit_cmp()` |
| Snippet | `buildSnippet()` — first matching line | `rag_snippet()` — first matching line |
| Recommend | `ragRecommendCommand()` | `build_rag_command_recommend()` |

---

## 6. Data Flow Diagrams

### Full request lifecycle (native C server)

```
  Client (nc / rpc / agent)
        │
        │  {"type":"tools/call","tool":"rag.docs.search",
        │   "arguments":{"query":"list files","topK":3}}
        │
        ▼  TCP  port 9000
  mcp_server  recv_line()
        │
        ├── handle_request()
        │      └── handle_tools_call()
        │             └── build_rag_docs_search()
        │                    ├── rag_tokenize("list files") → ["list","files"]
        │                    ├── rag_build_corpus()  (lazy, cached)
        │                    │     reads cmd_*/pkg.json + docs/*.md
        │                    ├── rag_top_hits()
        │                    │     scores each doc, sorts, returns top 3
        │                    └── builds JSON result array
        │
        ├── log_call(req, resp)  → artifacts/mcp_calls.log
        │
        └── send(resp + "\n")
              │
              ▼
  {"ok":true,"tool":"rag.docs.search","result":[
    {"command":"ls","sourcePath":"cmd_ls/docs/ls.md","score":2.25,"snippet":"list directory contents"},
    ...
  ]}
```

### Shell `@` query flow (coresh_llm helper)

```
  user types: @list all C files
        │
        ▼
  main.c  handle_llm_line()
        │
        ├── fork()
        │     └── execvp("coresh_llm", ["coresh_llm", "list all C files"])
        │              └── mock_llm() → "find . -type f -name '*.c'"
        │
        ├── read stdout line → suggested command
        ├── print "Suggested command: find . -type f -name '*.c'"
        ├── prompt "Run this? (y/n)"
        └── if y → dispatch_command(suggested)
```

---

## 7. Tool Catalogue

All tools are exposed on both servers unless marked.

| Tool | Arguments | Returns |
|---|---|---|
| `registry.packages.list` | — | Array of `{name, latestVersion, downloadUrl}` |
| `registry.package.lookup` | `name: string` | One package object or `PACKAGE_NOT_FOUND` |
| `shell.commands.list` | — | Array of `{name, summary, longDescription, docsPath}` |
| `shell.command.help` | `name: string` | `{name, summary, longDescription, docs}` |
| `shell.command.run` | `name: string`, `args?: string[]` | `{name, stdout}` — allowlist: `echo`, `pwd`, `help`, `ls`*, `cat`*, `stat`*, `head`* |
| `filesystem.delete_older_than_days` | `path: string`, `days: number`, `dryRun?: boolean` | `{path, days, dryRun, matchedCount, deletedCount, files[]}` |
| `rag.docs.search` | `query: string`, `topK?: number (1–8, default 3)` | Array of `{command, sourcePath, score, snippet}` |
| `rag.command.recommend` | `query: string` | `{command, rationale, citations[]}` |

> \* `ls`, `cat`, `stat`, `head` are available on the Node.js bridge only; native C mode exposes `echo`, `pwd`, `help`.

---

## 8. Testing — Step-by-Step

### 8.1 Prerequisites

```bash
# Build everything
make

# Install Node dependencies (first time only)
npm install
```

Verify binaries exist:
```bash
ls -l CoreShell mcp_server coresh_llm
```

---

### 8.2 Test the `rpc` built-in

**Step 1 — Start the Node.js registry server (provides a line-based listener on port 3000 via MCP port 9000)**

```bash
npm start &
```

**Step 2 — Test `rpc` help**

```bash
./CoreShell rpc --help
```

Expected: usage line listing `-H`, `-p`, `-t`, `-r`, `--json`, `--help-json`.

**Step 3 — Send a raw line to the MCP server**

```bash
./CoreShell rpc -H 127.0.0.1 -p 9000 '{"type":"tools/list"}'
```

Expected: JSON response beginning with `{"ok":true,"type":"tools/list",...}`.

**Step 4 — Test JSON output flag**

```bash
./CoreShell rpc --json -H 127.0.0.1 -p 9000 '{"type":"tools/list"}'
```

Expected: a JSON object containing `"stdout"` and `"ok"` keys.

**Step 5 — Test timeout path (connect to a closed port)**

```bash
./CoreShell rpc -H 127.0.0.1 -p 19999 -t 1 "ping"
echo "exit: $?"
```

Expected: error message and non-zero exit status within ~1 second.

**Step 6 — Stop the registry server**

```bash
kill %1
```

---

### 8.3 Test the MCP tools/list and tools/call protocol

Use the **native C server** for these tests (no Node dependency).

**Step 1 — Build and start the C MCP server**

```bash
make mcp_server
./mcp_server &
```

Expected output: `CoreShell native C MCP server listening on 127.0.0.1:9000`

**Step 2 — List all available tools**

```bash
printf '{"type":"tools/list"}\n' | nc 127.0.0.1 9000
```

Expected: `"ok":true` and a `"tools"` array with 8 entries including
`rag.docs.search` and `rag.command.recommend`.

**Step 3 — Look up a package**

```bash
printf '{"type":"tools/call","tool":"registry.package.lookup","arguments":{"name":"echo"}}\n' | nc 127.0.0.1 9000
```

Expected: `"ok":true`, `"name":"echo"`, a `latestVersion` string, and a `downloadUrl`.

**Step 4 — List all shell commands**

```bash
printf '{"type":"tools/call","tool":"shell.commands.list"}\n' | nc 127.0.0.1 9000
```

Expected: array of 20 commands including `rpc`, each with `docsPath`.

**Step 5 — Get help for a command**

```bash
printf '{"type":"tools/call","tool":"shell.command.help","arguments":{"name":"rpc"}}\n' | nc 127.0.0.1 9000
```

Expected: `"ok":true`, `"summary"` containing "TCP service", and `"docs"` containing
`## Usage`.

**Step 6 — Run an allowlisted command**

```bash
printf '{"type":"tools/call","tool":"shell.command.run","arguments":{"name":"echo","args":["hello","mcp"]}}\n' | nc 127.0.0.1 9000
```

Expected: `"stdout":"hello mcp"`.

**Step 7 — Dry-run delete older files**

```bash
printf '{"type":"tools/call","tool":"filesystem.delete_older_than_days","arguments":{"path":"artifacts","days":0,"dryRun":true}}\n' | nc 127.0.0.1 9000
```

Expected: `"dryRun":true`, `"matchedCount"` ≥ 0, no deletions.

**Step 8 — Reject an unknown method**

```bash
printf '{"type":"ping"}\n' | nc 127.0.0.1 9000
```

Expected: `"ok":false`, `"code":"UNKNOWN_METHOD"`.

**Step 9 — Verify call logging**

```bash
tail -1 artifacts/mcp_calls.log
```

Expected: a JSON line with `ts`, `request`, and `response` keys.

**Step 10 — Stop the server**

```bash
kill %1
```

---

### 8.4 Test the RAG tools

Start the C server again:

```bash
./mcp_server &
```

**Step 1 — Search docs by natural-language query**

```bash
printf '{"type":"tools/call","tool":"rag.docs.search","arguments":{"query":"print working directory","topK":3}}\n' | nc 127.0.0.1 9000
```

Expected:
- `"ok":true`
- `"result"` array with 3 entries
- Top hit: `"command":"pwd"`, `"sourcePath":"cmd_pwd/docs/pwd.md"`, score ≥ 3.0

**Step 2 — Vary the query to target a different command**

```bash
printf '{"type":"tools/call","tool":"rag.docs.search","arguments":{"query":"copy file recursively","topK":2}}\n' | nc 127.0.0.1 9000
```

Expected: `cp` appears as a top hit.

**Step 3 — Request a command recommendation**

```bash
printf '{"type":"tools/call","tool":"rag.command.recommend","arguments":{"query":"How do I list files in the current directory?"}}\n' | nc 127.0.0.1 9000
```

Expected:
- `"ok":true`
- `"command":"ls"`
- `"rationale"` citing `cmd_ls/docs/ls.md`
- `"citations"` non-empty array

**Step 4 — Recommendation with keyword override**

```bash
printf '{"type":"tools/call","tool":"rag.command.recommend","arguments":{"query":"where am i"}}\n' | nc 127.0.0.1 9000
```

Expected: `"command":"pwd"` (heuristic override fires on "where am i").

**Step 5 — Recommendation for an ambiguous query**

```bash
printf '{"type":"tools/call","tool":"rag.command.recommend","arguments":{"query":"show first lines of a file"}}\n' | nc 127.0.0.1 9000
```

Expected: `"command":"head"` (heuristic "show" + "first" fires) or `head` via retrieval.

**Step 6 — Reject empty query**

```bash
printf '{"type":"tools/call","tool":"rag.docs.search","arguments":{"query":""}}\n' | nc 127.0.0.1 9000
```

Expected: `"ok":false`, `"code":"BAD_ARGUMENTS"`.

**Step 7 — Stop the server**

```bash
kill %1
```

---

### 8.5 Run the automated test suites

**C shell tests (170 cases)**

```bash
make test
```

Expected: all test suites PASS, `test_report.md` generated.

**Native C MCP server protocol tests (6 cases)**

```bash
make test-mcp-c
```

Expected:
```
[PASS] tools/list
[PASS] registry.package.lookup
[PASS] filesystem.delete_older_than_days dry run
[PASS] unknown method
[PASS] rag.docs.search
[PASS] rag.command.recommend
All native C MCP server tests passed.
```

**Node.js MCP + registry tests (16 cases)**

```bash
npm test
```

Expected:
```
✔ MCP default server port matches course slide example
✔ MCP tools/list returns the registry tools
✔ MCP tools/call returns a package lookup result
✔ MCP tools/call returns the shell command catalog
✔ MCP tools/call runs allowlisted read-only shell commands
✔ MCP tools/call rejects non-allowlisted shell commands
✔ MCP tools/call returns shell command help metadata
✔ MCP tools/call returns stable not-found error payload
✔ MCP tools/call supports delete_older_than_days with dryRun and execute
✔ MCP tools/call retrieves command docs via RAG
✔ MCP tools/call recommends command grounded by retrieved docs
✔ MCP logs every request/response call
✔ MCP rejects unknown methods
✔ GET /packages returns full package list
✔ GET /packages/:name returns a matching package
✔ GET /packages/:name returns stable 404 error payload
pass 16 / fail 0
```

---

## Summary

| Layer | What was built | Files |
|---|---|---|
| **1 — rpc** | TCP socket client built-in, full arg parsing, timeout, retries, JSON output | `cmd_rpc/` |
| **2 — MCP Node bridge** | Line-JSON TCP server on port 9000, 8 tools, call logging | `server.js`, `tests/mcp-server.test.js` |
| **2 — MCP native C** | Same protocol compiled to standalone ELF binary | `mcp_server.c`, `tests/mcp_server_c_test.c` |
| **3 — RAG Node** | Local corpus retrieval over command docs, search + recommend | `server.js` |
| **3 — RAG C** | Same retrieval logic in C, full parity with Node bridge | `mcp_server.c` |
| **Artifacts** | Call log, agent demo, query set, live transcript | `artifacts/`, `agent_mcp_example.py`, `mcp_demo_*.md` |
