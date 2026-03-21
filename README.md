# Nixie clock - Divergence Meter

WIP

I Wanted to make a design close to Steins Gate's divergence meter.
Most of the features will come from software (esp32 - web configuration interface).

## Hardware

- Made for 8 IN-14 tubes
- HV DCDC generator made on board, based on [this design](https://nick.desmith.net/Electronics/NixiePSU.html)
- HV open drain shift registers HV5622
- ESP32C3 module
- Most of the components are hand-solderable (except the ESP32 because of the pads underneath)

#### Version 1.1 updates:

Fixed HV5622 footprint, used a bigger DCDC FET (>250V), Reduced GPIO9 cap size to avoid starting in DL mode, and added PD resistors to the level shifters' NFETs.

### Issues

The feedback line of the DCDC is picking up noise which creates a visible dimming oscillation at around 20Hz (50Vpp). At high load it even makes the DCDC saturate.
I fixed it by adding a small cap (100pF) on FB pin. 0603 size is perfect to sit between SHDN and FB pins of the MAX1771.

My DCDC has only a ~70% efficiency which is not super good compared to the original design (85%), I probably did a poor layout job. But in practice it does not matter since currents involved are small.

## Software

- Using ESP-IDF
- Programming via UART
- more soon
