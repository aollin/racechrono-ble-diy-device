#include <stdarg.h>
#include <CAN.h>
#include <Wire.h>
#include <Adafruit_GPS.h>
#include <bluefruit.h>
#include "PacketIdInfo.h"

//
// Disable if you do not have CAN-Bus board connected
//
#define HAS_CAN_BUS

//
// Disable if you do not have GPS board connected
//
//#define HAS_GPS

#ifdef HAS_GPS
void dummy_debug(...) {
}
#define debug dummy_debug
#define debugln dummy_debug
#else
#define HAS_DEBUG
#define debugln Serial.println
#define debug Serial.print
#endif

BLEService mainService = BLEService(0x00000001000000fd8933990d6f411ff8);
uint8_t tempData[20];
int ledState = LOW;

#ifdef HAS_CAN_BUS
BLECharacteristic canBusMainCharacteristic   = BLECharacteristic (0x01);
BLECharacteristic canBusFilterCharacteristic = BLECharacteristic (0x02);

static const int CAN_BUS_CMD_DENY_ALL = 0;
static const int CAN_BUS_CMD_ALLOW_ALL = 1;
static const int CAN_BUS_CMD_ADD_PID = 2;
PacketIdInfo canBusPacketIdInfo;
bool canBusAllowUnknownPackets = false;
uint32_t canBusLastNotifyMs = 0;
boolean isCanBusConnected = false;

#endif

#ifdef HAS_GPS
BLECharacteristic gpsMainCharacteristic = BLECharacteristic (0x03);
BLECharacteristic gpsTimeCharacteristic = BLECharacteristic (0x04);

Adafruit_GPS* gps = NULL;
int gpsPreviousDateAndHour = 0;
uint8_t gpsSyncBits = 0;

#endif

void bluetoothSetupMainService(void) {
    mainService.begin();

#ifdef HAS_CAN_BUS
    canBusMainCharacteristic.setProperties(CHR_PROPS_NOTIFY | CHR_PROPS_READ);
    canBusMainCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    canBusMainCharacteristic.begin();
    canBusFilterCharacteristic.setProperties(CHR_PROPS_WRITE);
    canBusFilterCharacteristic.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
    canBusFilterCharacteristic.setWriteCallback(*canBusFilterWriteCallback);
    canBusFilterCharacteristic.begin();
#endif
    
#ifdef HAS_GPS
    gpsMainCharacteristic.setProperties(CHR_PROPS_NOTIFY | CHR_PROPS_READ);
    gpsMainCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    gpsMainCharacteristic.begin();
    gpsTimeCharacteristic.setProperties(CHR_PROPS_NOTIFY | CHR_PROPS_READ);
    gpsTimeCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    gpsTimeCharacteristic.begin();
#endif
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
    snprintf(name, sizeof(name), "RC DIY #%2X%2X", mac[4], mac[5]);
    Bluefruit.setName(name);     
    bluetoothSetupMainService();
    bluetoothStartAdvertising(); 
}

#ifdef HAS_CAN_BUS
void canBusNotifyLatestPacket(BLECharacteristic* characteristic) {
    // Set packet id
    ((uint32_t*)tempData)[0] = CAN.packetId();

    // Set packet payload
    int pos = 4;
    int dataByte = CAN.read();
    while (dataByte != -1 && pos < sizeof(tempData)) {
        tempData[pos] = (uint8_t)dataByte;
        dataByte = CAN.read();
        pos++;
    }

    // Notify
    characteristic->notify(tempData, pos);
}

void canBusFilterWriteCallback(BLECharacteristic& chr, uint8_t* data, uint16_t len, uint16_t offset) {
    if (len < 1) {
        return;
    }
    uint8_t command = data[0];
    switch (command) {
        case CAN_BUS_CMD_DENY_ALL:
            if (len == 1) {
                canBusPacketIdInfo.reset();
                canBusAllowUnknownPackets = false;
                debugln("CAN-Bus command DENY"); 
            }
            break;
        case CAN_BUS_CMD_ALLOW_ALL:
            if (len == 3) {
                canBusPacketIdInfo.reset();
                uint16_t notifyIntervalMs = data[1] << 8 | data[2];
                canBusPacketIdInfo.setDefaultNotifyInterval(notifyIntervalMs); 
                canBusAllowUnknownPackets = true;
                debug("CAN-Bus command ALLOW interval ");
                debugln(notifyIntervalMs); 
            }
            break;
        case CAN_BUS_CMD_ADD_PID:
            if (len == 7) {
                uint16_t notifyIntervalMs = data[1] << 8 | data[2];
                uint32_t pid = data[3] << 24 | data[4] << 16 | data[5] << 8 | data[6];
                canBusPacketIdInfo.setNotifyInterval(pid, notifyIntervalMs);
                debug("CAN-Bus command ADD PID ");
                debug(pid);
                debug(" interval ");
                debugln(notifyIntervalMs); 
            }
            break;
        default:
            break;         
    }  
}

void canBusSetup() {
    // Ininitialize CAN board
    CAN.setClockFrequency(8E6);
    CAN.setPins(28, 29);
}

void canBusLoop() {
    // Manage CAN-Bus connection
    if (!isCanBusConnected && Bluefruit.connected()) {
        // Connect to CAN-Bus
        debug("Connecting CAN-Bus...");
        if (CAN.begin(500E3)) {
            isCanBusConnected = true;
            debugln("Connected!");
        } else {
            debugln("Failed.");
            delay(3000);
        }

        // Clear info
        canBusPacketIdInfo.reset();
    } else if (isCanBusConnected && !Bluefruit.connected()) {
        // Disconnect from CAN-Bus
        CAN.end();
        isCanBusConnected = false;      
    }

    // Handle CAN-Bus data
    if (isCanBusConnected) {   
        // Try to parse packet
        int packetSize = CAN.parsePacket();
        if (packetSize > 0) {
            // received a packet
            uint32_t packetId = CAN.packetId();
            PacketIdInfoItem* infoItem = canBusPacketIdInfo.findItem(packetId, canBusAllowUnknownPackets);
            if (infoItem && infoItem->shouldNotify()) {
                canBusNotifyLatestPacket(&canBusMainCharacteristic);    
                infoItem->markNotified();
            }
        }
    }
}
#endif

#ifdef HAS_GPS
void gpsSetup() {
    gps = new Adafruit_GPS(&Serial);
    gps->begin(9600);
    gps->sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
    gps->sendCommand(PMTK_SET_NMEA_UPDATE_5HZ);    
}

void gpsLoop() {
    char c = gps->read();
    if (gps->newNMEAreceived() && gps->parse(gps->lastNMEA())) {
        // Toggle red LED every time valid NMEA is received
        ledState = ledState == LOW ? HIGH : LOW;
        digitalWrite(LED_RED, ledState);

        // Calculate date field
        int dateAndHour = (gps->year * 8928) + ((gps->month-1) * 744) + ((gps->day-1) * 24) + gps->hour;
        if (gpsPreviousDateAndHour != dateAndHour) {
            gpsPreviousDateAndHour = dateAndHour;
            gpsSyncBits++;
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
        tempData[0] = ((gpsSyncBits & 0x7) << 5) | ((timeSinceHourStart >> 16) & 0x1F);
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
        tempData[0] = ((gpsSyncBits & 0x7) << 5) | ((dateAndHour >> 16) & 0x1F);
        tempData[1] = dateAndHour >> 8;
        tempData[2] = dateAndHour;

        // Notify time characteristics
        gpsTimeCharacteristic.notify(tempData, 3);
    }
}
#endif

void setup() {
#ifdef HAS_DEBUG
    Serial.begin(115200);
    while (!Serial);
#endif
    bluetoothStart();
    pinMode(LED_RED, OUTPUT);
    digitalWrite(LED_RED, ledState);
#ifdef HAS_CAN_BUS
    canBusSetup();
#endif
#ifdef HAS_GPS
    gpsSetup();
#endif    
}

void loop() {
#ifdef HAS_CAN_BUS
    canBusLoop();
#endif
#ifdef HAS_GPS
    gpsLoop();
#endif      
}
