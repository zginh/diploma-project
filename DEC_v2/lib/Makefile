LIBSRCS = bnet.c bnet.h htable.c htable.h Spooky.c Spooky.h
LIBOBJS = $(LIBSRCS:.c=.o)

%.o:%.c
	@echo "compling $<"
	g++ -O2 -g -c $<

all:liblib.a
	@echo "=== Make of lib is good ==="
	@echo " "

liblib.a:$(LIBOBJS)
	@echo "Making $@"
	@ar rc $@ $^
