CC := cc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O2

SRC := src/main.c src/med_scheduler.c src/hal_sim.c
BIN := build/med_dispenser_sim

.PHONY: all clean run

all: $(BIN)

$(BIN): $(SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)

run: $(BIN)
	./$(BIN)

clean:
	rm -rf build
