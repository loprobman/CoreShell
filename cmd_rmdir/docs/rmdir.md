# rmdir — remove empty directories

Remove one or more empty directories. Use -p to also remove parent directories if they become empty.

## Usage

```
rmdir  [-hp] <dir> [<dir>]...
```

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | show this help and exit |
| `-p, --parents` | remove each parent directory that becomes empty |
| `<dir>` | empty directories to remove |

## Examples

```sh
rmdir emptydir        # remove an empty directory
rmdir -p a/b/c        # remove c, then b, then a if all become empty
```
