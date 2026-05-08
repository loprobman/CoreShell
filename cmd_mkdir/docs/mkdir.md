# mkdir — create directories

Create one or more directories. Use -p to create parent directories as needed.

## Usage

```
mkdir  [-hpv] <dir> [<dir>]...
```

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | show this help and exit |
| `-p, --parents` | create parent directories as needed |
| `-v, --verbose` | print each created directory |
| `<dir>` | directories to create |

## Examples

```sh
mkdir newdir              # create a single directory
mkdir -p a/b/c            # create nested directories
mkdir -v dir1 dir2 dir3   # create multiple directories verbosely
```
