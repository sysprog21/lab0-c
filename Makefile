CC = gcc
CFLAGS = -O1 -g -Wall -Werror -Idudect -I.

# Emit a warning should any variable-length array be found within the code.
CFLAGS += -Wvla

GIT_HOOKS := .git/hooks/applied
DUT_DIR := dudect
all: $(GIT_HOOKS) qtest

tid := 0

# Control test case option of valgrind
ifeq ("$(tid)","0")
    TCASE :=
else
    TCASE := -t $(tid)
endif

# Control the build verbosity
ifeq ("$(VERBOSE)","1")
    Q :=
    VECHO = @true
else
    Q := @
    VECHO = @printf
endif

# Enable sanitizer(s) or not
ifeq ("$(SANITIZER)","1")
    # https://github.com/google/sanitizers/wiki/AddressSanitizerFlags
    CFLAGS += -fsanitize=address -fno-omit-frame-pointer -fno-common
    LDFLAGS += -fsanitize=address
endif

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

OBJS := qtest.o report.o console.o harness.o queue.o \
        random.o dudect/constant.o dudect/fixture.o dudect/ttest.o \
        shannon_entropy.o \
        linenoise.o web.o

deps := $(OBJS:%.o=.%.o.d)

qtest: $(OBJS)
	$(VECHO) "  LD\t$@\n"
	$(Q)$(CC) $(LDFLAGS) -o $@ $^ -lm

%.o: %.c
	@mkdir -p .$(DUT_DIR)
	$(VECHO) "  CC\t$@\n"
	$(Q)$(CC) -o $@ $(CFLAGS) -c -MMD -MF .$@.d $<

check: qtest
	./$< -v 3 -f traces/trace-eg.cmd

test: qtest scripts/driver.py
	scripts/driver.py -c

valgrind_existence:
	@which valgrind 2>&1 > /dev/null || (echo "FATAL: valgrind not found"; exit 1)

valgrind: valgrind_existence
	# Explicitly disable sanitizer(s)
	$(MAKE) clean SANITIZER=0 qtest
	$(eval patched_file := $(shell mktemp /tmp/qtest.XXXXXX))
	cp qtest $(patched_file)
	chmod u+x $(patched_file)
	sed -i "s/alarm/isnan/g" $(patched_file)
	scripts/driver.py -p $(patched_file) --valgrind $(TCASE)
	@echo
	@echo "Test with specific case by running command:" 
	@echo "scripts/driver.py -p $(patched_file) --valgrind -t <tid>"

clean:
	rm -f $(OBJS) $(deps) *~ qtest /tmp/qtest.*
	rm -rf .$(DUT_DIR)
	rm -rf *.dSYM
	(cd traces; rm -f *~)

-include $(deps)
