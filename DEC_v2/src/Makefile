SRCSRCS = chunking.c recipe.c
SRCOBJS = $(SRCSRCS:.c=.o)

%.o:%.c
	@echo "compling $<"
	g++ -g -c $<

all:libsrc.a
	@echo "=== Make of src is good ==="
	@echo " "

libsrc.a:$(SRCOBJS)
	@echo "Making $@"
	ar rc $@ $^
