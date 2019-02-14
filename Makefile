CC := gcc
CFLAGS := -Wall -std=c99
LFLAGS := -lSDL2
OUT := voicememo

SRC := src

TARGETS := \
	  $(SRC)/main.o

.PHONY: all clean

all: $(TARGETS)
	$(CC) $(LFLAGS) $^ -o $(OUT)

$(SRC)/main.o: $(SRC)/main.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OUT)
	rm -f $(SRC)/*.o
	rm -f *.dSYM
