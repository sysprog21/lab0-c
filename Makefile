CC = gcc
CFLAGS = -O0 -g -Wall -Werror

all: qtest
	-tar -cf handin.tar queue.c queue.h

queue.o: queue.c queue.h harness.h
	$(CC) $(CFLAGS) -c queue.c 

qtest: qtest.c report.c console.c harness.c queue.o
	$(CC) $(CFLAGS) -o qtest qtest.c report.c console.c harness.c queue.o
	tar cf handin.tar queue.c queue.h

test: qtest driver.py
	chmod +x driver.py
	./driver.py

clean:
	rm -f *.o *~ qtest 
	rm -rf *.dSYM
	(cd traces; rm -f *~)

