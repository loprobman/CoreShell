# pwd — print working directory

Displays the absolute path of the current directory.

## Usage

```
pwd  [-hLP]
```

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | show this help and exit |
| `-L, --logical` | use $PWD from environment (may contain symlinks) |
| `-P, --physical` | print physical path, resolving all symlinks |

## Examples

```sh
pwd      # print current directory
pwd -P   # print physical path (resolved symlinks)
```
