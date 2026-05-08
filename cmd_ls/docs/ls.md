# ls — list directory contents

List information about entries in the specified directory (default: current directory).

## Usage

```
ls  [-hal] [[path]]
```

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | show this help and exit |
| `-a, --all` | include hidden entries (starting with .) |
| `-l, --long` | use long listing format |
| `[path]` | directory to list (default: .) |

## Examples

```sh
ls               # list current directory
ls /etc          # list /etc
ls -la           # long listing including hidden files
ls -a ~/projects # list hidden entries in ~/projects
```
