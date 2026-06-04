# Week 8 Consistency Matrix: Sockets, MCP, and RAG

This matrix compares the Session 8 socket-client materials, MCP agent materials, and RAG/ShellAI materials to identify scope conflicts before implementation.

| Dimension | Session 8 Sockets Materials | MCP Agent Course Materials | RAG / ShellAI Materials | Consistency Status | Recommended Week 8 Decision |
|---|---|---|---|---|---|
| Primary role of CoreShell | Client command (http-get or toy rpc client) | C MCP server called by external AI agent | Shell runtime with retriever + safety + LLM bridge | Inconsistent | Treat Week 8 as client-first milestone; server role can be next phase |
| Communication direction | CoreShell calls remote service | External agent calls CoreShell tools | Bidirectional orchestration implied | Inconsistent | Define one direction for Week 8: outbound client calls only |
| Protocol complexity | Minimal format (line-based or length-prefix) | MCP-style tool contracts, schema, structured calls | Retrieval plus agent loop plus memory | Inconsistent | Use minimal protocol now; map to MCP later |
| Acceptance criteria focus | Connect, exchange data, timeout, error handling, help consistency | Agent integration, tool call routing, demo queries | Grounding, validation, retry, safety controls | Partially aligned | Implement socket reliability criteria first; keep agentic features out of core milestone |
| Timeout and retry behavior | Explicitly required | Not central in slides but needed in practice | Retry/validation loop emphasized | Mostly consistent | Add hard connect/read timeout and predictable retry policy |
| Output format expectations | Human-usable, optional json output with stable schema | Structured tool result display expected | Retrieval-grounded outputs, auditable reasoning | Mostly consistent | Provide text output plus optional json with stable keys |
| Safety model | Implied via clean error handling | Not deeply specified in assignment text | Human confirmation, allowlists, audit logging, isolation | Inconsistent depth | Add minimal confirmation gate for risky execution and leave full safety layer for later |
| Natural language @ mode | Optional stub to external service | External AI agent orchestration via MCP | LLM grounded by retrieval and safety | Partially aligned | Keep @ as optional preview only; do not make it core Week 8 dependency |
| Data grounding / retrieval | Not required for basic socket milestone | Tool discovery and invocation | Core principle (RAG, memory, reflection) | Inconsistent by scope | Defer full RAG pipeline; add simple metadata retrieval hook |
| Demo artifact requirement | Required demo + AI log + verification steps | Demonstrate with 3 NL queries | Strong emphasis on traceability | Consistent | Produce deterministic demo script with 3 queries and command logs |
| Dependency on external infra | Can be local mock service | Requires running MCP server + agent stack | Requires retriever/index/data pipeline | Inconsistent deployment burden | Prefer local deterministic test service for Week 8 grading reliability |
| Student learning objective | Socket API fundamentals in C | Agent-tool orchestration | Reliable grounded AI behavior | Complementary but staged | Sequence as: sockets first, MCP bridge second, RAG safety third |

## Practical Interpretation

1. The documents align on long-term architecture, but differ on Week 8 immediate scope.
2. The main conflict is client milestone versus server-centric MCP orchestration.
3. A staged path keeps delivery realistic:
   1. Week 8: reliable socket client command with timeout, error handling, help, optional json.
   2. Next: MCP-compatible service interface.
   3. Later: full RAG safety loop and memory.
