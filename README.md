# Nixie clock - Divergence Meter

WIP, hardware ordered.
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

The feedback line of the DCDC is picking up noise which creates a visible dimming oscillation at around 20Hz. It is fixable by adding a small cap on FB pin but it adds a real charm to the light so I'm keeping it.

## Software

soon