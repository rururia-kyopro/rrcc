CFLAGS=-std=c11 -g -static
SRCS=main.c parse.c codegen.c token.c vector.c pp.c token_common.c
OBJS=$(SRCS:.c=.o)

rrcc: $(OBJS)
	$(CC) -o rrcc $(OBJS) $(LDFLAGS)

$(OBJS): rrcc.h

tester: rrcc tester.c
	./rrcc tester.c > tester.s
	cc tester.s -o tester

test: rrcc tester
	./tester

clean:
	rm -f rrcc *.o *~ tmp.c tmp.s tmp.o tmp tester

.PHONY: test clean
