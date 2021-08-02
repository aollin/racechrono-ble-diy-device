# RaceChrono BLE DIY APIs

This project describes the new DIY (or "Do It Yourself") APIs in the RaceChrono app or "the app". The APIs are based on Bluetooth LE (BLE) and are supported in the app for both Android and iOS platforms.

A couple of example DIY device implementations are provided within this project. They are currently all built on Adafruit's "Arduino" boards, and programmed using the Arduino IDE and Adafruit's libraries.

# API description

## Bluetooth LE service

The BLE DIY device needs to explose one Bluetooth LE service, that contains several characteristics depending which features are implemented. The available features on the BLE DIY API are currently GPS, CAN-Bus and Monitor. You can choose which features to implement on your own DIY device.

* The GPS feature enables your device to feed GPS data to the app. 
* The CAN-Bus feature enables your device to feed CAN-Bus data, or pretty much any sensor data, to the app. 
* The Monitor API enables your device to monitor live data collected by the app. Any data that is being collected by the app, from the GPS receiver or the other connected sensors can be monitored, as well as the calculated values such as lap times and the time delta.

The service UUID is 00001ff8-0000-1000-8000-00805f9b34fb (or 0x1ff8 as 16-bit UUID). All encoded byte values are big-endian and unsigned if not stated otherwise.

## CAN-Bus main characteristic (UUID 0x0001)

This characteristic should be exposed as READ and NOTIFY.

byte index  | description
------ | -----------------------------------------------------------------------------------------------------------------
0-3 | 32-bit packet ID (Notice: this value is a little-endian integer, unlike other values in this API)
4-19 | packet payload, variable length of 1-16 bytes

## CAN-Bus filter characteristic (UUID 0x0002)

This characteristic should be exposed as WRITE (with response).

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

This characteristic should be exposed as READ and NOTIFY.

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

*) Sync bits is a 3-bit integer value, that increments every time the value of UUID 4 changes, and it is always same between UUID 0x0003 and 0x0004.

**) Notice the first equation has accuracy of 0.1 meters, but range of only [-500, +6053.5] meters. So be preprared to use the second equation when out of range with the first one.

***) Notice the first equation has accuracy of 0.01 km/h, but range of only [0, 655.35] km/h. So be preprared to use the second equation when out of range with the first one.


## GPS time characteristic (UUID 0x0004)

This characteristic should be exposed as READ and NOTIFY.

The app reads (polls) this characteristic when needed, but some other app might want it to be notified, in theory.

byte index | description
----------- | ----------------------------------
0-2 | Sync bits* (3 bits) and hour and date (21 bits = (year - 2000) * 8928 + (month - 1) * 744 + (day - 1) * 24 + hour)

The two GPS characteristics should be matched by comparing the sync bits. If sync bits differ, then the client waits for either of the characteristics to update.

*) Sync bits is a 3-bit integer value, that increments every time the value of UUID 4 changes, and it is always same between UUID 0x0003 and 0x0004.

## Monitor configuration characteristic (UUID 0x0005)

Will be supported in RaceChrono v7.4.0 beta.

This characteristic should be exposed as INDICATE and WRITE. This characteristic is used by the DIY device to define the values that it wants to monitor through the Monitor API.

### INDICATE operation

The DIY device configures the Monitor API by INDICATING the following.

byte index | description
----------- | ----------------------------------
0 | Command type
1-n | Rest of the bytes are defined by the command type, see below

#### Remove all

Removes all currently monitored values.

byte index | description
----------- | ----------------------------------
0 | Command type, 0 = Remove all

#### Remove

Removes the monitored value with the specified Monitor ID.

byte index | description
----------- | ----------------------------------
0 | Command type, 1 = Remove
1 | Monitor ID

#### Add incompete and Add complete

Adds a monitored value, defined by an equation. The added value can be referred later with the Monitor ID specified here. 

If the equation does not fit to one payload, the first INDICATE operations should be "Add incomplete" and only the last should be "Add complete". The payload sequence number starts from 0, increases on every subsequent payload.

TODO: Explain the equations

byte index | description
----------- | ----------------------------------
0 | Command type, 2 = Add incomplete, 3 = Add complete
1 | Monitor ID
2 | Payload sequence number
3-19 | Payload

#### Update all

Forces an update for all the configured values, even if they have not changed.

byte index | description
----------- | ----------------------------------
0 | Command type, 4 = Update all

#### Update

Forces an update for the value with the specified Monitor ID, even if it has not changed.

byte index | description
----------- | ----------------------------------
0 | Command type, 5 = Update
1 | Monitor ID

### WRITE operation

The app will respond to the INDICATE operations with a WRITE operation as below.

byte index | description
----------- | ----------------------------------
0 | Result
1-n | Rest of the bytes are defined by the result type, see below

#### Success
Everything went well. 

Only for command "Add complete". 

byte index | description
----------- | ----------------------------------
0 | Result, 0 = Success
1 | Monitor ID

#### Payload out-of-sequence
Payloads were received by the app out-of-sequence, please retry. 

Only for command "Add incomplete" and "Add complete".

byte index | description
----------- | ----------------------------------
0 | Result, 1 = Payload out-of-sequence
1 | Monitor ID

#### Equation exception
There was an exception when parsing the equation. The exception type -field shows what kind of problem caused the exception, and the exception position and length -fields show which part of the equation caused the exception.

Only for command "Add incomplete" and "Add complete".

byte index | description
----------- | ----------------------------------
0 | Result, 2 = Equation exception
1 | Monitor ID
2-3 | Equation exception type, 1 = No such variable, 2 = No such function, 3 = Missing left operand, 4 = Missing right operand, 5 = Parentheses mismatch, 6 = Integer out of range, 7 = Float out of range, 8 = Syntax error, 9 = Invalid number of parameters, 10 = Invalid X-axis channel, 11 = Internal error, 12 = No root filter
4-5 | Equation exception position
6-7 | Equation exception length

## Monitor values characteristic (UUID 0x0006)

Will be supported in RaceChrono v7.4.0 beta.

This characteristic should be exposed as WRITE_WITHOUT_RESPONSE. The app will write this characteristic with the changes in the monitored values, defined through the 0x0005 characteristic. Each write operation can contain one or more values. Currently the maximum is 4 values per WRITE, as the effective window size is limited to only 20 bytes. 

Notice, the effective update rate of the monitored values will depend on how many is being monitored.

byte index | description
----------- | ----------------------------------
0 | Monitor ID
1-4 | 32-bit integer value
... | ...
n * 5 + 0 | Monitor ID
n * 5 + 1-4 | 32-bit integer value




