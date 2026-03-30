# ECE3375 Design Project Final Report

## Team
- Jade Liu - 251348010
- Sabrina Shen - 251359262
- Freda Zhao - 251377618
- Caroline Shen - 251284651

## Project Title
Smart Medicine Dispenser Reminder

## 1) Problem Definition

Medication non-adherence is a common issue for elderly users, patients with chronic conditions, and people with complex schedules. Missed or delayed medication can reduce treatment effectiveness and can increase emergency visits and hospitalization risk.

The proposed embedded system addresses this issue by:
- tracking time continuously,
- comparing time against a configured medication schedule,
- generating visual and audible reminders when a dose is due,
- recording whether a dose is acknowledged on time, late, or missed.

### System Specifications
- Maintain a daily medication schedule with at least 4 dose slots.
- Trigger reminders at scheduled times with <= 1 second timing error in prototype mode.
- Detect user acknowledgement within 100 ms.
- Escalate reminders if no acknowledgement is received.
- Log taken/missed outcomes for each scheduled dose event.
- Run continuously in an infinite loop.

### Impact on Users and Society

Positive impact:
- Improves medication adherence and routine consistency.
- Supports independent living for elderly users.
- Reduces caregiver anxiety through structured tracking.
- Can reduce healthcare burden associated with missed doses.

Potential drawbacks:
- Alarm fatigue if reminder behavior is too aggressive.
- Dependence on the device if users stop using backup reminders.
- False negatives in acknowledgement detection if hardware is not robust.

Broader social considerations:
- Accessibility and ease of use are critical for adoption.
- Privacy must be considered if logs are uploaded or shared in future versions.

## 2) Functional Description

The system is modeled as a black box with time input and user acknowledgement input, producing alerts and adherence logs.

### Inputs
- Current time (from RTC over I2C in target hardware; simulated clock in prototype).
- User acknowledgement (push-button press or enclosure-open event on GPIO).

### Outputs
- Visual alert (LED on/blink).
- Audible alert (PWM buzzer).
- Internal event log (taken/late/missed + timestamp metadata).

### Functional Behavior
1. Initialize GPIO, timer/PWM, RTC interface, and schedule data.
2. Poll or interrupt on acknowledgement input.
3. Read current time once per second.
4. Compare current time to medication schedule.
5. When dose is due: activate LED and buzzer.
6. If user acknowledges: stop alert and log as taken.
7. If no acknowledgement within threshold: escalate alert and eventually log missed event.

## 3) Input/Output Requirements and Selected Components

### Inputs
- **Acknowledgement Input**
  - Selected: Momentary push-button on GPIO with 10 kOhm pull-down resistor.
  - Alternative: Reed switch on medication lid.
- **Timekeeping Input**
  - Selected for product: DS3231 RTC module over I2C.
  - Selected for software prototype: internal simulated clock API.

### Outputs
- **Visual**
  - Selected: Standard LED with 220-330 Ohm series resistor.
- **Audible**
  - Selected: Piezo buzzer with PWM control.

### Tools and Resources
- Component sourcing: DigiKey / Mouser.
- Datasheets: DS3231 register map, target MCU GPIO/PWM/I2C docs.
- Lab tools: Breadboard, jumper wires, multimeter, oscilloscope (optional).

## 4) Initial Software Design

### Modules
- `scheduler`: compares clock time vs schedule and owns alert state machine.
- `hal`: hardware abstraction layer for time, input, LED, buzzer, logging.
- `main`: initialization, loop, and call ordering.

### Data and Control Flow
1. Boot and initialize all HAL interfaces.
2. Load medication schedule.
3. Enter infinite loop:
   - read time,
   - read acknowledgement,
   - update scheduler,
   - apply outputs,
   - wait until next tick.

### Runtime Strategy
- Main loop architecture with 1-second scheduling tick.
- Deterministic state machine for alert and escalation behavior.
- No dynamic allocation; all static/global configuration.

## 5) Prototyping Plan

### Prototype Scope
- Scheduler logic
- Alert trigger/escalation logic
- Acknowledgement handling
- Event logging

### Tests
- **T1 Time monitoring:** verify monotonically increasing time.
- **T2 Alert triggering:** verify alert starts at scheduled timestamps.
- **T3 Acknowledgement path:** verify alert stops and logs taken.
- **T4 Escalation path:** verify blink/sound escalation over time.
- **T5 Missed-dose timeout:** verify missed status is logged without acknowledgement.

### Success Criteria
- Trigger jitter <= 1 second.
- Acknowledgement reaction <= 100 ms in simulated input loop.
- Stable operation through at least 24-hour simulation window.

## 6) Microcontroller Selection

Two candidates were compared:

### Option A: STM32F401 (ARM Cortex-M4)
- Pros: strong toolchain support, robust timers/PWM/I2C, low power modes, inexpensive dev boards.
- Cons: higher complexity than ultra-low-power MCUs, more configuration overhead.

### Option B: TI MSP430FR2433 (MSP430 family)
- Pros: extremely low power, simple architecture, suitable for battery-operated reminder devices.
- Cons: fewer high-performance peripherals, lower headroom for future feature expansion.

### Decision
For a production-ready medicine reminder focused on low power and predictable control, MSP430 is a strong fit. For rapid prototyping and easier peripheral bring-up in existing class workflows, STM32F401 is preferred. The software architecture in this project is kept portable to support either device.

## 7) Revised Software Design

Changes from initial design after implementation:
- Added explicit state machine (`IDLE`, `ALERT_ACTIVE`, `ALERT_ESCALATED`, `EVENT_LOGGED`) to avoid ambiguous transitions.
- Added debounce-friendly acknowledgement edge handling via HAL.
- Added explicit missed-dose timeout and event classification.
- Added configurable escalation threshold and missed timeout per schedule slot.
- Added structured logging API for later persistence integration.

Interrupt usage in revised design:
- Final target design should use GPIO interrupts for acknowledgement and timer interrupts for periodic tick.
- Current prototype keeps a polling API for portability; HAL can map to interrupt-backed flags.

## 8) Results from Prototyping

### Implementation Results
- Core scheduling and alert logic runs as expected in the software simulator.
- Alerts trigger at configured medication times.
- Acknowledgement clears active alerts and logs "taken".
- Non-acknowledged events escalate and eventually log "missed".

### Challenges
- Time representation and day rollover handling required explicit conversion between `hh:mm:ss` and seconds-of-day.
- Avoiding duplicate triggers in the same minute required slot-level guard fields.
- Escalation policy tuning (too aggressive vs too mild) required repeated simulation.

### Software Iterations
- **Iteration 1:** single scheduled dose and simple on/off alert.
- **Iteration 2:** multiple schedule slots and acknowledgement support.
- **Iteration 3:** escalation and missed-dose timeout.
- **Iteration 4:** modular HAL and structured event logging.

### Team Contribution (Code/Design)
- Jade Liu: I/O selection research and alert requirement definitions.
- Sabrina Shen: schedule logic and state transition design.
- Freda Zhao: microcontroller comparison and hardware-interface planning.
- Caroline Shen: C implementation integration, simulation harness, and documentation.

### Next Step Toward Physical System
- Replace simulated HAL with board-specific HAL for chosen MCU.
- Integrate DS3231 I2C driver and real GPIO/PWM peripherals.
- Validate on hardware with button debounce and buzzer current profiling.

### Debug/Test Plan for Hardware Bring-Up
1. Verify GPIO input levels and debounce timing with oscilloscope.
2. Verify LED and buzzer drive waveforms under normal/escalated modes.
3. Validate RTC read stability and time drift over 24 hours.
4. Run fault-injection tests (stuck button, disconnected buzzer, RTC read failure).
5. Confirm event logs match observed behavior.

## 9) Source Code

All source code is provided in:
- `src/main.c`
- `src/med_scheduler.c`
- `src/med_scheduler.h`
- `src/hal.h`
- `src/hal_sim.c`
- `src/hal_de10.c`
- `Makefile`

Code comments and function boundaries mark iteration milestones and extension points for real hardware ports.

## 10) Conclusions

### Feasibility
The system is feasible with low-cost components and a low-power microcontroller. Core software complexity is moderate and manageable. The most significant hardware risks are robust input detection and ensuring acceptable alarm audibility under varied environments.

### Sustainability and Environmental Considerations
- Prefer long-life rechargeable battery or low-self-discharge cells.
- Use low-power firmware strategies (sleep between events, interrupt wakeups).
- Select recyclable enclosure materials where possible.
- Design for modular replacement to reduce full-device disposal.

Environmental concerns at scale:
- Battery disposal and e-waste handling.
- End-of-life electronics recycling availability.
- Packaging and shipping footprint for commercial deployment.

### Learning Reflection
This project connected course topics (GPIO, timers, PWM, serial interfaces, state machines) into a practical embedded workflow. The team gained additional knowledge in:
- translating user problems into measurable firmware requirements,
- designing robust event-driven logic with clear states,
- planning portable software for different MCU families,
- structuring test strategy before full hardware availability.
