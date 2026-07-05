# Navigator (Blue Robotics) — Hardware Evaluation

## Document Status

`current`

Evaluated 2026-07-05 as a potential future hardware platform and a hedge against Navio2 being end-of-life (TODO task 13).

## Verdict (short)

Right *architecture* (open drivers, **no closed co-processor firmware**), but **not a drop-in**: it is **Raspberry Pi 4 only** (not Pi 5), and the **PCB itself is not published as an open-fab design**. Best treated as either a bought board from an EU reseller, or as a *reference design* for a Pi-5-native DIY HAT.

## Open-source status

| Layer | Open? | Notes |
|---|---|---|
| Software / firmware | ✅ Yes | `navigator-lib` (GitHub), BlueOS |
| Mechanical | ✅ Yes | STEP / STL files published |
| PCB design (schematic / Gerbers / BOM) | ❌ Not found published | BR states *"some elements, like proprietary designs or externally licensed systems, remain closed."* Self-fab would mean reverse-engineering or designing your own — **not** download-and-fab. |

## Architecture — why it is more reproducible than Navio2

- **No STM32 / RCIO co-processor.** (The Navio2's closed RCIO firmware on its STM32F103 was the real clone-blocker.)
- **PWM via PCA9685** — a standard I²C 16-channel PWM chip. (PWM0 is also exposed on the Pi's GPIO18.)
- IMU (accel/gyro), **dual magnetometers**, barometer, and ADC are driven **directly by the Raspberry Pi over I²C/SPI** — standard chips with open drivers.
- Net: a DIY *equivalent* is genuinely feasible here (unlike Navio2), because nothing is locked behind firmware.

## Compatibility — the catch

- **Raspberry Pi 4 Model B ONLY.** Depends on the BCM2711; **not compatible with Raspberry Pi 5** — the same RP1 problem class solved in this project for the Navio2.
- Adopting on Pi 5 would require a port (but *easier* than Navio2 — no RCIO; just sensor buses + PCA9685 over I²C). Alternative is dropping back to a Pi 4, which weakens the Hailo-8L AI story.

## Cost / sourcing

- Listed **from ~$220**. A one-off DIY clone is likely **not** cheaper once PCBA setup and fine-pitch QFN hand-assembly are counted; a small batch could be.
- **EU resellers exist** (avoid US import VAT/duty): BLUE ROV Solutions (Germany), Marine Thinking.

## Sources

- Navigator product page — https://bluerobotics.com/store/comm-control-power/control/navigator/
- Blue Robotics open source — https://bluerobotics.com/open-source/
- Navigator hardware setup — https://bluerobotics.com/learn/navigator-hardware-setup/
- navigator-lib (GitHub) — https://github.com/bluerobotics/navigator-lib
- EU reseller (BLUE ROV Solutions, DE) — https://bluerov-solutions.com/produkt/navigator-flight-controller/

## Open questions to confirm

- Exact hardware license, and whether schematics are included in the product page's "technical details".
- Feasibility/effort of a Pi 5 (RP1) port for the Navigator's sensors + PCA9685.
