# pwd

## Usage

```

Usage: pwd  [-hLP] [--help-json] [--json]

Print the absolute path of the current working directory.
Default behaviour uses getcwd(3) (physical path, symlinks resolved).
Use -L to return the logical $PWD value, which may contain unresolved symlinks.

```

## Options

```
Options:
  -h, --help             show this help and exit
  --help-json            print argument schema as JSON and exit
  --json                 output result as JSON
  -L, --logical          use $PWD from environment (may contain symlinks)
  -P, --physical         print physical path, resolving all symlinks


```
