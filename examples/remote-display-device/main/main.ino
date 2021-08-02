#include <Adafruit_Arcada.h>
#include <bluefruit.h>

#define CMD_TYPE_REMOVE_ALL 0
#define CMD_TYPE_REMOVE 1
#define CMD_TYPE_ADD_INCOMPLETE 2
#define CMD_TYPE_ADD 3
#define CMD_TYPE_UPDATE_ALL 4
#define CMD_TYPE_UPDATE 5

#define CMD_RESULT_OK 0
#define CMD_RESULT_PAYLOAD_OUT_OF_SEQUENCE 1
#define CMD_RESULT_EQUATION_EXCEPTION 2

#define MAX_REMAINING_PAYLOAD 2048
#define MAX_PAYLOAD_PART 17
#define MONITOR_NAME_MAX 32
#define MONITORS_MAX 255

static const int32_t INVALID_VALUE = 0x7fffffff;

Adafruit_Arcada arcada;
BLEService mainService = BLEService(0x1ff8);
BLECharacteristic monitorConfigCharacteristic = BLECharacteristic (0x05);
BLECharacteristic monitorNotificationCharacteristic = BLECharacteristic (0x06);

char monitorNames[MONITORS_MAX][MONITOR_NAME_MAX+1];
float monitorMultipliers[MONITORS_MAX];
int32_t monitorValues[MONITORS_MAX];
int nextMonitorId = 0;

boolean wasConnected = true;
boolean monitorConfigStarted = false;
boolean displayStarted = false;
int displayUpdateCount = 0;

void monitorConfigWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);

void monitorNotificationWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);

void bluetoothSetupMainService() {
    mainService.begin();
    monitorConfigCharacteristic.setProperties(CHR_PROPS_INDICATE | CHR_PROPS_WRITE);
    monitorConfigCharacteristic.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    monitorConfigCharacteristic.setWriteCallback(monitorConfigWriteCallback);
    monitorConfigCharacteristic.begin();
    monitorNotificationCharacteristic.setProperties(CHR_PROPS_WRITE_WO_RESP);
    monitorNotificationCharacteristic.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
    monitorNotificationCharacteristic.setWriteCallback(monitorNotificationWriteCallback);
    monitorNotificationCharacteristic.begin();
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
    Bluefruit.getAddr(mac);
    char name[255];
    snprintf(name, sizeof(name), "RC DIY #%02X%02X", mac[4], mac[5]);
    Bluefruit.setName(name);     
    bluetoothSetupMainService();
    bluetoothStartAdvertising(); 
}

boolean sendConfigCommand(int cmdType, int monitorId, const char* payload, int payloadSequence = 0) {
    // Initially use CMD_TYPE_ADD instead of CMD_TYPE_ADD_INCOMPLETE
    cmdType = cmdType == CMD_TYPE_ADD_INCOMPLETE ? CMD_TYPE_ADD : cmdType;

    // Figure out payload
    char* remainingPayload = NULL; 
    char payloadPart[MAX_PAYLOAD_PART + 1];
    if (payload && cmdType == CMD_TYPE_ADD) {
        // Copy first 17 characters to payload
        strncpy(payloadPart, payload, MAX_PAYLOAD_PART);
        payloadPart[MAX_PAYLOAD_PART] = '\0';
        
         // If it does not fit to one payload, save the remaining part
        int payloadLen = strlen(payload);
        if (payloadLen > MAX_PAYLOAD_PART) {
            int remainingPayloadLen = payloadLen - MAX_PAYLOAD_PART;
            remainingPayload = (char*)malloc(remainingPayloadLen + 1);
            strncpy(remainingPayload, payload + MAX_PAYLOAD_PART, remainingPayloadLen);
            remainingPayload[remainingPayloadLen] = '\0';
            cmdType = CMD_TYPE_ADD_INCOMPLETE;
        }
    } else {
        payloadPart[0] = '\0';
    }
  
    // Indicate the characteristic
    byte bytes[20];
    bytes[0] = (byte)cmdType;
    bytes[1] = (byte)monitorId;
    bytes[2] = (byte)payloadSequence;
    memcpy(bytes + 3, payloadPart, strlen(payloadPart));
    if (!monitorConfigCharacteristic.indicate(bytes,  3 + strlen(payloadPart))) {
        return false;
    }

    // Handle remaining payload
    if (remainingPayload) {
        boolean r = sendConfigCommand(CMD_TYPE_ADD, monitorId, remainingPayload, payloadSequence + 1);
        free(remainingPayload);
        return r;
    } else {
        return true;
    }
}

boolean addMonitor(const char* monitorName, const char* filterDef, float multiplier) {
    if (nextMonitorId < MONITORS_MAX) {
        if (!sendConfigCommand(CMD_TYPE_ADD, nextMonitorId, filterDef)) {
            return false;
        }
        strncpy(monitorNames[nextMonitorId], monitorName, MONITOR_NAME_MAX);
        monitorNames[nextMonitorId][MONITOR_NAME_MAX] = '\0';
        monitorMultipliers[nextMonitorId] = multiplier;
        nextMonitorId++;
    }
    return true;
}

boolean configureMonitors() {
    // Configure monitors
    nextMonitorId = 0;
    if (!addMonitor("Time", "channel(device(gps), elapsed_time)*10.0", 0.1) ||       
        !addMonitor("Speed", "channel(device(gps), speed)*10.0", 0.1) ||
        !addMonitor("Altitude", "channel(device(gps), altitude)", 1.0) ||
        !addMonitor("Curr lap", "channel(device(lap), lap_number)", 1.0) ||
        !addMonitor("Curr time", "channel(device(lap), lap_time)*10.0", 0.1) ||
        !addMonitor("Prev lap", "channel(device(lap), previous_lap_number)", 1.0) ||
        !addMonitor("Prev time", "channel(device(lap), previous_lap_time)*10.0", 0.1) ||
        !addMonitor("Best lap", "channel(device(lap), best_lap_number)", 1.0) ||
        !addMonitor("Best time", "channel(device(lap), best_lap_time)*10.0", 0.1)) {
        return false;
    }
    return true;    
}

void monitorConfigWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    if (len >= 1) {
        int result = data[0];
        int monitorId = data[1];
        switch (result) {
            case CMD_RESULT_PAYLOAD_OUT_OF_SEQUENCE:
                arcada.display->setTextColor(ARCADA_RED, ARCADA_BLACK);
                arcada.display->println("");
                arcada.display->print(monitorId);
                arcada.display->println(" out-of-sequence");
                break;
            case CMD_RESULT_EQUATION_EXCEPTION:
                // Notice exception details available in the data
                arcada.display->setTextColor(ARCADA_RED, ARCADA_BLACK);
                arcada.display->println("");
                arcada.display->print(monitorId);
                arcada.display->println(" exception");
                break;
            default:
                break;
        }
    }
}


void monitorNotificationWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    int dataPos = 0;
    while (dataPos + 5 <= len) {
        int monitorId = (int)data[dataPos];
        int32_t value = data[dataPos + 1] << 24 | data[dataPos + 2] << 16 | data[dataPos + 3] << 8 | data[dataPos + 4];
        if (monitorId < nextMonitorId) {
            monitorValues[monitorId] = value;
        }
        dataPos += 5;
    }
}

void setup() {
    arcada.displayBegin();
    arcada.setBacklight(250);
    arcada.display->setCursor(0, 0);
    arcada.display->setTextWrap(false);
    arcada.display->setTextSize(2);
    bluetoothStart();
}

void updateDisplay() {
    if (!displayStarted) {
        displayStarted = true;
        arcada.display->fillScreen(ARCADA_BLACK);
        arcada.display->setTextColor(ARCADA_WHITE, ARCADA_BLACK);
    }
    arcada.display->setCursor(0, 0);
    for (int i = 0; i < nextMonitorId; i++) {
        arcada.display->print(monitorNames[i]);
        arcada.display->print(" ");
        if (monitorValues[i] != INVALID_VALUE) {
            arcada.display->print((float)monitorValues[i] * monitorMultipliers[i]);
        } else {
            arcada.display->print("N/A");      
        }
        arcada.display->println("         ");
    }
}

void handleConnected() {
    // Reset state
    for (int i = 0; i < MONITORS_MAX; i++) {
        monitorValues[i] = INVALID_VALUE;
    }
    monitorConfigStarted = false;
    displayStarted = false;
    displayUpdateCount = 0;

    // Print status
    arcada.display->fillScreen(ARCADA_BLACK);
    arcada.display->setCursor(0, 0);
    arcada.display->setTextColor(ARCADA_BLUE, ARCADA_BLACK);
    arcada.display->println("BLE connected!");

}

void handleDisconnected() {
    // Print status
    arcada.display->fillScreen(ARCADA_BLACK);
    arcada.display->setCursor(0, 0);
    arcada.display->setTextColor(ARCADA_LIGHTGREY, ARCADA_BLACK);
    arcada.display->println("No BLE connection");
}

/**
 * @return true if everything is configured
 */
boolean handleConfigure() {
    // Start configuring when inidicate is enabled for the first time 
    if (!monitorConfigStarted) {
        boolean isIndicating = monitorConfigCharacteristic.indicateEnabled();
        if (isIndicating) {
            // Configure in loop until success or disconnected
            monitorConfigStarted = true;
            while (1) {
                arcada.display->setTextColor(ARCADA_LIGHTGREY, ARCADA_BLACK);
                arcada.display->print("Configuring... ");    
                if (!Bluefruit.connected() || configureMonitors()) {
                    arcada.display->println("Done");    
                    return true;
                }
                arcada.display->setTextColor(ARCADA_RED, ARCADA_BLACK);
                arcada.display->println("Fail");    
            }
        }
        return false;
    }
    return true;
}
  
void loop() {
    // Monitor change in Bluetooth connection status
    boolean isConnected = Bluefruit.connected();
    if (wasConnected != isConnected) {
        wasConnected = isConnected;

        // Print Bluetooth connection status
        if (isConnected) {
            handleConnected();       
        } else {
            handleDisconnected();
        }
    }

    if (isConnected) {
        // Try configuring
        if (handleConfigure()) {
            // Update display, every 200 ms
            displayUpdateCount++;
            if (displayUpdateCount >= 8) {
                updateDisplay();
                displayUpdateCount = 0;   
            }
        }
    }

    delay(25);
}
