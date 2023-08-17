CFLAGS=-std=c11 -g -static
SRCS=main.c parse.c codegen.c token.c vector.c
OBJS=$(SRCS:.c=.o)

9cc: $(OBJS)
	$(CC) -o 9cc $(OBJS) $(LDFLAGS)

$(OBJS): 9cc.h

tester: 9cc tester.c
	./9cc tester.c > tester.s
	cc tester.s -o tester

test: 9cc tester
	./tester

clean:
	rm -f 9cc *.o *~ tmp*

.PHONY: test clean
