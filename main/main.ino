#include <Adafruit_GPS.h>
#include <SoftwareSerial.h>
#include <bluefruit.h>

BLEService        mainService          = BLEService        (0x00000001000000fd8933990d6f411ff8);
BLECharacteristic gpsMainCharacteristic   = BLECharacteristic (0x03);
BLECharacteristic gpsTimeCharacteristic = BLECharacteristic (0x04);

void bluetoothSetupMainService(void) {
    mainService.begin();
    gpsMainCharacteristic.setProperties(CHR_PROPS_NOTIFY | CHR_PROPS_READ);
    gpsMainCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    gpsMainCharacteristic.begin();
    gpsTimeCharacteristic.setProperties(CHR_PROPS_READ);
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
    Bluefruit.autoConnLed(false);
}

Adafruit_GPS* gps = NULL;
int ledState = LOW;

void gpsStart() {
    gps = new Adafruit_GPS(&Serial);
    gps->begin(9600);
    gps->sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
    gps->sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);    
}

void setup() {
    bluetoothStart();
    gpsStart();
    pinMode(LED_RED, OUTPUT);
    digitalWrite(LED_RED, ledState);
}

void loop() {
    char c = gps->read();
    if (gps->newNMEAreceived() && gps->parse(gps->lastNMEA())) {
        ledState = ledState == LOW ? HIGH : LOW;
        digitalWrite(LED_RED, ledState);
    }
}
