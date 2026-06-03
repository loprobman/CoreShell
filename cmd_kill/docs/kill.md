# kill

## Usage

```
Usage: kill [-s SIGNAL] <pid|%jobid> ...
Send a signal to a process or background job.
Default signal: SIGTERM
```

## Options

```
Options:
  -h, --help             show this help and exit
  --help-json            print argument schema as JSON and exit
  -s, --signal SIGNAL    signal name (e.g. TERM, KILL)
  <pid|%jobid>           PID or %jobid to signal

Examples:
  kill 1234
  kill %1
  kill -s KILL %2
```
