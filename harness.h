#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>

/*
  This test harness enables us to do stringent testing of code.
  It overloads the library versions of malloc and free with ones that
  allow checking for common allocation errors.
*/

void *test_malloc(size_t size);
void test_free(void *p);
char *test_strdup(const char *s);

#ifdef INTERNAL
/* Report number of allocated blocks */
size_t allocation_check();

/* Probability of malloc failing, expressed as percent */
int fail_probability;

/*
  Set/unset cautious mode.
  In this mode, makes extra sure any block to be freed is currently allocated.
*/
void set_cautious_mode(bool cautious);

/*
  Set/unset restricted allocation mode.
  In this mode, calls to malloc and free are disallowed.
 */
void set_noallocate_mode(bool noallocate);


/*
  Return whether any errors have occurred since last time checked
 */
bool error_check();

/*
 * Prepare for a risky operation using setjmp.
 * Function returns true for initial return, false for error return
 */
bool exception_setup(bool limit_time);

/*
 * Call once past risky code
 */
void exception_cancel();

/*
 * Use longjmp to return to most recent exception setup.  Include error message
 */
void trigger_exception(char *msg);


#else
/* Tested program use our versions of malloc and free */
#define malloc test_malloc
#define free test_free
#define strdup test_strdup
#endif
