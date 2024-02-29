/* linenoise.c -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * You can find the latest source code at:
 *
 *   http://github.com/antirez/linenoise
 *
 * Does a number of crazy assumptions that happen to be true in 99.9999% of
 * the 2010 UNIX computers around.
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2010-2016, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2013, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ------------------------------------------------------------------------
 *
 * References:
 * - http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * - http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html
 *
 * Todo list:
 * - Filter bogus Ctrl+<char> combinations.
 * - Win32 support
 *
 * Bloat:
 * - History search like Ctrl+r in readline?
 *
 * List of escape sequences used by this program, we do everything just
 * with three sequences. In order to be so cheap we may have some
 * flickering effect with some slow terminal, but the lesser sequences
 * the more compatible.
 *
 * EL (Erase Line)
 *    Sequence: ESC [ n K
 *    Effect: if n is 0 or missing, clear from cursor to end of line
 *    Effect: if n is 1, clear from beginning of line to cursor
 *    Effect: if n is 2, clear entire line
 *
 * CUF (CUrsor Forward)
 *    Sequence: ESC [ n C
 *    Effect: moves cursor forward n chars
 *
 * CUB (CUrsor Backward)
 *    Sequence: ESC [ n D
 *    Effect: moves cursor backward n chars
 *
 * The following is used to get the terminal width if getting
 * the width with the TIOCGWINSZ ioctl fails
 *
 * DSR (Device Status Report)
 *    Sequence: ESC [ 6 n
 *    Effect: reports the current cusor position as ESC [ n ; m R
 *            where n is the row and m is the column
 *
 * When multi line mode is enabled, we also use an additional escape
 * sequence. However multi line editing is disabled by default.
 *
 * CUU (Cursor Up)
 *    Sequence: ESC [ n A
 *    Effect: moves cursor up of n chars.
 *
 * CUD (Cursor Down)
 *    Sequence: ESC [ n B
 *    Effect: moves cursor down of n chars.
 *
 * When line_clear_screen() is called, two additional escape sequences
 * are used in order to clear the screen and position the cursor at home
 * position.
 *
 * CUP (Cursor position)
 *    Sequence: ESC [ H
 *    Effect: moves the cursor to upper left corner
 *
 * ED (Erase display)
 *    Sequence: ESC [ 2 J
 *    Effect: clear the whole screen
 *
 */

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "linenoise.h"

#define LINENOISE_DEFAULT_HISTORY_MAX_LEN 100
#define LINENOISE_MAX_LINE 4096

static char *unsupported_term[] = {"dumb", "cons25", "emacs", NULL};
static line_completion_callback_t *completion_callback = NULL;
static line_hints_callback_t *hints_callback = NULL;
static line_free_hints_callback_t *free_hints_callback = NULL;
static line_eventmux_callback_t *eventmux_callback = NULL;

static struct termios orig_termios; /* In order to restore at exit.*/
static bool maskmode = false; /* Show "***" instead of input. For passwords. */
static bool rawmode =
    false; /* For atexit() function to check if restore is needed*/
static bool mlmode = false; /* Multi line mode. Default is single line. */
static bool atexit_registered = false; /* Register atexit just 1 time. */
static int history_max_len = LINENOISE_DEFAULT_HISTORY_MAX_LEN;
static int history_len = 0;
static char **history = NULL;

/* The line_state structure represents the state during line editing.
 * We pass this state to functions implementing specific editing
 * functionalities.
 */
struct line_state {
    int ifd;            /* Terminal stdin file descriptor. */
    int ofd;            /* Terminal stdout file descriptor. */
    char *buf;          /* Edited line buffer. */
    size_t buflen;      /* Edited line buffer size. */
    const char *prompt; /* Prompt to display. */
    size_t plen;        /* Prompt length. */
    size_t pos;         /* Current cursor position. */
    size_t oldpos;      /* Previous refresh cursor position. */
    size_t len;         /* Current edited line length. */
    size_t cols;        /* Number of columns in terminal. */
    size_t maxrows;     /* Maximum num of rows used so far (multiline mode) */
    int history_index;  /* The history index we are currently editing. */
};

enum KEY_ACTION {
    KEY_NULL = 0,   /* NULL */
    CTRL_A = 1,     /* Ctrl+a */
    CTRL_B = 2,     /* Ctrl-b */
    CTRL_C = 3,     /* Ctrl-c */
    CTRL_D = 4,     /* Ctrl-d */
    CTRL_E = 5,     /* Ctrl-e */
    CTRL_F = 6,     /* Ctrl-f */
    CTRL_H = 8,     /* Ctrl-h */
    TAB = 9,        /* Tab */
    CTRL_K = 11,    /* Ctrl+k */
    CTRL_L = 12,    /* Ctrl+l */
    ENTER = 13,     /* Enter */
    CTRL_N = 14,    /* Ctrl-n */
    CTRL_P = 16,    /* Ctrl-p */
    CTRL_T = 20,    /* Ctrl-t */
    CTRL_U = 21,    /* Ctrl+u */
    CTRL_W = 23,    /* Ctrl+w */
    ESC = 27,       /* Escape */
    BACKSPACE = 127 /* Backspace */
};

static void line_atexit(void);
int line_history_add(const char *line);
static void refresh_line(struct line_state *l);

/* Debugging macro. */
#if 0
FILE *lndebug_fp = NULL;
#define lndebug(...)                                                           \
    do {                                                                       \
        if (lndebug_fp == NULL) {                                              \
            lndebug_fp = fopen("/tmp/lndebug.txt", "a");                       \
            fprintf(                                                           \
                lndebug_fp,                                                    \
                "[%d %d %d] p: %d, rows: %d, rpos: %d, max: %d, oldmax: %d\n", \
                (int) l->len, (int) l->pos, (int) l->oldpos, plen, rows, rpos, \
                (int) l->maxrows, old_rows);                                   \
        }                                                                      \
        fprintf(lndebug_fp, ", " __VA_ARGS__);                                 \
        fflush(lndebug_fp);                                                    \
    } while (0)
#else
#define lndebug(fmt, ...)
#endif

/* ======================= Low level terminal handling ====================== */

/* Enable "mask mode". When it is enabled, instead of the input that
 * the user is typing, the terminal will just display a corresponding
 * number of asterisks, like "****". This is useful for passwords and other
 * secrets that should not be displayed.
 */
void line_mask_mode_enable(void)
{
    maskmode = true;
}

/* Disable mask mode. */
void line_mask_mode_disable(void)
{
    maskmode = false;
}

/* Set if to use or not the multi line mode. */
void line_set_multi_line(int ml)
{
    mlmode = (bool) ml;
}

/* Return true if the terminal name is in the list of terminals we know are
 * not able to understand basic escape sequences. */
static bool is_unsupported_term(void)
{
    char *term = getenv("TERM");
    if (!term)
        return false;

    for (int j = 0; unsupported_term[j]; j++)
        if (!strcasecmp(term, unsupported_term[j]))
            return true;
    return false;
}

/* Raw mode: 1960 magic shit. */
static int enable_raw_mode(int fd)
{
    if (!isatty(STDIN_FILENO))
        goto fatal;
    if (tcgetattr(fd, &orig_termios) == -1)
        goto fatal;

    struct termios raw;
    raw = orig_termios; /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control.
     */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    raw.c_oflag &= ~(OPOST);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - choing off, canonical off, no extended functions,
     * no signal chars (^Z,^C)
     */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer.
     * We want read to return every single byte, without timeout.
     */
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

    /* put terminal in raw mode after flushing */
    if (tcsetattr(fd, TCSAFLUSH, &raw) < 0)
        goto fatal;
    rawmode = true;
    return 0;

fatal:
    errno = ENOTTY;
    return -1;
}

static void disable_raw_mode(int fd)
{
    /* Don't even check the return value as it's too late. */
    if (rawmode && tcsetattr(fd, TCSAFLUSH, &orig_termios) != -1)
        rawmode = false;
}

/* Use the ESC [6n escape sequence to query the horizontal cursor position
 * and return it. On error -1 is returned, on success the position of the
 * cursor.
 */
static int get_cursor_position(int ifd, int ofd)
{
    char buf[32];
    int cols, rows;

    /* Report cursor location */
    if (write(ofd, "\x1b[6n", 4) != 4)
        return -1;

    unsigned int i = 0;
    /* Read the response: ESC [ rows ; cols R */
    while (i < sizeof(buf) - 1) {
        if (read(ifd, buf + i, 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';

    /* Parse it. */
    if (buf[0] != ESC || buf[1] != '[')
        return -1;
    if (sscanf(buf + 2, "%d;%d", &rows, &cols) != 2)
        return -1;
    return cols;
}

/* Try to get the number of columns in the current terminal, or assume 80
 * if it fails.
 */
static int get_columns(int ifd, int ofd)
{
    struct winsize ws;

    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        /* ioctl() failed. Try to query the terminal itself. */
        int start, cols;

        /* Get the initial position so we can restore it later. */
        start = get_cursor_position(ifd, ofd);
        if (start == -1)
            goto failed;

        /* Go to right margin and get position. */
        if (write(ofd, "\x1b[999C", 6) != 6)
            goto failed;
        cols = get_cursor_position(ifd, ofd);
        if (cols == -1)
            goto failed;

        /* Restore position. */
        if (cols > start) {
            char seq[32];
            snprintf(seq, 32, "\x1b[%dD", cols - start);
            if (write(ofd, seq, strlen(seq)) == -1) {
                /* Can't recover... */
            }
        }
        return cols;
    }
    return ws.ws_col;

failed:
    return 80;
}

/* Clear the screen. Used to handle ctrl+l */
void line_clear_screen(void)
{
    if (write(STDOUT_FILENO, "\x1b[H\x1b[2J", 7) <= 0) {
        /* nothing to do, just to avoid warning. */
    }
}

/* Beep, used for completion when there is nothing to complete or when all
 * the choices were already shown.
 */
static void line_beep(void)
{
    fprintf(stderr, "\x7");
    fflush(stderr);
}

/* ============================== Completion ================================ */

/* Free a list of completion option populated by line_add_completion(). */
static void free_completions(line_completions_t *lc)
{
    for (size_t i = 0; i < lc->len; i++)
        free(lc->cvec[i]);
    free(lc->cvec);
}

/* This is an helper function for line_edit() and is called when the
 * user types the <tab> key in order to complete the string currently in the
 * input.
 *
 * The state of the editing is encapsulated into the pointed line_state
 * structure as described in the structure definition.
 */
static int complete_line(struct line_state *ls)
{
    line_completions_t lc = {0, NULL};
    char c = 0;

    completion_callback(ls->buf, &lc);
    if (lc.len == 0) {
        line_beep();
    } else {
        bool stop = false;
        size_t i = 0;

        while (!stop) {
            /* Show completion or original buffer */
            if (i < lc.len) {
                struct line_state saved = *ls;

                ls->len = ls->pos = strlen(lc.cvec[i]);
                ls->buf = lc.cvec[i];
                refresh_line(ls);
                ls->len = saved.len;
                ls->pos = saved.pos;
                ls->buf = saved.buf;
            } else {
                refresh_line(ls);
            }

            int nread = read(ls->ifd, &c, 1);
            if (nread <= 0) {
                free_completions(&lc);
                return -1;
            }

            switch (c) {
            case 9: /* tab */
                i = (i + 1) % (lc.len + 1);
                if (i == lc.len)
                    line_beep();
                break;
            case 27: /* escape */
                /* Re-show original buffer */
                if (i < lc.len)
                    refresh_line(ls);
                stop = true;
                break;
            default:
                /* Update buffer and return */
                if (i < lc.len) {
                    int nwritten =
                        snprintf(ls->buf, ls->buflen, "%s", lc.cvec[i]);
                    ls->len = ls->pos = nwritten;
                }
                stop = true;
                break;
            }
        }
    }

    free_completions(&lc);
    return c; /* Return last read character */
}

/* Register a callback function to be called for tab-completion. */
void line_set_completion_callback(line_completion_callback_t *fn)
{
    completion_callback = fn;
}

/* Register a hits function to be called to show hits to the user at the
 * right of the prompt. */
void line_set_hints_callback(line_hints_callback_t *fn)
{
    hints_callback = fn;
}

/* Register a function to free the hints returned by the hints callback
 * registered with line_set_hints_callback().
 */
void line_set_free_hints_callback(line_free_hints_callback_t *fn)
{
    free_hints_callback = fn;
}

/* Register a function to perform I/O multiplexing to monitor multiple file
 * descriptor from different input at the same time, so we can allow the ability
 * of receiving commands from different input sources and still provides the
 * command-line auto-complete feature of this package. For example, the main
 * loop of this package can only deal with standard input file descriptor
 * originally. When this callback function is invoked, it allows the main loop
 * of this package to deal with multiple file descriptors from different input
 * alongside with the origin feature to deal with the standard input.
 */
void line_set_eventmux_callback(line_eventmux_callback_t *fn)
{
    eventmux_callback = fn;
}

/* This function is used by the callback function registered by the user
 * in order to add completion options given the input string when the
 * user typed <tab>.
 */
void line_add_completion(line_completions_t *lc, const char *str)
{
    size_t len = strlen(str);

    char *copy = malloc(len + 1);
    if (!copy)
        return;
    memcpy(copy, str, len + 1);
    char **cvec = realloc(lc->cvec, sizeof(char *) * (lc->len + 1));
    if (cvec == NULL) {
        free(copy);
        return;
    }
    lc->cvec = cvec;
    lc->cvec[lc->len++] = copy;
}

/* =========================== Line editing ================================= */

/* We define a very simple "append buffer" structure, that is an heap
 * allocated string where we can append to. This is useful in order to
 * write all the escape sequences in a buffer and flush them to the standard
 * output in a single call, to avoid flickering effects.
 */
struct abuf {
    char *b;
    int len;
};

static void ab_init(struct abuf *ab)
{
    ab->b = NULL;
    ab->len = 0;
}

static void ab_append(struct abuf *ab, const char *s, int len)
{
    char *new = realloc(ab->b, ab->len + len);
    if (!new)
        return;

    memcpy(new + ab->len, s, len);
    ab->b = new;
    ab->len += len;
}

static inline void ab_free(struct abuf *ab)
{
    free(ab->b);
}

/* Helper of refresh_single_line() and refresh_multi_Line() to show hints
 * to the right of the prompt.
 */
void refresh_show_hints(struct abuf *ab, struct line_state *l, int plen)
{
    if (hints_callback && plen + l->len < l->cols) {
        int color = -1, bold = 0;
        char *hint = hints_callback(l->buf, &color, &bold);
        if (hint) {
            char seq[64];
            int hintlen = strlen(hint);
            int hintmaxlen = l->cols - (plen + l->len);
            if (hintlen > hintmaxlen)
                hintlen = hintmaxlen;
            if (bold == 1 && color == -1)
                color = 37;
            if (color != -1 || bold != 0)
                snprintf(seq, 64, "\033[%d;%d;49m", bold, color);
            else
                seq[0] = '\0';
            ab_append(ab, seq, strlen(seq));
            ab_append(ab, hint, hintlen);
            if (color != -1 || bold != 0)
                ab_append(ab, "\033[0m", 4);
            /* Call the function to free the hint returned. */
            if (free_hints_callback)
                free_hints_callback(hint);
        }
    }
}

/* Single line low level line refresh.
 *
 * Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal.
 */
static void refresh_single_line(struct line_state *l)
{
    char seq[64];
    size_t plen = strlen(l->prompt);
    int fd = l->ofd;
    char *buf = l->buf;
    size_t len = l->len;
    size_t pos = l->pos;
    struct abuf ab;

    while ((plen + pos) >= l->cols) {
        buf++;
        len--;
        pos--;
    }
    while (plen + len > l->cols)
        len--;

    ab_init(&ab);
    /* Cursor to left edge */
    snprintf(seq, 64, "\r");
    ab_append(&ab, seq, strlen(seq));
    /* Write the prompt and the current buffer content */
    ab_append(&ab, l->prompt, strlen(l->prompt));
    if (maskmode) {
        while (len--)
            ab_append(&ab, "*", 1);
    } else {
        ab_append(&ab, buf, len);
    }
    /* Show hits if any. */
    refresh_show_hints(&ab, l, plen);
    /* Erase to right */
    snprintf(seq, 64, "\x1b[0K");
    ab_append(&ab, seq, strlen(seq));
    /* Move cursor to original position. */
    snprintf(seq, 64, "\r\x1b[%dC", (int) (pos + plen));
    ab_append(&ab, seq, strlen(seq));
    if (write(fd, ab.b, ab.len) == -1) {
    } /* Can't recover from write error. */
    ab_free(&ab);
}

/* Multi line low level line refresh.
 *
 * Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal.
 */
static void refresh_multi_Line(struct line_state *l)
{
    char seq[64];
    int plen = strlen(l->prompt);
    int rows =
        (plen + l->len + l->cols - 1) / l->cols; /* rows used by current buf. */
    int rpos =
        (plen + l->oldpos + l->cols) / l->cols; /* cursor relative row. */
    int rpos2;                                  /* rpos after refresh. */
    int col; /* colum position, zero-based. */
    int old_rows = l->maxrows;
    int fd = l->ofd;
    struct abuf ab;

    /* Update maxrows if needed. */
    if (rows > (int) l->maxrows)
        l->maxrows = rows;

    /* First step: clear all the lines used before. To do so start by
     * going to the last row. */
    ab_init(&ab);
    if (old_rows - rpos > 0) {
        lndebug("go down %d", old_rows - rpos);
        snprintf(seq, 64, "\x1b[%dB", old_rows - rpos);
        ab_append(&ab, seq, strlen(seq));
    }

    /* Now for every row clear it, go up. */
    for (int j = 0; j < old_rows - 1; j++) {
        lndebug("clear+up");
        snprintf(seq, 64, "\r\x1b[0K\x1b[1A");
        ab_append(&ab, seq, strlen(seq));
    }

    /* Clean the top line. */
    lndebug("clear");
    snprintf(seq, 64, "\r\x1b[0K");
    ab_append(&ab, seq, strlen(seq));

    /* Write the prompt and the current buffer content */
    ab_append(&ab, l->prompt, strlen(l->prompt));
    if (maskmode) {
        for (unsigned int i = 0; i < l->len; i++)
            ab_append(&ab, "*", 1);
    } else {
        ab_append(&ab, l->buf, l->len);
    }

    /* Show hits if any. */
    refresh_show_hints(&ab, l, plen);

    /* If we are at the very end of the screen with our prompt, we need to
     * emit a newline and move the prompt to the first column.
     */
    if (l->pos && l->pos == l->len && (l->pos + plen) % l->cols == 0) {
        lndebug("<newline>");
        ab_append(&ab, "\n", 1);
        snprintf(seq, 64, "\r");
        ab_append(&ab, seq, strlen(seq));
        rows++;
        if (rows > (int) l->maxrows)
            l->maxrows = rows;
    }

    /* Move cursor to right position. */
    rpos2 =
        (plen + l->pos + l->cols) / l->cols; /* current cursor relative row. */
    lndebug("rpos2 %d", rpos2);

    /* Go up till we reach the expected positon. */
    if (rows - rpos2 > 0) {
        lndebug("go-up %d", rows - rpos2);
        snprintf(seq, 64, "\x1b[%dA", rows - rpos2);
        ab_append(&ab, seq, strlen(seq));
    }

    /* Set column. */
    col = (plen + (int) l->pos) % (int) l->cols;
    lndebug("set col %d", 1 + col);
    if (col)
        snprintf(seq, 64, "\r\x1b[%dC", col);
    else
        snprintf(seq, 64, "\r");
    ab_append(&ab, seq, strlen(seq));

    lndebug("\n");
    l->oldpos = l->pos;

    if (write(fd, ab.b, ab.len) == -1) {
    } /* Can't recover from write error. */
    ab_free(&ab);
}

/* Calls the two low level functions refresh_single_line() or
 * refresh_multi_Line() according to the selected mode.
 */
static void refresh_line(struct line_state *l)
{
    if (mlmode)
        refresh_multi_Line(l);
    else
        refresh_single_line(l);
}

/* Insert the character 'c' at cursor current position.
 *
 * On error writing to the terminal -1 is returned, otherwise 0.
 */
int line_edit_insert(struct line_state *l, char c)
{
    if (l->len < l->buflen) {
        if (l->len == l->pos) {
            l->buf[l->pos] = c;
            l->pos++;
            l->len++;
            l->buf[l->len] = '\0';
            if ((!mlmode && l->plen + l->len < l->cols && !hints_callback)) {
                /* Avoid a full update of the line in the
                 * trivial case. */
                char d = maskmode ? '*' : c;
                if (write(l->ofd, &d, 1) == -1)
                    return -1;
            } else {
                refresh_line(l);
            }
        } else {
            memmove(l->buf + l->pos + 1, l->buf + l->pos, l->len - l->pos);
            l->buf[l->pos] = c;
            l->len++;
            l->pos++;
            l->buf[l->len] = '\0';
            refresh_line(l);
        }
    }
    return 0;
}

/* Move cursor on the left. */
void line_edit_move_left(struct line_state *l)
{
    if (l->pos > 0) {
        l->pos--;
        refresh_line(l);
    }
}

/* Move cursor on the right. */
void line_edit_move_right(struct line_state *l)
{
    if (l->pos != l->len) {
        l->pos++;
        refresh_line(l);
    }
}

/* Move cursor to the start of the line. */
void line_edit_move_home(struct line_state *l)
{
    if (l->pos != 0) {
        l->pos = 0;
        refresh_line(l);
    }
}

/* Move cursor to the end of the line. */
void line_edit_move_end(struct line_state *l)
{
    if (l->pos != l->len) {
        l->pos = l->len;
        refresh_line(l);
    }
}

/* Substitute the currently edited line with the next or previous history
 * entry as specified by 'dir'.
 */
enum { LINENOISE_HISTORY_NEXT = 0, LINENOISE_HISTORY_PREV = 1 };
void line_edit_history_next(struct line_state *l, int dir)
{
    if (history_len > 1) {
        /* Update the current history entry before to
         * overwrite it with the next one. */
        free(history[history_len - 1 - l->history_index]);
        history[history_len - 1 - l->history_index] = strdup(l->buf);
        /* Show the new entry */
        l->history_index += (dir == LINENOISE_HISTORY_PREV) ? 1 : -1;
        if (l->history_index < 0) {
            l->history_index = 0;
            return;
        } else if (l->history_index >= history_len) {
            l->history_index = history_len - 1;
            return;
        }
        strncpy(l->buf, history[history_len - 1 - l->history_index], l->buflen);
        l->buf[l->buflen - 1] = '\0';
        l->len = l->pos = strlen(l->buf);
        refresh_line(l);
    }
}

/* Delete the character at the right of the cursor without altering the cursor
 * position. Basically this is what happens with the "Delete" keyboard key.
 */
void line_edit_delete(struct line_state *l)
{
    if (l->len > 0 && l->pos < l->len) {
        memmove(l->buf + l->pos, l->buf + l->pos + 1, l->len - l->pos - 1);
        l->len--;
        l->buf[l->len] = '\0';
        refresh_line(l);
    }
}

/* Backspace implementation. */
void line_edit_backspace(struct line_state *l)
{
    if (l->pos > 0 && l->len > 0) {
        memmove(l->buf + l->pos - 1, l->buf + l->pos, l->len - l->pos);
        l->pos--;
        l->len--;
        l->buf[l->len] = '\0';
        refresh_line(l);
    }
}

/* Delete the previous word, maintaining the cursor at the start of the
 * current word.
 */
void line_edit_delete_prev_word(struct line_state *l)
{
    size_t old_pos = l->pos;

    while (l->pos > 0 && l->buf[l->pos - 1] == ' ')
        l->pos--;
    while (l->pos > 0 && l->buf[l->pos - 1] != ' ')
        l->pos--;
    size_t diff = old_pos - l->pos;
    memmove(l->buf + l->pos, l->buf + old_pos, l->len - old_pos + 1);
    l->len -= diff;
    refresh_line(l);
}

/* Jump to the begining of the previous word */
static void line_edit_prev_word(struct line_state *l)
{
    while (l->pos > 0 && l->buf[l->pos - 1] == ' ')
        l->pos--;
    while (l->pos > 0 && l->buf[l->pos - 1] != ' ')
        l->pos--;
    refresh_line(l);
}

/* Jump to the space after the current word */
static void line_edit_next_word(struct line_state *l)
{
    while (l->pos < l->len) {
        if (l->buf[l->pos] != ' ')
            break;
        l->pos++;
    }

    while (l->pos < l->len) {
        if (l->buf[l->pos] == ' ')
            break;
        if (l->buf[l->pos] == '\0')
            break;

        l->pos++;
    }

    refresh_line(l);
}

/* This function is the core of the line editing capability of linenoise.
 * It expects 'fd' to be already in "raw mode" so that every key pressed
 * will be returned ASAP to read().
 *
 * The resulting string is put into 'buf' when the user type enter, or
 * when ctrl+d is typed.
 *
 * The function returns the length of the current buffer.
 */
static int line_edit(int stdin_fd,
                     int stdout_fd,
                     char *buf,
                     size_t buflen,
                     const char *prompt)
{
    struct line_state l;

    /* Populate the linenoise state that we pass to functions implementing
     * specific editing functionalities.
     */
    l.ifd = stdin_fd;
    l.ofd = stdout_fd;
    l.buf = buf;
    l.buflen = buflen;
    l.prompt = prompt;
    l.plen = strlen(prompt);
    l.oldpos = l.pos = 0;
    l.len = 0;
    l.cols = get_columns(stdin_fd, stdout_fd);
    l.maxrows = 0;
    l.history_index = 0;

    /* Buffer starts empty. */
    l.buf[0] = '\0';
    l.buflen--; /* Make sure there is always space for the nulterm */

    /* The latest history entry is always our current buffer, that
     * initially is just an empty string.
     */
    line_history_add("");

    if (write(l.ofd, prompt, l.plen) == -1)
        return -1;
    while (1) {
        signed char c;
        int nread;
        char seq[5];

        if (eventmux_callback != NULL) {
            int result = eventmux_callback(l.buf);
            if (result != 0)
                return result;
        }

        nread = read(l.ifd, &c, 1);
        if (nread <= 0)
            return l.len;

        /* Only autocomplete when the callback is set. It returns < 0 when
         * there was an error reading from fd. Otherwise it will return the
         * character that should be handled next.
         */
        if (c == 9 && completion_callback != NULL) {
            c = complete_line(&l);
            /* Return on errors */
            if (c < 0)
                return l.len;
            /* Read next character when 0 */
            if (c == 0)
                continue;
        }

        switch (c) {
        case ENTER: /* enter */
            history_len--;
            free(history[history_len]);
            if (mlmode)
                line_edit_move_end(&l);
            if (hints_callback) {
                /* Force a refresh without hints to leave the previous
                 * line as the user typed it after a newline.
                 */
                line_hints_callback_t *hc = hints_callback;
                hints_callback = NULL;
                refresh_line(&l);
                hints_callback = hc;
            }
            return (int) l.len;
        case CTRL_C: /* ctrl-c */
            errno = EAGAIN;
            return -1;
        case BACKSPACE: /* backspace */
        case 8:         /* ctrl-h */
            line_edit_backspace(&l);
            break;
        case CTRL_D: /* ctrl-d, remove char at right of cursor, or if the line
                      * is empty, act as end-of-file.
                      */
            if (l.len > 0) {
                line_edit_delete(&l);
            } else {
                history_len--;
                free(history[history_len]);
                return -1;
            }
            break;
        case CTRL_T: /* ctrl-t, swaps current character with previous. */
            if (l.pos > 0 && l.pos < l.len) {
                int aux = buf[l.pos - 1];
                buf[l.pos - 1] = buf[l.pos];
                buf[l.pos] = aux;
                if (l.pos != l.len - 1)
                    l.pos++;
                refresh_line(&l);
            }
            break;
        case CTRL_B: /* ctrl-b */
            line_edit_move_left(&l);
            break;
        case CTRL_F: /* ctrl-f */
            line_edit_move_right(&l);
            break;
        case CTRL_P: /* ctrl-p */
            line_edit_history_next(&l, LINENOISE_HISTORY_PREV);
            break;
        case CTRL_N: /* ctrl-n */
            line_edit_history_next(&l, LINENOISE_HISTORY_NEXT);
            break;
        case ESC: /* escape sequence */
            /* Read the next two bytes representing the escape sequence.
             * Use two calls to handle slow terminals returning the two
             * chars at different times.
             */
            if (read(l.ifd, seq, 1) == -1)
                break;
            if (read(l.ifd, seq + 1, 1) == -1)
                break;

            /* ESC [ sequences. */
            if (seq[0] == '[') {
                if (seq[1] >= '0' && seq[1] <= '9') {
                    /* Extended escape, read additional byte. */
                    if (read(l.ifd, seq + 2, 1) == -1)
                        break;
                    switch (seq[2]) {
                    case '~':
                        switch (seq[1]) {
                        case '3': /* Delete key. */
                            line_edit_delete(&l);
                            break;
                        }
                        break;

                    case ';':
                        /* Even more extended escape, read additional 2 bytes */
                        if (read(l.ifd, seq + 3, 1) == -1)
                            break;
                        if (read(l.ifd, seq + 4, 1) == -1)
                            break;
                        if (seq[3] == '5') {
                            switch (seq[4]) {
                            case 'D': /* Ctrl Left */
                                line_edit_prev_word(&l);
                                break;
                            case 'C': /* Ctrl Right */
                                line_edit_next_word(&l);
                                break;
                            }
                        }
                        break;
                    }
                } else {
                    switch (seq[1]) {
                    case 'A': /* Up */
                        line_edit_history_next(&l, LINENOISE_HISTORY_PREV);
                        break;
                    case 'B': /* Down */
                        line_edit_history_next(&l, LINENOISE_HISTORY_NEXT);
                        break;
                    case 'C': /* Right */
                        line_edit_move_right(&l);
                        break;
                    case 'D': /* Left */
                        line_edit_move_left(&l);
                        break;
                    case 'H': /* Home */
                        line_edit_move_home(&l);
                        break;
                    case 'F': /* End*/
                        line_edit_move_end(&l);
                        break;
                    }
                }
            }

            /* ESC O sequences. */
            else if (seq[0] == 'O') {
                switch (seq[1]) {
                case 'H': /* Home */
                    line_edit_move_home(&l);
                    break;
                case 'F': /* End*/
                    line_edit_move_end(&l);
                    break;
                }
            }
            break;
        default:
            if (line_edit_insert(&l, c))
                return -1;
            break;
        case CTRL_U: /* Ctrl+u, delete the whole line. */
            buf[0] = '\0';
            l.pos = l.len = 0;
            refresh_line(&l);
            break;
        case CTRL_K: /* Ctrl+k, delete from current to end of line. */
            buf[l.pos] = '\0';
            l.len = l.pos;
            refresh_line(&l);
            break;
        case CTRL_A: /* Ctrl+a, go to the start of the line */
            line_edit_move_home(&l);
            break;
        case CTRL_E: /* ctrl+e, go to the end of the line */
            line_edit_move_end(&l);
            break;
        case CTRL_L: /* ctrl+l, clear screen */
            line_clear_screen();
            refresh_line(&l);
            break;
        case CTRL_W: /* ctrl+w, delete previous word */
            line_edit_delete_prev_word(&l);
            break;
        }
    }
    return l.len;
}

/* This function calls the line editing function line_edit() using
 * the STDIN file descriptor set in raw mode.
 */
static int line_raw(char *buf, size_t buflen, const char *prompt)
{
    if (buflen == 0) {
        errno = EINVAL;
        return -1;
    }

    if (enable_raw_mode(STDIN_FILENO) == -1)
        return -1;
    int count = line_edit(STDIN_FILENO, STDOUT_FILENO, buf, buflen, prompt);
    disable_raw_mode(STDIN_FILENO);
    printf("\n");
    return count;
}

/* This function is called when linenoise() is called with the standard
 * input file descriptor not attached to a TTY. So for example when the
 * program using linenoise is called in pipe or with a file redirected
 * to its standard input. In this case, we want to be able to return the
 * line regardless of its length (by default we are limited to 4k).
 */
static char *line_no_tty(void)
{
    char *line = NULL;
    size_t len = 0, maxlen = 0;

    while (1) {
        if (len == maxlen) {
            if (maxlen == 0)
                maxlen = 16;
            maxlen *= 2;
            char *oldval = line;
            line = realloc(line, maxlen);
            if (line == NULL) {
                if (oldval)
                    free(oldval);
                return NULL;
            }
        }
        int c = fgetc(stdin);
        if (c == EOF || c == '\n') {
            if (c == EOF && len == 0) {
                free(line);
                return NULL;
            } else {
                line[len] = '\0';
                return line;
            }
        } else {
            line[len] = c;
            len++;
        }
    }
}

/* The high level function that is the main API of the linenoise library.
 * This function checks if the terminal has basic capabilities, just checking
 * for a blacklist of stupid terminals, and later either calls the line
 * editing function or uses dummy fgets() so that you will be able to type
 * something even in the most desperate of the conditions.
 */
char *linenoise(const char *prompt)
{
    char buf[LINENOISE_MAX_LINE];

    if (!atexit_registered) {
        atexit(line_atexit);
        atexit_registered = true;
    }

    if (!isatty(STDIN_FILENO)) {
        /* Not a tty: read from file / pipe. In this mode we don't want any
         * limit to the line size, so we call a function to handle that. */
        return line_no_tty();
    } else if (is_unsupported_term()) {
        size_t len;

        printf("%s", prompt);
        fflush(stdout);
        if (!fgets(buf, LINENOISE_MAX_LINE, stdin))
            return NULL;
        len = strlen(buf);
        while (len && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            len--;
            buf[len] = '\0';
        }
        return strdup(buf);
    }

    int count = line_raw(buf, LINENOISE_MAX_LINE, prompt);
    if (count == -1)
        return NULL;
    return strdup(buf);
}

/* This is just a wrapper the user may want to call in order to make sure
 * the linenoise returned buffer is freed with the same allocator it was
 * created with. Useful when the main program is using an alternative
 * allocator.
 */
void line_free(void *ptr)
{
    free(ptr);
}

/* ================================ History ================================= */

/* Free the history, but does not reset it. Only used when we have to
 * exit() to avoid memory leaks are reported by valgrind & co.
 */
static void free_history(void)
{
    if (history) {
        for (int j = 0; j < history_len; j++)
            free(history[j]);
        free(history);
    }
}

/* At exit we'll try to fix the terminal to the initial conditions. */
static void line_atexit(void)
{
    disable_raw_mode(STDIN_FILENO);
    free_history();
}

/* This is the API call to add a new entry in the linenoise history.
 * It uses a fixed array of char pointers that are shifted (memmoved)
 * when the history max length is reached in order to remove the older
 * entry and make room for the new one, so it is not exactly suitable for huge
 * histories, but will work well for a few hundred of entries.
 *
 * Using a circular buffer is smarter, but a bit more complex to handle.
 */
int line_history_add(const char *line)
{
    if (history_max_len == 0)
        return 0;

    /* Initialization on first call. */
    if (!history) {
        history = malloc(sizeof(char *) * history_max_len);
        if (!history)
            return 0;
        memset(history, 0, (sizeof(char *) * history_max_len));
    }

    /* Don't add duplicated lines. */
    if (history_len && !strcmp(history[history_len - 1], line))
        return 0;

    /* Add an heap allocated copy of the line in the history.
     * If we reached the max length, remove the older line. */
    char *linecopy = strdup(line);
    if (!linecopy)
        return 0;
    if (history_len == history_max_len) {
        free(history[0]);
        memmove(history, history + 1, sizeof(char *) * (history_max_len - 1));
        history_len--;
    }
    history[history_len] = linecopy;
    history_len++;
    return 1;
}

/* Set the maximum length for the history. This function can be called even
 * if there is already some history, the function will make sure to retain
 * just the latest 'len' elements if the new history length value is smaller
 * than the amount of items already inside the history.
 */
int line_history_set_max_len(int len)
{
    if (len < 1)
        return 0;

    if (history) {
        int tocopy = history_len;

        char **new = malloc(sizeof(char *) * len);
        if (!new)
            return 0;

        /* If we can't copy everything, free the elements we'll not use. */
        if (len < tocopy) {
            for (int j = 0; j < tocopy - len; j++)
                free(history[j]);
            tocopy = len;
        }
        memset(new, 0, sizeof(char *) * len);
        memcpy(new, history + (history_len - tocopy), sizeof(char *) * tocopy);
        free(history);
        history = new;
    }
    history_max_len = len;
    if (history_len > history_max_len)
        history_len = history_max_len;
    return 1;
}

/* Save the history in the specified file. On success 0 is returned
 * otherwise -1 is returned.
 */
int line_history_save(const char *filename)
{
    mode_t old_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);

    FILE *fp = fopen(filename, "w");
    umask(old_umask);
    if (!fp)
        return -1;

    chmod(filename, S_IRUSR | S_IWUSR);
    for (int j = 0; j < history_len; j++)
        fprintf(fp, "%s\n", history[j]);
    fclose(fp);
    return 0;
}

/* Load the history from the specified file. If the file does not exist
 * zero is returned and no operation is performed.
 *
 * If the file exists and the operation succeeded 0 is returned, otherwise
 * on error -1 is returned.
 */
int line_history_load(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
        return -1;

    char buf[LINENOISE_MAX_LINE];
    while (fgets(buf, LINENOISE_MAX_LINE, fp) != NULL) {
        char *p = strchr(buf, '\r');
        if (!p)
            p = strchr(buf, '\n');
        if (p)
            *p = '\0';
        line_history_add(buf);
    }
    fclose(fp);
    return 0;
}