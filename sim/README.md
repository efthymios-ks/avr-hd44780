# sim/

SimulIDE circuit for the demo.

## Setup (one-time, Windows)

1. Run `.\Build.ps1 -Mcu atmega328p -OutDir build/sim` from the repo root.
2. Open SimulIDE and build the circuit:
   - MCU: ATmega328P
   - Push button on PD2 → GND, with internal pull-up (no external resistor)
   - LED on PB5 via 330 Ω → GND
3. Load the firmware: right-click the MCU → *Load firmware* → `build/sim/demo.hex`.
4. Save the circuit as `demo.sim1` here.

After that, `.\Simulate.ps1` builds and opens the circuit automatically.

## Expected behavior

LED follows the button. Pressing the button (closing it to GND) turns the LED on.
