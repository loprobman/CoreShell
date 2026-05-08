# ls

## Usage

```

Usage: ls  [-hal] [--help-json] [--json] [[path]]

List directory contents.
Hidden entries (names starting with '.') are omitted by default; use -a to include them.
Use -l for a long listing showing permissions, owner, size, and modification time.
Use --json to output the listing as a JSON object with path and entries array.

```

## Options

```
Options:
  -h, --help             show this help and exit
  --help-json            print argument schema as JSON and exit
  --json                 output directory listing as JSON
  -a, --all              include hidden entries (starting with .)
  -l, --long             use long listing format
  [path]                 directory to list (default: .)

Examples:
  ls
  ls -la /etc
  ls --json /tmp


```
