# STM32G030F6P6TR OBI Battery Tester

Firmware for a compact Makita LXT battery diagnostic tool built around the
STM32G030F6P6TR, a 132x32 ST7567 graphical LCD, and a rotary encoder.

The tester communicates with a compatible battery through the Makita
one-wire-style OBI interface. It can display identification, voltage,
temperature, and status information and can issue battery LED test commands.

## Current features

- ST7567 132x32 LCD driver using software serial GPIO
- 6x8 ASCII text font
- Debounced quadrature rotary encoder and push-button input
- Rotary-driven LCD menu
- Timing-sensitive OBI communication on a released/open-drain-style GPIO
- Battery interface power/enable control
- Detection of no-battery and invalid all-`00`/all-`FF` responses
- Debug and Release CMake presets for GNU Arm Embedded

The current menu contains seven operations:

1. Read the 8-byte ROM identifier
2. Read the battery model
3. Read pack voltage and five individual cell voltages, including the
   highest-to-lowest cell voltage difference
4. Read battery temperature
5. Read lock status, status code, capacity, charge cycles, and date data
6. Turn the battery indicator LED on
7. Turn the battery indicator LED off

Battery clear-errors/reset commands are intentionally not included. Those
commands can change battery safety state and require a separate guarded
workflow with cell, temperature, and physical-condition checks.

## Hardware

| Function | STM32 pin | Configuration |
|---|---:|---|
| Battery interface enable | PB7 | Push-pull output |
| Battery one-wire data | PB3 | Released input; switched to output only to drive low |
| Rotary encoder button | PA1 | Input with pull-up |
| Rotary encoder S2 | PA2 | Input with pull-up |
| Rotary encoder S1 | PA3 | Input with pull-up |
| LCD reset | PA4 | Push-pull output |
| LCD command/data | PA5 | Push-pull output |
| LCD chip select | PA6 | Push-pull output |
| LCD serial data | PA7 | Push-pull output |
| LCD serial clock | PB0 | Push-pull output |

Target device details:

- MCU: STM32G030F6P6TR
- Core: Arm Cortex-M0+
- Package: TSSOP20
- Flash: 32 KB
- RAM: 8 KB
- System clock: 16 MHz HSI
- Display: ST7567-compatible 132x32 LCD

## Battery interface warning

The PB3 firmware behavior emulates an open-drain one-wire connection: it
drives the line low and changes the pin to input mode to release it.

Use a suitable battery-interface circuit and an appropriate pull-up to a safe
logic voltage. Do not connect PB3 directly to the battery pack voltage and do
not pull the data line up to a voltage outside the STM32 GPIO ratings.

This project is intended for diagnostics, development, and protocol study.
Never use a communication result alone to declare a battery safe. Inspect the
pack physically and verify cell balance, cell voltage, and temperature.

## User controls

- Rotate the encoder to move through the menu.
- Press the encoder to execute the selected operation.
- Press again to repeat the current read or LED command.
- Rotate after viewing a result to return to the menu and select another item.

The LCD reports `NO BATTERY`, `BAD RESPONSE`, or `READ ERROR` when a
transaction cannot be completed successfully.

## Building

The project uses CMake, Ninja, and the GNU Arm Embedded toolchain supplied by
STM32CubeCLT.

Debug build:

```powershell
cmake --preset Debug
cmake --build --preset Debug -j
```

Release build:

```powershell
cmake --preset Release
cmake --build --preset Release -j
```

Generated ELF files are placed in:

```text
build/Debug/STM32G030F6P6TR_OBI_BATTERY_TESTER.elf
build/Release/STM32G030F6P6TR_OBI_BATTERY_TESTER.elf
```

The project can also be opened using the included
`STM32G030F6P6TR_OBI_BATTERY_TESTER.ioc` file and STM32Cube development tools.

## Project structure

```text
Core/Inc/makita_obi.h       Public battery diagnostic API and data types
Core/Src/makita_obi.c       One-wire timing and Makita OBI commands
Core/Inc/obi_ui.h           Battery menu interface
Core/Src/obi_ui.c           Rotary-controlled LCD diagnostic screens
Core/Inc/rotary_encoder.h   Rotary encoder event API
Core/Src/rotary_encoder.c   Quadrature decoding and button debounce
Core/Inc/st7567.h           LCD API
Core/Src/st7567.c           ST7567 software-serial driver
Core/Src/font6x8.c          Display font data
Core/Src/main.c             Hardware initialization and main event loop
```

## Public battery API

The main read and test functions are declared in `Core/Inc/makita_obi.h`:

```c
void makita_obi_init(void);
MakitaObiResult makita_obi_read_rom(uint8_t rom[MAKITA_OBI_ROM_SIZE]);
MakitaObiResult makita_obi_read_model(char model[MAKITA_OBI_MODEL_LENGTH + 1U]);
MakitaObiResult makita_obi_read_voltages(MakitaObiVoltages *voltages);
MakitaObiResult makita_obi_read_temperature(MakitaObiTemperature *temperature);
MakitaObiResult makita_obi_read_status(MakitaObiStatus *status);
MakitaObiResult makita_obi_led_on(void);
MakitaObiResult makita_obi_led_off(void);
```

One-wire transactions temporarily mask interrupts during timing-critical bit
slots and restore the previous interrupt state afterward.

## Reference projects

This firmware was developed with help from these earlier implementations:

- [stm32g0-rtc-lcd-ui](https://github.com/chamindahettiarachchi/stm32g0-rtc-lcd-ui)
- [Makita_OBI_Arduino_code](https://github.com/chamindahettiarachchi/Makita_OBI_Arduino_code)
- [attiny1614_OBI_With_RESET_btn](https://github.com/chamindahettiarachchi/attiny1614_OBI_With_RESET_btn)

## Author

Chaminda Hetti Arachchi
