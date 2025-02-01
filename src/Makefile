# (c)2018 ogi-it, Ognian Tschakalov

#VERSION = 0.01
CC      = /usr/bin/gcc
CFLAGS  = -Wall -g -D_REENTRANT -DVERSION="\"$(VERSION_STRING)\"" -I. -ffunction-sections
LDFLAGS = -lm -Wl,--gc-sections -static

OBJ = sdmon.o json.o json-builder.o
BIN = sdmon

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -rf $(BIN) $(OBJ)
