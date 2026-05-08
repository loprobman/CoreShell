# tail — print the last lines of a file

Output the last N lines (default: 10) of the given file.

## Usage

```
tail  [-h] [-n <N>] <file>
```

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | show this help and exit |
| `-n, --lines=<N>` | number of lines to print (default: 10) |
| `<file>` | input file |

## Examples

```sh
tail file.txt        # print last 10 lines
tail -n 5 file.txt   # print last 5 lines
```
