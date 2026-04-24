# cat — concatenate files and print to stdout

Concatenate one or more files and write them to standard output.

## Usage

```
cat  [-hn] <file> [<file>]...
```

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | show this help and exit |
| `-n, --number` | number all output lines |
| `<file>` | files to concatenate |

## Examples

```sh
cat file.txt           # print a file
cat a.txt b.txt        # concatenate two files
cat -n file.txt        # print with line numbers
```
