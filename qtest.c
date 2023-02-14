/* Implementation of testing code for queue code */

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* strcasecmp */
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach/mach_time.h>
#else /* Assume POSIX environments */
#include <time.h>
#endif

#include "dudect/fixture.h"
#include "list.h"
#include "random.h"

/* Shannon entropy */
extern double shannon_entropy(const uint8_t *input_data);
extern int show_entropy;

/* Our program needs to use regular malloc/free */
#define INTERNAL 1
#include "harness.h"

/* What character limit will be used for displaying strings? */
#define MAXSTRING 1024

/* How much padding should be added to check for string overrun? */
#define STRINGPAD MAXSTRING

/* It is a bit sketchy to use this #include file on the solution version of the
 * code.
 * OK as long as head field of queue_t structure is in first position in
 * solution code
 */
#include "queue.h"

#include "console.h"
#include "report.h"

/* Settable parameters */

#define HISTORY_LEN 20

/* How large is a queue before it's considered big.
 * This affects how it gets printed
 * and whether cautious mode is used when freeing the queue
 */
#define BIG_LIST_SIZE 30

/* Global variables */

typedef struct {
    struct list_head head;
    int size;
} queue_chain_t;

static queue_chain_t chain = {.size = 0};
static queue_contex_t *current = NULL;

/* How many times can queue operations fail */
static int fail_limit = BIG_LIST_SIZE;
static int fail_count = 0;

static int string_length = MAXSTRING;

#define MIN_RANDSTR_LEN 5
#define MAX_RANDSTR_LEN 10
static const char charset[] = "abcdefghijklmnopqrstuvwxyz";

/* Forward declarations */
static bool q_show(int vlevel);

static bool do_free(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    bool ok = true;
    if (!chain.size || !current || !current->q) {
        report(3,
               "Warning: There is no available queue or calling free on null "
               "queue");
    }
    error_check();

    if (current && current->size > BIG_LIST_SIZE)
        set_cautious_mode(false);

    struct list_head *qnext = NULL;
    if (chain.size > 1) {
        qnext = ((uintptr_t) &current->chain.next == (uintptr_t) &chain.head)
                    ? chain.head.next
                    : current->chain.next;
    }

    if (current) {
        list_del(&current->chain);

        if (exception_setup(true))
            q_free(current->q);
        exception_cancel();
        set_cautious_mode(true);
    }

    if (current) {
        free(current);
        chain.size--;
        current = qnext ? list_entry(qnext, queue_contex_t, chain) : NULL;
    }

    q_show(3);

    size_t bcnt = allocation_check();
    if (bcnt > 0) {
        report(1, "ERROR: Freed queue, but %lu blocks are still allocated",
               bcnt);
        ok = false;
    }

    return ok && !error_check();
}

static bool do_new(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    bool ok = true;

    if (exception_setup(true)) {
        queue_contex_t *qctx = malloc(sizeof(queue_contex_t));
        list_add_tail(&qctx->chain, &chain.head);

        qctx->size = 0;
        qctx->q = q_new();
        qctx->id = chain.size++;

        current = qctx;
    }
    exception_cancel();
    q_show(3);

    return ok && !error_check();
}

/* TODO: Add a buf_size check of if the buf_size may be less
 * than MIN_RANDSTR_LEN.
 */
static void fill_rand_string(char *buf, size_t buf_size)
{
    size_t len = 0;
    while (len < MIN_RANDSTR_LEN)
        len = rand() % buf_size;

    randombytes((uint8_t *) buf, len);
    for (size_t n = 0; n < len; n++)
        buf[n] = charset[buf[n] % (sizeof(charset) - 1)];
    buf[len] = '\0';
}

/* insert head */
static bool do_ih(int argc, char *argv[])
{
    if (simulation) {
        if (argc != 1) {
            report(1, "%s does not need arguments in simulation mode", argv[0]);
            return false;
        }
        bool ok = is_insert_head_const();
        if (!ok) {
            report(1,
                   "ERROR: Probably not constant time or wrong implementation");
            return false;
        }
        report(1, "Probably constant time");
        return ok;
    }

    char *lasts = NULL;
    char randstr_buf[MAX_RANDSTR_LEN];
    int reps = 1;
    bool ok = true, need_rand = false;
    if (argc != 2 && argc != 3) {
        report(1, "%s needs 1-2 arguments", argv[0]);
        return false;
    }

    char *inserts = argv[1];
    if (argc == 3) {
        if (!get_int(argv[2], &reps)) {
            report(1, "Invalid number of insertions '%s'", argv[2]);
            return false;
        }
    }

    if (!strcmp(inserts, "RAND")) {
        need_rand = true;
        inserts = randstr_buf;
    }

    if (!current || !current->q)
        report(3, "Warning: Calling insert head on null queue");
    error_check();

    if (current && exception_setup(true)) {
        for (int r = 0; ok && r < reps; r++) {
            if (need_rand)
                fill_rand_string(randstr_buf, sizeof(randstr_buf));
            bool rval = q_insert_head(current->q, inserts);
            if (rval) {
                current->size++;
                char *cur_inserts =
                    list_entry(current->q->next, element_t, list)->value;
                if (!cur_inserts) {
                    report(1, "ERROR: Failed to save copy of string in queue");
                    ok = false;
                } else if (r == 0 && inserts == cur_inserts) {
                    report(1,
                           "ERROR: Need to allocate and copy string for new "
                           "queue element");
                    ok = false;
                    break;
                } else if (r == 1 && lasts == cur_inserts) {
                    report(1,
                           "ERROR: Need to allocate separate string for each "
                           "queue element");
                    ok = false;
                    break;
                }
                lasts = cur_inserts;
            } else {
                fail_count++;
                if (fail_count < fail_limit)
                    report(2, "Insertion of %s failed", inserts);
                else {
                    report(1,
                           "ERROR: Insertion of %s failed (%d failures total)",
                           inserts, fail_count);
                    ok = false;
                }
            }
            ok = ok && !error_check();
        }
    }
    exception_cancel();

    q_show(3);
    return ok;
}

/* insert tail */
static bool do_it(int argc, char *argv[])
{
    if (simulation) {
        if (argc != 1) {
            report(1, "%s does not need arguments in simulation mode", argv[0]);
            return false;
        }
        bool ok = is_insert_tail_const();
        if (!ok) {
            report(1,
                   "ERROR: Probably not constant time or wrong implementation");
            return false;
        }
        report(1, "Probably constant time");
        return ok;
    }

    char randstr_buf[MAX_RANDSTR_LEN];
    int reps = 1;
    bool ok = true, need_rand = false;
    if (argc != 2 && argc != 3) {
        report(1, "%s needs 1-2 arguments", argv[0]);
        return false;
    }

    char *inserts = argv[1];
    if (argc == 3) {
        if (!get_int(argv[2], &reps)) {
            report(1, "Invalid number of insertions '%s'", argv[2]);
            return false;
        }
    }

    if (!strcmp(inserts, "RAND")) {
        need_rand = true;
        inserts = randstr_buf;
    }

    if (!current || !current->q)
        report(3, "Warning: Calling insert tail on null queue");
    error_check();

    if (current && exception_setup(true)) {
        for (int r = 0; ok && r < reps; r++) {
            if (need_rand)
                fill_rand_string(randstr_buf, sizeof(randstr_buf));
            bool rval = q_insert_tail(current->q, inserts);
            if (rval) {
                current->size++;
                char *cur_inserts =
                    list_entry(current->q->prev, element_t, list)->value;
                if (!cur_inserts) {
                    report(1, "ERROR: Failed to save copy of string in queue");
                    ok = false;
                }
            } else {
                fail_count++;
                if (fail_count < fail_limit)
                    report(2, "Insertion of %s failed", inserts);
                else {
                    report(1,
                           "ERROR: Insertion of %s failed (%d failures total)",
                           inserts, fail_count);
                    ok = false;
                }
            }
            ok = ok && !error_check();
        }
    }
    exception_cancel();
    q_show(3);
    return ok;
}

static bool do_remove(int option, int argc, char *argv[])
{
    // option 0 is for remove head; option 1 is for remove tail

    /* FIXME: It is known that both functions is_remove_tail_const() and
     * is_remove_head_const() can not pass dudect on Apple M1 (based on Arm64).
     * We shall figure out the exact reasons and resolve later.
     */
#if !(defined(__aarch64__) && defined(__APPLE__))
    if (simulation) {
        if (argc != 1) {
            report(1, "%s does not need arguments in simulation mode", argv[0]);
            return false;
        }
        bool ok = option ? is_remove_tail_const() : is_remove_head_const();
        if (!ok) {
            report(1,
                   "ERROR: Probably not constant time or wrong implementation");
            return false;
        }
        report(1, "Probably constant time");
        return ok;
    }
#endif

    if (argc != 1 && argc != 2) {
        report(1, "%s needs 0-1 arguments", argv[0]);
        return false;
    }

    char *removes = malloc(string_length + STRINGPAD + 1);
    if (!removes) {
        report(1,
               "INTERNAL ERROR.  Could not allocate space for removed strings");
        return false;
    }

    char *checks = malloc(string_length + 1);
    if (!checks) {
        report(1,
               "INTERNAL ERROR.  Could not allocate space for removed strings");
        free(removes);
        return false;
    }

    bool check = argc > 1;
    bool ok = true;
    if (check) {
        strncpy(checks, argv[1], string_length + 1);
        checks[string_length] = '\0';
    }

    removes[0] = '\0';
    memset(removes + 1, 'X', string_length + STRINGPAD - 1);
    removes[string_length + STRINGPAD] = '\0';

    if (!current || !current->size)
        report(3, "Warning: Calling remove head on empty queue");
    error_check();

    element_t *re = NULL;
    if (current && exception_setup(true))
        re = option ? q_remove_tail(current->q, removes, string_length + 1)
                    : q_remove_head(current->q, removes, string_length + 1);
    exception_cancel();

    bool is_null = re ? false : true;

    if (!is_null) {
        // q_remove_head and q_remove_tail are not responsible for releasing
        // node
        q_release_element(re);

        removes[string_length + STRINGPAD] = '\0';
        if (removes[0] == '\0') {
            report(1, "ERROR: Failed to store removed value");
            ok = false;
        }

        /* Check whether padding in array removes are still initial value 'X'.
         * If there's other character in padding, it's overflowed.
         */
        int i = string_length + 1;
        while ((i < string_length + STRINGPAD) && (removes[i] == 'X'))
            i++;
        if (i != string_length + STRINGPAD) {
            report(1,
                   "ERROR: copying of string in remove_head overflowed "
                   "destination buffer.");
            ok = false;
        } else {
            report(2, "Removed %s from queue", removes);
        }
        current->size--;
    } else {
        fail_count++;
        if (!check && fail_count < fail_limit) {
            report(2, "Removal from queue failed");
        } else {
            report(1, "ERROR: Removal from queue failed (%d failures total)",
                   fail_count);
            ok = false;
        }
    }

    if (ok && check && strcmp(removes, checks)) {
        report(1, "ERROR: Removed value %s != expected value %s", removes,
               checks);
        ok = false;
    }

    q_show(3);

    free(removes);
    free(checks);
    return ok && !error_check();
}

static inline bool do_rh(int argc, char *argv[])
{
    return do_remove(0, argc, argv);
}

static inline bool do_rt(int argc, char *argv[])
{
    return do_remove(1, argc, argv);
}

static bool do_dedup(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    LIST_HEAD(l_copy);
    element_t *item = NULL, *tmp = NULL;

    // Copy current->q to l_copy
    if (current->q && !list_empty(current->q)) {
        list_for_each_entry (item, current->q, list) {
            size_t slen;
            tmp = malloc(sizeof(element_t));
            if (!tmp)
                break;
            INIT_LIST_HEAD(&tmp->list);
            slen = strlen(item->value) + 1;
            tmp->value = malloc(slen);
            if (!tmp->value) {
                free(tmp);
                break;
            }
            memcpy(tmp->value, item->value, slen);
            list_add_tail(&tmp->list, &l_copy);
        }
        // Return false if the loop does not leave properly
        if (&item->list != current->q) {
            list_for_each_entry_safe (item, tmp, &l_copy, list) {
                free(item->value);
                free(item);
            }
            report(1,
                   "INTERNAL ERROR.  Could not allocate space for "
                   "duplicate checking");
            return false;
        }
    }

    bool ok = true;
    if (exception_setup(true))
        ok = q_delete_dup(current->q);
    exception_cancel();

    if (!ok) {
        list_for_each_entry_safe (item, tmp, &l_copy, list) {
            free(item->value);
            free(item);
        }
        report(1, "ERROR: Calling delete duplicate on null queue");
        return false;
    }

    struct list_head *l_tmp = current->q->next;
    bool is_this_dup = false;
    // Compare between new list and old one
    list_for_each_entry (item, &l_copy, list) {
        // Skip comparison with new list if the string is duplicate
        bool is_next_dup =
            item->list.next != &l_copy &&
            strcmp(list_entry(item->list.next, element_t, list)->value,
                   item->value) == 0;
        if (is_this_dup || is_next_dup) {
            // Update list size
            current->size--;
        } else if (l_tmp != current->q &&
                   strcmp(list_entry(l_tmp, element_t, list)->value,
                          item->value) == 0)
            l_tmp = l_tmp->next;
        else
            ok = false;
        is_this_dup = is_next_dup;
    }
    // All elements in new list should be traversed
    ok = ok && l_tmp == current->q;
    if (!ok)
        report(1,
               "ERROR: Duplicate strings are in queue or distinct strings are "
               "not in queue");

    list_for_each_entry_safe (item, tmp, &l_copy, list) {
        free(item->value);
        free(item);
    }

    q_show(3);
    return ok && !error_check();
}

static bool do_reverse(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    if (!current || !current->q)
        report(3, "Warning: Calling reverse on null queue");
    error_check();

    set_noallocate_mode(true);
    if (current && exception_setup(true))
        q_reverse(current->q);
    exception_cancel();

    set_noallocate_mode(false);
    q_show(3);
    return !error_check();
}

static bool do_size(int argc, char *argv[])
{
    if (argc != 1 && argc != 2) {
        report(1, "%s takes 0-1 arguments", argv[0]);
        return false;
    }

    int reps = 1;
    bool ok = true;
    if (argc != 1 && argc != 2) {
        report(1, "%s needs 0-1 arguments", argv[0]);
        return false;
    }

    if (argc == 2) {
        if (!get_int(argv[1], &reps))
            report(1, "Invalid number of calls to size '%s'", argv[2]);
    }

    int cnt = 0;
    if (!current || !current->q)
        report(3, "Warning: Calling size on null queue");
    error_check();

    if (current && exception_setup(true)) {
        for (int r = 0; ok && r < reps; r++) {
            cnt = q_size(current->q);
            ok = ok && !error_check();
        }
    }
    exception_cancel();

    if (current && ok) {
        if (current->size == cnt) {
            report(2, "Queue size = %d", cnt);
        } else {
            report(1,
                   "ERROR: Computed queue size as %d, but correct value is %d",
                   cnt, (int) current->size);
            ok = false;
        }
    }

    q_show(3);

    return ok && !error_check();
}

bool do_sort(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    int cnt = 0;
    if (!current || !current->q)
        report(3, "Warning: Calling sort on null queue");
    else
        cnt = q_size(current->q);
    error_check();

    if (cnt < 2)
        report(3, "Warning: Calling sort on single node");
    error_check();

    set_noallocate_mode(true);
    if (current && exception_setup(true))
        q_sort(current->q);
    exception_cancel();
    set_noallocate_mode(false);

    bool ok = true;
    if (current && current->size) {
        for (struct list_head *cur_l = current->q->next;
             cur_l != current->q && --cnt; cur_l = cur_l->next) {
            /* Ensure each element in ascending order */
            /* FIXME: add an option to specify sorting order */
            element_t *item, *next_item;
            item = list_entry(cur_l, element_t, list);
            next_item = list_entry(cur_l->next, element_t, list);
            if (strcmp(item->value, next_item->value) > 0) {
                report(1, "ERROR: Not sorted in ascending order");
                ok = false;
                break;
            }
        }
    }

    q_show(3);
    return ok && !error_check();
}

static bool do_dm(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    if (!current || !current->q)
        report(3, "Warning: Try to access null queue");
    error_check();

    bool ok = true;
    if (exception_setup(true))
        ok = q_delete_mid(current->q);
    exception_cancel();

    current->size--;
    q_show(3);
    return ok && !error_check();
}

static bool do_swap(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    if (!current || !current->q)
        report(3, "Warning: Try to access null queue");
    error_check();

    set_noallocate_mode(true);
    if (exception_setup(true))
        q_swap(current->q);
    exception_cancel();

    set_noallocate_mode(false);

    q_show(3);
    return !error_check();
}

static bool do_descend(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes too much arguments", argv[0]);
        return false;
    }

    if (!current || !current->q)
        report(3, "Warning: Calling ascend on null queue");
    error_check();


    int cnt = q_size(current->q);
    if (cnt < 2)
        report(3, "Warning: Calling ascend on single node");
    error_check();

    if (exception_setup(true))
        current->size = q_descend(current->q);
    set_noallocate_mode(false);

    bool ok = true;

    cnt = current->size;
    if (current->size) {
        for (struct list_head *cur_l = current->q->next;
             cur_l != current->q && --cnt; cur_l = cur_l->next) {
            element_t *item, *next_item;
            item = list_entry(cur_l, element_t, list);
            next_item = list_entry(cur_l->next, element_t, list);
            if (strcmp(item->value, next_item->value) < 0) {
                report(1,
                       "ERROR: There is at least on nodes did not follow the "
                       "ordering rule");
                ok = false;
                break;
            }
        }
    }

    q_show(3);
    return ok && !error_check();
}

static bool do_reverseK(int argc, char *argv[])
{
    int k = 0;

    if (!current || !current->q)
        report(3, "Warning: Calling reverseK on null queue");
    error_check();

    if (argc == 2) {
        if (!get_int(argv[1], &k)) {
            report(1, "Invalid number of K");
            return false;
        }
    } else {
        report(1, "Invalid number of arguments for reverseK");
        return false;
    }

    set_noallocate_mode(true);
    if (exception_setup(true))
        q_reverseK(current->q, k);
    exception_cancel();

    set_noallocate_mode(false);
    q_show(3);
    return !error_check();
}

static bool do_merge(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    if (!current || !current->q) {
        report(3, "Warning: Calling merge on null queue");
        return false;
    }
    error_check();

    int len = 0;
    set_noallocate_mode(true);
    if (current && exception_setup(true))
        len = q_merge(&chain.head);
    exception_cancel();
    set_noallocate_mode(false);

    if (q_size(&chain.head) > 1) {
        chain.size = 1;
        current = list_entry(chain.head.next, queue_contex_t, chain);
        current->size = len;

        struct list_head *cur = chain.head.next->next;
        while ((uintptr_t) cur != (uintptr_t) &chain.head) {
            queue_contex_t *ctx = list_entry(cur, queue_contex_t, chain);
            cur = cur->next;
            q_free(ctx->q);
            free(ctx);
        }

        chain.head.prev = &current->chain;
        current->chain.next = &chain.head;
    }

    bool ok = true;
    if (current && current->size) {
        for (struct list_head *cur_l = current->q->next;
             cur_l != current->q && --len; cur_l = cur_l->next) {
            /* Ensure each element in ascending order */
            element_t *item, *next_item;
            item = list_entry(cur_l, element_t, list);
            next_item = list_entry(cur_l->next, element_t, list);
            if (strcmp(item->value, next_item->value) > 0) {
                report(1,
                       "ERROR: Not sorted in ascending order (It might because "
                       "of unsorted queues are merged or there're some flaws "
                       "in 'q_merge')");
                ok = false;
                break;
            }
        }
    }

    q_show(3);
    return ok && !error_check();
}

static bool is_circular()
{
    struct list_head *cur = current->q->next;
    while (cur != current->q) {
        if (!cur)
            return false;
        cur = cur->next;
    }

    cur = current->q->prev;
    while (cur != current->q) {
        if (!cur)
            return false;
        cur = cur->prev;
    }
    return true;
}

static bool q_show(int vlevel)
{
    bool ok = true;
    if (verblevel < vlevel)
        return true;

    int cnt = 0;
    if (!current || !current->q) {
        report(vlevel, "l = NULL");
        return true;
    }

    if (!is_circular()) {
        report(vlevel, "ERROR:  Queue is not doubly circular");
        return false;
    }

    report_noreturn(vlevel, "l = [");

    struct list_head *ori = current->q;
    struct list_head *cur = current->q->next;

    if (exception_setup(true)) {
        while (ok && ori != cur && cnt < current->size) {
            element_t *e = list_entry(cur, element_t, list);
            if (cnt < BIG_LIST_SIZE) {
                report_noreturn(vlevel, cnt == 0 ? "%s" : " %s", e->value);
                if (show_entropy) {
                    report_noreturn(
                        vlevel, "(%3.2f%%)",
                        shannon_entropy((const uint8_t *) e->value));
                }
            }
            cnt++;
            cur = cur->next;
            ok = ok && !error_check();
        }
    }
    exception_cancel();

    if (!ok) {
        report(vlevel, " ... ]");
        return false;
    }

    if (cur == ori) {
        if (cnt <= BIG_LIST_SIZE)
            report(vlevel, "]");
        else
            report(vlevel, " ... ]");
    } else {
        report(vlevel, " ... ]");
        report(vlevel, "ERROR:  Queue has more than %d elements",
               current->size);
        ok = false;
    }

    return ok;
}

static bool do_show(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    if (current)
        report(1, "Current queue ID: %d", current->id);

    return q_show(0);
}

static bool do_prev(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    if (!current) {
        report(3, "Warning: Try to operate null queue");
        return false;
    }

    struct list_head *prev;
    if (chain.size > 1) {
        prev = ((uintptr_t) chain.head.next == (uintptr_t) &current->chain)
                   ? chain.head.prev
                   : current->chain.prev;
        current = prev ? list_entry(prev, queue_contex_t, chain) : NULL;
    }

    return q_show(0);
}

static bool do_next(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    if (!current) {
        report(3, "Warning: Try to operate null queue");
        return false;
    }

    struct list_head *next;
    if (chain.size > 1) {
        next = ((uintptr_t) chain.head.prev == (uintptr_t) &current->chain)
                   ? chain.head.next
                   : current->chain.next;
        current = next ? list_entry(next, queue_contex_t, chain) : NULL;
    }

    return q_show(0);
}

static void console_init()
{
    ADD_COMMAND(new, "Create new queue", "");
    ADD_COMMAND(free, "Delete queue", "");
    ADD_COMMAND(prev, "Switch to previous queue", "");
    ADD_COMMAND(next, "Switch to next queue", "");
    ADD_COMMAND(ih,
                "Insert string str at head of queue n times. Generate random "
                "string(s) if str equals RAND. (default: n == 1)",
                "str [n]");
    ADD_COMMAND(it,
                "Insert string str at tail of queue n times. Generate random "
                "string(s) if str equals RAND. (default: n == 1)",
                "str [n]");
    ADD_COMMAND(
        rh,
        "Remove from head of queue. Optionally compare to expected value str",
        "[str]");
    ADD_COMMAND(
        rt,
        "Remove from tail of queue. Optionally compare to expected value str",
        "[str]");
    ADD_COMMAND(reverse, "Reverse queue", "");
    ADD_COMMAND(sort, "Sort queue in ascending order", "");
    ADD_COMMAND(size, "Compute queue size n times (default: n == 1)", "[n]");
    ADD_COMMAND(show, "Show queue contents", "");
    ADD_COMMAND(dm, "Delete middle node in queue", "");
    ADD_COMMAND(dedup, "Delete all nodes that have duplicate string", "");
    ADD_COMMAND(merge, "Merge all the queues into one sorted queue", "");
    ADD_COMMAND(swap, "Swap every two adjacent nodes in queue", "");
    ADD_COMMAND(descend,
                "Remove every node which has a node with a strictly greater "
                "value anywhere to the right side of it",
                "");
    ADD_COMMAND(reverseK, "Reverse the nodes of the queue 'K' at a time",
                "[K]");
    add_param("length", &string_length, "Maximum length of displayed string",
              NULL);
    add_param("malloc", &fail_probability, "Malloc failure probability percent",
              NULL);
    add_param("fail", &fail_limit,
              "Number of times allow queue operations to return false", NULL);
}

/* Signal handlers */
static void sigsegv_handler(int sig)
{
    /* Avoid possible non-reentrant signal function be used in signal handler */
    assert(write(1,
                 "Segmentation fault occurred.  You dereferenced a NULL or "
                 "invalid pointer",
                 73) == 73);
    /* Raising a SIGABRT signal to produce a core dump for debugging. */
    abort();
}

static void sigalrm_handler(int sig)
{
    trigger_exception(
        "Time limit exceeded.  Either you are in an infinite loop, or your "
        "code is too inefficient");
}

static void q_init()
{
    fail_count = 0;
    INIT_LIST_HEAD(&chain.head);
    signal(SIGSEGV, sigsegv_handler);
    signal(SIGALRM, sigalrm_handler);
}

static bool q_quit(int argc, char *argv[])
{
    return true;
    report(3, "Freeing queue");
    if (current && current->size > BIG_LIST_SIZE)
        set_cautious_mode(false);

    if (exception_setup(true)) {
        struct list_head *cur = chain.head.next;
        while (chain.size > 0) {
            queue_contex_t *qctx, *tmp;
            tmp = qctx = list_entry(cur, queue_contex_t, chain);
            cur = cur->next;
            q_free(qctx->q);
            free(tmp);
            chain.size--;
        }
    }

    exception_cancel();
    set_cautious_mode(true);

    size_t bcnt = allocation_check();
    if (bcnt > 0) {
        report(1, "ERROR: Freed queue, but %lu blocks are still allocated",
               bcnt);
        return false;
    }

    return true;
}

static void usage(char *cmd)
{
    printf("Usage: %s [-h] [-f IFILE][-v VLEVEL][-l LFILE]\n", cmd);
    printf("\t-h         Print this information\n");
    printf("\t-f IFILE   Read commands from IFILE\n");
    printf("\t-v VLEVEL  Set verbosity level\n");
    printf("\t-l LFILE   Echo results to LFILE\n");
    exit(0);
}

#define GIT_HOOK ".git/hooks/"
static bool sanity_check()
{
    struct stat buf;
    /* Directory .git not found */
    if (stat(".git", &buf)) {
        fprintf(stderr,
                "FATAL: You should run qtest in the directory containing valid "
                "git workspace.\n");
        return false;
    }
    /* Expected pre-commit and pre-push hooks not found */
    if (stat(GIT_HOOK "commit-msg", &buf) ||
        stat(GIT_HOOK "pre-commit", &buf) || stat(GIT_HOOK "pre-push", &buf)) {
        fprintf(stderr, "FATAL: Git hooks are not properly installed.\n");

        /* Attempt to install Git hooks */
        char *argv[] = {"sh", "-c", "scripts/install-git-hooks", NULL};
        extern char **environ;
        pid_t pid;
        int status = posix_spawn(&pid, "/bin/sh", NULL, NULL, argv, environ);
        if (status == 0) {
            /* Finally, succeed */
            if ((waitpid(pid, &status, 0) != -1) && (status == 0))
                return true;
            perror("waitpid");
        }
        return false;
    }
    return true;
}

uintptr_t os_random(uintptr_t seed)
{
    /* ASLR makes the address random */
    uintptr_t x = (uintptr_t) &os_random ^ seed;
#if defined(__APPLE__)
    x ^= (uintptr_t) mach_absolute_time();
#else
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    x ^= (uintptr_t) time.tv_sec;
    x ^= (uintptr_t) time.tv_nsec;
#endif
    /* Do a few randomization steps */
    uintptr_t max = ((x ^ (x >> 17)) & 0x0F) + 1;
    for (uintptr_t i = 0; i < max; i++)
        x = random_shuffle(x);
    assert(x);
    return x;
}

#define BUFSIZE 256
int main(int argc, char *argv[])
{
    /* sanity check for git hook integration */
    if (!sanity_check())
        return -1;

    /* To hold input file name */
    char buf[BUFSIZE];
    char *infile_name = NULL;
    char lbuf[BUFSIZE];
    char *logfile_name = NULL;
    int level = 4;
    int c;

    while ((c = getopt(argc, argv, "hv:f:l:")) != -1) {
        switch (c) {
        case 'h':
            usage(argv[0]);
            break;
        case 'f':
            strncpy(buf, optarg, BUFSIZE);
            buf[BUFSIZE - 1] = '\0';
            infile_name = buf;
            break;
        case 'v': {
            char *endptr;
            errno = 0;
            level = strtol(optarg, &endptr, 10);
            if (errno != 0 || endptr == optarg) {
                fprintf(stderr, "Invalid verbosity level\n");
                exit(EXIT_FAILURE);
            }
            break;
        }
        case 'l':
            strncpy(lbuf, optarg, BUFSIZE);
            buf[BUFSIZE - 1] = '\0';
            logfile_name = lbuf;
            break;
        default:
            printf("Unknown option '%c'\n", c);
            usage(argv[0]);
            break;
        }
    }

    /* A better seed can be obtained by combining getpid() and its parent ID
     * with the Unix time.
     */
    srand(os_random(getpid() ^ getppid()));

    q_init();
    init_cmd();
    console_init();

    /* Initialize linenoise only when infile_name not exist */
    if (!infile_name) {
        /* Trigger call back function(auto completion) */
        line_set_completion_callback(completion);

        line_history_set_max_len(HISTORY_LEN);
        line_hostory_load(HISTORY_FILE); /* Load the history at startup */
    }

    set_verblevel(level);
    if (level > 1)
        set_echo(true);
    if (logfile_name)
        set_logfile(logfile_name);

    add_quit_helper(q_quit);

    bool ok = true;
    ok = ok && run_console(infile_name);

    /* Do finish_cmd() before check whether ok is true or false */
    ok = finish_cmd() && ok;

    return !ok;
}
