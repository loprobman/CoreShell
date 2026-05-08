# echo — print arguments to standard output

Echo the given string arguments to stdout, separated by spaces. Supports -n and -e flags similar to GNU echo.

## Usage

```
echo  [-hne] [[string]]...
```

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | show this help and exit |
| `-n, --no-newline` | do not output trailing newline |
| `-e, --escape` | enable backslash escape interpretation |
| `[string]` | strings to print |

## Examples

```sh
echo Hello World      # print "Hello World"
echo -n no newline    # print without trailing newline
echo -e "tab\there"  # interpret \t as a tab character
```
