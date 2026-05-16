const express = require("express");
const fs = require("fs");
const path = require("path");

const app = express();
const PORT = 3000;
const ARTIFACTS_DIR = path.join(__dirname, "artifacts");

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
}

module.exports = {
  app,
  packages,
};
