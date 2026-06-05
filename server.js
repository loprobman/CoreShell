const express = require("express");
const fs = require("fs");
const net = require("net");
const path = require("path");

const app = express();
const PORT = 3000;
const MCP_PORT = 9000;
const ARTIFACTS_DIR = path.join(__dirname, "artifacts");
const MCP_CALL_LOG = path.join(ARTIFACTS_DIR, "mcp_calls.log");

if (!fs.existsSync(ARTIFACTS_DIR)) {
  fs.mkdirSync(ARTIFACTS_DIR, { recursive: true });
}

// Serve package files from artifacts directory
app.use("/downloads", express.static(ARTIFACTS_DIR));

function compareVersions(a, b) {
  const pa = String(a || "").split(".").map((part) => parseInt(part, 10) || 0);
  const pb = String(b || "").split(".").map((part) => parseInt(part, 10) || 0);
  const length = Math.max(pa.length, pb.length);

  for (let i = 0; i < length; i += 1) {
    const va = pa[i] || 0;
    const vb = pb[i] || 0;
    if (va < vb) return -1;
    if (va > vb) return 1;
  }
  return 0;
}

function loadPackagesFromModules(rootDir) {
  const latestByName = new Map();
  const entries = fs.readdirSync(rootDir, { withFileTypes: true });

  for (const entry of entries) {
    if (!entry.isDirectory() || !entry.name.startsWith("cmd_")) continue;

    const pkgJsonPath = path.join(rootDir, entry.name, "pkg.json");
    if (!fs.existsSync(pkgJsonPath)) continue;

    try {
      const raw = fs.readFileSync(pkgJsonPath, "utf8");
      const parsed = JSON.parse(raw);
      const name = parsed.name;
      const version = parsed.version;

      if (!name || !version) continue;

      const current = latestByName.get(name);
      if (!current || compareVersions(version, current.latestVersion) > 0) {
        latestByName.set(name, {
          name,
          latestVersion: version,
          downloadUrl: `http://localhost:${PORT}/downloads/${name}-${version}.tar.gz`,
        });
      }
    } catch (error) {
      console.warn(`Skipping invalid pkg metadata at ${pkgJsonPath}: ${error.message}`);
    }
  }

  return Array.from(latestByName.values()).sort((a, b) => a.name.localeCompare(b.name));
}

const packages = loadPackagesFromModules(__dirname);

function buildToolCatalog() {
  return [
    {
      name: "registry.packages.list",
      description: "List the CoreShell packages known to the registry",
      inputSchema: {
        type: "object",
        properties: {},
        additionalProperties: false,
      },
      outputSchema: {
        type: "array",
        items: {
          type: "object",
          properties: {
            name: { type: "string" },
            latestVersion: { type: "string" },
            downloadUrl: { type: "string" },
          },
          required: ["name", "latestVersion", "downloadUrl"],
          additionalProperties: false,
        },
      },
    },
    {
      name: "registry.package.lookup",
      description: "Look up one package by name",
      inputSchema: {
        type: "object",
        properties: {
          name: { type: "string" },
        },
        required: ["name"],
        additionalProperties: false,
      },
      outputSchema: {
        type: "object",
        properties: {
          name: { type: "string" },
          latestVersion: { type: "string" },
          downloadUrl: { type: "string" },
        },
        required: ["name", "latestVersion", "downloadUrl"],
        additionalProperties: false,
      },
    },
    {
      name: "shell.commands.list",
      description: "List the CoreShell commands exposed by the shell",
      inputSchema: {
        type: "object",
        properties: {},
        additionalProperties: false,
      },
      outputSchema: {
        type: "array",
        items: {
          type: "object",
          properties: {
            name: { type: "string" },
            summary: { type: "string" },
            longDescription: { type: "string" },
            docsPath: { type: "string" },
          },
          required: ["name", "summary", "longDescription", "docsPath"],
          additionalProperties: false,
        },
      },
    },
    {
      name: "shell.command.help",
      description: "Return the help metadata for a CoreShell command",
      inputSchema: {
        type: "object",
        properties: {
          name: { type: "string" },
        },
        required: ["name"],
        additionalProperties: false,
      },
      outputSchema: {
        type: "object",
        properties: {
          name: { type: "string" },
          summary: { type: "string" },
          longDescription: { type: "string" },
          docs: { type: "string" },
        },
        required: ["name", "summary", "longDescription", "docs"],
        additionalProperties: false,
      },
    },
    {
      name: "shell.command.run",
      description: "Run a small allowlisted read-only shell command",
      inputSchema: {
        type: "object",
        properties: {
          name: { type: "string" },
          args: {
            type: "array",
            items: { type: "string" },
          },
        },
        required: ["name"],
        additionalProperties: false,
      },
      outputSchema: {
        type: "object",
        properties: {
          name: { type: "string" },
          stdout: { type: "string" },
        },
        required: ["name", "stdout"],
        additionalProperties: false,
      },
    },
    {
      name: "filesystem.delete_older_than_days",
      description: "Delete files older than N days under a workspace path",
      inputSchema: {
        type: "object",
        properties: {
          path: { type: "string" },
          days: { type: "number" },
          dryRun: { type: "boolean" },
        },
        required: ["path", "days"],
        additionalProperties: false,
      },
      outputSchema: {
        type: "object",
        properties: {
          path: { type: "string" },
          days: { type: "number" },
          dryRun: { type: "boolean" },
          matchedCount: { type: "number" },
          deletedCount: { type: "number" },
          files: {
            type: "array",
            items: { type: "string" },
          },
        },
        required: ["path", "days", "dryRun", "matchedCount", "deletedCount", "files"],
        additionalProperties: false,
      },
    },
  ];
}

function resolveWorkspacePath(rawPath) {
  const relativePath = String(rawPath || "").trim();
  if (!relativePath) return null;
  const resolvedPath = path.resolve(__dirname, relativePath);
  const isInWorkspace =
    resolvedPath === __dirname || resolvedPath.startsWith(`${__dirname}${path.sep}`);
  if (!isInWorkspace) return null;
  return resolvedPath;
}

function walkFiles(rootPath, sink) {
  const entries = fs.readdirSync(rootPath, { withFileTypes: true });
  for (const entry of entries) {
    const entryPath = path.join(rootPath, entry.name);
    if (entry.isDirectory()) {
      walkFiles(entryPath, sink);
      continue;
    }
    if (entry.isFile()) {
      sink.push(entryPath);
    }
  }
}

function deleteOlderThanDays(args) {
  const rawPath = args && typeof args.path === "string" ? args.path : "";
  const daysValue = args ? Number(args.days) : Number.NaN;
  const dryRun = !(args && args.dryRun === false);

  if (!rawPath.trim()) {
    return {
      ok: false,
      tool: "filesystem.delete_older_than_days",
      error: {
        code: "BAD_ARGUMENTS",
        message: "path is required",
      },
    };
  }

  if (!Number.isFinite(daysValue) || daysValue < 0) {
    return {
      ok: false,
      tool: "filesystem.delete_older_than_days",
      error: {
        code: "BAD_ARGUMENTS",
        message: "days must be a non-negative number",
      },
    };
  }

  const resolvedRoot = resolveWorkspacePath(rawPath);
  if (!resolvedRoot) {
    return {
      ok: false,
      tool: "filesystem.delete_older_than_days",
      error: {
        code: "COMMAND_NOT_ALLOWED",
        message: "path must be inside the CoreShell workspace",
      },
    };
  }

  let rootStats;
  try {
    rootStats = fs.statSync(resolvedRoot);
  } catch (error) {
    return {
      ok: false,
      tool: "filesystem.delete_older_than_days",
      error: {
        code: "COMMAND_EXECUTION_FAILED",
        message: `stat failed: ${error.message}`,
      },
    };
  }

  const files = [];
  if (rootStats.isFile()) {
    files.push(resolvedRoot);
  } else if (rootStats.isDirectory()) {
    walkFiles(resolvedRoot, files);
  } else {
    return {
      ok: false,
      tool: "filesystem.delete_older_than_days",
      error: {
        code: "BAD_ARGUMENTS",
        message: "path must be a regular file or directory",
      },
    };
  }

  const thresholdMs = Date.now() - daysValue * 24 * 60 * 60 * 1000;
  const matchedFiles = [];
  for (const filePath of files) {
    try {
      const stats = fs.statSync(filePath);
      if (stats.mtimeMs < thresholdMs) {
        matchedFiles.push(filePath);
      }
    } catch (_error) {
      // Skip files that disappear or become inaccessible while scanning.
    }
  }

  let deletedCount = 0;
  if (!dryRun) {
    for (const filePath of matchedFiles) {
      try {
        fs.unlinkSync(filePath);
        deletedCount += 1;
      } catch (_error) {
        // Keep processing; report only successful deletions.
      }
    }
  }

  return {
    ok: true,
    tool: "filesystem.delete_older_than_days",
    result: {
      path: path.relative(__dirname, resolvedRoot) || ".",
      days: daysValue,
      dryRun,
      matchedCount: matchedFiles.length,
      deletedCount,
      files: matchedFiles.map((filePath) => path.relative(__dirname, filePath) || "."),
    },
  };
}

function logMcpCall(entry) {
  try {
    fs.appendFileSync(MCP_CALL_LOG, `${JSON.stringify(entry)}\n`, "utf8");
  } catch (_error) {
    // Logging failures should not break request processing.
  }
}

function loadCommandCatalog() {
  const entries = [];
  const modules = fs.readdirSync(__dirname, { withFileTypes: true });

  for (const entry of modules) {
    if (!entry.isDirectory() || !entry.name.startsWith("cmd_")) continue;

    const pkgJsonPath = path.join(__dirname, entry.name, "pkg.json");
    if (!fs.existsSync(pkgJsonPath)) continue;

    try {
      const pkg = JSON.parse(fs.readFileSync(pkgJsonPath, "utf8"));
      const commandName = String(pkg.name || entry.name.replace(/^cmd_/, "")).trim();
      entries.push({
        name: commandName,
        summary: pkg.description || "",
        longDescription: pkg.long_description || pkg.longDescription || "",
        docsPath: `cmd_${commandName}/docs/${commandName}.md`,
      });
    } catch (error) {
      console.warn(`Skipping invalid command metadata at ${pkgJsonPath}: ${error.message}`);
    }
  }

  return entries.sort((a, b) => a.name.localeCompare(b.name));
}

function loadCommandHelp(name) {
  if (!/^[a-z][a-z0-9_-]*$/i.test(name || "")) {
    return {
      ok: false,
      tool: "shell.command.help",
      error: {
        code: "INVALID_COMMAND_NAME",
        message: "Command name must be a simple shell command identifier",
      },
    };
  }

  const pkgJsonPath = path.join(__dirname, `cmd_${name}`, "pkg.json");
  const docsPath = path.join(__dirname, `cmd_${name}`, "docs", `${name}.md`);

  if (!fs.existsSync(pkgJsonPath)) {
    return {
      ok: false,
      tool: "shell.command.help",
      error: {
        code: "COMMAND_NOT_FOUND",
        message: "Command not found",
        command: name,
      },
    };
  }

  let pkg;
  try {
    pkg = JSON.parse(fs.readFileSync(pkgJsonPath, "utf8"));
  } catch (error) {
    return {
      ok: false,
      tool: "shell.command.help",
      error: {
        code: "BAD_METADATA",
        message: `Unable to read metadata for ${name}`,
      },
    };
  }

  let docs = "";
  try {
    docs = fs.readFileSync(docsPath, "utf8");
  } catch (error) {
    docs = `Documentation file not found: ${docsPath}`;
  }

  return {
    ok: true,
    tool: "shell.command.help",
    result: {
      name: pkg.name || name,
      summary: pkg.description || "",
      longDescription: pkg.long_description || "",
      docs,
    },
  };
}

function runShellCommand(name, args) {
  const commandName = String(name || "").trim();
  const commandArgs = Array.isArray(args) ? args.map((value) => String(value)) : [];
  const maxReadBytes = 64 * 1024;

  if (commandName === "echo") {
    const stdout = commandArgs.join(" ");
    return {
      ok: true,
      tool: "shell.command.run",
      result: {
        name: "echo",
        stdout,
      },
    };
  }

  if (commandName === "pwd") {
    return {
      ok: true,
      tool: "shell.command.run",
      result: {
        name: "pwd",
        stdout: process.cwd(),
      },
    };
  }

  if (commandName === "help") {
    const summary = loadCommandCatalog()
      .map((entry) => `${entry.name} - ${entry.summary}`)
      .join("\n");
    const filter = commandArgs[0] ? commandArgs[0].trim() : "";
    const stdout = filter
      ? summary
          .split("\n")
          .filter((line) => line.startsWith(`${filter} -`) || line.startsWith(`${filter} `))
          .join("\n")
      : summary;

    return {
      ok: true,
      tool: "shell.command.run",
      result: {
        name: "help",
        stdout,
      },
    };
  }

  if (commandName === "ls") {
    const targetPath = commandArgs[0] && commandArgs[0].trim() ? commandArgs[0].trim() : ".";
    try {
      const entries = fs
        .readdirSync(targetPath, { withFileTypes: true })
        .map((entry) => (entry.isDirectory() ? `${entry.name}/` : entry.name))
        .sort((a, b) => a.localeCompare(b));

      return {
        ok: true,
        tool: "shell.command.run",
        result: {
          name: "ls",
          stdout: entries.join("\n"),
        },
      };
    } catch (error) {
      return {
        ok: false,
        tool: "shell.command.run",
        error: {
          code: "COMMAND_EXECUTION_FAILED",
          message: `ls failed: ${error.message}`,
          command: "ls",
        },
      };
    }
  }

  if (commandName === "cat") {
    const targetArg = commandArgs[0] && commandArgs[0].trim() ? commandArgs[0].trim() : "";
    if (!targetArg) {
      return {
        ok: false,
        tool: "shell.command.run",
        error: {
          code: "BAD_ARGUMENTS",
          message: "cat requires a file path argument",
          command: "cat",
        },
      };
    }

    const resolvedPath = path.resolve(__dirname, targetArg);
    const isInWorkspace =
      resolvedPath === __dirname || resolvedPath.startsWith(`${__dirname}${path.sep}`);
    if (!isInWorkspace) {
      return {
        ok: false,
        tool: "shell.command.run",
        error: {
          code: "COMMAND_NOT_ALLOWED",
          message: "cat can only read files under the CoreShell workspace",
          command: "cat",
        },
      };
    }

    try {
      const stats = fs.statSync(resolvedPath);
      if (!stats.isFile()) {
        return {
          ok: false,
          tool: "shell.command.run",
          error: {
            code: "BAD_ARGUMENTS",
            message: "cat target must be a regular file",
            command: "cat",
          },
        };
      }

      const buffer = fs.readFileSync(resolvedPath);
      const limited = buffer.subarray(0, maxReadBytes).toString("utf8");
      const stdout =
        buffer.length > maxReadBytes ? `${limited}\n[truncated: output exceeds 65536 bytes]` : limited;

      return {
        ok: true,
        tool: "shell.command.run",
        result: {
          name: "cat",
          stdout,
        },
      };
    } catch (error) {
      return {
        ok: false,
        tool: "shell.command.run",
        error: {
          code: "COMMAND_EXECUTION_FAILED",
          message: `cat failed: ${error.message}`,
          command: "cat",
        },
      };
    }
  }

  if (commandName === "stat") {
    const targetArg = commandArgs[0] && commandArgs[0].trim() ? commandArgs[0].trim() : "";
    if (!targetArg) {
      return {
        ok: false,
        tool: "shell.command.run",
        error: {
          code: "BAD_ARGUMENTS",
          message: "stat requires a file path argument",
          command: "stat",
        },
      };
    }

    const resolvedPath = path.resolve(__dirname, targetArg);
    const isInWorkspace =
      resolvedPath === __dirname || resolvedPath.startsWith(`${__dirname}${path.sep}`);
    if (!isInWorkspace) {
      return {
        ok: false,
        tool: "shell.command.run",
        error: {
          code: "COMMAND_NOT_ALLOWED",
          message: "stat can only read paths under the CoreShell workspace",
          command: "stat",
        },
      };
    }

    try {
      const stats = fs.statSync(resolvedPath);
      const kind = stats.isDirectory() ? "directory" : stats.isFile() ? "file" : "other";
      const stdout = [
        `path: ${path.relative(__dirname, resolvedPath) || "."}`,
        `type: ${kind}`,
        `size: ${stats.size}`,
        `mtimeMs: ${stats.mtimeMs}`,
      ].join("\n");

      return {
        ok: true,
        tool: "shell.command.run",
        result: {
          name: "stat",
          stdout,
        },
      };
    } catch (error) {
      return {
        ok: false,
        tool: "shell.command.run",
        error: {
          code: "COMMAND_EXECUTION_FAILED",
          message: `stat failed: ${error.message}`,
          command: "stat",
        },
      };
    }
  }

  if (commandName === "head") {
    const targetArg = commandArgs[0] && commandArgs[0].trim() ? commandArgs[0].trim() : "";
    if (!targetArg) {
      return {
        ok: false,
        tool: "shell.command.run",
        error: {
          code: "BAD_ARGUMENTS",
          message: "head requires a file path argument",
          command: "head",
        },
      };
    }

    const resolvedPath = path.resolve(__dirname, targetArg);
    const isInWorkspace =
      resolvedPath === __dirname || resolvedPath.startsWith(`${__dirname}${path.sep}`);
    if (!isInWorkspace) {
      return {
        ok: false,
        tool: "shell.command.run",
        error: {
          code: "COMMAND_NOT_ALLOWED",
          message: "head can only read files under the CoreShell workspace",
          command: "head",
        },
      };
    }

    try {
      const stats = fs.statSync(resolvedPath);
      if (!stats.isFile()) {
        return {
          ok: false,
          tool: "shell.command.run",
          error: {
            code: "BAD_ARGUMENTS",
            message: "head target must be a regular file",
            command: "head",
          },
        };
      }

      const fileData = fs.readFileSync(resolvedPath, "utf8");
      const lineLimit = 10;
      const lines = fileData.split(/\r?\n/).slice(0, lineLimit);
      return {
        ok: true,
        tool: "shell.command.run",
        result: {
          name: "head",
          stdout: lines.join("\n"),
        },
      };
    } catch (error) {
      return {
        ok: false,
        tool: "shell.command.run",
        error: {
          code: "COMMAND_EXECUTION_FAILED",
          message: `head failed: ${error.message}`,
          command: "head",
        },
      };
    }
  }

  return {
    ok: false,
    tool: "shell.command.run",
    error: {
      code: "COMMAND_NOT_ALLOWED",
      message: "Only read-only commands are exposed",
      command: commandName,
    },
  };
}

function buildToolResponse(toolName, args) {
  if (toolName === "registry.packages.list") {
    return {
      ok: true,
      tool: toolName,
      result: packages,
    };
  }

  if (toolName === "registry.package.lookup") {
    const name = args && typeof args.name === "string" ? args.name : "";
    const match = packages.find((pkg) => pkg.name === name);
    if (!match) {
      return {
        ok: false,
        tool: toolName,
        error: {
          code: "PACKAGE_NOT_FOUND",
          message: "Package not found",
          packageName: name,
        },
      };
    }

    return {
      ok: true,
      tool: toolName,
      result: match,
    };
  }

  if (toolName === "shell.commands.list") {
    return {
      ok: true,
      tool: toolName,
      result: loadCommandCatalog(),
    };
  }

  if (toolName === "shell.command.help") {
    const name = args && typeof args.name === "string" ? args.name : "";
    return loadCommandHelp(name);
  }

  if (toolName === "shell.command.run") {
    const name = args && typeof args.name === "string" ? args.name : "";
    const commandArgs = args && Array.isArray(args.args) ? args.args : [];
    return runShellCommand(name, commandArgs);
  }

  if (toolName === "filesystem.delete_older_than_days") {
    return deleteOlderThanDays(args);
  }

  return {
    ok: false,
    tool: toolName || null,
    error: {
      code: "TOOL_NOT_FOUND",
      message: "Tool not found",
      toolName,
    },
  };
}

function createMcpServer() {
  return net.createServer((socket) => {
    socket.setEncoding("utf8");
    let buffer = "";
    let replied = false;
    const remote = `${socket.remoteAddress || "unknown"}:${socket.remotePort || "unknown"}`;

    const reply = (payload, requestForLog) => {
      if (replied) return;
      replied = true;
      logMcpCall({
        ts: new Date().toISOString(),
        remote,
        request: requestForLog || null,
        response: payload,
      });
      socket.end(`${JSON.stringify(payload)}\n`);
    };

    socket.on("data", (chunk) => {
      buffer += chunk;
      let newlineIndex = buffer.indexOf("\n");
      while (newlineIndex >= 0 && !replied) {
        const line = buffer.slice(0, newlineIndex).trim();
        buffer = buffer.slice(newlineIndex + 1);
        newlineIndex = buffer.indexOf("\n");

        if (line.length === 0) continue;

        let request;
        try {
          request = JSON.parse(line);
        } catch (error) {
          reply({
            ok: false,
            error: {
              code: "BAD_REQUEST",
              message: "Request must be valid JSON",
            },
          }, { rawLine: line });
          return;
        }

        const type = String(request.type || request.method || "").trim();
        if (type === "tools/list") {
          reply({
            ok: true,
            type,
            protocol: "mcp-line-json",
            tools: buildToolCatalog(),
          }, request);
          return;
        }

        if (type === "tools/call") {
          const tool = String(request.tool || request.name || "").trim();
          const args = request.arguments || request.params || request.args || {};
          reply({
            ...buildToolResponse(tool, args),
            type,
            protocol: "mcp-line-json",
          }, request);
          return;
        }

        reply({
          ok: false,
          type,
          error: {
            code: "UNKNOWN_METHOD",
            message: 'Expected "tools/list" or "tools/call"',
          },
        }, request);
      }
    });

    socket.on("error", () => {});
  });
}

function assertUniquePackageNames(packageList) {
  const seen = new Set();
  for (const pkg of packageList) {
    if (seen.has(pkg.name)) {
      throw new Error(`Duplicate package name detected at startup: ${pkg.name}`);
    }
    seen.add(pkg.name);
  }
}

assertUniquePackageNames(packages);

app.get("/packages", (_req, res) => {
  res.status(200).json(packages);
});

app.get("/packages/:name", (req, res) => {
  const match = packages.find((pkg) => pkg.name === req.params.name);
  if (!match) {
    return res.status(404).json({
      error: {
        code: "PACKAGE_NOT_FOUND",
        message: "Package not found",
        packageName: req.params.name,
      },
    });
  }

  return res.status(200).json(match);
});

if (require.main === module) {
  app.listen(PORT, () => {
    console.log(`CoreShell package registry listening on port ${PORT}`);
  });

  createMcpServer().listen(MCP_PORT, () => {
    console.log(`CoreShell MCP-compatible service listening on port ${MCP_PORT}`);
  });
}

module.exports = {
  app,
  createMcpServer,
  MCP_PORT,
  PORT,
  packages,
};
