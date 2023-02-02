/* Test support code */

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "report.h"

/* Our program needs to use regular malloc/free */
#define INTERNAL 1
#include "harness.h"

/** Special values **/

/* Value at start of every allocated block */
#define MAGICHEADER 0xdeadbeef

/* Value when deallocate block */
#define MAGICFREE 0xffffffff

/* Value at end of every block */
#define MAGICFOOTER 0xbeefdead

/* Byte to fill newly malloced space with */
#define FILLCHAR 0x55

/* Data structures used by our code */

/* Represent allocated blocks as doubly-linked list, with
 * next and prev pointers at beginning
 */
typedef struct __block_element {
    struct __block_element *next, *prev;
    size_t payload_size;
    size_t magic_header; /* Marker to see if block seems legitimate */
    unsigned char payload[0];
    /* Also place magic number at tail of every block */
} block_element_t;

static block_element_t *allocated = NULL;
static size_t allocated_count = 0;

/* Percent probability of malloc failure */
int fail_probability = 0;

static bool cautious_mode = true;
static bool noallocate_mode = false;
static bool error_occurred = false;
static char *error_message = "";

static int time_limit = 1;

/* Data for managing exceptions */
static jmp_buf env;
static volatile sig_atomic_t jmp_ready = false;
static bool time_limited = false;

/* Internal functions */

/* Should this allocation fail? */
static bool fail_allocation()
{
    double weight = (double) random() / RAND_MAX;
    return (weight < 0.01 * fail_probability);
}

/* Find header of block, given its payload.
 * Signal error if doesn't seem like legitimate block
 */
static block_element_t *find_header(void *p)
{
    if (!p) {
        report_event(MSG_ERROR, "Attempting to free null block");
        error_occurred = true;
    }

    block_element_t *b =
        (block_element_t *) ((size_t) p - sizeof(block_element_t));
    if (cautious_mode) {
        /* Make sure this is really an allocated block */
        block_element_t *ab = allocated;
        bool found = false;
        while (ab && !found) {
            found = ab == b;
            ab = ab->next;
        }
        if (!found) {
            report_event(MSG_ERROR,
                         "Attempted to free unallocated block.  Address = %p",
                         p);
            error_occurred = true;
        }
    }

    if (b->magic_header != MAGICHEADER) {
        report_event(
            MSG_ERROR,
            "Attempted to free unallocated or corrupted block.  Address = %p",
            p);
        error_occurred = true;
    }

    return b;
}

/* Given pointer to block, find its footer */
static size_t *find_footer(block_element_t *b)
{
    // cppcheck-suppress nullPointerRedundantCheck
    size_t *p =
        (size_t *) ((size_t) b + b->payload_size + sizeof(block_element_t));
    return p;
}

/* Implementation of application functions */

void *test_malloc(size_t size)
{
    if (noallocate_mode) {
        report_event(MSG_FATAL, "Calls to malloc disallowed");
        return NULL;
    }

    if (fail_allocation()) {
        report_event(MSG_WARN, "Malloc returning NULL");
        return NULL;
    }

    block_element_t *new_block =
        malloc(size + sizeof(block_element_t) + sizeof(size_t));
    if (!new_block) {
        report_event(MSG_FATAL, "Couldn't allocate any more memory");
        error_occurred = true;
    }

    // cppcheck-suppress nullPointerRedundantCheck
    new_block->magic_header = MAGICHEADER;
    // cppcheck-suppress nullPointerRedundantCheck
    new_block->payload_size = size;
    *find_footer(new_block) = MAGICFOOTER;
    void *p = (void *) &new_block->payload;
    memset(p, FILLCHAR, size);
    // cppcheck-suppress nullPointerRedundantCheck
    new_block->next = allocated;
    // cppcheck-suppress nullPointerRedundantCheck
    new_block->prev = NULL;

    if (allocated)
        allocated->prev = new_block;
    allocated = new_block;
    allocated_count++;

    return p;
}

// cppcheck-suppress unusedFunction
void *test_calloc(size_t nelem, size_t elsize)
{
    /* Reference: Malloc tutorial
     * https://danluu.com/malloc-tutorial/
     */
    size_t size = nelem * elsize;  // TODO: check for overflow
    void *ptr = test_malloc(size);
    memset(ptr, 0, size);
    return ptr;
}

void test_free(void *p)
{
    if (noallocate_mode) {
        report_event(MSG_FATAL, "Calls to free disallowed");
        return;
    }

    if (!p)
        return;

    block_element_t *b = find_header(p);
    size_t footer = *find_footer(b);
    if (footer != MAGICFOOTER) {
        report_event(MSG_ERROR,
                     "Corruption detected in block with address %p when "
                     "attempting to free it",
                     p);
        error_occurred = true;
    }
    b->magic_header = MAGICFREE;
    *find_footer(b) = MAGICFREE;
    memset(p, FILLCHAR, b->payload_size);

    /* Unlink from list */
    block_element_t *bn = b->next;
    block_element_t *bp = b->prev;
    if (bp)
        bp->next = bn;
    else
        allocated = bn;
    if (bn)
        bn->prev = bp;

    free(b);
    allocated_count--;
}

// cppcheck-suppress unusedFunction
char *test_strdup(const char *s)
{
    size_t len = strlen(s) + 1;
    void *new = test_malloc(len);
    if (!new)
        return NULL;

    return memcpy(new, s, len);
}

size_t allocation_check()
{
    return allocated_count;
}

/* Implementation of functions for testing */

/* Set/unset cautious mode.
 * In this mode, makes extra sure any block to be freed is currently allocated.
 */
void set_cautious_mode(bool cautious)
{
    cautious_mode = cautious;
}

/* Set/unset restricted allocation mode.
 * In this mode, calls to malloc and free are disallowed.
 */
void set_noallocate_mode(bool noallocate)
{
    noallocate_mode = noallocate;
}

/* Return whether any errors have occurred since last time set error limit */
bool error_check()
{
    bool e = error_occurred;
    error_occurred = false;
    return e;
}

/* Prepare for a risky operation using setjmp.
 * Function returns true for initial return, false for error return
 */
bool exception_setup(bool limit_time)
{
    if (sigsetjmp(env, 1)) {
        /* Got here from longjmp */
        jmp_ready = false;
        if (time_limited) {
            alarm(0);
            time_limited = false;
        }

        if (error_message)
            report_event(MSG_ERROR, error_message);
        error_message = "";
        return false;
    }

    /* Got here from initial call */
    jmp_ready = true;
    if (limit_time) {
        alarm(time_limit);
        time_limited = true;
    }
    return true;
}

/* Call once past risky code */
void exception_cancel()
{
    if (time_limited) {
        alarm(0);
        time_limited = false;
    }

    jmp_ready = false;
    error_message = "";
}

/* Use longjmp to return to most recent exception setup */
void trigger_exception(char *msg)
{
    error_occurred = true;
    error_message = msg;
    if (jmp_ready)
        siglongjmp(env, 1);
    else
        exit(1);
}
