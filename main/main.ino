#include <Adafruit_GPS.h>
#include <SoftwareSerial.h>
#include <bluefruit.h>

BLEService mainService = BLEService(0x00000001000000fd8933990d6f411ff8);
BLECharacteristic gpsMainCharacteristic = BLECharacteristic (0x03);
BLECharacteristic gpsTimeCharacteristic = BLECharacteristic (0x04);

void bluetoothSetupMainService(void) {
    mainService.begin();
    gpsMainCharacteristic.setProperties(CHR_PROPS_NOTIFY | CHR_PROPS_READ);
    gpsMainCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    gpsMainCharacteristic.begin();
    gpsTimeCharacteristic.setProperties(CHR_PROPS_NOTIFY | CHR_PROPS_READ);
    gpsTimeCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    gpsTimeCharacteristic.begin();
}

void bluetoothStartAdvertising(void) {
    Bluefruit.setTxPower(+4);
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addService(mainService);
    Bluefruit.Advertising.addName();
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(160, 160); // in unit of 0.625 ms
    Bluefruit.Advertising.setFastTimeout(30);
    Bluefruit.Advertising.start(0);
}

void bluetoothStart() {
    Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
    Bluefruit.begin();
    uint8_t mac[6] = { 0, 0, 0, 0, 0, 0 };
    Bluefruit.Gap.getAddr(mac);
    char name[255];
    snprintf(name, sizeof(name), "DIY GPS #%2X%2X", mac[4], mac[5]);
    Bluefruit.setName(name);     
    bluetoothSetupMainService();
    bluetoothStartAdvertising(); 
}

Adafruit_GPS* gps = NULL;
int ledState = LOW;
int previousDateAndHour = 0;
uint8_t syncBits = 0;
uint8_t tempData[20];

void gpsStart() {
    gps = new Adafruit_GPS(&Serial);
    gps->begin(9600);
    gps->sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
    gps->sendCommand(PMTK_SET_NMEA_UPDATE_5HZ);    
}

void setup() {
    bluetoothStart();
    gpsStart();
    pinMode(LED_RED, OUTPUT);
    digitalWrite(LED_RED, ledState);
}

//
// All values are big-endian and unsigned if not stated otherwise
// 
// Main characteristic (UUID 3)
//
// byte
// index
// 0-2   Sync bits (3 bits) and time from hour start (21 bits = (minute * 30000) + (seconds * 500) + (milliseconds / 2))
// 3     Fix quality (2 bits), locked satellites (6 bits)
// 4-7   Latitude in (degrees * 6_000_000, or minutes * 100_000), signed 2's complement, invalid value 0x7FFFFFFF
// 8-11  Longitude in (degrees * 6_000_000, or minutes * 100_000), signed 2's complement, invalid value 0x7FFFFFFF
// 12-13 Altitude (((meters + 500) * 10) & 0x7FFF) or (((meters + 500) & 0x7FFF) | 0x8000), invalid value 0xFFFF
// 14-15 Speed in ((knots * 100) & 0x7FFF) or (((knots * 10) & 0x7FFF) | 0x8000), invalid value 0xFFFF
// 16-17 Bearing (degrees * 100), invalid value 0xFFFF
// 18    HDOP (dop * 10), invalid value 0xFF
// 19    VDOP (dop * 10), invalid value 0xFF
//
// Time characteristic (UUID 4)
//
// byte
// index
// 3     Sync bits (3 bits) and hour and date (21 bits = (year - 2000) * 8928 + (month - 1) * 744 + (day - 1) * 24 + hour)
//
// The two characteristics should be matched by comparing the sync bits. If sync bits differ, then either of the characteristics should be updated.
//

void loop() {
    char c = gps->read();
    if (gps->newNMEAreceived() && gps->parse(gps->lastNMEA())) {
        // Toggle red LED every time valid NMEA is received
        ledState = ledState == LOW ? HIGH : LOW;
        digitalWrite(LED_RED, ledState);

        // Calculate date field
        int dateAndHour = (gps->year * 8928) + ((gps->month-1) * 744) + ((gps->day-1) * 24) + gps->hour;
        if (previousDateAndHour != dateAndHour) {
            previousDateAndHour = dateAndHour;
            syncBits++;
        }

        // Calculate time field
        int timeSinceHourStart = (gps->minute * 30000) + (gps->seconds * 500) + (gps->milliseconds / 2);

        // Calculate latitude and longitude
        int latitude = gps->latitude_fixed; // / 10000000) * 6000000 + (gps->latitude_fixed % 10000000);
        int longitude = gps->longitude_fixed; // / 10000000) * 6000000 + (gps->longitude_fixed % 10000000);

        // Calculate altitude, speed and bearing
        int altitude = gps->altitude > 6000.f ? (max(0, round(gps->altitude + 500.f)) & 0x7FFF) | 0x8000 : max(0, round((gps->altitude + 500.f) * 10.f)) & 0x7FFF; 
        int speed = gps->speed > 600.f ? ((max(0, round(gps->speed * 10.f))) & 0x7FFF) | 0x8000 : (max(0, round(gps->speed * 100.f))) & 0x7FFF; 
        int bearing = max(0, round(gps->angle * 100.f));

        // Create main data
        tempData[0] = ((syncBits & 0x7) << 5) | ((timeSinceHourStart >> 16) & 0x1F);
        tempData[1] = timeSinceHourStart >> 8;
        tempData[2] = timeSinceHourStart;
        tempData[3] = ((min(0x3, gps->fixquality) & 0x3) << 6) | ((min(0x3F, gps->satellites)) & 0x3F);
        tempData[4] = latitude >> 24;
        tempData[5] = latitude >> 16;
        tempData[6] = latitude >> 8;
        tempData[7] = latitude >> 0;
        tempData[8] = longitude >> 24;
        tempData[9] = longitude >> 16;
        tempData[10] = longitude >> 8;
        tempData[11] = longitude >> 0;
        tempData[12] = altitude >> 8;
        tempData[13] = altitude;
        tempData[14] = speed >> 8;
        tempData[15] = speed;
        tempData[16] = bearing >> 8;
        tempData[17] = bearing;
        tempData[18] = round(gps->HDOP * 10.f);
        tempData[19] = 0xFF; // Unimplemented 
       
        // Notify main characteristics
        gpsMainCharacteristic.notify(tempData, 20);

        // Create time data
        tempData[0] = ((syncBits & 0x7) << 5) | ((previousDateAndHour >> 16) & 0x1F);
        tempData[1] = previousDateAndHour >> 8;
        tempData[2] = previousDateAndHour;

        // Notify time characteristics
        gpsTimeCharacteristic.notify(tempData, 3);
    }
}
