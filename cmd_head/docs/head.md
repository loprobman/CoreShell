# head — print the first lines of a file

Output the first N lines (default: 10) of the given file.

## Usage

```
head  [-h] [-n <N>] <file>
```

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | show this help and exit |
| `-n, --lines=<N>` | number of lines to print (default: 10) |
| `<file>` | input file |

## Examples

```sh
head file.txt        # print first 10 lines
head -n 5 file.txt   # print first 5 lines
```
