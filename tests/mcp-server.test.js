const test = require("node:test");
const assert = require("node:assert/strict");
const http = require("node:http");
const net = require("node:net");
const fs = require("node:fs");
const path = require("node:path");

const { app, createMcpServer, MCP_PORT } = require("../server");

const REPO_ROOT = path.join(__dirname, "..");
const MCP_LOG_PATH = path.join(REPO_ROOT, "artifacts", "mcp_calls.log");

let httpServer;
let httpBaseUrl;
let mcpServer;
let mcpPort;

function requestJson(pathname) {
  return new Promise((resolve, reject) => {
    http
      .get(`${httpBaseUrl}${pathname}`, (res) => {
        let raw = "";
        res.setEncoding("utf8");
        res.on("data", (chunk) => {
          raw += chunk;
        });
        res.on("end", () => {
          try {
            const body = raw.length ? JSON.parse(raw) : null;
            resolve({ statusCode: res.statusCode, body });
          } catch (err) {
            reject(err);
          }
        });
      })
      .on("error", reject);
  });
}

function mcpRequest(payload) {
  return new Promise((resolve, reject) => {
    const client = net.createConnection({ host: "127.0.0.1", port: mcpPort }, () => {
      client.write(`${JSON.stringify(payload)}\n`);
    });

    let raw = "";
    client.setEncoding("utf8");
    client.on("data", (chunk) => {
      raw += chunk;
    });
    client.on("end", () => {
      try {
        resolve(JSON.parse(raw.trim()));
      } catch (err) {
        reject(err);
      }
    });
    client.on("error", reject);
  });
}

test.before(async () => {
  try {
    fs.unlinkSync(MCP_LOG_PATH);
  } catch (_error) {
    // File may not exist yet.
  }

  await new Promise((resolve) => {
    httpServer = app.listen(0, "127.0.0.1", resolve);
  });
  const address = httpServer.address();
  httpBaseUrl = `http://127.0.0.1:${address.port}`;

  mcpServer = createMcpServer();
  await new Promise((resolve) => {
    mcpServer.listen(0, "127.0.0.1", resolve);
  });
  mcpPort = mcpServer.address().port;
});

test("MCP default server port matches course slide example", () => {
  assert.equal(MCP_PORT, 9000);
});

test.after(async () => {
  if (mcpServer) {
    await new Promise((resolve, reject) => {
      mcpServer.close((err) => {
        if (err) reject(err);
        else resolve();
      });
    });
  }
  if (httpServer) {
    await new Promise((resolve, reject) => {
      httpServer.close((err) => {
        if (err) reject(err);
        else resolve();
      });
    });
  }
});

test("MCP tools/list returns the registry tools", async () => {
  const response = await mcpRequest({ type: "tools/list" });
  assert.equal(response.ok, true);
  assert.equal(response.protocol, "mcp-line-json");
  assert.ok(Array.isArray(response.tools));
  assert.ok(response.tools.some((tool) => tool.name === "registry.packages.list"));
  assert.ok(response.tools.some((tool) => tool.name === "registry.package.lookup"));
  assert.ok(response.tools.some((tool) => tool.name === "shell.commands.list"));
});

test("MCP tools/call returns a package lookup result", async () => {
  const response = await mcpRequest({
    type: "tools/call",
    tool: "registry.package.lookup",
    arguments: { name: "echo" },
  });
  assert.equal(response.ok, true);
  assert.equal(response.protocol, "mcp-line-json");
  assert.equal(response.tool, "registry.package.lookup");
  assert.equal(response.result.name, "echo");
});

test("MCP tools/call returns the shell command catalog", async () => {
  const response = await mcpRequest({
    type: "tools/call",
    tool: "shell.commands.list",
  });
  assert.equal(response.ok, true);
  assert.equal(response.tool, "shell.commands.list");
  assert.ok(Array.isArray(response.result));
  assert.ok(response.result.some((command) => command.name === "rpc"));
  assert.ok(response.result.some((command) => command.name === "echo"));
  const rpcCommand = response.result.find((command) => command.name === "rpc");
  assert.match(rpcCommand.docsPath, /cmd_rpc\/docs\/rpc\.md$/);
});

test("MCP tools/call runs allowlisted read-only shell commands", async () => {
  const echoResponse = await mcpRequest({
    type: "tools/call",
    tool: "shell.command.run",
    arguments: { name: "echo", args: ["hello", "world"] },
  });
  assert.equal(echoResponse.ok, true);
  assert.equal(echoResponse.tool, "shell.command.run");
  assert.equal(echoResponse.result.name, "echo");
  assert.equal(echoResponse.result.stdout, "hello world");

  const pwdResponse = await mcpRequest({
    type: "tools/call",
    tool: "shell.command.run",
    arguments: { name: "pwd" },
  });
  assert.equal(pwdResponse.ok, true);
  assert.equal(pwdResponse.result.name, "pwd");
  assert.equal(pwdResponse.result.stdout, process.cwd());

  const helpResponse = await mcpRequest({
    type: "tools/call",
    tool: "shell.command.run",
    arguments: { name: "help" },
  });
  assert.equal(helpResponse.ok, true);
  assert.equal(helpResponse.result.name, "help");
  assert.match(helpResponse.result.stdout, /echo - print arguments to standard output/);
  assert.match(helpResponse.result.stdout, /pwd - print working directory/);

  const lsResponse = await mcpRequest({
    type: "tools/call",
    tool: "shell.command.run",
    arguments: { name: "ls" },
  });
  assert.equal(lsResponse.ok, true);
  assert.equal(lsResponse.result.name, "ls");
  assert.match(lsResponse.result.stdout, /server\.js/);

  const catResponse = await mcpRequest({
    type: "tools/call",
    tool: "shell.command.run",
    arguments: { name: "cat", args: ["README.md"] },
  });
  assert.equal(catResponse.ok, true);
  assert.equal(catResponse.result.name, "cat");
  assert.match(catResponse.result.stdout, /CoreShell/i);

  const statResponse = await mcpRequest({
    type: "tools/call",
    tool: "shell.command.run",
    arguments: { name: "stat", args: ["README.md"] },
  });
  assert.equal(statResponse.ok, true);
  assert.equal(statResponse.result.name, "stat");
  assert.match(statResponse.result.stdout, /path: README\.md/);
  assert.match(statResponse.result.stdout, /type: file/);

  const headResponse = await mcpRequest({
    type: "tools/call",
    tool: "shell.command.run",
    arguments: { name: "head", args: ["README.md"] },
  });
  assert.equal(headResponse.ok, true);
  assert.equal(headResponse.result.name, "head");
  assert.match(headResponse.result.stdout, /CoreShell/i);
});

test("MCP tools/call rejects non-allowlisted shell commands", async () => {
  const response = await mcpRequest({
    type: "tools/call",
    tool: "shell.command.run",
    arguments: { name: "cd", args: ["/tmp"] },
  });
  assert.equal(response.ok, false);
  assert.equal(response.error.code, "COMMAND_NOT_ALLOWED");
  assert.equal(response.error.command, "cd");

  const restrictedCatResponse = await mcpRequest({
    type: "tools/call",
    tool: "shell.command.run",
    arguments: { name: "cat", args: ["/etc/passwd"] },
  });
  assert.equal(restrictedCatResponse.ok, false);
  assert.equal(restrictedCatResponse.error.code, "COMMAND_NOT_ALLOWED");
  assert.equal(restrictedCatResponse.error.command, "cat");

  const restrictedStatResponse = await mcpRequest({
    type: "tools/call",
    tool: "shell.command.run",
    arguments: { name: "stat", args: ["/etc/passwd"] },
  });
  assert.equal(restrictedStatResponse.ok, false);
  assert.equal(restrictedStatResponse.error.code, "COMMAND_NOT_ALLOWED");
  assert.equal(restrictedStatResponse.error.command, "stat");

  const restrictedHeadResponse = await mcpRequest({
    type: "tools/call",
    tool: "shell.command.run",
    arguments: { name: "head", args: ["/etc/passwd"] },
  });
  assert.equal(restrictedHeadResponse.ok, false);
  assert.equal(restrictedHeadResponse.error.code, "COMMAND_NOT_ALLOWED");
  assert.equal(restrictedHeadResponse.error.command, "head");
});

test("MCP tools/call returns shell command help metadata", async () => {
  const response = await mcpRequest({
    type: "tools/call",
    tool: "shell.command.help",
    arguments: { name: "rpc" },
  });
  assert.equal(response.ok, true);
  assert.equal(response.tool, "shell.command.help");
  assert.equal(response.result.name, "rpc");
  assert.match(response.result.summary, /TCP service/i);
  assert.match(response.result.longDescription, /TCP host\/port/);
  assert.match(response.result.docs, /# rpc/);
});

test("MCP tools/call returns stable not-found error payload", async () => {
  const response = await mcpRequest({
    type: "tools/call",
    tool: "registry.package.lookup",
    arguments: { name: "unknown-package" },
  });
  assert.equal(response.ok, false);
  assert.equal(response.error.code, "PACKAGE_NOT_FOUND");
  assert.equal(response.error.packageName, "unknown-package");
});

test("MCP tools/call supports delete_older_than_days with dryRun and execute", async () => {
  const baseDir = path.join(REPO_ROOT, "artifacts", "mcp-delete-test");
  fs.mkdirSync(baseDir, { recursive: true });

  const oldFile = path.join(baseDir, "old.txt");
  fs.writeFileSync(oldFile, "old-data\n", "utf8");
  const oldDate = new Date(Date.now() - 2 * 24 * 60 * 60 * 1000);
  fs.utimesSync(oldFile, oldDate, oldDate);

  const dryRunResponse = await mcpRequest({
    type: "tools/call",
    tool: "filesystem.delete_older_than_days",
    arguments: { path: "artifacts/mcp-delete-test", days: 1, dryRun: true },
  });
  assert.equal(dryRunResponse.ok, true);
  assert.equal(dryRunResponse.tool, "filesystem.delete_older_than_days");
  assert.equal(dryRunResponse.result.dryRun, true);
  assert.equal(dryRunResponse.result.matchedCount, 1);
  assert.equal(fs.existsSync(oldFile), true);

  const executeResponse = await mcpRequest({
    type: "tools/call",
    tool: "filesystem.delete_older_than_days",
    arguments: { path: "artifacts/mcp-delete-test", days: 1, dryRun: false },
  });
  assert.equal(executeResponse.ok, true);
  assert.equal(executeResponse.result.dryRun, false);
  assert.equal(executeResponse.result.deletedCount, 1);
  assert.equal(fs.existsSync(oldFile), false);
});

test("MCP logs every request/response call", async () => {
  await mcpRequest({
    type: "tools/call",
    tool: "registry.package.lookup",
    arguments: { name: "echo" },
  });

  const logRaw = fs.readFileSync(MCP_LOG_PATH, "utf8").trim();
  assert.notEqual(logRaw.length, 0);
  const entries = logRaw
    .split("\n")
    .filter(Boolean)
    .map((line) => JSON.parse(line));
  const lastEntry = entries[entries.length - 1];
  assert.equal(typeof lastEntry.ts, "string");
  assert.equal(typeof lastEntry.remote, "string");
  assert.equal(lastEntry.request.type, "tools/call");
  assert.equal(lastEntry.response.ok, true);
});

test("MCP rejects unknown methods", async () => {
  const response = await mcpRequest({ type: "ping" });
  assert.equal(response.ok, false);
  assert.equal(response.error.code, "UNKNOWN_METHOD");
});
