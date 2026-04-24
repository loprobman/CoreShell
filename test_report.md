# CoreShell Test Report

Generated: 2026-04-23 18:24:18  
Fixture dir: `/tmp/coreshell_test_UcqZlJ`

## Summary

| Metric  | Value |
|---------|-------|
| Total   | 57    |
| Passed  | 57    |
| Failed  | 0    |
| Rate    | 100.0% |

## Results

| # | Result | Description | Expected exit | Actual exit | stdout snippet |
|---|--------|-------------|---------------|-------------|----------------|
| 1 | **PASS** | help: no args lists all commands | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 2 | **PASS** | help: valid command name shows usage | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 3 | **PASS** | help: unknown command returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 4 | **PASS** | exit: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 5 | **PASS** | exit: bare call terminates child with status 0 | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 6 | **PASS** | cd: change to /tmp succeeds | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 7 | **PASS** | cd: nonexistent path returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 8 | **PASS** | cd: no args navigates to HOME | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 9 | **PASS** | cd: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 10 | **PASS** | pwd: prints an absolute path | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 11 | **PASS** | pwd: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 12 | **PASS** | echo: no args prints empty line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 13 | **PASS** | echo: multiple strings printed space-separated | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 14 | **PASS** | echo: -n suppresses trailing newline | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 15 | **PASS** | echo: -e interprets \t as tab | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 16 | **PASS** | echo: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 17 | **PASS** | ls: lists fixture directory | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 18 | **PASS** | ls: -a shows dot entries | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 19 | **PASS** | ls: -l shows long format with test.txt | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 20 | **PASS** | ls: -la combines long + all flags | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 21 | **PASS** | ls: nonexistent path returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 22 | **PASS** | ls: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 23 | **PASS** | stat: prints file size for existing file | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 24 | **PASS** | stat: nonexistent file returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 25 | **PASS** | stat: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 26 | **PASS** | cat: prints full content of test.txt | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 27 | **PASS** | cat: empty file succeeds with no output | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 28 | **PASS** | cat: nonexistent file returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 29 | **PASS** | cat: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 30 | **PASS** | head: default 10 lines includes first line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 31 | **PASS** | head: -n 2 returns only first two lines | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 32 | **PASS** | head: -n 1 returns exactly the first line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 33 | **PASS** | head: nonexistent file returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 34 | **PASS** | head: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 35 | **PASS** | tail: default output includes last line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 36 | **PASS** | tail: -n 2 includes last two lines | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 37 | **PASS** | tail: -n 1 returns only last line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 38 | **PASS** | tail: nonexistent file returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 39 | **PASS** | tail: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 40 | **PASS** | cp: copies existing file to new destination | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 41 | **PASS** | cp: nonexistent source returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 42 | **PASS** | cp: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 43 | **PASS** | mv: renames/moves source file to destination | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 44 | **PASS** | mv: nonexistent source returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 45 | **PASS** | mv: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 46 | **PASS** | rm: removes existing file | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 47 | **PASS** | rm: nonexistent file returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 48 | **PASS** | rm: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 49 | **PASS** | mkdir: creates a new directory | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 50 | **PASS** | mkdir: existing directory returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 51 | **PASS** | mkdir: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 52 | **PASS** | rmdir: removes an empty directory | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 53 | **PASS** | rmdir: nonexistent directory returns error | `non-zero` | `1` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 54 | **PASS** | rmdir: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 55 | **PASS** | touch: creates a new file | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 56 | **PASS** | touch: updates timestamp of existing file | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
| 57 | **PASS** | touch: --help prints usage line | `0` | `0` | ` Setting up fixtures in /tmp/coreshell_test_UcqZlJ` |
