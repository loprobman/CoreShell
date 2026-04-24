# cp — copy a file or directory

Copy SOURCE to DEST. Use -r to copy directories recursively.

## Usage

```
cp  [-hrv] <src> <dst>
```

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | show this help and exit |
| `-r, --recursive` | copy directories recursively |
| `-v, --verbose` | explain what is being done |
| `<src> <dst>` | source file and destination file/directory |

## Examples

```sh
cp a.txt b.txt       # copy a.txt to b.txt
cp -r src/ dst/      # copy directory recursively
cp -v file.txt /tmp/ # copy with verbose output
```
