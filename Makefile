CFLAGS=-std=c11 -g -static -fsanitize=undefined
LDFLAGS=-fsanitize=undefined
SRCS=main.c parse.c codegen.c token.c vector.c pp.c token_common.c
OBJS=$(SRCS:.c=.o)
OBJS_2=$(SRCS:.c=_2.o)
RRCCFLAGS=-I/usr/lib/gcc/x86_64-linux-gnu/11/include -I/usr/local/include -I/usr/include/x86_64-linux-gnu -I/usr/include

rrcc: $(OBJS)
	$(CC) -o rrcc $(OBJS) $(LDFLAGS)

rrcc2: rrcc $(OBJS_2)
	cc -o rrcc2 $(OBJS_2) $(LDFLAGS)

$(OBJS): rrcc.h

$(OBJS_2): %_2.o: %.c rrcc
	./rrcc $(RRCCFLAGS) $< > $(<:.c=_2.s)
	cc -c $(<:.c=_2.s) -o $@

tester: rrcc tester.c
	./rrcc tester.c > tester.s
	cc tester.s -o tester

test: rrcc tester
	./tester

clean:
	rm -f rrcc *.o *~ tmp.c tmp.s tmp.o tmp tester

.PHONY: test clean
