# RaceChrono BLE DIY device (GPS and CAN-Bus) 

This project describes a reference "Do it yourself" or "DIY" device to be used with RaceChrono Pro mobile app. The device consists of the main Adafruit board, and GPS and/or CAN-Bus boards.

The new DIY protocols introduced here are based on Bluetooth LE, so that both Android and iOS phones can connect to it.

The device described here is in no way ready as a product. The CAN-Bus part has been tested quite a bit, and is reasonably reliable and fast, but the GPS part is merely a quick add-on to test and demonstrate the new API. Also the GPS board might not be the best choice for racing. I just bought something that is easy to connect.

# Required knowledge

To build a DIY devices like this will always need some knowledge about programming and electronics. Also some soldering skills are required. Also going through some Arduino tutorials will help a lot. Here's how to setup the Arduino IDE to be used with the Adafruit board: https://github.com/MagnusThome/RejsaRubberTrac/blob/master/installArduino.md

In addition to the skills required to build the device, you'll need some skills and knowledge to either reverse engineer the CAN-Bus packets on your vehicle, or find them already reverse engineered from somewhere.

# Performance

This device as CAN-Bus reader alone (without GPS) will achieve ~20 Hz update rate when monitoring 5 different CAN-Bus PIDs. With a GPS board added, the update rate will drop to 10 Hz, probably due to the Bluetooth LE chip that is used here. The characteristic UUID 3 in notify mode (used for GPS data) will halve the performance of the characteristic UUID 1 (used for CAN-Bus data). The GPS board runs at 5 Hz in this example, but can probably be made to run at 10 Hz with some configuration.

# Parts list

Here's the main parts needed to build the device. The small stuff like casing, wires etc. are not listed. Also the required tools like soldering iron etc. are not listed here.

Part | Price
----- | --------
Adafruit Feather nRF52 Bluefruit (nRF52832) | $25
Adafruit Ultimate GPS Breakout v3 | $40 (optional)
MCP2515 breakout board | $5-10 (optional)
5V stepdown | $5 (optional)

# Build photos

Here's a prototype build with only CAN-Bus board connected.

![alt text](../../blob/master/photos/proto-can-bus.jpg?raw=true)

Here's a prototype build with both CAN-Bus and GPS boards connected.

![alt text](../../blob/master/photos/proto-can-bus-gps.jpg?raw=true)

And finally here's what I use for testing on a KTM motorcycle. It's CAN-Bus only, with Sumimoto connector that plugs in directly to the bike. You could use OBD-II connector or what ever is available in your vehicle.

![alt text](../../blob/master/photos/built-can-bus.jpg?raw=true)

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

If you've connected only the GPS module, it's enough to connect a battery to the battery connector onboard the nRF52. If you're connected the MCP2515, you'll need higher voltage, meaning you'll need to either connect to the USB port or somehow else power the USB pin. One option is to connect a ~12 V => 5 V stepdown.

| Adafruit Feather nRF52 Bluefruit (nRF52832) | Step down module 5 V | External power
| --------------------------------------------------- | --------------------- | --------------------
| USB | V-out 5V | -
| GND | GND   | GND
| -   | V-in ~12V  | ~12V


# Protocol description

## Bluetooth LE service

The device has one Bluetooth LE service that contains four characteristics, two for GPS and two for CAN-Bus. The service UUID is 00001ff8-0000-1000-8000-00805f9b34fb. All characteristic values are big-endian and unsigned if not stated otherwise.

## CAN-Bus main characteristic (UUID 1)

This characteristic is read and notify only.

byte index  | description
------ | -----------------------------------------------------------------------------------------------------------------
0-3 | 32-bit packet ID
4-11 | 8 bytes of raw packet payload

## CAN-Bus filter characteristic (UUID 2)

This characteristic is write only.

### Deny all

Denies all incoming packet IDs. Called before exceptions for specific PIDs.

byte index  | description
------ | -----------------------------------------------------------------------------------------------------------------
0 | Command ID = 0

### Allow all

Allows all incoming packet IDs. Used for "promiscuous mode".

byte index  | description
------ | -----------------------------------------------------------------------------------------------------------------
0 | Command ID = 1
1-2 | Notify interval (PID specific notify interval in milliseconds)

### Allow one PID

Allows all incoming packet IDs. Used for "promiscuous mode".

byte index  | description
------ | -----------------------------------------------------------------------------------------------------------------
0 | Command ID = 2
1-2 | Notify interval (PID specific notify interval in milliseconds)
3-6 | 32-bit PID to allow

## GPS main characteristic (UUID 3)

This characteristic is read and notify only.

byte index  | description
------ | -----------------------------------------------------------------------------------------------------------------
0-2 | Sync bits (3 bits) and time from hour start (21 bits = (minute * 30000) + (seconds * 500) + (milliseconds / 2))
3 | Fix quality (2 bits), locked satellites (6 bits)
4-7 | Latitude in (degrees * 10_000_000), signed 2's complement, invalid value 0x7FFFFFFF
8-11 | Longitude in (degrees * 10_000_000), signed 2's complement, invalid value 0x7FFFFFFF
12-13 | Altitude (((meters + 500) * 10) & 0x7FFF) or (((meters + 500) & 0x7FFF) | 0x8000), invalid value 0xFFFF
14-15 | Speed in ((knots * 100) & 0x7FFF) or (((knots * 10) & 0x7FFF) | 0x8000), invalid value 0xFFFF
16-17 | Bearing (degrees * 100), invalid value 0xFFFF
18 | HDOP (dop * 10), invalid value 0xFF
19 | VDOP (dop * 10), invalid value 0xFF

## GPS time characteristic (UUID 4)

This characteristic is read and notify only.

byte index | description
----------- | ----------------------------------
0-3 | Sync bits (3 bits) and hour and date (21 bits = (year - 2000) * 8928 + (month - 1) * 744 + (day - 1) * 24 + hour)

The two GPS characteristics should be matched by comparing the sync bits. If sync bits differ, then the client waits for either of the characteristics to update.

