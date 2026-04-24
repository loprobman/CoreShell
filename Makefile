CC     = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700

INCLUDES = \
    -I. \
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
    -Icmd_touch

TARGET = CoreShell

SRC = main.c \
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
      cmd_touch/cmd_touch.c

OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(OBJ) -lm

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

debug: CFLAGS += -g -O0
debug: clean $(TARGET)

clean:
	rm -f $(OBJ) $(TARGET) $(TEST_OBJ) $(TEST_BIN) test_report.md

# ── Test runner ──────────────────────────────────────────────────────── #
LIB_OBJ  = $(filter-out main.o, $(OBJ))
TEST_SRC = tests/test_runner.c
TEST_OBJ = tests/test_runner.o
TEST_BIN = test_runner

test: $(TEST_BIN)
	@printf "Running CoreShell test suite...\n"
	./$(TEST_BIN)

$(TEST_BIN): $(LIB_OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TEST_BIN) $(LIB_OBJ) $(TEST_OBJ) -lm

.PHONY: all clean debug test
