CC = gcc
CFLAGS = -O0 -g -Wall -Werror

GIT_HOOKS := .git/hooks/applied
all: $(GIT_HOOKS) qtest

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

queue.o: queue.c queue.h harness.h
	$(CC) $(CFLAGS) -c queue.c 

qtest: qtest.c report.c console.c harness.c queue.o
	$(CC) $(CFLAGS) -o qtest qtest.c report.c console.c harness.c queue.o

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
	rm -f *.o *~ qtest /tmp/qtest.*
	rm -rf *.dSYM
	(cd traces; rm -f *~)

