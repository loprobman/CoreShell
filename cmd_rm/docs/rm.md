# rm — remove files or directories

Remove one or more files. Use -r to remove directories recursively.

## Usage

```
rm  [-hrfv] <file> [<file>]...
```

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | show this help and exit |
| `-r, --recursive` | remove directories recursively |
| `-f, --force` | ignore nonexistent files, no error |
| `-v, --verbose` | explain what is being done |
| `<file>` | files or directories to remove |

## Examples

```sh
rm file.txt          # remove a file
rm -r mydir/         # remove a directory recursively
rm -f missing.txt    # suppress error if file doesn't exist
rm -rv old/          # recursive removal with verbose output
```
