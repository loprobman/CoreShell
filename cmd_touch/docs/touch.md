# touch — create a file or update its timestamp

Update the access and modification timestamps of each file. Create the file if it does not exist, unless -c is specified.

## Usage

```
touch  [-hc] <file> [<file>]...
```

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | show this help and exit |
| `-c, --no-create` | do not create any files |
| `<file>` | files to create or update |

## Examples

```sh
touch newfile.txt       # create empty file or update its timestamp
touch a.txt b.txt       # touch multiple files
touch -c maybe.txt      # update timestamp only if file already exists
```
