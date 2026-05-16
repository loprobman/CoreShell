const test = require("node:test");
const assert = require("node:assert/strict");
const http = require("node:http");

const { app } = require("../server");

let server;
let baseUrl;

function requestJson(pathname) {
  return new Promise((resolve, reject) => {
    http
      .get(`${baseUrl}${pathname}`, (res) => {
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

test.before(async () => {
  await new Promise((resolve) => {
    server = app.listen(0, "127.0.0.1", resolve);
  });
  const address = server.address();
  baseUrl = `http://127.0.0.1:${address.port}`;
});

test.after(async () => {
  if (!server) return;
  await new Promise((resolve, reject) => {
    server.close((err) => {
      if (err) reject(err);
      else resolve();
    });
  });
});

test("GET /packages returns full package list", async () => {
  const response = await requestJson("/packages");
  assert.equal(response.statusCode, 200);
  assert.ok(Array.isArray(response.body));
  assert.ok(response.body.length >= 2);
  for (const pkg of response.body) {
    assert.equal(typeof pkg.name, "string");
    assert.equal(typeof pkg.latestVersion, "string");
    assert.equal(typeof pkg.downloadUrl, "string");
  }
});

test("GET /packages/:name returns a matching package", async () => {
  const response = await requestJson("/packages/echo");
  assert.equal(response.statusCode, 200);
  assert.deepEqual(response.body, {
    name: "echo",
    latestVersion: "1.1.0",
    downloadUrl: "http://localhost:3000/downloads/echo-1.1.0.tar.gz",
  });
});

test("GET /packages/:name returns stable 404 error payload", async () => {
  const response = await requestJson("/packages/unknown-package");
  assert.equal(response.statusCode, 404);
  assert.deepEqual(response.body, {
    error: {
      code: "PACKAGE_NOT_FOUND",
      message: "Package not found",
      packageName: "unknown-package",
    },
  });
});
