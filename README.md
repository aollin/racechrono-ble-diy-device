# diy-ble-gps


All values are big-endian and unsigned if not stated otherwise

# GPS main characteristic (UUID 3)

byte index  | description
------ | -----------------------------------------------------------------------------------------------------------------
0-2 | Sync bits (3 bits) and time from hour start (21 bits = (minute * 30000) + (seconds * 500) + (milliseconds / 2))
3 | Fix quality (2 bits), locked satellites (6 bits)
4-7 | Latitude in (degrees * 10_000_000, or minutes * 100_000), signed 2's complement, invalid value 0x7FFFFFFF
8-11 | Longitude in (degrees * 10_000_000, or minutes * 100_000), signed 2's complement, invalid value 0x7FFFFFFF
12-13 | Altitude (((meters + 500) * 10) & 0x7FFF) or (((meters + 500) & 0x7FFF) | 0x8000), invalid value 0xFFFF
14-15 | Speed in ((knots * 100) & 0x7FFF) or (((knots * 10) & 0x7FFF) | 0x8000), invalid value 0xFFFF
16-17 | Bearing (degrees * 100), invalid value 0xFFFF
18 | HDOP (dop * 10), invalid value 0xFF
19 | VDOP (dop * 10), invalid value 0xFF

# GPS time characteristic (UUID 4)

 byte index | description
 ----------- | ----------------------------------
 0-3 | Sync bits (3 bits) and hour and date (21 bits = (year - 2000) * 8928 + (month - 1) * 744 + (day - 1) * 24 + hour)

The two GPS characteristics should be matched by comparing the sync bits. If sync bits differ, then either of the characteristics should be updated.

# Parts list

You can build a GPS receiver, a CAN-Bus reader or a device that has both.

Part | Price
----- | --------
Adafruit Feather nRF52 Bluefruit (nRF52832) | $25
Adafruit Ultimate GPS Breakout v3 | $40 (optional)
MCP2515 breakout board | $5-10 (optional)
Connecting wires |
Casing |

# Connecting the GPS module

The nRF52832 board has only one hardware serial port, so connecting it causes us to lose debug access through serial port. The nRF52840 has more ports, but its availability was not great when creating this reference implementation. The GPS needs to be disconnected when uploading new firmware, as the USB port is using the same serial port as the GPS (the only one).


| Adafruit Feather nRF52 Bluefruit (nRF52832) | Adafruit Ultimate GPS Breakout v3
| --------------------------------------------------- | ---------------------------------------
| 3.3V | VIN
| GND | GND
| TX | RX
| RX | TX

# Connecting the CAN-Bus module

The CAN-Bus module needs to be connected to USB power. The 3.3 V outputs are not sufficient, as the board requires 5V.

| Adafruit Feather nRF52 Bluefruit (nRF52832) | MCP2515 breakout
| --------------------------------------------------- | -----------------------
| USB | VCC
| GND | GND
| MISO | SO
| MOSI | SI
| SCK | SCK
| A5 | INT
| A4 | SC

# Connecting to power

If you've connected only the GPS module, it's enough to connect a battery to the battery connector onboard the nRF52. If you're connected the MCP2515, you'll need higher voltage, meaning you'll need to either connect to the USB port or somehow else power the USB pin. One option is to connect a 6-18 V => 5 V stepdown.

| Adafruit Feather nRF52 Bluefruit (nRF52832) | Step down module 5 V | External power
| --------------------------------------------------- | --------------------- | --------------------
| USB | V-out | -
| GND | GND   | GND
| -   | V-in  | 12V



