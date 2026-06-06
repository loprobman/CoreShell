#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int file_contains(const char *path, const char *needle)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;

    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';

    return strstr(buf, needle) != NULL;
}

int main(void)
{
    const char *out = "artifacts/at_query_test.out";
    int rc = system("mkdir -p artifacts");
    if (rc != 0) {
        fprintf(stderr, "[FAIL] unable to create artifacts directory\n");
        return 1;
    }

    rc = system("printf '@where am i\\ny\\nexit\\n' | ./CoreShell > artifacts/at_query_test.out 2>&1");
    if (rc != 0) {
        fprintf(stderr, "[FAIL] CoreShell @query execution failed\n");
        return 1;
    }

    if (!file_contains(out, "Suggested command:")) {
        fprintf(stderr, "[FAIL] missing suggested command output\n");
        return 1;
    }

    if (!file_contains(out, "Run this? (y/n)")) {
        fprintf(stderr, "[FAIL] missing confirmation prompt\n");
        return 1;
    }

    if (!file_contains(out, "pwd") && !file_contains(out, "local-rag-fallback")) {
        fprintf(stderr, "[FAIL] expected @query suggestion to include a pwd-like outcome\n");
        return 1;
    }

    fprintf(stdout, "[PASS] @query helper integration\n");
    return 0;
}
