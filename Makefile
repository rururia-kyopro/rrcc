CFLAGS=-std=c11 -g -static -fsanitize=undefined
LDFLAGS=-fsanitize=undefined
SRCS=main.c parse.c codegen.c token.c vector.c pp.c token_common.c util.c
OBJS=$(SRCS:.c=.o)
OBJS_2=$(SRCS:.c=_2.o)
OBJS_3=$(SRCS:.c=_3.o)
RRCCFLAGS=-I/usr/lib/gcc/x86_64-linux-gnu/11/include -I/usr/local/include -I/usr/include/x86_64-linux-gnu -I/usr/include

rrcc: $(OBJS)
	$(CC) -o rrcc $(OBJS) $(LDFLAGS)

rrcc2: rrcc $(OBJS_2)
	cc -o rrcc2 $(OBJS_2) $(LDFLAGS)

rrcc3: rrcc2 $(OBJS_3)
	cc -o rrcc3 $(OBJS_3) $(LDFLAGS)

$(OBJS): rrcc.h

$(OBJS_2): %_2.o: %.c rrcc
	./rrcc $(RRCCFLAGS) $< > $(<:.c=_2.s)
	cc -c $(<:.c=_2.s) -o $@

$(OBJS_3): %_3.o: %.c rrcc2
	./rrcc2 $(RRCCFLAGS) $< > $(<:.c=_3.s)
	cc -c $(<:.c=_3.s) -o $@

tester: rrcc tester.c
	./rrcc tester.c > tester.s
	cc tester.s -o tester

tester2: rrcc2 tester.c
	./rrcc2 tester.c > tester_2.s
	cc tester_2.s -o tester2

tester3: rrcc3 tester.c
	./rrcc3 tester.c > tester_3.s
	cc tester_3.s -o tester3

test: rrcc tester
	./tester

test2: rrcc2 tester2
	./tester2

test3: rrcc3 tester3
	./tester3

clean:
	rm -f rrcc *.o *~ tmp.c tmp.s tmp.o tmp tester tester2 tester3 *_2.s *_3.s rrcc2 rrcc3

.PHONY: test clean
