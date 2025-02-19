# Contributing to `lab0-c`

:+1::tada: First off, thanks for taking the time to contribute! :tada::+1:

The following is a set of guidelines for contributing to [lab0-c](https://github.com/sysprog21/lab0-c)
hosted on GitHub. These are mostly guidelines, not rules.
Use your best judgment, and feel free to propose changes to this document in a pull request.

## Issues

This project uses GitHub Issues to track ongoing development, discuss project plans, and keep track of bugs.
Be sure to search for existing issues before you create another one.

Initially, it is advisable to create an issue on GitHub for bug reports, feature requests,
or substantial pull requests, as this offers a platform for discussion with both the community and project maintainers.

Engaging in a conversation through a GitHub issue before making a contribution is crucial to ensure the acceptance of your work.
We aim to prevent situations where significant effort is expended on a pull request that might not align with the project's design principles.
For example, it might turn out that the feature you propose is more suited as an independent module that complements this project,
in which case we would recommend that direction.

For minor corrections, such as typo fixes, small refactoring, or updates to documentation/comments,
filing an issue is not typically necessary.
What constitutes a "minor" fix involves discretion; however, examples include:
- Correcting spelling mistakes
- Minor code refactoring
- Updating or editing documentation and comments

Nevertheless, there may be instances where, upon reviewing your pull requests,
we might request an issue to be filed to facilitate discussion on broader design considerations.

Visit our [Issues page on GitHub](https://github.com/sysprog21/lab0-c/issues) to search and submit.

## Coding Convention

Contributions from developers across corporations, academia, and individuals are welcome.
However, participation requires adherence to fundamental ground rules:
* Code must strictly adhere to the established C coding style (refer to the guidelines below).
  While there is some flexibility in basic style, it is crucial to stick to the current coding standards.
  Complex algorithmic constructs without proper comments will not be accepted.
* External pull requests should include thorough documentation in the pull request comments for consideration.
* When composing documentation, code comments, and other materials in English,
  please adhere to the American English (`en_US`) dialect.
  This variant should be considered the standard for all documentation efforts.
  For instance, opt for "initialize" over "initialise" and "color" rather than "colour".

Software requirement: [clang-format](https://clang.llvm.org/docs/ClangFormat.html) version 18 or later.

This repository consistently contains an up-to-date `.clang-format` file with rules that match the explained ones.
For maintaining a uniform coding style, execute the command `clang-format -i *.[ch]`.

## Coding Style for Modern C

This coding style is a variant of the K&R style.
Adhere to established practices while being open to innovation.
Maintain consistency, adopt the latest C standards,
and embrace modern compilers along with their advanced static analysis capabilities and sanitizers.

### Indentation

In this coding style guide, the use of 4 spaces for indentation instead of tabs is strongly enforced to ensure consistency.
Consistently apply a single space before and after comparison and assignment operators to maintain readable code.
Additionally, it is crucial to include a single space after every comma.
e.g.,
```c
for (int i = 0; i < 10; i++) {
    printf("%d\n", i);
    /* some operations */
}
```

### Line length

All lines should typically stay within 80 characters, and longer lines should be wrapped.
There are valid rationales for this practice:
* It encourages concise code writing by developers.
* Smaller portions of information are easier for humans to process.
* It helps users of vi/vim (and potentially other editors) who use vertical splits.

### Comments

Multi-line comments shall have the opening and closing characters
in a separate line, with the lines containing the content prefixed by a space
and the `*` characters for alignment, e.g.,
```c
/*
 * This is a multi-line comment.
 */

/* One line comment. */
```

Use multi-line comments for more elaborative descriptions or before more
significant logical block of code.

Single-line comments shall be written in C89 style:
```c
    return (uintptr_t) val;  /* return a bitfield */
```

Leave two spaces between the statement and the inline comment.

### Spacing and brackets

Use one space after the conditional or loop keyword, no spaces around
their brackets, and one space before the opening curly bracket. e.g.,
```c
do {
    /* some operations */
} while (condition);
```

Functions (their declarations or calls), `sizeof` operator or similar
macros shall not have a space after their name/keyword or around the
brackets, e.g.,
```c
unsigned total_len = offsetof(obj_t, items[n]);
unsigned obj_len = sizeof(obj_t);
```

Use brackets to avoid ambiguity and with operators such as `sizeof`,
but otherwise avoid redundant or excessive brackets.

### Variable names and declarations

- Ensure that functions, variables, and comments are consistently named using English names/text.

- Use descriptive names for global variables and short names for locals.
Find the right balance between descriptive and succinct.

- Use [snakecase](https://en.wikipedia.org/wiki/Snake_case).
Do not use "camelcase".

- Do not use Hungarian notation or other unnecessary prefixing or suffixing.

- Use the following spacing for pointers:
```c
const char *name;  /* const pointer; '*' with the name and space before it */
conf_t * const cfg;  /* pointer to a const data; spaces around 'const' */
const uint8_t * const charmap;  /* const pointer and const data */
const void * restrict key;  /* const pointer which does not alias */
```

- Local variables of the same type should be declared in the same line.
```c
void func(void)
{
    char a, b; /* OK */

    char a;
    char b;    /* Incorrect: A variable with char type already exists. */
}
```

- Always include a trailing comma in the last element of structure initialization, including its children, to assist clang-format in correctly formatting structures. However, this can be omitted in very simple and short structures.
```c
typedef struct {
    int width, height;
} screen_t;

screen_t s = {
    .width = 640,
    .height = 480,   /* comma here */
}
```

### Type definitions

Declarations shall be on the same line, e.g.,
```c
typedef void (*dir_iter_t)(void *, const char *, struct dirent *);
```

_Typedef_ structures rather than pointers.  Note that structures can be kept
opaque if they are not dereferenced outside the translation unit where they
are defined.  Pointers can be _typedefed_ only if there is a very compelling
reason.

New types may be suffixed with `_t`.  Structure name, when used within the
translation unit, may be omitted, e.g.:

```c
typedef struct {
    unsigned if_index;
    unsigned addr_len;
    addr_t next_hop;
} route_info_t;
```

### Initialization

Embrace C99 structure initialization where reasonable, e.g.,
```c
static const crypto_ops_t openssl_ops = {
    .create = openssl_crypto_create,
    .destroy = openssl_crypto_destroy,
    .encrypt = openssl_crypto_encrypt,
    .decrypt = openssl_crypto_decrypt,
    .hmac = openssl_crypto_hmac,
};
```

Embrace C99 array initialization, especially for the state machines, e.g.,
```c
static const uint8_t tcp_fsm[TCP_NSTATES][2][TCPFC_COUNT] = {
    [TCPS_CLOSED] = {
        [FLOW_FORW] = {
            /* Handshake (1): initial SYN. */
            [TCPFC_SYN]	= TCPS_SYN_SENT,
        },
    },
    ...
}
```

### Control structures

Try to make the control flow easy to follow.  Avoid long convoluted logic
expressions; try to split them where possible (into inline functions,
separate if-statements, etc).

The control structure keyword and the expression in the brackets should be
separated by a single space.  The opening curly bracket shall be in the
same line, also separated by a single space.  Example:

```c
    for (;;) {
        obj = get_first();
        while ((obj = get_next(obj))) {
            ...
        }
        if (done)
            break;
    }
```

Do not add inner spaces around the brackets. There should be one space after
the semicolon when `for` has expressions:
```c
    for (unsigned i = 0; i < __arraycount(items); i++) {
        ...
    }
```

#### Avoid unnecessary nesting levels

Avoid:
```c
int inspect(obj_t *obj)
{
    if (cond) {
        ...
        /* long code block */
        ...
        return 0;
    }
    return -1;
}
```

Consider:
```c
int inspect(obj_t *obj)
{
    if (!cond)
        return -1;

    ...
    return 0;
}
```

However, do not make logic more convoluted.

### `if` statements

Curly brackets and spacing follow the K&R style:
```c
    if (a == b) {
        ..
    } else if (a < b) {
        ...
    } else {
        ...
    }
```

Simple and succinct one-line if-statements may omit curly brackets:
```c
    if (!valid)
        return -1;
```

However, do prefer curly brackets with multi-line or more complex statements.
If one branch uses curly brackets, then all other branches shall use the
curly brackets too.

Wrap long conditions to the if-statement indentation adding extra 4 spaces:
```c
    if (some_long_expression &&
        another_expression) {
        ...
    }
```

#### Avoid redundant `else`

Avoid:
```c
    if (flag & F_FEATURE_X) {
        ...
        return 0;
    } else {
        return -1;
    }
```

Consider:
```c
    if (flag & F_FEATURE_X) {
        ...
        return 0;
    }
    return -1;
```

### `switch` statements

Switch statements should have the `case` blocks at the same indentation
level, e.g.:
```c
    switch (expr) {
    case A:
        ...
        break;
    case B:
        /* fallthrough */
    case C:
        ...
        break;
    }
```

If the case block does not break, then it is strongly recommended to add a
comment containing "fallthrough" to indicate it.  Modern compilers can also
be configured to require such comment (see gcc `-Wimplicit-fallthrough`).

### Function definitions

The opening and closing curly brackets shall also be in the separate lines (K&R style).

```c
ssize_t hex_write(FILE *stream, const void *buf, size_t len)
{
    ...
}
```

Do not use old style K&R style C definitions.

### Object abstraction

Objects are often "simulated" by the C programmers with a `struct` and
its "public API".  To enforce the information hiding principle, it is a
good idea to define the structure in the source file (translation unit)
and provide only the _declaration_ in the header.  For example, `obj.c`:

```c
#include "obj.h"

struct obj {
    int value;
}

obj_t *obj_create(void)
{
    return calloc(1, sizeof(obj_t));
}

void obj_destroy(obj_t *obj)
{
    free(obj);
}
```

With an example `obj.h`:
```c
#ifndef _OBJ_H_
#define _OBJ_H_

typedef struct obj;

obj_t *obj_create(void);
void obj_destroy(obj_t *);

#endif
```

Such structuring will prevent direct access of the `obj_t` members outside
the `obj.c` source file.  The implementation (of such "class" or "module")
may be large and abstracted within separate source files.  In such case,
consider separating structures and "methods" into separate headers (think of
different visibility), for example `obj_impl.h` (private) and `obj.h` (public).

Consider `crypto_impl.h`:
```c
#ifndef _CRYPTO_IMPL_H_
#define _CRYPTO_IMPL_H_

#if !defined(__CRYPTO_PRIVATE)
#error "only to be used by the crypto modules"
#endif

#include "crypto.h"

typedef struct crypto {
    crypto_cipher_t cipher;
    void *key;
    size_t key_len;
    ...
}
...

#endif
```

And `crypto.h` (public API):

```c
#ifndef _CRYPTO_H_
#define _CRYPTO_H_

typedef struct crypto crypto_t;

crypto_t *crypto_create(crypto_cipher_t);
void crypto_destroy(crypto_t *);
...

#endif
```

### Use reasonable types

Use `unsigned` for general iterators; use `size_t` for general sizes; use
`ssize_t` to return a size which may include an error.  Of course, consider
possible overflows.

Avoid using `uint8_t` or `uint16_t` or other sub-word types for general
iterators and similar cases, unless programming for micro-controllers or
other constrained environments.

C has rather peculiar _type promotion rules_ and unnecessary use of sub-word
types might contribute to a bug once in a while.

### Embrace portability

#### Byte-order

Do not assume x86 or little-endian architecture.  Use endian conversion
functions for operating the on-disk and on-the-wire structures or other
cases where it is appropriate.

#### Types

- Do not assume a particular 32-bit vs 64-bit architecture, e.g., do not
assume the size of `long` or `unsigned long`.  Use `int64_t` or `uint64_t`
for the 8-byte integers.

- Do not assume `char` is signed; for example, on Arm it is unsigned.

- Use C99 macros for constant prefixes or formatting of the fixed-width
types.

Use:
```c
#define	SOME_CONSTANT (UINT64_C(1) << 48)
printf("val %" PRIu64 "\n", SOME_CONSTANT);
```

Do not use:
```c
#define	SOME_CONSTANT (1ULL << 48)
printf("val %lld\n", SOME_CONSTANT);
```

#### Avoid unaligned access

Avoid assuming that unaligned access is safe.
It is not secure on architectures like Arm, POWER, and others.
Additionally, even on x86, unaligned access can be slower.

#### Avoid extreme portability

Unless programming for micro-controllers or exotic CPU architectures,
focus on the common denominator of the modern CPU architectures, avoiding
the very maximum portability which can make the code unnecessarily cumbersome.

Some examples:
- It is fair to assume `sizeof(int) == 4` since it is the case on all modern
mainstream architectures.  PDP-11 era is long gone.
- Using `1U` instead of `UINT32_C(1)` or `(uint32_t) 1` is also fine.
- It is fair to assume that `NULL` is matching `(uintptr_t) 0` and it is fair
to `memset()` structures with zero.  Non-zero `NULL` is for retro computing.

## References
- [Linux kernel coding style](https://www.kernel.org/doc/html/latest/process/coding-style.html)
- 1999, Brian W. Kernighan and Rob Pike, The Practice of Programming, Addison–Wesley.
- 1993, Bill Shannon, [C Style and Coding Standards for SunOS](https://devnull-cz.github.io/unix-linux-prog-in-c/cstyle.ms.pdf)
