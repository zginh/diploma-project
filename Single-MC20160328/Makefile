CC = g++
CFLAGS = -w -O3 -g
TARGET = mzip
LIBS = -lpthread -lcrypt -lcrypto -lglib

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

SOURCES = $(wildcard *.c lib/*.c src/*.c)
OBJS = $(patsubst %.c,%.o,$(SOURCES))

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)
	@chmod a+x $(TARGET)
	@echo "=== Make of $(TARGET) is good==="
	@echo " "

clean:
	@rm -rf *.o
	@rm -rf lib/*.o
	@rm -rf src/*.o
	@rm -rf lib/*.a
	@rm -rf src/*.a
	@rm -rf log/*
	@rm -rf compress/*
	@rm -rf decompress/*
	@rm -rf tmp/*
	@rm -rf core
	@rm -rf $(TARGET)
