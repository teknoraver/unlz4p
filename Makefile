CFLAGS := -pipe -O2 -Wall -Wno-pointer-sign
LDLIBS := -llz4
BIN := unlz4p

all: $(BIN)

clean::
	rm -f $(BIN) *.o
