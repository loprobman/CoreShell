# BNFC Implementation in CoreShell

## Overview

This document explains how **BNFC (BNF Converter)** is used in CoreShell to automatically generate a **lexer**, **parser**, and **abstract syntax tree (AST)** from a formal grammar specification.

---

## What is BNFC?

**BNFC** is a compiler construction tool that takes a **Context-Free Grammar** written in **Extended BNF (EBNF)** notation and generates:

- **Lexer** (tokenizer) - splits input into tokens
- **Parser** - validates syntax and builds abstract syntax tree
- **AST constructors** - create tree nodes
- **Pretty printer** - outputs parsed structures
- **Type definitions** - C structs for AST nodes

### Why Use BNFC?

Without BNFC, you'd manually write:
- ✗ Handcoded lexer (thousands of lines)
- ✗ Handcoded parser (LALR tables, shift/reduce logic)
- ✗ AST node structures
- ✗ All the glue code

With BNFC, you simply:
- ✓ Write grammar in `Grammar.cf`
- ✓ Run `bnfc --c Grammar.cf`
- ✓ Get all the code automatically

---

## CoreShell Grammar (Grammar/Grammar.cf)

### What It Defines

The grammar defines the **syntax structure** of shell commands:

```
Input    = a complete command (e.g., "ls -la | grep .c > output.txt")
Job      = foreground or background command
Pipeline = commands connected with pipes (|)
Command  = a single executable with arguments
Redirect = input/output redirection (>, <, >>, 2>, 2>>)
```

### Example Grammar Rules

```
comment "/*" "*/" ;
comment "//" ;

token Word (...);

entrypoints Input ;

StartInput. Input ::= Job ;

Foreground. Job ::= CommandLine ;
Background. Job ::= CommandLine "&" ;

MkCmdLine. CommandLine ::= Pipeline OptRedir ;

Single. Pipeline ::= SimpleCommand ;
Pipe. Pipeline ::= SimpleCommand "|" Pipeline ;

MkSimpleCommand. SimpleCommand ::= Word ListWord ;

NoMoreWord. ListWord ::= ;
MoreWord. ListWord ::= Word ListWord ;
```

### Grammar Notation

| Symbol | Meaning |
|--------|---------|
| `.`    | Rule terminator |
| `::=`  | "is defined as" (production rule) |
| `;`    | Alternative separator |
| `*`    | Zero or more |
| `+`    | One or more |
| `?`    | Optional |
| `\|`   | Pipe (in token definitions) |

---

## BNFC Pipeline: From Grammar to Code

```
    Grammar.cf (user writes)
         ↓
    [bnfc --c Grammar.cf]
         ↓
    ┌────────────────────────────────────┐
    │  Generated Code (in subfolders)    │
    ├────────────────────────────────────┤
    │ • Absyn/Absyn.c, Absyn.h          │
    │ • Buffer/Buffer.c, Buffer.h        │
    │ • Grammar/Grammar.l (lexer rules)  │
    │ • Grammar/Grammar.y (parser rules) │
    │ • Printer/Printer.c, Printer.h     │
    │ • Skeleton/Skeleton.c, Skeleton.h  │
    │ • Test/Test.c                      │
    └────────────────────────────────────┘
         ↓
    [bison + flex compilation]
         ↓
    ┌────────────────────────────────────┐
    │  Final Components                  │
    ├────────────────────────────────────┤
    │ • Parser/Parser.c (parser logic)   │
    │ • Lexer/Lexer.c (lexer logic)      │
    │ • Bison/Bison.h (token definitions)│
    └────────────────────────────────────┘
```

---

## The Three Generated Components

### 1. **Lexer** (Grammar/Grammar.l → Lexer/Lexer.c)

**Purpose**: Break input into tokens

**Example**:
```
Input:  "ls -la | grep file"
         ↓ [Lexer processes]
Tokens: [Word: "ls"] [Word: "-la"] [BAR] [Word: "grep"] [Word: "file"]
```

**Generated Lexer Rules** (from Grammar.l):
```lex
<INITIAL>"&"      	 return _AMP;      /* & token */
<INITIAL>"|"      	 return _BAR;      /* | token */
<INITIAL>"<"      	 return _LT;       /* < redirect */
<INITIAL>">"      	 return _GT;       /* > redirect */
<INITIAL>...                           /* more rules */
```

### 2. **Parser** (Grammar/Grammar.y → Parser/Parser.c)

**Purpose**: Build Abstract Syntax Tree from tokens

**Example Parser Rule** (from Grammar.y):
```yacc
Pipeline : SimpleCommand { $$ = make_Single($1); }
         | SimpleCommand _BAR Pipeline { $$ = make_Pipe($1, $3); }
         ;
```

**What It Does**:
```
Tokens: [Word:"ls"] [Word:"-la"] [BAR] [Word:"grep"] [Word:"file"]
         ↓ [Parser processes according to Grammar.y rules]
AST:     Pipe(
           SimpleCommand("ls", ["-la"]),
           SimpleCommand("grep", ["file"])
         )
```

### 3. **AST (Abstract Syntax Tree)** (Absyn/Absyn.c)

**Purpose**: Represent parsed input as a tree structure

**C Structs Generated** (from Absyn.h):
```c
typedef struct {
  int kind;  /* which variant? */
  union {
    struct {
      Pipeline pipeline_;
      OptRedir optredir_;
    } mkcmdline_;    /* variant for MkCmdLine */
  } u;  /* union of all variants */
} CommandLine;
```

**Example Tree Structure**:
```
Input
 └─ Job (Foreground)
     └─ CommandLine
         ├─ Pipeline
         │   ├─ SimpleCommand: "ls" ["-la"]
         │   └─ Pipe to:
         │       └─ SimpleCommand: "grep" ["file"]
         └─ OptRedir: None
```

---

## Integration with BISON & FLEX

### What BISON Does

**BISON** (GNU Parser Generator) produces `Parser.c` from `Grammar.y`:

- **Creates parsing tables** (shift/reduce actions)
- **Implements LALR(1) algorithm** to handle token stream
- **Calls semantic actions** (the `{ ... }` code) to build AST
- **Handles error recovery**

### What FLEX Does

**FLEX** (Fast Lexical Analyzer) produces `Lexer.c` from `Grammar.l`:

- **Generates finite automaton** for token recognition
- **Scans input character-by-character**
- **Returns tokens to parser** one at a time
- **Tracks line/column** for error messages

### How They Work Together

```
User Input
    ↓
[Lexer/Lexer.c (from FLEX)]
    ↓
Token Stream (Word, BAR, LT, GT, etc.)
    ↓
[Parser/Parser.c (from BISON)]
    ↓
Calls AST constructors from Absyn/Absyn.h
    ↓
Complete AST in memory
    ↓
Main program executes the AST
```

---

## CoreShell-Specific Implementation

### File Organization

```
CoreShell/
├── Grammar/
│   ├── Grammar.cf          ← Grammar definition (user edits this)
│   ├── Grammar.l           ← Lexer rules (auto-generated)
│   ├── Grammar.l.bak       ← Backup
│   └── Grammar.y           ← Parser rules (auto-generated)
├── Absyn/                  ← Abstract Syntax Tree
│   ├── Absyn.c
│   └── Absyn.h
├── Lexer/                  ← Lexer (tokenizer)
│   └── Lexer.c
├── Parser/                 ← Parser
│   ├── Parser.c
│   └── Parser.h
├── Bison/                  ← Bison-generated definitions
│   └── Bison.h
├── Buffer/, Printer/, ... ← Other generated components
└── Makefile                ← Orchestrates BNFC + BISON + FLEX
```

### Build Process (Makefile)

```makefile
# 1. Generate all code from Grammar.cf
$(BNFC_STAMP): $(BNFC_GRAMMAR)
   $(BNFC) --c $(BNFC_GRAMMAR)  # Calls bnfc tool
   # Moves generated files into component folders

# 2. Generate parser from Grammar.y (using BISON)
Parser/Parser.c Bison/Bison.h: Grammar/Grammar.y
   bison -d -o Parser/Parser.c Grammar/Grammar.y

# 3. Generate lexer from Grammar.l (using FLEX)
Lexer/Lexer.c: Grammar/Grammar.l Bison/Bison.h
   flex -o Lexer/Lexer.c Grammar/Grammar.l

# 4. Compile everything together
$(TARGET): $(OBJ)
   $(CC) -o CoreShell $(OBJ) -lm
```

### How CoreShell Uses the Parser

In `main.c`:

```c
#include "Parser.h"

int main() {
    char input[256];
    while (1) {
        printf("$ ");
        fgets(input, sizeof(input), stdin);
        
        // Parse input string into AST
        Input ast = psInput(input);  // Call generated parser
        
        if (!ast) {
            printf("Parse error\n");
            continue;
        }
        
        // Execute the AST
        execute_input(ast);
    }
}
```

---

## Key Grammar Concepts for CoreShell

### 1. **Token Definition**

```
token Word ((letter | digit | '_' | '.' | '/' | '-')
           (letter | digit | '_' | '.' | '/' | '-')*) ;
```

- Matches: `ls`, `-la`, `/tmp/file.txt`, `cmd_name`
- Generated into lexer rules

### 2. **Production Rules**

```
Foreground. Job ::= CommandLine ;
Background. Job ::= CommandLine "&" ;
```

- `Foreground` = constructor name
- `Job` = nonterminal (return type)
- `CommandLine` = right-hand side (input)
- Generates AST constructor: `make_Foreground(CommandLine)`

### 3. **Alternatives**

```
OptRedir : /* empty */ { ... }
         | _LT T_Word { ... }
         | _GT T_Word { ... }
         ;
```

- Multiple production rules for same nonterminal
- Each produces different AST node kind

### 4. **Lists**

```
NoMoreWord. ListWord ::= ;
MoreWord. ListWord ::= Word ListWord ;
```

- `NoMoreWord` = empty list
- `MoreWord` = cons cell (head + tail)
- Example: `["/opt/bin", "-l", "-a"]`

---

## Step-by-Step Execution Example

### Input: `echo hello > output.txt`

#### Step 1: Lexical Analysis (Lexer)
```
Input string:  "echo hello > output.txt"
Token stream:  [Word("echo"), Word("hello"), GT, Word("output.txt")]
```

#### Step 2: Syntactic Analysis (Parser)
```
Apply Grammar Rules:
  SimpleCommand ::= T_Word ListWord
    → SimpleCommand("echo", [Word("hello")])
  
  Pipeline ::= SimpleCommand
    → Pipeline(SimpleCommand(...))
  
  OptRedir ::= ">" T_Word
    → OptRedir(OutRedir("output.txt"))
  
  CommandLine ::= Pipeline OptRedir
    → CommandLine(Pipeline(...), OptRedir(...))
  
  Job ::= CommandLine
    → Job(Foreground(CommandLine(...)))
  
  Input ::= Job
    → Input(Job(...))
```

#### Step 3: AST Construction
```c
typedef struct {
  Input {
    Job (Foreground) {
      CommandLine {
        Pipeline {
          SimpleCommand {
            word_: "echo",
            listword_: ListWord {
              MoreWord {
                word_: "hello",
                listword_: NoMoreWord
              }
            }
          }
        }
        OptRedir {
          kind: is_OutRedir,
          word_: "output.txt"
        }
      }
    }
  }
} AST;
```

#### Step 4: Execution (CoreShell)
```c
execute_input(AST);
  → execute_job(AST->job)
    → execute_commandline(...)
      → redirect stdout to "output.txt"
      → execute SimpleCommand("echo", ["hello"])
      → printf("hello");
```

---

## Modifying the Grammar

### To Add a New Feature (e.g., `2>&1` redirection)

1. **Edit** `Grammar/Grammar.cf`:
```
ErrRedirStderr. OptRedir ::= "2>&1" ;
```

2. **Regenerate**:
```bash
make clean
make
```

3. **BNFC produces**:
   - New AST variant in `Absyn.h`
   - New parser rule in `Grammar.y`
   - New lexer pattern in `Grammar.l`

4. **Implement handling** in `main.c`:
```c
if (redir->kind == is_ErrRedirStderr) {
    dup2(STDOUT_FILENO, STDERR_FILENO);
}
```

---

## Debugging

### View Generated Files

| File | Purpose |
|------|---------|
| `Grammar/Grammar.l` | Lexer patterns |
| `Grammar/Grammar.y` | Parser rules |
| `Absyn/Absyn.h` | AST node definitions |
| `Parser/Parser.c` | Parser state machine |
| `Lexer/Lexer.c` | Lexer state machine |

### Common Errors

| Error | Cause |
|-------|-------|
| "syntax error at end of file" | Grammar doesn't match input |
| "undefined reference to yylex" | Lexer not compiled |
| "Parse error in Grammar.cf" | Invalid grammar syntax |
| Infinite loop | Left recursion in grammar |

### Test Parser

```bash
echo "ls -la | grep .c > output.txt" | ./CoreShell
```

---

## Summary

| Component | Source | Purpose |
|-----------|--------|---------|
| **Grammar** | `Grammar.cf` | User writes this |
| **BNFC** | Tool | Generates code from grammar |
| **Lexer** | FLEX + `Grammar.l` | Tokenizes input |
| **Parser** | BISON + `Grammar.y` | Builds AST |
| **AST** | `Absyn/` | In-memory tree structure |
| **Execution** | `main.c` | Interprets AST |

---

## References

- **BNFC Documentation**: http://bnfc.digitalgrammars.com/
- **BISON Manual**: https://www.gnu.org/software/bison/manual/
- **FLEX Manual**: https://westes.github.io/flex/manual/
- **Context-Free Grammars**: https://en.wikipedia.org/wiki/Context-free_grammar

## Using the Shell

1. Start with a simple command.

  ```bash
  user@CoreShell> pwd
  user@CoreShell> echo hello world
  ```
2. Run commands through a pipeline.

  ```bash
  user@CoreShell> echo hello from coreshell | tr a-z A-Z
  ```
3. Redirect output to a file and read it back.

  ```bash
  user@CoreShell> echo sample > out.txt
  user@CoreShell> cat < out.txt
  ```
4. Run a job in the background.

  ```bash
  user@CoreShell> sleep 5 &
  ```
5. Use built-in commands directly.

  ```bash
  user@CoreShell> help
  user@CoreShell> cd /tmp
  ```
6. Try the optional natural-language helper.

  ```bash
  user@CoreShell> @list all C files here
  ```

Press `Ctrl+D` to exit the shell.