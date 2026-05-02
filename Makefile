CC = gcc
# -Wall -Wextra -Werror -pedantic
CFLAGS = -std=gnu99
LDFLAGS = -pthread -lrt

TARGET = proj2
SRCS = proj2.c
HDRS = proj2.h

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
