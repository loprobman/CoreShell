# mv — move (rename) a file or directory

Rename SOURCE to DEST, moving it if they are on the same filesystem.

## Usage

```
mv  [-hv] <src> <dst>
```

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | show this help and exit |
| `-v, --verbose` | explain what is being done |
| `<src> <dst>` | source and destination |

## Examples

```sh
mv old.txt new.txt     # rename a file
mv file.txt /tmp/      # move file to /tmp
mv -v src.txt dst.txt  # move with verbose output
```
