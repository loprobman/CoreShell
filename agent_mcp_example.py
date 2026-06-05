from openai import OpenAI

client = OpenAI()

TOOLS = [
    {
        "type": "mcp",
        "mcp_server": {
            "transport": "tcp",
            "host": "127.0.0.1",
            "port": 9000,
        },
    }
]


def _print_tool_results(resp):
    output = getattr(resp, "output", None)
    if not output:
        print(resp)
        return

    printed = False
    for item in output:
        item_type = getattr(item, "type", "")
        if item_type == "tool_call_output":
            tool_name = getattr(item, "name", "unknown_tool")
            content = getattr(item, "content", "")
            print(f"\n[TOOL] {tool_name}")
            if isinstance(content, list):
                for chunk in content:
                    text = getattr(chunk, "text", str(chunk))
                    print(text)
            else:
                print(content)
            printed = True

    if not printed:
        print(resp)


def query_agent(prompt: str):
    response = client.chat.completions.create(
        model="gpt-5-nano",
        messages=[{"role": "user", "content": prompt}],
        tools=TOOLS,
        tool_choice="auto",
    )
    _print_tool_results(response)


if __name__ == "__main__":
    print("Agent example ready. Ensure your MCP server is running: ./mcp_server")
    print("Try 3 NL prompts:")
    print("1) Show available tools")
    print("2) List CoreShell commands")
    print("3) Preview files older than 30 days under artifacts")
