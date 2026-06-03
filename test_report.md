# CoreShell Test Report

Generated: 2026-05-26 21:28:26  
Fixture dir: `/tmp/coreshell_test_bFQjvg`

## Summary

| Metric  | Value |
|---------|-------|
| Total   | 166    |
| Passed  | 166    |
| Failed  | 0    |
| Rate    | 100.0% |

## Results

| # | Result | Description | Expected exit | Actual exit | stdout snippet |
|---|--------|-------------|---------------|-------------|----------------|
| 1 | **PASS** | help: no args lists all commands | `0` | `0` | `[0m` |
| 2 | **PASS** | help: valid command name shows usage | `0` | `0` | `[0m` |
| 3 | **PASS** | help: unknown command returns error | `non-zero` | `1` | `[0m` |
| 4 | **PASS** | exit: --help prints usage line | `0` | `0` | `[0m` |
| 5 | **PASS** | exit: bare call terminates child with status 0 | `0` | `0` | `[0m` |
| 6 | **PASS** | cd: change to /tmp succeeds | `0` | `0` | `[0m` |
| 7 | **PASS** | cd: nonexistent path returns error | `non-zero` | `1` | `[0m` |
| 8 | **PASS** | cd: no args navigates to HOME | `0` | `0` | `[0m` |
| 9 | **PASS** | cd: --help prints usage line | `0` | `0` | `[0m` |
| 10 | **PASS** | pwd: prints an absolute path | `0` | `0` | `[0m/home/rmlr/Development/CoreShell/CoreShell` |
| 11 | **PASS** | pwd: --help prints usage line | `0` | `0` | `[0m` |
| 12 | **PASS** | echo: no args prints empty line | `0` | `0` | `[0m` |
| 13 | **PASS** | echo: multiple strings printed space-separated | `0` | `0` | `[0mhello world` |
| 14 | **PASS** | echo: -n suppresses trailing newline | `0` | `0` | `[0mnolf` |
| 15 | **PASS** | echo: -e interprets \t as tab | `0` | `0` | `[0mcol1	col2` |
| 16 | **PASS** | echo: --help prints usage line | `0` | `0` | `[0m` |
| 17 | **PASS** | ls: lists fixture directory | `0` | `0` | `[0mempty.txt  test.txt  subdir  ` |
| 18 | **PASS** | ls: -a shows dot entries | `0` | `0` | `[0mempty.txt  test.txt  .  ..  subdir  ` |
| 19 | **PASS** | ls: -l shows long format with test.txt | `0` | `0` | `[0m-rw-rw-r--   1 rmlr     rmlr            0 May 26 21:28 empty.txt` |
| 20 | **PASS** | ls: -la combines long + all flags | `0` | `0` | `[0m-rw-rw-r--   1 rmlr     rmlr            0 May 26 21:28 empty.txt` |
| 21 | **PASS** | ls: nonexistent path returns error | `non-zero` | `1` | `[0m` |
| 22 | **PASS** | ls: --help prints usage line | `0` | `0` | `[0m` |
| 23 | **PASS** | stat: prints file size for existing file | `0` | `0` | `[0m  File: /tmp/coreshell_test_bFQjvg/test.txt` |
| 24 | **PASS** | stat: nonexistent file returns error | `non-zero` | `1` | `[0m` |
| 25 | **PASS** | stat: --help prints usage line | `0` | `0` | `[0m` |
| 26 | **PASS** | cat: prints full content of test.txt | `0` | `0` | `[0mHello World` |
| 27 | **PASS** | cat: empty file succeeds with no output | `0` | `0` | `[0m` |
| 28 | **PASS** | cat: nonexistent file returns error | `non-zero` | `1` | `[0m` |
| 29 | **PASS** | cat: --help prints usage line | `0` | `0` | `[0m` |
| 30 | **PASS** | head: default 10 lines includes first line | `0` | `0` | `[0mHello World` |
| 31 | **PASS** | head: -n 2 returns only first two lines | `0` | `0` | `[0mHello World` |
| 32 | **PASS** | head: -n 1 returns exactly the first line | `0` | `0` | `[0mHello World` |
| 33 | **PASS** | head: nonexistent file returns error | `non-zero` | `1` | `[0m` |
| 34 | **PASS** | head: --help prints usage line | `0` | `0` | `[0m` |
| 35 | **PASS** | tail: default output includes last line | `0` | `0` | `[0mHello World` |
| 36 | **PASS** | tail: -n 2 includes last two lines | `0` | `0` | `[0mLine 4` |
| 37 | **PASS** | tail: -n 1 returns only last line | `0` | `0` | `[0mLine 5` |
| 38 | **PASS** | tail: nonexistent file returns error | `non-zero` | `1` | `[0m` |
| 39 | **PASS** | tail: --help prints usage line | `0` | `0` | `[0m` |
| 40 | **PASS** | cp: copies existing file to new destination | `0` | `0` | `[0m` |
| 41 | **PASS** | cp: nonexistent source returns error | `non-zero` | `1` | `[0m` |
| 42 | **PASS** | cp: --help prints usage line | `0` | `0` | `[0m` |
| 43 | **PASS** | mv: renames/moves source file to destination | `0` | `0` | `[0m` |
| 44 | **PASS** | mv: nonexistent source returns error | `non-zero` | `1` | `[0m` |
| 45 | **PASS** | mv: --help prints usage line | `0` | `0` | `[0m` |
| 46 | **PASS** | rm: removes existing file | `0` | `0` | `[0m` |
| 47 | **PASS** | rm: nonexistent file returns error | `non-zero` | `1` | `[0m` |
| 48 | **PASS** | rm: --help prints usage line | `0` | `0` | `[0m` |
| 49 | **PASS** | mkdir: creates a new directory | `0` | `0` | `[0m` |
| 50 | **PASS** | mkdir: existing directory returns error | `non-zero` | `1` | `[0m` |
| 51 | **PASS** | mkdir: --help prints usage line | `0` | `0` | `[0m` |
| 52 | **PASS** | rmdir: removes an empty directory | `0` | `0` | `[0m` |
| 53 | **PASS** | rmdir: nonexistent directory returns error | `non-zero` | `1` | `[0m` |
| 54 | **PASS** | rmdir: --help prints usage line | `0` | `0` | `[0m` |
| 55 | **PASS** | touch: creates a new file | `0` | `0` | `[0m` |
| 56 | **PASS** | touch: updates timestamp of existing file | `0` | `0` | `[0m` |
| 57 | **PASS** | touch: --help prints usage line | `0` | `0` | `[0m` |
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
| 94 | **PASS** | pkg.json: jobs has all required fields | `0` | `0` | `{` |
| 95 | **PASS** | pkg.json: kill has all required fields | `0` | `0` | `{` |
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
| 113 | **PASS** | docs: jobs.md has Usage and Options sections | `0` | `0` | `# jobs` |
| 114 | **PASS** | docs: kill.md has Usage and Options sections | `0` | `0` | `# kill` |
| 115 | **PASS** | multicall mode2: echo prints argument | `0` | `0` | `hello_multicall` |
| 116 | **PASS** | multicall mode2: pwd returns an absolute path | `0` | `0` | `/home/rmlr/Development/CoreShell/CoreShell` |
| 117 | **PASS** | multicall mode2: ls --help shows Usage | `0` | `0` | `` |
| 118 | **PASS** | multicall mode2: help lists built-in commands | `0` | `0` | `` |
| 119 | **PASS** | multicall mode2: unknown command returns non-zero | `non-zero` | `127` | `CoreShell: unknown command 'xyznosuchcmd99'` |
| 120 | **PASS** | multicall mode1: symlink 'echo' dispatches correctly | `0` | `0` | `mode1_ok` |
| 121 | **PASS** | pwd: --help lists --logical long option (format bug regression) | `0` | `0` | `[0m` |
| 122 | **PASS** | pwd: --help lists --physical long option (format bug regression) | `0` | `0` | `[0m` |
| 123 | **PASS** | json: echo --help-json emits JSON schema with "name" key | `0` | `0` | `[0m{` |
| 124 | **PASS** | json: ls --help-json emits JSON schema with "options" key | `0` | `0` | `[0m{` |
| 125 | **PASS** | json: pwd --help-json emits JSON schema with "name" key | `0` | `0` | `[0m{` |
| 126 | **PASS** | json: echo --json returns {"output": ...} | `0` | `0` | `[0m{` |
| 127 | **PASS** | json: pwd --json returns {"path": ...} | `0` | `0` | `[0m{` |
| 128 | **PASS** | json: ls --json returns {"entries": ...} | `0` | `0` | `[0m{` |
| 129 | **PASS** | pkg: --help lists build subcommand | `0` | `0` | `` |
| 130 | **PASS** | pkg: list reports installed packages | `0` | `0` | `Installed packages:` |
| 131 | **PASS** | pkg: build with missing args returns error | `non-zero` | `1` | `Usage: pkg build <src-dir> <output.tar.gz>` |
| 132 | **PASS** | pkg: install of nonexistent archive returns error | `non-zero` | `1` | `/tmp/no_such_pkg_coreshell_99.tar.gz: No such file or directory` |
| 133 | **PASS** | pkg: CoreShell multicall dispatch supports pkg --help | `0` | `0` | `` |
| 134 | **PASS** | pkg compile --dry-run: mentions docs | `0` | `0` | `  [dry-run] mkdir -p bin build` |
| 135 | **PASS** | pkg compile --dry-run: mentions pkg.json | `0` | `0` | `  [dry-run] mkdir -p bin build` |
| 136 | **PASS** | pkg compile cmd_echo: succeeds and prints [OK] | `0` | `0` | `pkg compile: single module 'cmd_echo'` |
| 137 | **PASS** | compile output: echo.md has Usage and Options | `0` | `0` | `# echo` |
| 138 | **PASS** | compile output: pwd.md has Usage and Options | `0` | `0` | `# pwd` |
| 139 | **PASS** | compile output: ls.md has Usage and Options | `0` | `0` | `# ls` |
| 140 | **PASS** | compile output: echo pkg.json has all fields | `0` | `0` | `{` |
| 141 | **PASS** | compile output: pwd pkg.json has all fields | `0` | `0` | `{` |
| 142 | **PASS** | compile output: ls pkg.json has all fields | `0` | `0` | `{` |
| 143 | **PASS** | jobs: prints no-jobs message when table is empty | `0` | `0` | `[0mNo background jobs.` |
| 144 | **PASS** | jobs: --help prints usage line | `0` | `0` | `[0mUsage: jobs` |
| 145 | **PASS** | kill: --help prints usage line | `0` | `0` | `[0mUsage: kill [-s SIGNAL] <pid|%jobid> ...` |
| 146 | **PASS** | kill: unknown job %99 returns error | `non-zero` | `1` | `[0m` |
| 147 | **PASS** | threading: pwd executes in thread and returns success | `0` | `0` | `[0m/home/rmlr/Development/CoreShell/CoreShell` |
| 148 | **PASS** | threading: echo executes in thread with correct output | `0` | `0` | `[0mhello from thread` |
| 149 | **PASS** | threading: cd . succeeds with exit code 0 and modifies process state | `0` | `0` | `[0m` |
| 150 | **PASS** | threading: cd /nonexistent fails with error message | `non-zero` | `1` | `[0m` |
| 151 | **PASS** | threading: help returns command list from thread | `0` | `0` | `[0m` |
| 152 | **PASS** | threading: help pwd shows pwd usage from thread | `0` | `0` | `[0m` |
| 153 | **PASS** | threading: echo -n returns success from thread | `0` | `0` | `[0mno_newline` |
| 154 | **PASS** | threading: ls . returns success from thread | `0` | `0` | `[0mmain.o  test_report.md  cmd_touch  main.c  CoreShellBNFC_Prompts.md  build ` |
| 155 | **PASS** | threading: mkdir test_threading_dir succeeds from thread | `0` | `0` | `[0m` |
| 156 | **PASS** | threading: pwd --help returns usage (exit 0) from thread | `0` | `0` | `[0m` |
| 157 | **PASS** | threading: echo -e interprets escapes from thread | `0` | `0` | `[0mline1` |
| 158 | **PASS** | threading: stat returns file info from thread | `0` | `0` | `[0m  File: /tmp/coreshell_test_bFQjvg` |
| 159 | **PASS** | threading: exit --help returns usage from thread | `0` | `0` | `[0m` |
| 160 | **PASS** | threading: cd with no args goes to HOME from thread | `0` | `0` | `[0m` |
| 161 | **PASS** | repl: echo command is parsed and executed | `0` | `0` | `CoreShell v2.0 - Simple Linux Shell` |
| 162 | **PASS** | repl: option token in pipeline is parsed | `0` | `0` | `CoreShell v2.0 - Simple Linux Shell` |
| 163 | **PASS** | repl: foreground pipeline does not emit waitpid ECHILD warning | `0` | `0` | `CoreShell v2.0 - Simple Linux Shell` |
| 164 | **PASS** | repl: redirection writes a file through the shell parser | `0` | `0` | `CoreShell v2.0 - Simple Linux Shell` |
| 165 | **PASS** | repl: builtin cat reads stdin via input redirection | `0` | `0` | `CoreShell v2.0 - Simple Linux Shell` |
| 166 | **PASS** | repl: absolute-path redirection is parsed | `0` | `0` | `CoreShell v2.0 - Simple Linux Shell` |
