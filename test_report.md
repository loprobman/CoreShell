# CoreShell Test Report

Generated: 2026-05-16 17:48:03  
Fixture dir: `/tmp/coreshell_test_usQlbD`

## Summary

| Metric  | Value |
|---------|-------|
| Total   | 160    |
| Passed  | 149    |
| Failed  | 11    |
| Rate    | 93.1% |

## Results

| # | Result | Description | Expected exit | Actual exit | stdout snippet |
|---|--------|-------------|---------------|-------------|----------------|
| 1 | **PASS** | help: no args lists all commands | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 2 | **PASS** | help: valid command name shows usage | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 3 | **PASS** | help: unknown command returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 4 | **PASS** | exit: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 5 | **PASS** | exit: bare call terminates child with status 0 | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 6 | **PASS** | cd: change to /tmp succeeds | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 7 | **PASS** | cd: nonexistent path returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 8 | **PASS** | cd: no args navigates to HOME | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 9 | **PASS** | cd: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 10 | **PASS** | pwd: prints an absolute path | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 11 | **PASS** | pwd: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 12 | **PASS** | echo: no args prints empty line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 13 | **PASS** | echo: multiple strings printed space-separated | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 14 | **PASS** | echo: -n suppresses trailing newline | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 15 | **PASS** | echo: -e interprets \t as tab | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 16 | **PASS** | echo: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 17 | **PASS** | ls: lists fixture directory | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 18 | **PASS** | ls: -a shows dot entries | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 19 | **PASS** | ls: -l shows long format with test.txt | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 20 | **PASS** | ls: -la combines long + all flags | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 21 | **PASS** | ls: nonexistent path returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 22 | **PASS** | ls: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 23 | **PASS** | stat: prints file size for existing file | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 24 | **PASS** | stat: nonexistent file returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 25 | **PASS** | stat: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 26 | **PASS** | cat: prints full content of test.txt | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 27 | **PASS** | cat: empty file succeeds with no output | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 28 | **PASS** | cat: nonexistent file returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 29 | **PASS** | cat: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 30 | **PASS** | head: default 10 lines includes first line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 31 | **PASS** | head: -n 2 returns only first two lines | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 32 | **PASS** | head: -n 1 returns exactly the first line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 33 | **PASS** | head: nonexistent file returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 34 | **PASS** | head: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 35 | **PASS** | tail: default output includes last line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 36 | **PASS** | tail: -n 2 includes last two lines | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 37 | **PASS** | tail: -n 1 returns only last line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 38 | **PASS** | tail: nonexistent file returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 39 | **PASS** | tail: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 40 | **PASS** | cp: copies existing file to new destination | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 41 | **PASS** | cp: nonexistent source returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 42 | **PASS** | cp: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 43 | **PASS** | mv: renames/moves source file to destination | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 44 | **PASS** | mv: nonexistent source returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 45 | **PASS** | mv: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 46 | **PASS** | rm: removes existing file | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 47 | **PASS** | rm: nonexistent file returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 48 | **PASS** | rm: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 49 | **PASS** | mkdir: creates a new directory | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 50 | **PASS** | mkdir: existing directory returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 51 | **PASS** | mkdir: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 52 | **PASS** | rmdir: removes an empty directory | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 53 | **PASS** | rmdir: nonexistent directory returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 54 | **PASS** | rmdir: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 55 | **PASS** | touch: creates a new file | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 56 | **PASS** | touch: updates timestamp of existing file | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 57 | **PASS** | touch: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 58 | **PASS** | cmd_spec: ls has complete metadata | `0` | `0` | `` |
| 59 | **PASS** | cmd_spec: cat has complete metadata | `0` | `0` | `` |
| 60 | **PASS** | cmd_spec: cd has complete metadata | `0` | `0` | `` |
| 61 | **PASS** | cmd_spec: cp has complete metadata | `0` | `0` | `` |
| 62 | **PASS** | cmd_spec: echo has complete metadata | `0` | `0` | `` |
| 63 | **PASS** | cmd_spec: exit has complete metadata | `0` | `0` | `` |
| 64 | **PASS** | cmd_spec: head has complete metadata | `0` | `0` | `` |
| 65 | **PASS** | cmd_spec: help has complete metadata | `0` | `0` | `` |
| 66 | **PASS** | cmd_spec: mkdir has complete metadata | `0` | `0` | `` |
| 67 | **PASS** | cmd_spec: mv has complete metadata | `0` | `0` | `` |
| 68 | **PASS** | cmd_spec: pwd has complete metadata | `0` | `0` | `` |
| 69 | **PASS** | cmd_spec: rm has complete metadata | `0` | `0` | `` |
| 70 | **PASS** | cmd_spec: rmdir has complete metadata | `0` | `0` | `` |
| 71 | **PASS** | cmd_spec: stat has complete metadata | `0` | `0` | `` |
| 72 | **PASS** | cmd_spec: tail has complete metadata | `0` | `0` | `` |
| 73 | **PASS** | cmd_spec: touch has complete metadata | `0` | `0` | `` |
| 74 | **PASS** | cmd_spec: pkg has complete metadata | `0` | `0` | `` |
| 75 | **PASS** | cmd_spec: jobs has complete metadata | `0` | `0` | `` |
| 76 | **PASS** | cmd_spec: kill has complete metadata | `0` | `0` | `` |
| 77 | **PASS** | pkg.json: ls has all required fields | `0` | `0` | `{` |
| 78 | **PASS** | pkg.json: cat has all required fields | `0` | `0` | `{` |
| 79 | **PASS** | pkg.json: cd has all required fields | `0` | `0` | `{` |
| 80 | **PASS** | pkg.json: cp has all required fields | `0` | `0` | `{` |
| 81 | **PASS** | pkg.json: echo has all required fields | `0` | `0` | `{` |
| 82 | **PASS** | pkg.json: exit has all required fields | `0` | `0` | `{` |
| 83 | **PASS** | pkg.json: head has all required fields | `0` | `0` | `{` |
| 84 | **PASS** | pkg.json: help has all required fields | `0` | `0` | `{` |
| 85 | **PASS** | pkg.json: mkdir has all required fields | `0` | `0` | `{` |
| 86 | **PASS** | pkg.json: mv has all required fields | `0` | `0` | `{` |
| 87 | **PASS** | pkg.json: pwd has all required fields | `0` | `0` | `{` |
| 88 | **PASS** | pkg.json: rm has all required fields | `0` | `0` | `{` |
| 89 | **PASS** | pkg.json: rmdir has all required fields | `0` | `0` | `{` |
| 90 | **PASS** | pkg.json: stat has all required fields | `0` | `0` | `{` |
| 91 | **PASS** | pkg.json: tail has all required fields | `0` | `0` | `{` |
| 92 | **PASS** | pkg.json: touch has all required fields | `0` | `0` | `{` |
| 93 | **PASS** | pkg.json: pkg has all required fields | `0` | `0` | `{` |
| 94 | **FAIL** | pkg.json: jobs has all required fields | `0` | `1` | `cannot open: cmd_jobs/pkg.json` |
| 95 | **FAIL** | pkg.json: kill has all required fields | `0` | `1` | `cannot open: cmd_kill/pkg.json` |
| 96 | **PASS** | docs: ls.md has Usage and Options sections | `0` | `0` | `# ls` |
| 97 | **PASS** | docs: cat.md has Usage and Options sections | `0` | `0` | `# cat — concatenate files and print to stdout` |
| 98 | **PASS** | docs: cd.md has Usage and Options sections | `0` | `0` | `# cd — change the current directory` |
| 99 | **PASS** | docs: cp.md has Usage and Options sections | `0` | `0` | `# cp — copy a file or directory` |
| 100 | **PASS** | docs: echo.md has Usage and Options sections | `0` | `0` | `# echo` |
| 101 | **PASS** | docs: exit.md has Usage and Options sections | `0` | `0` | `# exit — exit the shell` |
| 102 | **PASS** | docs: head.md has Usage and Options sections | `0` | `0` | `# head` |
| 103 | **PASS** | docs: help.md has Usage and Options sections | `0` | `0` | `# help — show help for built-in commands` |
| 104 | **PASS** | docs: mkdir.md has Usage and Options sections | `0` | `0` | `# mkdir — create directories` |
| 105 | **PASS** | docs: mv.md has Usage and Options sections | `0` | `0` | `# mv — move (rename) a file or directory` |
| 106 | **PASS** | docs: pwd.md has Usage and Options sections | `0` | `0` | `# pwd` |
| 107 | **PASS** | docs: rm.md has Usage and Options sections | `0` | `0` | `# rm — remove files or directories` |
| 108 | **PASS** | docs: rmdir.md has Usage and Options sections | `0` | `0` | `# rmdir — remove empty directories` |
| 109 | **PASS** | docs: stat.md has Usage and Options sections | `0` | `0` | `# stat — display file status` |
| 110 | **PASS** | docs: tail.md has Usage and Options sections | `0` | `0` | `# tail — print the last lines of a file` |
| 111 | **PASS** | docs: touch.md has Usage and Options sections | `0` | `0` | `# touch — create a file or update its timestamp` |
| 112 | **PASS** | docs: pkg.md has Usage and Options sections | `0` | `0` | `# pkg` |
| 113 | **FAIL** | docs: jobs.md has Usage and Options sections | `0` | `1` | `cannot open: cmd_jobs/docs/jobs.md` |
| 114 | **FAIL** | docs: kill.md has Usage and Options sections | `0` | `1` | `cannot open: cmd_kill/docs/kill.md` |
| 115 | **PASS** | multicall mode2: echo prints argument | `0` | `0` | `hello_multicall` |
| 116 | **PASS** | multicall mode2: pwd returns an absolute path | `0` | `0` | `/home/rmlr/Development/CoreShell/CoreShell` |
| 117 | **PASS** | multicall mode2: ls --help shows Usage | `0` | `0` | `` |
| 118 | **PASS** | multicall mode2: help lists built-in commands | `0` | `0` | `` |
| 119 | **FAIL** | multicall mode2: unknown command returns non-zero | `non-zero` | `127` | `xyznosuchcmd99: No such file or directory` |
| 120 | **FAIL** | multicall mode1: symlink 'echo' dispatches correctly | `0` | `1` | `CoreShell: './echo': command not found` |
| 121 | **PASS** | pwd: --help lists --logical long option (format bug regression) | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 122 | **PASS** | pwd: --help lists --physical long option (format bug regression) | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 123 | **PASS** | json: echo --help-json emits JSON schema with "name" key | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 124 | **PASS** | json: ls --help-json emits JSON schema with "options" key | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 125 | **PASS** | json: pwd --help-json emits JSON schema with "name" key | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 126 | **PASS** | json: echo --json returns {"output": ...} | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 127 | **PASS** | json: pwd --json returns {"path": ...} | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 128 | **PASS** | json: ls --json returns {"entries": ...} | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 129 | **FAIL** | pkg: --help lists build subcommand | `0` | `127` | `./pkg/pkg: No such file or directory` |
| 130 | **FAIL** | pkg: list reports installed packages | `0` | `127` | `./pkg/pkg: No such file or directory` |
| 131 | **PASS** | pkg: build with missing args returns error | `non-zero` | `127` | `./pkg/pkg: No such file or directory` |
| 132 | **PASS** | pkg: install of nonexistent archive returns error | `non-zero` | `127` | `./pkg/pkg: No such file or directory` |
| 133 | **PASS** | pkg: CoreShell multicall dispatch supports pkg --help | `0` | `0` | `` |
| 134 | **FAIL** | pkg compile --dry-run: mentions docs | `0` | `127` | `./pkg/pkg: No such file or directory` |
| 135 | **FAIL** | pkg compile --dry-run: mentions pkg.json | `0` | `127` | `./pkg/pkg: No such file or directory` |
| 136 | **FAIL** | pkg compile cmd_echo: succeeds and prints [OK] | `0` | `127` | `./pkg/pkg: No such file or directory` |
| 137 | **PASS** | compile output: echo.md has Usage and Options | `0` | `0` | `# echo` |
| 138 | **PASS** | compile output: pwd.md has Usage and Options | `0` | `0` | `# pwd` |
| 139 | **PASS** | compile output: ls.md has Usage and Options | `0` | `0` | `# ls` |
| 140 | **PASS** | compile output: echo pkg.json has all fields | `0` | `0` | `{` |
| 141 | **PASS** | compile output: pwd pkg.json has all fields | `0` | `0` | `{` |
| 142 | **PASS** | compile output: ls pkg.json has all fields | `0` | `0` | `{` |
| 143 | **PASS** | jobs: prints no-jobs message when table is empty | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 144 | **PASS** | jobs: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 145 | **PASS** | kill: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 146 | **PASS** | kill: unknown job %99 returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 147 | **PASS** | threading: pwd executes in thread and returns success | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 148 | **PASS** | threading: echo executes in thread with correct output | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 149 | **PASS** | threading: cd . succeeds with exit code 0 and modifies process state | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 150 | **PASS** | threading: cd /nonexistent fails with error message | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 151 | **PASS** | threading: help returns command list from thread | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 152 | **PASS** | threading: help pwd shows pwd usage from thread | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 153 | **PASS** | threading: echo -n returns success from thread | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 154 | **PASS** | threading: ls . returns success from thread | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 155 | **PASS** | threading: mkdir test_threading_dir succeeds from thread | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 156 | **PASS** | threading: pwd --help returns usage (exit 0) from thread | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 157 | **PASS** | threading: echo -e interprets escapes from thread | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 158 | **PASS** | threading: stat returns file info from thread | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 159 | **PASS** | threading: exit --help returns usage from thread | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |
| 160 | **PASS** | threading: cd with no args goes to HOME from thread | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_usQlbD` |

## Failed Tests

### 94. pkg.json: jobs has all required fields

- **Exit code**: got `1`, expected `0`
- **captured stderr**:
```
cannot open: cmd_jobs/pkg.json
```

### 95. pkg.json: kill has all required fields

- **Exit code**: got `1`, expected `0`
- **captured stderr**:
```
cannot open: cmd_kill/pkg.json
```

### 113. docs: jobs.md has Usage and Options sections

- **Exit code**: got `1`, expected `0`
- **captured stderr**:
```
cannot open: cmd_jobs/docs/jobs.md
```

### 114. docs: kill.md has Usage and Options sections

- **Exit code**: got `1`, expected `0`
- **captured stderr**:
```
cannot open: cmd_kill/docs/kill.md
```

### 119. multicall mode2: unknown command returns non-zero

- **Exit code**: got `127`, expected any non-zero
- **stderr**: expected to contain `"unknown command"`
- **captured stderr**:
```
xyznosuchcmd99: No such file or directory

```

### 120. multicall mode1: symlink 'echo' dispatches correctly

- **Exit code**: got `1`, expected `0`
- **stdout**: expected to contain `"mode1_ok"`
- **captured stderr**:
```
CoreShell: './echo': command not found

```

### 129. pkg: --help lists build subcommand

- **Exit code**: got `127`, expected `0`
- **stdout**: expected to contain `"build"`
- **captured stderr**:
```
./pkg/pkg: No such file or directory

```

### 130. pkg: list reports installed packages

- **Exit code**: got `127`, expected `0`
- **stdout**: expected to contain `"packages"`
- **captured stderr**:
```
./pkg/pkg: No such file or directory

```

### 134. pkg compile --dry-run: mentions docs

- **Exit code**: got `127`, expected `0`
- **stdout**: expected to contain `"docs"`
- **captured stderr**:
```
./pkg/pkg: No such file or directory

```

### 135. pkg compile --dry-run: mentions pkg.json

- **Exit code**: got `127`, expected `0`
- **stdout**: expected to contain `"pkg.json"`
- **captured stderr**:
```
./pkg/pkg: No such file or directory

```

### 136. pkg compile cmd_echo: succeeds and prints [OK]

- **Exit code**: got `127`, expected `0`
- **stdout**: expected to contain `"[OK]"`
- **captured stderr**:
```
./pkg/pkg: No such file or directory

```

