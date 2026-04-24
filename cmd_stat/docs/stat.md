# stat — display file status

Show size, mode, link count, owner ids, and timestamps for one or more files.

## Usage

```
stat  [-h] <file> [<file>]...
```

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | show this help and exit |
| `<file>` | files to stat |

## Examples

```sh
stat file.txt          # display status of file.txt
stat /etc/passwd       # display status of /etc/passwd
stat a.txt b.txt       # display status of multiple files
```
