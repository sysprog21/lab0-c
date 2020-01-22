CC = gcc
CFLAGS = -O0 -g -Wall -Werror

GIT_HOOKS := .git/hooks/applied
all: $(GIT_HOOKS) qtest

# Control the build verbosity
ifeq ("$(VERBOSE)","1")
    Q :=
    VECHO = @true
else
    Q := @
    VECHO = @printf
endif

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

OBJS := qtest.o report.o console.o harness.o queue.o
deps := $(OBJS:%.o=.%.o.d)

qtest: $(OBJS)
	$(VECHO) "  LD\t$@\n"
	$(Q)$(CC) $(LDFLAGS)  -o $@ $^

%.o: %.c
	$(VECHO) "  CC\t$@\n"
	$(Q)$(CC) -o $@ $(CFLAGS) -c -MMD -MF .$@.d $<


check: test
test: qtest scripts/driver.py
	scripts/driver.py

valgrind_existence:
	@which valgrind 2>&1 > /dev/null

valgrind: qtest valgrind_existence
	$(eval patched_file := $(shell mktemp /tmp/qtest.XXXXXX))
	cp qtest $(patched_file)
	chmod u+x $(patched_file)
	sed -i "s/alarm/isnan/g" $(patched_file)
	scripts/driver.py -p $(patched_file) --valgrind
	@echo
	@echo "Test with specific case by running command:" 
	@echo "scripts/driver.py -p $(patched_file) --valgrind -t <tid>"

clean:
	rm -f $(OBJS) $(deps) *~ qtest /tmp/qtest.*
	rm -rf *.dSYM
	(cd traces; rm -f *~)

-include $(deps)
