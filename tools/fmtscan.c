#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Remove C escape sequences */
#define OPT_ESCAPE_STRIP (0x00000001)

/* Identify messages missing a newline character (\n) */
#define OPT_MISSING_NEWLINE (0x00000002)

/* Output literal strings */
#define OPT_LITERAL_STRINGS (0x00000004)

/* Omit the source file name from the output */
#define OPT_SOURCE_NAME (0x00000008)

/* Replace %% format specifiers with a space */
#define OPT_FORMAT_STRIP (0x00000010)

/* Verify words against the dictionary */
#define OPT_CHECK_WORDS (0x00000020)

/* Analyze all literal strings instead of print statements */
#define OPT_PARSE_STRINGS (0x00000040)

#define PARSER_OK (0)
#define PARSER_COMMENT_FOUND (1)
#define PARSER_EOF (256)
#define PARSER_CONTINUE (512)

#define TOKEN_CHUNK_SIZE (32768)
#define TABLE_SIZE (4 * 16384)
#define HASH_MASK (TABLE_SIZE - 1)

#define MAX_WORD_NODES (27) /* a..z -> 0..25 and _/0..9 as 26 */
#define WORD_NODES_HEAP_SIZE (250000)
#define PRINTK_NODES_HEAP_SIZE (12000)
#define SIZEOF_ARRAY(x) (sizeof(x) / sizeof(x[0]))

#define BAD_MAPPING (0xff)

#if defined(__GNUC__) || defined(__clang__)
#define ALIGNED(a) __attribute__((aligned(a)))
#define UNUSED __attribute__((unused))
#else
#define ALIGNED(a)
#define UNUSED
#endif

#define UNLIKELY(c) __builtin_expect((c), 0)
#define LIKELY(c) __builtin_expect((c), 1)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static const char *dictionary_paths[] = {
    "/usr/share/dict/american-english",
    "scripts/aspell-pws",
};

/* token types used for parsing C source code */
typedef enum {
    TOKEN_UNKNOWN,        /* Token not recognized */
    TOKEN_NUMBER,         /* Numeric literal */
    TOKEN_LITERAL_STRING, /* String literal (e.g., "string") */
    TOKEN_LITERAL_CHAR,   /* Character literal (e.g., 'x') */
    TOKEN_IDENTIFIER,     /* identifier */
    TOKEN_PAREN_OPENED,   /* ( */
    TOKEN_PAREN_CLOSED,   /* ) */
    TOKEN_SQUARE_OPENED,  /* [ */
    TOKEN_SQUARE_CLOSED,  /* ] */
    TOKEN_CPP,            /* Preprocessor directive (e.g., #include) */
    TOKEN_WHITE_SPACE,    /* ' ', '\t', '\r', '\n' white space */
    TOKEN_LESS_THAN,      /* < */
    TOKEN_GREATER_THAN,   /* > */
    TOKEN_COMMA,          /* , */
    TOKEN_ARROW,          /* -> */
    TOKEN_TERMINAL,       /* Statement terminator: ; */
} token_type_t;

/* Represents a lexical token. */
typedef struct {
    char *ptr;         /* Pointer to the current end of the token during lexical
                        * analysis
                        */
    char *token;       /* Collected token string */
    char *token_end;   /* Pointer to the end of the token */
    size_t len;        /* Buffer length for the token */
    token_type_t type; /* Inferred token type */
} token_t;

typedef void (*parse_func_t)(const char *restrict path,
                             unsigned char *restrict data,
                             unsigned char *restrict data_end,
                             token_t *restrict t,
                             token_t *restrict line,
                             token_t *restrict str);

typedef uint16_t get_char_t;

typedef struct {
    void *data;
    size_t size;
    parse_func_t parse_func;
    char filename[PATH_MAX];
} msg_t;

typedef struct {
    char *path;
    mqd_t mq;
} context_t;

/* Parser state context. */
typedef struct {
    unsigned char *ptr;      /* current data position */
    unsigned char *data;     /* Start of the data */
    unsigned char *data_end; /* End of the data */
    bool skip_white_space;   /* Flag to skip whitespace characters */
} parser_t;

/* Hash table entry for tokens (forms a linked list). */
typedef struct hash_entry {
    struct hash_entry *next;
    char token[0]; /* Flexible array member for token data */
} hash_entry_t;

typedef get_char_t (*get_token_action_t)(parser_t *restrict p,
                                         token_t *restrict t,
                                         get_char_t ch);

/* Entry for the printf format string table. */
typedef struct {
    char *format; /* The printf format string */
    size_t len;   /* Length of the format string */
} format_t;

typedef struct {
    uint32_t lo32;
} index_t;

typedef struct word_node {
    index_t word_node_index[MAX_WORD_NODES];
    bool eow; /* Flag indicating the end of a word */
} word_node_t;

static uint64_t bytes_total;
static uint32_t files;
static uint32_t lines;
static uint32_t lineno;
static uint32_t bad_spellings;
static uint32_t bad_spellings_total;
static uint32_t words;
static uint32_t dict_size;

static uint8_t opt_flags = OPT_SOURCE_NAME;
static void (*token_cat)(token_t *restrict token,
                         const token_t *restrict token_to_add);
static char quotes[] = "\"";
static char space[] = " ";
static bool is_not_whitespace[256] ALIGNED(64);
static bool is_not_identifier[256] ALIGNED(64);

/* Flat array representing the dictionary words tree. */
static word_node_t word_node_heap[WORD_NODES_HEAP_SIZE];
static word_node_t *word_nodes = &word_node_heap[0];
static word_node_t *word_node_heap_next = &word_node_heap[1];

/* Flat array representing the tree of printf-like function names. */
static word_node_t printf_node_heap[PRINTK_NODES_HEAP_SIZE];
static word_node_t *printf_nodes = &printf_node_heap[0];
static word_node_t *printf_node_heap_next = &printf_node_heap[1];

/* Hash table storing misspelled words. */
static hash_entry_t *hash_bad_spellings[TABLE_SIZE];

/* printf format specifiers. */
static format_t formats[] ALIGNED(64) = {
    {"%", 1},     {"s", 1},    {"llu", 3},  {"lld", 3},  {"llx", 3},
    {"llX", 3},   {"lu", 2},   {"ld", 2},   {"lx", 2},   {"lX", 2},
    {"u", 1},     {"d", 1},    {"x", 1},    {"X", 1},    {"pF", 2},
    {"pf", 2},    {"ps", 2},   {"pSR", 3},  {"pS", 2},   {"pB", 2},
    {"pK", 2},    {"pr", 2},   {"pap", 3},  {"pa", 2},   {"pad", 3},
    {"*pE", 3},   {"*pEa", 4}, {"*pEc", 4}, {"*pEh", 4}, {"*pEn", 4},
    {"*pEo", 4},  {"*pEp", 4}, {"*pEs", 4}, {"*ph", 3},  {"*phC", 4},
    {"*phD", 4},  {"*phN", 4}, {"pM", 2},   {"pMR", 3},  {"pMF", 3},
    {"pm", 2},    {"pmR", 3},  {"pi4", 3},  {"pI4", 3},  {"pi4h", 4},
    {"pI4h", 4},  {"pi4n", 4}, {"pI4n", 4}, {"pi4b", 4}, {"pI4b", 4},
    {"pi4l", 4},  {"pI4l", 4}, {"pi6", 3},  {"pI6", 3},  {"pI6c", 4},
    {"piS", 3},   {"pIS", 3},  {"piSc", 4}, {"pISc", 4}, {"piSpc", 5},
    {"pISpc", 5}, {"piSf", 4}, {"pISf", 4}, {"piSs", 4}, {"pISs", 4},
    {"piSh", 4},  {"pISh", 4}, {"piSn", 4}, {"pISn", 4}, {"piSb", 4},
    {"pISb", 4},  {"piSl", 4}, {"pISl", 4}, {"pUb", 3},  {"pUB", 3},
    {"pUl", 3},   {"pUL", 3},  {"pd", 2},   {"pd2", 3},  {"pd3", 3},
    {"pd4", 3},   {"pD", 2},   {"pD2", 3},  {"pD3", 3},  {"pD4", 3},
    {"pg", 2},    {"pV", 2},   {"pC", 2},   {"pCn", 3},  {"pCr", 3},
    {"*pb", 3},   {"*pbl", 4}, {"pGp", 3},  {"pGg", 3},  {"pGv", 3},
    {"pNF", 3},
};

/* Multiple printf-style functions used to populate the hash table. */
static char *printfs[] = {
    "BUG",      "BUG_ON",    "debug",     "DEBUG",    "die",        "dprintf",
    "info",     "INFO",      "fprintf",   "kdebug",   "log",        "LOG",
    "LOG_DBG",  "log_debug", "log_bug",   "log_err",  "LOG_ERROR",  "log_error",
    "LOG_INFO", "log_info",  "log_print", "LOG_WARN", "log_warn",   "panic",
    "PANIC",    "perror",    "pr_alert",  "pr_crit",  "pr_debug",   "pr_err",
    "pr_fmt",   "pr_info",   "pr_init",   "print",    "PRINT",      "printf",
    "printk",   "pr_notice", "pr_trace",  "pr_warn",  "pr_warning", "puts",
    "report",   "snprintf",  "sprintf",   "trace",    "TRACE",      "warn",
    "WARN",     "warning",   "WARNING",   "WARN_ON",  "warnx",
};

static uint8_t mapping[256] ALIGNED(64);

static void set_mapping(void)
{
    size_t i;

    for (i = 0; i < SIZEOF_ARRAY(mapping); i++)
        mapping[i] = BAD_MAPPING;
    for (i = 'a'; i <= 'z'; i++)
        mapping[i] = i - 'a';
    for (i = 'A'; i <= 'Z'; i++)
        mapping[i] = i - 'A';
    for (i = '0'; i <= '9'; i++)
        mapping[i] = 26;
    mapping['_'] = 26;
}

static inline get_char_t map(const get_char_t ch)
{
    return mapping[ch];
}

static inline size_t token_len(const token_t *t)
{
    return t->ptr - t->token;
}

static inline uint64_t hash_ror_uint64(const uint64_t x, const uint32_t bits)
{
    return (x >> bits) | x << (64 - bits);
}

/* Processes 64-bit chunks in the fast path by multiplying the hash with
 * each 64-bit segment from the string and performing a partial right rotation
 * to mix the bits back into the hash.
 */
static uint32_t stress_hash_mulxror64(const char *str, const size_t len)
{
    uint64_t hash = len;

    for (size_t i = len >> 3; i; i--) {
        uint64_t v;

        memcpy(&v, str, sizeof(v));
        str += sizeof(v);
        hash *= v;
        hash ^= hash_ror_uint64(hash, 40);
    }
    for (size_t i = len & 7; *str && i; i--) {
        hash *= (uint8_t) *str++;
        hash ^= hash_ror_uint64(hash, 5);
    }
    return (uint32_t) ((hash >> 32) ^ hash) & HASH_MASK;
}

static int parse_file(char *restrict path, const mqd_t mq);

static void out_of_memory(void)
{
    fprintf(stderr, "Out of memory performing an allocation\n");
    exit(EXIT_FAILURE);
}

static inline index_t *index_unpack_ptr(word_node_t *restrict node,
                                        get_char_t ch)
{
    void *vptr = (void *) &node->word_node_index[ch];

    return (index_t *) vptr; /* Cast away our sins */
}

static inline void add_word(char *restrict str,
                            word_node_t *restrict node,
                            word_node_t *restrict node_heap,
                            word_node_t **restrict node_heap_next,
                            const ssize_t heap_size)
{
    if ((*node_heap_next - node_heap) >= heap_size)
        out_of_memory();

    get_char_t ch = map(*str);

    if (LIKELY(ch != BAD_MAPPING)) {
        index_t *restrict ptr = index_unpack_ptr(node, ch);
        word_node_t *new_node;
        uint32_t index32 = ptr->lo32;

        if (index32) {
            new_node = &node_heap[index32];
        } else {
            new_node = *node_heap_next;
            index32 = new_node - node_heap;
            (*node_heap_next)++;

            ptr->lo32 = index32;
        }
        add_word(++str, new_node, node_heap, node_heap_next, heap_size);
    } else {
        node->eow = true;
    }
}

static inline bool find_word(const char *restrict word,
                             word_node_t *restrict node,
                             word_node_t *restrict node_heap)
{
    for (;;) {
        if (UNLIKELY(!node))
            return false;
        get_char_t ch = *word;
        if (!ch)
            return node->eow;
        ch = map(ch);
        if (UNLIKELY(ch == BAD_MAPPING))
            return true;
        const index_t *ptr = index_unpack_ptr(node, ch);
        uint32_t index32 = ptr->lo32;
        node = index32 ? &node_heap[index32] : NULL;
        word++;
    }
}

static inline int read_dictionary(const char *dictfile)
{
    struct stat buf;
    char buffer[4096];
    const char *buffer_end = buffer + (sizeof(buffer)) - 1;

    int fd = open(dictfile, O_RDONLY);
    if (fd < 0)
        return -1;
    if (fstat(fd, &buf) < 0) {
        close(fd);
        return -1;
    }

    const char *ptr, *dict;
    ptr = dict =
        mmap(NULL, buf.st_size, PROT_READ, MAP_SHARED | MAP_POPULATE, fd, 0);
    if (dict == MAP_FAILED) {
        close(fd);
        return -1;
    }
    const char *dict_end = dict + buf.st_size;

    while (ptr < dict_end) {
        char *bptr = buffer;

        while (ptr < dict_end && bptr < buffer_end && *ptr != '\n') {
            *bptr++ = *ptr++;
        }
        dict_size += bptr - buffer;
        *bptr = '\0';
        ptr++;
        words++;
        add_word(buffer, word_nodes, word_node_heap, &word_node_heap_next,
                 WORD_NODES_HEAP_SIZE);
    }
    munmap((void *) dict, buf.st_size);
    close(fd);

    return 0;
}

static inline void add_bad_spelling(const char *word, const size_t len)
{
    if (find_word(word, printf_nodes, printf_node_heap))
        return;

    bad_spellings_total++;
    hash_entry_t **head = &hash_bad_spellings[stress_hash_mulxror64(word, len)];
    hash_entry_t *he;
    for (he = *head; he; he = he->next) {
        if (!strcmp(he->token, word))
            return;
    }
    he = malloc(sizeof(*he) + len);
    if (UNLIKELY(!he))
        out_of_memory();

    he->next = *head;
    *head = he;
    memcpy(he->token, word, len);
    bad_spellings++;
}

static void check_words(token_t *token)
{
    char *p1 = token->token;
    const char *p3 = p1 + token_len(token);

    while (p1 < p3) {
        /* skip non-alhabetics */
        while (*p1 && !isalpha(*p1))
            p1++;
        if (!*p1)
            return;
        char *p2 = p1;
        while (LIKELY((*p2 && (isalpha(*p2)))))
            p2++;
        *p2 = '\0';

        if (LIKELY(p2 - p1 > 1)) {
            if (!find_word(p1, word_nodes, word_node_heap))
                add_bad_spelling(p1, 1 + p2 - p1);
        }
        p1 = p2 + 1;
    }
    return;
}

/* Set up a new parser instance. */
static inline void parser_new(parser_t *restrict p,
                              unsigned char *restrict data,
                              unsigned char *restrict data_end,
                              const bool skip_white_space)
{
    p->data = data;
    p->data_end = data_end;
    p->ptr = data;
    p->skip_white_space = skip_white_space;
}

/* Fetch the next character from the input. */
static inline get_char_t get_char(parser_t *p)
{
    if (LIKELY(p->ptr < p->data_end))
        return *(p->ptr++);
    return PARSER_EOF;
}

/* Push a character back into the input stream. */
static inline void unget_char(parser_t *p)
{
    p->ptr--;
}

static int cmp_format(const void *restrict p1, const void *restrict p2)
{
    const format_t *restrict f1 = (const format_t *restrict) p1;
    const format_t *restrict f2 = (const format_t *restrict) p2;

    const size_t l1 = f1->len;
    const size_t l2 = f2->len;

    if (l1 < l2)
        return 1;
    if (l1 > l2)
        return -1;
    return strcmp(f1->format, f2->format);
}

/* Reset the token structure for reuse. */
static inline void token_clear(token_t *t)
{
    char *ptr = t->token;

    t->ptr = ptr;
    t->token_end = ptr + t->len;
    *ptr = '\0';
    t->type = TOKEN_UNKNOWN;
}

/* Initialize a new token with extra buffer space to minimize reallocations
 * while appending characters during lexing.
 */
static void token_new(token_t *t)
{
    int ret;

    ret = posix_memalign((void **) &t->token, 64, TOKEN_CHUNK_SIZE);
    if (ret != 0)
        out_of_memory();
    t->len = TOKEN_CHUNK_SIZE;
    token_clear(t);
}

/* Deallocate the token's resources. */
static void token_free(token_t *t)
{
    free(t->token);
    t->ptr = NULL;
    t->token = NULL;
    t->token_end = NULL;
    t->len = 0;
}

/* Expand the token's buffer when additional space is needed. */
static void token_expand(token_t *t)
{
    /* Increase buffer size by TOKEN_CHUNK_SIZE */
    ptrdiff_t diff = t->ptr - t->token;

    t->len += TOKEN_CHUNK_SIZE;
    t->token_end += TOKEN_CHUNK_SIZE;
    t->token = realloc(t->token, t->len);
    if (UNLIKELY(!t->token))
        out_of_memory();
    t->ptr = t->token + diff;
}

/* Append a single character to the token, expanding the buffer if necessary. */
static inline void token_append(token_t *t, const get_char_t ch)
{
    if (LIKELY(t->ptr < (t->token_end))) {
        *(t->ptr++) = ch;
    } else {
        token_expand(t);
        *(t->ptr++) = ch;
    }
}

static inline void token_eos(token_t *t)
{
    *(t->ptr) = '\0';
}

static inline void token_cat_str(token_t *restrict t, const char *restrict str)
{
    while (*str) {
        token_append(t, *str);
        str++;
    }
    token_eos(t);
}

/* Skip over C-style comments, discarding their contents. */
static get_char_t skip_comments(parser_t *p)
{
    get_char_t ch;
    get_char_t nextch = get_char(p);

    if (nextch == '/') {
        do {
            ch = get_char(p);
            if (UNLIKELY(ch == PARSER_EOF))
                return ch;
        } while (ch != '\n');

        return PARSER_COMMENT_FOUND;
    }
    if (LIKELY(nextch == '*')) {
        for (;;) {
            ch = get_char(p);

            if (UNLIKELY(ch == '*')) {
                ch = get_char(p);

                if (LIKELY(ch == '/'))
                    return PARSER_COMMENT_FOUND;
                if (UNLIKELY(ch == PARSER_EOF))
                    return ch;

                unget_char(p);
            }
            if (UNLIKELY(ch == PARSER_EOF))
                return ch;
        }
    }
    if (UNLIKELY(nextch == PARSER_EOF))
        return nextch;

    /* Not a comment, push back */
    unget_char(p);

    return PARSER_OK;
}

static get_char_t skip_macros(parser_t *p)
{
    bool continuation = false;

    for (;;) {
        get_char_t ch;

        ch = get_char(p);
        if (ch == '\n') {
            lines++;
            lineno++;
            if (!continuation)
                return ch;
            continuation = false;
        } else if (ch == '\\') {
            continuation = true;
        } else if (UNLIKELY(ch == '/')) {
            get_char_t ret = skip_comments(p);

            if (LIKELY(ret == PARSER_COMMENT_FOUND))
                continue;
            if (UNLIKELY(ret == PARSER_EOF))
                return ret;
        } else if (UNLIKELY(ch == PARSER_EOF))
            break;
    }
    return PARSER_EOF;
}

/* Parse an integer value.
 * Since the Linux kernel does not support floats or doubles, only decimal,
 * octal, and hexadecimal formats are handled.
 * TODO: allow more types
 */
static get_char_t parse_number(parser_t *restrict p,
                               token_t *restrict t,
                               get_char_t ch)
{
    bool ishex = false, isoct = false;

    /* Determine the integer format based on its prefix. */
    if (LIKELY(ch == '0')) {
        token_append(t, ch);

        get_char_t nextch1 = get_char(p);

        if (nextch1 >= '0' && nextch1 <= '8') {
            /* Treat as an octal value */
            ch = nextch1;
            isoct = true;
        } else if (nextch1 == 'x' || nextch1 == 'X') {
            /* Check for hexadecimal notation */
            get_char_t nextch2 = get_char(p);

            if (LIKELY(nextch2 != PARSER_EOF)) {
                /* If not EOF, revert state and finish token */
                unget_char(p);
                unget_char(p);
                token_eos(t);
                return PARSER_OK;
            }
            if (isxdigit(nextch2)) {
                /* Recognized as hexadecimal */
                token_append(t, nextch1);
                ch = nextch2;
                ishex = true;
            } else {
                unget_char(p);
                token_eos(t);
                return PARSER_OK;
            }
        } else if (LIKELY(nextch1 != PARSER_EOF)) {
            unget_char(p);
            token_eos(t);
            return PARSER_OK;
        } else {
            token_append(t, ch);
            token_eos(t);
            return PARSER_OK;
        }
    }

    /* With the integer type determined, collect the digits. */
    token_append(t, ch);

    for (;;) {
        ch = get_char(p);

        if (UNLIKELY(ch == PARSER_EOF)) {
            unget_char(p);
            token_eos(t);
            return PARSER_OK;
        }

        if (ishex) {
            if (LIKELY(isxdigit(ch))) {
                token_append(t, ch);
            } else {
                unget_char(p);
                token_eos(t);
                return PARSER_OK;
            }
        } else if (isoct) {
            if (LIKELY(ch >= '0' && ch <= '8')) {
                token_append(t, ch);
            } else {
                unget_char(p);
                token_eos(t);
                return PARSER_OK;
            }
        } else {
            if (isdigit(ch)) {
                token_append(t, ch);
            } else {
                unget_char(p);
                token_eos(t);
                return PARSER_OK;
            }
        }
    }
}

/* Parse an identifier. */
static get_char_t parse_identifier(parser_t *restrict p,
                                   token_t *restrict t,
                                   get_char_t ch)
{
    t->type = TOKEN_IDENTIFIER;
    token_append(t, ch);

    for (;;) {
        ch = get_char(p);
        uint8_t ch8 = (uint8_t) ch;

        if (UNLIKELY(is_not_identifier[ch8])) {
            unget_char(p);
            token_eos(t);
            return PARSER_OK;
        }
        token_append(t, ch);
    }
}

/* Process escape sequences at the end of a string literal.
 * Transformations:
 *   "foo\n"         becomes "foo"
 *   "foo\nbar"      becomes "foo bar"
 *   "foo\n" + whitespace + "bar" becomes "foo " + whitespace + "bar"
 */
static inline void literal_peek(parser_t *restrict p,
                                token_t *restrict t,
                                const get_char_t literal)
{
    get_char_t ch = get_char(p);
    if (UNLIKELY(ch != literal)) {
        unget_char(p);
        token_append(t, ' ');
        return;
    }

    uint32_t got = 1;
    for (;;) {
        got++;
        ch = get_char(p);
        if (LIKELY(ch == literal)) {
            token_append(t, ' ');
            break;
        } else if (UNLIKELY(ch == TOKEN_WHITE_SPACE)) {
            continue;
        } else if (UNLIKELY(ch == PARSER_EOF)) {
            break;
        }
        break;
    }
    while (got) {
        unget_char(p);
        got--;
    }
}

/* Parse literal strings */
static get_char_t parse_literal(parser_t *restrict p,
                                token_t *restrict t,
                                const get_char_t literal,
                                const token_type_t type)
{
    t->type = type;

    token_append(t, literal);

    for (;;) {
        get_char_t ch = get_char(p);

        if (ch == '\\') {
            if (opt_flags & OPT_ESCAPE_STRIP) {
                ch = get_char(p);
                if (UNLIKELY(ch == PARSER_EOF)) {
                    token_eos(t);
                    return ch;
                }
                switch (ch) {
                case '?':
                    token_append(t, ch);
                    continue;
                case 'a':
                case 'b':
                case 'f':
                case 'n':
                case 'r':
                case 't':
                case 'v':
                    literal_peek(p, t, literal);
                    continue;
                case 'x':
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '9':
                default:
                    token_append(t, '\\');
                    token_append(t, ch);
                    continue;
                }
            } else {
                token_append(t, ch);
                ch = get_char(p);
                if (LIKELY(ch != PARSER_EOF)) {
                    token_append(t, ch);
                    continue;
                }
                token_eos(t);
                return ch;
            }
        }

        if (UNLIKELY(ch == literal)) {
            token_append(t, ch);
            token_eos(t);
            return PARSER_OK;
        }
        if (UNLIKELY(ch == PARSER_EOF)) {
            token_eos(t);
            return PARSER_OK;
        }

        token_append(t, ch);
    }
    token_eos(t);

    return PARSER_OK;
}

/* Parse operators like '+' or '-' that may occur as a single or doubled symbol.
 */
static inline get_char_t parse_op(parser_t *restrict p,
                                  token_t *restrict t,
                                  const get_char_t op)
{
    token_append(t, op);

    if (get_char(p) == op) {
        token_append(t, op);
        token_eos(t);
        return PARSER_OK;
    }

    unget_char(p);
    token_eos(t);
    return PARSER_OK;
}

/* Parse -, --, -> */
static inline get_char_t parse_minus(parser_t *restrict p,
                                     token_t *restrict t,
                                     const get_char_t op)
{
    token_append(t, op);

    get_char_t ch = get_char(p);

    if (ch == op) {
        token_append(t, ch);
        token_eos(t);
        return PARSER_OK;
    }

    if (LIKELY(ch == '>')) {
        token_append(t, ch);
        token_eos(t);
        t->type = TOKEN_ARROW;
        return PARSER_OK;
    }

    unget_char(p);
    token_eos(t);
    return PARSER_OK;
}

static inline get_char_t parse_skip_comments(parser_t *restrict p,
                                             token_t *restrict t,
                                             get_char_t ch)
{
    get_char_t ret = skip_comments(p);

    if (ret == PARSER_COMMENT_FOUND) {
        ret |= PARSER_CONTINUE;
        return ret;
    }
    if (UNLIKELY(ret == PARSER_EOF))
        return ret;

    token_append(t, ch);
    token_eos(t);
    return PARSER_OK;
}

static inline get_char_t parse_simple(token_t *t,
                                      get_char_t ch,
                                      const token_type_t type)
{
    token_append(t, ch);
    token_eos(t);
    t->type = type;
    return PARSER_OK;
}

static inline get_char_t parse_hash(parser_t *restrict p UNUSED,
                                    token_t *restrict t,
                                    get_char_t ch UNUSED)
{
    skip_macros(p);
    token_clear(t);

    return PARSER_OK;
}

static inline get_char_t parse_paren_opened(parser_t *restrict p UNUSED,
                                            token_t *restrict t,
                                            get_char_t ch)
{
    return parse_simple(t, ch, TOKEN_PAREN_OPENED);
}

static inline get_char_t parse_paren_closed(parser_t *restrict p UNUSED,
                                            token_t *restrict t,
                                            get_char_t ch)
{
    return parse_simple(t, ch, TOKEN_PAREN_CLOSED);
}


static inline get_char_t parse_square_opened(parser_t *restrict p UNUSED,
                                             token_t *restrict t,
                                             get_char_t ch)
{
    return parse_simple(t, ch, TOKEN_SQUARE_OPENED);
}

static inline get_char_t parse_square_closed(parser_t *restrict p UNUSED,
                                             token_t *restrict t,
                                             get_char_t ch)
{
    return parse_simple(t, ch, TOKEN_SQUARE_CLOSED);
}

static inline get_char_t parse_less_than(parser_t *restrict p UNUSED,
                                         token_t *restrict t,
                                         get_char_t ch)
{
    return parse_simple(t, ch, TOKEN_LESS_THAN);
}

static inline get_char_t parse_greater_than(parser_t *restrict p UNUSED,
                                            token_t *restrict t,
                                            get_char_t ch)
{
    return parse_simple(t, ch, TOKEN_GREATER_THAN);
}

static inline get_char_t parse_comma(parser_t *restrict p UNUSED,
                                     token_t *restrict t,
                                     get_char_t ch)
{
    return parse_simple(t, ch, TOKEN_COMMA);
}

static inline get_char_t parse_terminal(parser_t *restrict p UNUSED,
                                        token_t *restrict t,
                                        get_char_t ch)
{
    return parse_simple(t, ch, TOKEN_TERMINAL);
}

static inline get_char_t parse_misc_char(parser_t *restrict p UNUSED,
                                         token_t *restrict t,
                                         get_char_t ch)
{
    token_append(t, ch);
    token_eos(t);
    return PARSER_OK;
}

static inline get_char_t parse_literal_string(parser_t *restrict p,
                                              token_t *restrict t,
                                              get_char_t ch)
{
    return parse_literal(p, t, ch, TOKEN_LITERAL_STRING);
}

static inline get_char_t parse_literal_char(parser_t *restrict p,
                                            token_t *restrict t,
                                            get_char_t ch)
{
    return parse_literal(p, t, ch, TOKEN_LITERAL_CHAR);
}

static inline get_char_t parse_backslash(parser_t *restrict p,
                                         token_t *restrict t,
                                         get_char_t ch)
{
    if (p->skip_white_space)
        return PARSER_OK | PARSER_CONTINUE;

    if (opt_flags & OPT_ESCAPE_STRIP) {
        token_append(t, ch);
        token_eos(t);
        t->type = TOKEN_WHITE_SPACE;
    } else {
        token_append(t, ch);
        ch = get_char(p);
        if (UNLIKELY(ch == PARSER_EOF))
            return ch;
        token_append(t, ch);
        token_eos(t);
    }
    return PARSER_OK;
}

static inline get_char_t parse_newline(parser_t *restrict p,
                                       token_t *restrict t,
                                       get_char_t ch)
{
    lines++;
    lineno++;
    return parse_backslash(p, t, ch);
}

static inline get_char_t parse_eof(parser_t *restrict p UNUSED,
                                   token_t *restrict t UNUSED,
                                   get_char_t ch UNUSED)
{
    return PARSER_EOF;
}

static inline get_char_t parse_whitespace(parser_t *restrict p UNUSED,
                                          token_t *restrict t,
                                          const get_char_t ch)
{
    t->type = TOKEN_IDENTIFIER;
    token_append(t, ch);

    for (;;) {
        uint8_t ch8 = (uint8_t) get_char(p);
        if (is_not_whitespace[ch8])
            break;
    }
    unget_char(p);
    token_eos(t);

    return parse_simple(t, ' ', TOKEN_WHITE_SPACE);
}

static get_token_action_t get_token_actions[] = {
    ['/'] = parse_skip_comments,
    ['#'] = parse_hash,
    ['('] = parse_paren_opened,
    [')'] = parse_paren_closed,
    ['['] = parse_square_opened,
    [']'] = parse_square_closed,
    ['<'] = parse_less_than,
    ['>'] = parse_greater_than,
    [','] = parse_comma,
    [';'] = parse_terminal,
    ['{'] = parse_misc_char,
    ['}'] = parse_misc_char,
    [':'] = parse_misc_char,
    ['~'] = parse_misc_char,
    ['?'] = parse_misc_char,
    ['*'] = parse_misc_char,
    ['%'] = parse_misc_char,
    ['!'] = parse_misc_char,
    ['.'] = parse_misc_char,
    ['0'] = parse_number,
    ['1'] = parse_number,
    ['2'] = parse_number,
    ['3'] = parse_number,
    ['4'] = parse_number,
    ['5'] = parse_number,
    ['6'] = parse_number,
    ['7'] = parse_number,
    ['8'] = parse_number,
    ['9'] = parse_number,
    ['+'] = parse_op,
    ['='] = parse_op,
    ['|'] = parse_op,
    ['&'] = parse_op,
    ['-'] = parse_minus,
    ['a'] = parse_identifier,
    ['b'] = parse_identifier,
    ['c'] = parse_identifier,
    ['d'] = parse_identifier,
    ['e'] = parse_identifier,
    ['f'] = parse_identifier,
    ['g'] = parse_identifier,
    ['h'] = parse_identifier,
    ['i'] = parse_identifier,
    ['j'] = parse_identifier,
    ['k'] = parse_identifier,
    ['l'] = parse_identifier,
    ['m'] = parse_identifier,
    ['n'] = parse_identifier,
    ['o'] = parse_identifier,
    ['p'] = parse_identifier,
    ['q'] = parse_identifier,
    ['r'] = parse_identifier,
    ['s'] = parse_identifier,
    ['t'] = parse_identifier,
    ['u'] = parse_identifier,
    ['v'] = parse_identifier,
    ['w'] = parse_identifier,
    ['x'] = parse_identifier,
    ['y'] = parse_identifier,
    ['z'] = parse_identifier,
    ['A'] = parse_identifier,
    ['B'] = parse_identifier,
    ['C'] = parse_identifier,
    ['D'] = parse_identifier,
    ['E'] = parse_identifier,
    ['F'] = parse_identifier,
    ['G'] = parse_identifier,
    ['H'] = parse_identifier,
    ['I'] = parse_identifier,
    ['J'] = parse_identifier,
    ['K'] = parse_identifier,
    ['L'] = parse_identifier,
    ['M'] = parse_identifier,
    ['N'] = parse_identifier,
    ['O'] = parse_identifier,
    ['P'] = parse_identifier,
    ['Q'] = parse_identifier,
    ['R'] = parse_identifier,
    ['S'] = parse_identifier,
    ['T'] = parse_identifier,
    ['U'] = parse_identifier,
    ['V'] = parse_identifier,
    ['W'] = parse_identifier,
    ['X'] = parse_identifier,
    ['Y'] = parse_identifier,
    ['Z'] = parse_identifier,
    ['_'] = parse_identifier,
    ['"'] = parse_literal_string,
    ['\''] = parse_literal_char,
    ['\\'] = parse_backslash,
    ['\n'] = parse_newline,
    [' '] = parse_whitespace,
    ['\t'] = parse_whitespace,
    [PARSER_EOF] = parse_eof,
};

/* Extract a token from the input stream. */
static get_char_t get_token(parser_t *restrict p, token_t *restrict t)
{
    for (;;) {
        get_char_t ret, ch = get_char(p);
        get_token_action_t action = get_token_actions[ch];

        if (UNLIKELY(!action))
            continue;

        ret = action(p, t, ch);
        if (UNLIKELY(ret & PARSER_CONTINUE))
            continue;
        return ret;
    }

    return PARSER_OK;
}

/* Remove the surrounding quotes from literal values,
 * converting strings like "foo" or characters like 'f' to their unquoted form.
 */
static inline void literal_strip_quotes(token_t *t)
{
    size_t len = token_len(t);

    t->token[len - 1] = '\0';

    memmove(t->token, t->token + 1, len - 1);

    t->ptr -= 2;
}

/* Append a new string to an existing one. The original string may be NULL or
 * already allocated on the heap. Returns the newly concatenated string.
 */
static void token_cat_normal(token_t *restrict token,
                             const token_t *restrict token_to_add)
{
    token_cat_str(token, token_to_add->token);
}

/* Parse a formatted message similar to printf(). */
static get_char_t parse_message(const char *restrict path UNUSED,
                                bool *restrict source_emit UNUSED,
                                parser_t *restrict p,
                                token_t *restrict t,
                                token_t *restrict line,
                                token_t *restrict str)
{
    bool got_string = false;

    token_clear(line);

    token_cat(line, t);
    token_clear(t);
    if (UNLIKELY(get_token(p, t) == PARSER_EOF)) {
        return PARSER_EOF;
    }

    /* Skip over any whitespace. */
    if (t->type == TOKEN_WHITE_SPACE) {
        parse_whitespace(p, t, ' ');

        if (UNLIKELY(get_token(p, t) == PARSER_EOF)) {
            return PARSER_EOF;
        }
    }

    if (t->type != TOKEN_PAREN_OPENED) {
        for (;;) {
            if (UNLIKELY(get_token(p, t) == PARSER_EOF))
                return PARSER_EOF;
            if (t->type == TOKEN_TERMINAL)
                break;
        }
        token_clear(t);
        return PARSER_OK;
    }
    token_cat(line, t);
    token_clear(t);

    token_clear(str);
    for (;;) {
        get_char_t ret = get_token(p, t);

        if (UNLIKELY(ret == PARSER_EOF))
            return PARSER_EOF;

        /* Encounter a ';' indicating the end of the current statement. */
        if (t->type == TOKEN_TERMINAL) {
            token_clear(t);
            return PARSER_OK;
        }

        if (t->type == TOKEN_LITERAL_STRING) {
            literal_strip_quotes(t);
            token_cat(str, t);

            if (!got_string)
                token_cat_str(line, quotes);

            got_string = true;
        } else {
            if (got_string) {
                token_cat_str(line, quotes);
            }
            got_string = false;

            if (token_len(str)) {
                token_clear(str);
            }
        }

        token_cat(line, t);
        if (t->type == TOKEN_COMMA)
            token_cat_str(line, space);

        token_clear(t);
    }
}

/* Process the input to locate function calls resembling printf. */
static void parse_messages(const char *restrict path,
                           unsigned char *restrict data,
                           unsigned char *restrict data_end,
                           token_t *restrict t,
                           token_t *restrict line,
                           token_t *restrict str)
{
    parser_t p;

    parser_new(&p, data, data_end, true);
    bool source_emit = false;

    token_clear(t);

    while ((get_token(&p, t)) != PARSER_EOF) {
        if ((t->type == TOKEN_IDENTIFIER) &&
            (find_word(t->token, printf_nodes, printf_node_heap))) {
            parse_message(path, &source_emit, &p, t, line, str);
        }
        token_clear(t);
    }

    if (opt_flags & OPT_CHECK_WORDS)
        return;
    if (source_emit && (opt_flags & OPT_SOURCE_NAME))
        putchar('\n');
}

/* Process the input to locate literal strings. */
static void parse_literal_strings(const char *restrict path UNUSED,
                                  unsigned char *restrict data,
                                  unsigned char *restrict data_end,
                                  token_t *restrict t,
                                  token_t *restrict line UNUSED,
                                  token_t *restrict str UNUSED)
{
    parser_t p;
    parser_new(&p, data, data_end, true);

    token_clear(t);

    while ((get_token(&p, t)) != PARSER_EOF) {
        if (t->type == TOKEN_LITERAL_STRING)
            check_words(t);
        token_clear(t);
    }
}

static int parse_dir(char *restrict path, const mqd_t mq)
{
    DIR *dp;
    struct dirent *d;
    char filepath[PATH_MAX];

    if (UNLIKELY((dp = opendir(path)) == NULL)) {
        fprintf(stderr, "Cannot open directory %s, errno=%d (%s)\n", path,
                errno, strerror(errno));
        return -1;
    }

    char *ptr1 = filepath;
    const char *ptr2 = path;

    while ((*ptr1 = *(ptr2++)))
        ptr1++;

    *ptr1++ = '/';

    while ((d = readdir(dp)) != NULL) {
        struct stat buf;

        if (LIKELY(d->d_name[0] != '.')) {
            char *ptr = ptr1;
            ptr2 = d->d_name;
            while ((*ptr = *(ptr2++)))
                ptr++;
            *ptr = '\0';
            if (lstat(filepath, &buf) < 0)
                continue;
            /* Don't follow symlinks */
            if (S_ISLNK(buf.st_mode))
                continue;
            parse_file(filepath, mq);
        }
    }
    closedir(dp);

    return 0;
}

static int parse_file(char *restrict path, const mqd_t mq)
{
    struct stat buf;
    int fd;
    int rc = 0;

    const parse_func_t parse_func = (opt_flags & OPT_PARSE_STRINGS)
                                        ? parse_literal_strings
                                        : parse_messages;

    fd = open(path, O_RDONLY | O_NOATIME);
    if (UNLIKELY(fd < 0)) {
        fprintf(stderr, "Cannot open %s, errno=%d (%s)\n", path, errno,
                strerror(errno));
        return -1;
    }
    if (UNLIKELY(fstat(fd, &buf) < 0)) {
        fprintf(stderr, "Cannot stat %s, errno=%d (%s)\n", path, errno,
                strerror(errno));
        close(fd);
        return -1;
    }
    lineno = 0;

    if (LIKELY(S_ISREG(buf.st_mode))) {
        size_t len = strlen(path);

        if (LIKELY(((len >= 2) && !strcmp(path + len - 2, ".c")) ||
                   ((len >= 2) && !strcmp(path + len - 2, ".h")) ||
                   ((len >= 4) && !strcmp(path + len - 4, ".cpp")))) {
            if (LIKELY(buf.st_size > 0)) {
                msg_t msg;

                msg.data = mmap(NULL, (size_t) buf.st_size, PROT_READ,
                                MAP_PRIVATE | MAP_POPULATE, fd, 0);
                if (UNLIKELY(msg.data == MAP_FAILED)) {
                    close(fd);
                    fprintf(stderr, "Cannot mmap %s, errno=%d (%s)\n", path,
                            errno, strerror(errno));
                    return -1;
                }
                bytes_total += buf.st_size;

                msg.parse_func = parse_func;
                msg.size = buf.st_size;
                strncpy(msg.filename, path, sizeof(msg.filename) - 1);
                mq_send(mq, (char *) &msg, sizeof(msg), 1);
            }
            files++;
        }
        close(fd);
    } else {
        close(fd);
        if (S_ISDIR(buf.st_mode))
            rc = parse_dir(path, mq);
    }
    return rc;
}


static void *reader(void *arg)
{
    static void *nowt = NULL;
    const context_t *ctxt = arg;
    msg_t msg = {NULL, 0, NULL, ""};

    parse_file(ctxt->path, ctxt->mq);
    mq_send(ctxt->mq, (char *) &msg, sizeof(msg), 1);

    return &nowt;
}

static int parse_path(char *path,
                      token_t *restrict t,
                      token_t *restrict line,
                      token_t *restrict str)
{
    mqd_t mq = -1;
    struct mq_attr attr;
    char mq_name[64];
    int rc;
    context_t ctxt;
    pthread_t pthread;

    snprintf(mq_name, sizeof(mq_name), "/fmtscan-%i", getpid());

    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(msg_t);
    attr.mq_curmsgs = 0;

    mq = mq_open(mq_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attr);
    if (mq < 0)
        return -1;

    ctxt.path = path;
    ctxt.mq = mq;

    rc = pthread_create(&pthread, NULL, reader, &ctxt);
    if (rc) {
        rc = -1;
        goto err;
    }

    for (;;) {
        msg_t msg;

        rc = mq_receive(mq, (char *) &msg, sizeof(msg), NULL);
        if (UNLIKELY(rc < 0))
            break;
        if (UNLIKELY(msg.data == 0))
            break;

        msg.parse_func(msg.filename, msg.data, (uint8_t *) msg.data + msg.size,
                       t, line, str);
        munmap(msg.data, msg.size);
    }

    rc = 0;
err:
    pthread_join(pthread, NULL);
    mq_close(mq);
    mq_unlink(mq_name);

    return rc;
}

static int cmpstr(const void *p1, const void *p2)
{
    return strcmp(*(char *const *) p1, *(char *const *) p2);
}

static void dump_bad_spellings(void)
{
    size_t i, j;
    char **bad_spellings_sorted;
    const size_t sz = bad_spellings * sizeof(char *);

    bad_spellings_sorted = malloc(sz);
    if (!bad_spellings_sorted)
        out_of_memory();

    for (i = 0, j = 0; i < SIZEOF_ARRAY(hash_bad_spellings); i++) {
        hash_entry_t *he = hash_bad_spellings[i];

        while (he) {
            hash_entry_t *next = he->next;
            bad_spellings_sorted[j++] = he->token;
            he = next;
        }
    }

    qsort(bad_spellings_sorted, j, sizeof(char *), cmpstr);

    for (i = 0; i < bad_spellings; i++) {
        const char *ptr = bad_spellings_sorted[i];
        hash_entry_t *const he = (hash_entry_t *) (ptr - sizeof(hash_entry_t));
        char ch;

        while ((ch = *(ptr++)))
            putchar(ch);
        putchar('\n');

        free(he);
    }

    free(bad_spellings_sorted);
}

static inline void load_printfs(void)
{
    for (size_t i = 0; i < SIZEOF_ARRAY(printfs); i++) {
        add_word(printfs[i], printf_nodes, printf_node_heap,
                 &printf_node_heap_next, PRINTK_NODES_HEAP_SIZE);
    }
}

static void set_is_not_whitespace(void)
{
    memset(is_not_whitespace, true, sizeof(is_not_whitespace));
    is_not_whitespace[' '] = false;
    is_not_whitespace['\t'] = false;
}

static void set_is_not_identifier(void)
{
    (void) memset(is_not_identifier, true, sizeof(is_not_identifier));
    for (size_t i = 0; i < 26; i++) {
        is_not_identifier[i + 'a'] = false;
        is_not_identifier[i + 'A'] = false;
    }
    for (size_t i = 0; i < 10; i++) {
        is_not_identifier[i + '0'] = false;
    }
    is_not_identifier['_'] = false;
}

/* Scan C source files to locate printf-style statements. */
/* TODO: exclude strings in 'getopt' */
int main(int argc, char **argv)
{
    token_t t, line, str;
    static char buffer[65536];

    token_cat = token_cat_normal;

    opt_flags |= (OPT_CHECK_WORDS | OPT_ESCAPE_STRIP | OPT_FORMAT_STRIP |
                  OPT_LITERAL_STRINGS | OPT_PARSE_STRINGS);
    opt_flags &= ~OPT_SOURCE_NAME;

    set_is_not_whitespace();
    set_is_not_identifier();

    set_mapping();
    load_printfs();
    qsort(formats, SIZEOF_ARRAY(formats), sizeof(format_t), cmp_format);
    if (opt_flags & OPT_CHECK_WORDS) {
        int ret = 0;

        for (size_t i = 0; i < ARRAY_SIZE(dictionary_paths); i++) {
            if (read_dictionary(dictionary_paths[i]) < 0) {
                ret = -1;
                break;
            }
        }

        if (ret) {
            fprintf(stderr, "No dictionary found.\n");
            exit(EXIT_FAILURE);
        }
    }

    token_new(&t);
    token_new(&line);
    token_new(&str);

    fflush(stdout);
    setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));

    if (argc == optind) {
        parse_path(".", &t, &line, &str);
        optind++;
    } else {
        while (argc > optind) {
            parse_path(argv[optind], &t, &line, &str);
            optind++;
        }
    }

    token_free(&str);
    token_free(&line);
    token_free(&t);

    dump_bad_spellings();

    printf("%" PRIu32
           " lines scanned (%.3f"
           "M bytes)\n",
           lines, (float) bytes_total / (float) (1024 * 1024));
    printf("%zu printf style statements being processed\n",
           SIZEOF_ARRAY(printfs));
    if (bad_spellings)
        printf("%" PRIu32 " unique bad spellings found (%" PRIu32
               " non-unique)\n",
               bad_spellings, bad_spellings_total);

    fflush(stdout);

    /* TODO: Report if format string is invalid */
    if (bad_spellings)
        return -1;
    exit(EXIT_SUCCESS);
}
