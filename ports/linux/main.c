/*
 * main.c - Linux host: `tl file.py` runs a file, `tl` alone starts a REPL.
 *
 * Implements lang_port.h with stdout (program output) and stderr
 * (errors). Programs run in slices of SLICE instructions, so Ctrl-C can
 * stop a `while True` loop between slices: the same budget mechanism a
 * scheduler uses to keep one task from blocking the others.
 *
 * Exit codes: 0 finished, 1 compile or runtime error (or Ctrl-C),
 * 2 bad arguments or unreadable file.
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "lang.h"
#include "lang_port.h"

#define SLICE       10000u     /* instructions between Ctrl-C checks */
#define SOURCE_MAX  65536      /* bytes of source per file / REPL input */
#define LINE_MAX_   1024       /* bytes per REPL line */

static volatile sig_atomic_t interrupted;

static char source[SOURCE_MAX];
static lang_program_t program;
static lang_names_t names;
static lang_vm_t vm;

void lang_port_write(const char *s)
{
    fputs(s, stdout);
}

void lang_port_error(const char *s)
{
    /* Flush program output first so the two streams appear in the order
     * they were written when both go to the same place. */
    fflush(stdout);
    fputs(s, stderr);
    fflush(stderr);
}

static void on_sigint(int sig)
{
    (void)sig;
    interrupted = 1;
}

static void install_sigint(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;   /* no SA_RESTART: Ctrl-C must interrupt fgets */
    sigaction(SIGINT, &sa, NULL);
}

/* Compiles src and runs it to the end, a slice at a time. Returns true
 * if it finished without an error. */
static bool compile_and_run(const char *src, bool repl)
{
    char err[128];
    lang_status_t st;

    if (!lang_compile(src, &names, repl, &program, err, sizeof err)) {
        lang_port_error(err);
        lang_port_error("\n");
        return false;
    }
    if (repl) {
        lang_vm_load(&vm, &program);
    } else {
        lang_vm_init(&vm, &program);
    }
    interrupted = 0;
    do {
        if (interrupted) {
            interrupted = 0;
            lang_port_error("interrupted\n");
            return false;
        }
        st = lang_vm_run(&vm, SLICE);
    } while (st == LANG_BUDGET);
    fflush(stdout);
    return st == LANG_DONE;
}

static int run_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    size_t n;

    if (f == NULL) {
        fprintf(stderr, "tl: cannot open %s: %s\n", path, strerror(errno));
        return 2;
    }
    n = fread(source, 1, sizeof source, f);
    if (ferror(f) || n == sizeof source) {
        fprintf(stderr, "tl: cannot read %s (too large or read error)\n", path);
        fclose(f);
        return 2;
    }
    fclose(f);
    source[n] = '\0';
    if (strlen(source) != n) {
        fprintf(stderr, "tl: %s contains a NUL byte\n", path);
        return 2;
    }
    return compile_and_run(source, false) ? 0 : 1;
}

/* ---- REPL ---------------------------------------------------------- */

/* Bracket balance of one line, ignoring a comment. */
static int bracket_change(const char *line)
{
    int depth = 0;
    for (; *line != '\0' && *line != '#'; line++) {
        if (*line == '(') {
            depth++;
        } else if (*line == ')') {
            depth--;
        }
    }
    return depth;
}

/* True if the line, without comment and trailing space, ends in ':'. */
static bool opens_block(const char *line)
{
    const char *end = strchr(line, '#');
    if (end == NULL) {
        end = line + strlen(line);
    }
    while (end > line && (end[-1] == ' ' || end[-1] == '\t' ||
                          end[-1] == '\n' || end[-1] == '\r')) {
        end--;
    }
    return end > line && end[-1] == ':';
}

static bool is_blank(const char *line)
{
    for (; *line != '\0'; line++) {
        if (*line != ' ' && *line != '\t' && *line != '\n' && *line != '\r') {
            return false;
        }
    }
    return true;
}

static int repl(void)
{
    char line[LINE_MAX_];
    size_t used = 0;     /* bytes of the current input in source[] */
    int depth = 0;       /* open brackets in the current input */
    bool block = false;  /* current input opened a block */

    lang_vm_init(&vm, NULL);
    for (;;) {
        size_t len;
        bool complete;

        fputs(used == 0 ? ">>> " : "... ", stdout);
        fflush(stdout);
        if (fgets(line, sizeof line, stdin) == NULL) {
            if (interrupted) {
                /* Ctrl-C at the prompt: throw away the current input. */
                interrupted = 0;
                clearerr(stdin);
                used = 0;
                depth = 0;
                block = false;
                fputs("\n", stdout);
                continue;
            }
            /* Ctrl-D / end of input: run what was typed, then leave. */
            if (used > 0) {
                compile_and_run(source, true);
            }
            fputs("\n", stdout);
            return 0;
        }
        len = strlen(line);
        if (used == 0 && is_blank(line)) {
            continue;
        }
        if (used + len >= sizeof source) {
            lang_port_error("input too long\n");
            used = 0;
            depth = 0;
            block = false;
            continue;
        }
        memcpy(source + used, line, len + 1);
        used += len;
        depth += bracket_change(line);
        if (depth == 0 && opens_block(line)) {
            block = true;
        }
        /* An input is complete when no bracket is open, and, if it opened
         * a block, a blank line has ended it. */
        complete = depth <= 0 && (!block || is_blank(line));
        if (complete) {
            compile_and_run(source, true);
            used = 0;
            depth = 0;
            block = false;
        }
    }
}

int main(int argc, char **argv)
{
    install_sigint();
    if (argc == 1) {
        return repl();
    }
    if (argc == 2) {
        return run_file(argv[1]);
    }
    fputs("usage: tl [file.py]\n", stderr);
    return 2;
}
