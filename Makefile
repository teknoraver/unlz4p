CFLAGS := -pipe -O2 -Wall -Wno-pointer-sign
BIN := unlz4p

all: $(BIN)

clean::
	rm -f $(BIN) *.o
