CC     = gcc
CFLAGS = -Wall -Wextra -g -std=c99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -pthread -Dyylex=grammar_lex -Dyy_scan_string=grammar__scan_string -Dyy_delete_buffer=grammar__delete_buffer -Dyylex_destroy=grammar_lex_destroy -Dyyget_text=grammar_get_text

BNFC = bnfc
BNFC_GRAMMAR = Grammar/Grammar.cf
BNFC_STAMP = .bnfc.stamp
.RECIPEPREFIX = >

INCLUDES = \
    -I. \
  -IAbsyn \
  -IBison \
  -IBuffer \
  -IGrammar \
  -ILexer \
  -IParser \
  -IPrinter \
  -ISkeleton \
    -Iargtable3 \
    -Icmd_spec \
    -Icmd_registry \
    -Icmd_help \
    -Icmd_exit \
    -Icmd_cd \
    -Icmd_pwd \
    -Icmd_echo \
    -Icmd_ls \
    -Icmd_stat \
    -Icmd_cat \
    -Icmd_head \
    -Icmd_tail \
    -Icmd_cp \
    -Icmd_mv \
    -Icmd_rm \
    -Icmd_mkdir \
    -Icmd_rmdir \
    -Icmd_touch \
    -Icmd_pkg \
    -Icmd_jobs \
    -Icmd_kill \
    -Icmd_rpc

TARGET = CoreShell

SRC = main.c \
  Absyn/Absyn.c \
  Buffer/Buffer.c \
  Lexer/Lexer.c \
  Parser/Parser.c \
  Printer/Printer.c \
  Skeleton/Skeleton.c \
      argtable3/argtable3.c \
      cmd_registry/cmd_registry.c \
      cmd_help/cmd_help.c \
      cmd_exit/cmd_exit.c \
      cmd_cd/cmd_cd.c \
      cmd_pwd/cmd_pwd.c \
      cmd_echo/cmd_echo.c \
      cmd_ls/cmd_ls.c \
      cmd_stat/cmd_stat.c \
      cmd_cat/cmd_cat.c \
      cmd_head/cmd_head.c \
      cmd_tail/cmd_tail.c \
      cmd_cp/cmd_cp.c \
      cmd_mv/cmd_mv.c \
      cmd_rm/cmd_rm.c \
      cmd_mkdir/cmd_mkdir.c \
      cmd_rmdir/cmd_rmdir.c \
    cmd_touch/cmd_touch.c \
    cmd_pkg/cmd_pkg.c \
    cmd_jobs/cmd_jobs.c \
    cmd_kill/cmd_kill.c \
    cmd_rpc/cmd_rpc.c \
    pkg/pkg.c

OBJ = $(SRC:.c=.o)

PKG_TARGET = pkg/pkg
PKG_SRC    = pkg/pkg.c

LLM_TARGET = coresh_llm
LLM_SRC    = coresh_llm.c
MCP_TARGET = mcp_server
MCP_SRC    = mcp_server.c

all: $(BNFC_STAMP) $(TARGET) $(PKG_TARGET) $(LLM_TARGET) $(MCP_TARGET)

$(BNFC_STAMP): $(BNFC_GRAMMAR)
>$(BNFC) --c $(BNFC_GRAMMAR)
>mkdir -p Absyn Buffer Bison Grammar Lexer Parser Printer Skeleton Test
>test -f Absyn.c && mv -f Absyn.c Absyn/Absyn.c || true
>test -f Absyn.h && mv -f Absyn.h Absyn/Absyn.h || true
>test -f Buffer.c && mv -f Buffer.c Buffer/Buffer.c || true
>test -f Buffer.h && mv -f Buffer.h Buffer/Buffer.h || true
>test -f Grammar.l && mv -f Grammar.l Grammar/Grammar.l || true
>test -f Grammar.y && mv -f Grammar.y Grammar/Grammar.y || true
>test -f Bison.h && mv -f Bison.h Bison/Bison.h || true
>test -f Lexer.c && mv -f Lexer.c Lexer/Lexer.c || true
>test -f Parser.c && mv -f Parser.c Parser/Parser.c || true
>test -f Parser.h && mv -f Parser.h Parser/Parser.h || true
>test -f Printer.c && mv -f Printer.c Printer/Printer.c || true
>test -f Printer.h && mv -f Printer.h Printer/Printer.h || true
>test -f Skeleton.c && mv -f Skeleton.c Skeleton/Skeleton.c || true
>test -f Skeleton.h && mv -f Skeleton.h Skeleton/Skeleton.h || true
>test -f Test.c && mv -f Test.c Test/Test.c || true
>touch $(BNFC_STAMP)

Parser/Parser.c Bison/Bison.h: Grammar/Grammar.y
>bison -d -o Parser/Parser.c Grammar/Grammar.y
>test -f Bison.h && mv -f Bison.h Bison/Bison.h || true

Lexer/Lexer.c: Grammar/Grammar.l Bison/Bison.h
>flex -o Lexer/Lexer.c Grammar/Grammar.l

$(PKG_TARGET): $(PKG_SRC)
>$(CC) $(CFLAGS) -o $(PKG_TARGET) $(PKG_SRC)

$(LLM_TARGET): $(LLM_SRC)
>$(CC) $(CFLAGS) -o $(LLM_TARGET) $(LLM_SRC)

$(MCP_TARGET): $(MCP_SRC)
>$(CC) $(CFLAGS) -o $(MCP_TARGET) $(MCP_SRC)

pkg/pkg.o: CFLAGS += -DPKG_NO_MAIN

$(TARGET): $(BNFC_STAMP) $(OBJ)
>$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(OBJ) -lm

%.o: %.c
>$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

debug: CFLAGS += -g -O0
debug: clean $(TARGET)

clean:
>rm -f $(OBJ) $(TARGET) $(PKG_TARGET) $(LLM_TARGET) $(MCP_TARGET) $(TEST_OBJ) $(TEST_BIN) $(MCP_C_TEST_BIN) test_report.md test_output.log $(BNFC_STAMP)
>rm -rf bin/ build/

# ── Test runner ──────────────────────────────────────────────────────── #
LIB_OBJ  = $(filter-out main.o, $(OBJ))
TEST_SRC = tests/test_runner.c
TEST_OBJ = tests/test_runner.o
TEST_BIN = test_runner

MCP_C_TEST_SRC = tests/mcp_server_c_test.c
MCP_C_TEST_BIN = tests/mcp_server_c_test
AT_QUERY_TEST_SRC = tests/at_query_test.c
AT_QUERY_TEST_BIN = tests/at_query_test

test: $(TARGET) $(TEST_BIN) $(AT_QUERY_TEST_BIN)
>@printf "Running CoreShell test suite...\n"
>./$(TEST_BIN)
>@printf "Running @query integration test...\n"
>./$(AT_QUERY_TEST_BIN)

test-mcp-c: $(MCP_TARGET) $(MCP_C_TEST_BIN)
>@printf "Running native C MCP server test suite...\n"
>./$(MCP_C_TEST_BIN)

$(TEST_BIN): $(BNFC_STAMP) $(LIB_OBJ) $(TEST_OBJ)
>$(CC) $(CFLAGS) $(INCLUDES) -o $(TEST_BIN) $(LIB_OBJ) $(TEST_OBJ) -lm

$(MCP_C_TEST_BIN): $(MCP_C_TEST_SRC)
>$(CC) $(CFLAGS) $(INCLUDES) -o $(MCP_C_TEST_BIN) $(MCP_C_TEST_SRC)

$(AT_QUERY_TEST_BIN): $(AT_QUERY_TEST_SRC)
>$(CC) $(CFLAGS) $(INCLUDES) -o $(AT_QUERY_TEST_BIN) $(AT_QUERY_TEST_SRC)

.PHONY: all clean debug test test-mcp-c pkg
