# echo

## Usage

```

Usage: echo  [-hne] [--help-json] [--json] [[string]]...

Print arguments to standard output, separated by spaces.
With -e, backslash sequences are interpreted: \n (newline), \t (tab), \\ etc.
With -n, the trailing newline is suppressed.

```

## Options

```
Options:
  -h, --help             show this help and exit
  --help-json            print argument schema as JSON and exit
  --json                 output result as JSON
  -n, --no-newline       do not output trailing newline
  -e, --escape           enable backslash escape interpretation
  [string]               strings to print

Examples:
  echo Hello World
  echo -e "line1\nline2"
  echo -n no-newline


```
