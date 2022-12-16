CFLAGS=-std=c11 -Wall -Wextra -Wsign-conversion -D _DEFAULT_SOURCE

SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

mhz19c: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJS): mhz19c.h

clean:
	$(RM) mhz19c $(OBJS)

.PHONY: clean
