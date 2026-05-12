# Node.js Registry Server Prompt

I need a minimal Node.js + Express server that acts as a package registry for CoreShell.

## Scope
- Implement only the Node registry server and related Node docs/tests.
- Do not modify the C test runner unless explicit CoreShell C integration is requested.

## Requirements
- Keep package data in memory as an array of objects with this exact shape:

```json
{ "name": "string", "latestVersion": "string", "downloadUrl": "string" }
```

- Include at least two example entries, such as:
  - hello
  - wcplus
- Enforce unique package names at startup.
- Use exact, case-sensitive string matching for package lookup.
- Server must listen on port 3000.

## API
- GET /packages
  - Returns HTTP 200 and a JSON array of all package objects.
- GET /packages/:name
  - Returns HTTP 200 and the matching package object when found.
  - Returns HTTP 404 and JSON error when not found.

## Error Format
- Use a stable JSON shape for not-found responses:

```json
{ "error": { "code": "PACKAGE_NOT_FOUND", "message": "Package not found", "packageName": "requested-name" } }
```

- For not-found, set:
  - code = "PACKAGE_NOT_FOUND"
  - message = "Package not found"
  - packageName = requested :name

## Setup Constraints
- Use npm init -y.
- Install express via npm install express.

## package.json Minimum
- name
- version
- private
- main: server.js
- scripts.start: node server.js
- dependencies.express

## Testing
- Add a Node test file covering both endpoints:
  - success path for GET /packages
  - success path for GET /packages/:name
  - 404 path for GET /packages/:name with unknown package
- Do not change tests/test_runner.c for this task.

## Documentation
- Update README.md with:
  - how to install dependencies
  - how to start the registry server
  - how to run Node endpoint tests

## Deliverables
- package.json contents
- server.js implementation
- Node test file for endpoint coverage
- README.md updates with start/test instructions