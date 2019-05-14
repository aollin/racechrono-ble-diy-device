# diy-ble-gps


All values are big-endian and unsigned if not stated otherwise

# GPS main characteristic (UUID 3)

byte index  | description
------ | -----------------------------------------------------------------------------------------------------------------
0-2 | Sync bits (3 bits) and time from hour start (21 bits = (minute * 30000) + (seconds * 500) + (milliseconds / 2))
3 | Fix quality (2 bits), locked satellites (6 bits)
4-7 | Latitude in (degrees * 6_000_000, or minutes * 100_000), signed 2's complement, invalid value 0x7FFFFFFF
8-11 | Longitude in (degrees * 6_000_000, or minutes * 100_000), signed 2's complement, invalid value 0x7FFFFFFF
12-13 | Altitude (((meters + 500) * 10) & 0x7FFF) or (((meters + 500) & 0x7FFF) | 0x8000), invalid value 0xFFFF
14-15 | Speed in ((knots * 100) & 0x7FFF) or (((knots * 10) & 0x7FFF) | 0x8000), invalid value 0xFFFF
16-17 | Bearing (degrees * 100), invalid value 0xFFFF
18 | HDOP (dop * 10), invalid value 0xFF
19 | VDOP (dop * 10), invalid value 0xFF

# GPS time characteristic (UUID 4)

 byte
// index
// 3     Sync bits (3 bits) and hour and date (21 bits = (year - 2000) * 8928 + (month - 1) * 744 + (day - 1) * 24 + hour)
//
// The two characteristics should be matched by comparing the sync bits. If sync bits differ, then either of the characteristics should be updated.
//
