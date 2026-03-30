CC := cc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O2

COMMON_SRC := src/main.c src/med_scheduler.c
SIM_SRC := src/hal_sim.c
DE10_SRC := src/hal_de10.c
BIN_SIM := build/med_dispenser_sim
BIN_DE10 := build/med_dispenser_de10

.PHONY: all sim de10 clean run

all: sim

sim: $(BIN_SIM)

de10: $(BIN_DE10)

$(BIN_SIM): $(COMMON_SRC) $(SIM_SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) -DTARGET_SIM $(COMMON_SRC) $(SIM_SRC) -o $(BIN_SIM)

$(BIN_DE10): $(COMMON_SRC) $(DE10_SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) -DTARGET_DE10 $(COMMON_SRC) $(DE10_SRC) -o $(BIN_DE10)

run: $(BIN_SIM)
	./$(BIN_SIM)

clean:
	rm -rf build
