# ECE3375 Project - Smart Medicine Dispenser Reminder

This repository contains a complete final-deliverable package for the ECE3375 design project:

- Final report: `docs/final_report.md`
- Source code prototype: `src/`
- Build/run script: `Makefile`

## Simulator Build/Run (local)

```bash
make
make run
```

This builds and runs a simulation of the medication scheduler, alert escalation, acknowledgement, and event logging logic.

## DE10-Standard Hardware Integration

The hardware target uses the DE10 memory map style from ECE3375 labs:
- `LEDR_BASE = 0xFF200000`
- `HEX3_HEX0_BASE = 0xFF200020`
- `HEX5_HEX4_BASE = 0xFF200030`
- `SW_BASE = 0xFF200040`
- `KEY_BASE = 0xFF200050`
- `TIMER0_BASE = 0xFF202000`

Hardware behavior:
- `KEY0` acknowledges an active medication alert (edge-detected, active-low).
- `LEDR0` = visual alert indicator (blink behavior).
- `LEDR1` = buzzer output state indicator (useful before external buzzer wiring).
- `HEX3..HEX0` display software clock as `HHMM`.
- `HEX5..HEX4` show alert/event status codes.
- `SW0` enables 20x software-clock speed for fast demonstrations.

### In Intel FPGA Monitor Program

1. Create a new project with:
   - Architecture: ARM Cortex-A9
   - System: DE10-Standard Computer
   - Language: C
2. Add these files to the project:
   - `src/main.c`
   - `src/med_scheduler.c`
   - `src/med_scheduler.h`
   - `src/hal.h`
   - `src/hal_de10.c`
3. Build and Load in Monitor (`Actions -> Compile & Load`).
4. Run the program; use `KEY0` to acknowledge reminders.

If you are building outside Monitor with a normal toolchain, use:

```bash
make de10
```