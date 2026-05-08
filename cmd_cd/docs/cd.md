# cd — change the current directory

Change the current working directory to the specified path, or $HOME if no path is given.

## Usage

```
cd  [-h] [[dir]]
```

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | show this help and exit |
| `[dir]` | directory to change to (default: $HOME) |

## Examples

```sh
cd               # go to $HOME
cd /tmp          # change to /tmp
cd ..            # go up one level
```
