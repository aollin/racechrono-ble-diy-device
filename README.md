# RaceChrono BLE DIY APIs

This project describes the new DIY (or "Do It Yourself") protocols in RaceChrono. The protocols are based on Bluetooth LE (BLE), so that both Android and iOS phones can connect to it. 
Example device implementations are provided within this project. They are currently all built on Adafruit's "Arduino" boards, and programmed using the Arduino IDE and Adafruit's libraries.

# Protocol description

## Bluetooth LE service

The device has one Bluetooth LE service that contains four characteristics, two for GPS and two for CAN-Bus. The service UUID is 00001ff8-0000-1000-8000-00805f9b34fb (or 0x1ff8 as 16-bit UUID). All characteristic values are big-endian and unsigned if not stated otherwise.

## CAN-Bus main characteristic (UUID 0x0001)

This characteristic is read and notify only.

byte index  | description
------ | -----------------------------------------------------------------------------------------------------------------
0-3 | 32-bit packet ID (Notice: this value is a little-endian integer, unlike other values in this API)
4-19 | packet payload, variable length of 1-16 bytes

## CAN-Bus filter characteristic (UUID 0x0002)

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

Allows one PID to go through (used after Deny all command). Multiple calls will allow multiple PIDs to go through.

byte index  | description
------ | -----------------------------------------------------------------------------------------------------------------
0 | Command ID = 2
1-2 | Notify interval (PID specific notify interval in milliseconds)
3-6 | 32-bit PID to allow

## GPS main characteristic (UUID 0x0003)

This characteristic is read and notify only.

byte index  | description
------ | -----------------------------------------------------------------------------------------------------------------
0-2 | Sync bits* (3 bits) and time from hour start (21 bits = (minute * 30000) + (seconds * 500) + (milliseconds / 2))
3 | Fix quality (2 bits), locked satellites (6 bits, invalid value 0x3F)
4-7 | Latitude in (degrees * 10_000_000), signed 2's complement, invalid value 0x7FFFFFFF
8-11 | Longitude in (degrees * 10_000_000), signed 2's complement, invalid value 0x7FFFFFFF
12-13 | Altitude (((meters + 500) * 10) & 0x7FFF) or (((meters + 500) & 0x7FFF) \| 0x8000), invalid value 0xFFFF. **
14-15 | Speed in ((km/h * 100) & 0x7FFF) or (((km/h * 10) & 0x7FFF) \| 0x8000), invalid value 0xFFFF. ***
16-17 | Bearing (degrees * 100), invalid value 0xFFFF
18 | HDOP (dop * 10), invalid value 0xFF
19 | VDOP (dop * 10), invalid value 0xFF

*) Sync bits is a 3-bit integer value, that increments every time the value of UUID 4 changes, and it is always same between UUID 3 and 4.

**) Notice the first equation has accuracy of 0.1 meters, but range of only [-500, +6053.5] meters. So be preprared to use the second equation when out of range with the first one.

***) Notice the first equation has accuracy of 0.01 km/h, but range of only [0, 655.35] km/h. So be preprared to use the second equation when out of range with the first one.


## GPS time characteristic (UUID 0x0004)

This characteristic is read and notify only. RaceChrono reads (polls) this characteristic when needed, but some other app might want it to be notified, so please support that too.

byte index | description
----------- | ----------------------------------
0-2 | Sync bits* (3 bits) and hour and date (21 bits = (year - 2000) * 8928 + (month - 1) * 744 + (day - 1) * 24 + hour)

The two GPS characteristics should be matched by comparing the sync bits. If sync bits differ, then the client waits for either of the characteristics to update.

*) Sync bits is a 3-bit integer value, that increments every time the value of UUID 4 changes, and it is always same between UUID 3 and 4.


