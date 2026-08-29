// Relevant API documented on Github:
// https://github.com/aollin/racechrono-ble-diy-device

// Imports
#include "esp32_racechrono.hpp"


// Class for an equation to monitor
ESP32RaceChrono::Equation::Equation(std::string equation, float scale)
    : scale_inv(1.0f / scale)
    , equation(equation)
    , value(NAN) {}

// Convert from a raw int32 value to the float
void ESP32RaceChrono::Equation::update_from_raw(int32_t raw)
{
    // If raw value is 0x7FFFFFFF (INT32_MAX), the data is invalid so set to NAN
    value = (raw == INT32_MAX) ? NAN : static_cast<float>(raw) * scale_inv;
}

// Create the service and configure advertising if necessary. Then add config
// and notify characteristics for the Monitor API
ESP32RaceChrono::Monitor::Monitor(BLEServer* server)
    : SERVICE_UUID((uint16_t) 0x1FF8)
    , MON_CONFIG_CHAR_UUID((uint16_t) 0x0005)
    , MON_NOTIFY_CHAR_UUID((uint16_t) 0x0006)
    , server(server)
    , eqs()
    , state(impl::monitor_state_t::UNINITIALIZED)
{
    // Sanity checks
    assert(TIMEOUT_RESET_MS > TIMEOUT_REFRESH_MS);

    // Establish service if necessary
    service = server->getServiceByUUID(SERVICE_UUID);
    if (service == nullptr)
    {
        service = server->createService(SERVICE_UUID);
    }
    server->getAdvertising()->addServiceUUID(SERVICE_UUID);
    server->startAdvertising();

    // Create the config and notify characteristics with callbacks
    config_ch = service->createCharacteristic(
        MON_CONFIG_CHAR_UUID,
        BLECharacteristic::PROPERTY_INDICATE |
        BLECharacteristic::PROPERTY_WRITE);
    notify_ch = service->createCharacteristic(
        MON_NOTIFY_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE_NR);
    server_callbacks = new impl::ServerCallbacks();
    mon_config_callbacks = new impl::MonConfigCallbacks(this);
    mon_notify_callbacks = new impl::MonNotifyCallbacks(this);
    server->setCallbacks(server_callbacks);
    config_ch->setCallbacks(mon_config_callbacks);
    notify_ch->setCallbacks(mon_notify_callbacks);

    // Start the configured service
    service->start();
    state = impl::monitor_state_t::STARTED;
    configure_equations();
}

// Destructor, untested since current implementation never destroys the class
ESP32RaceChrono::Monitor::~Monitor()
{
    server->removeService(service);
    server->setCallbacks(nullptr);
    config_ch->setCallbacks(nullptr);
    notify_ch->setCallbacks(nullptr);
    delete server_callbacks;
    delete mon_config_callbacks;
    delete mon_notify_callbacks;
}

// Add an equation to the active monitors
void ESP32RaceChrono::Monitor::add(std::string equation, float scale)
{
    eqs.push_back(Equation(equation, scale));
}

// Attempt to register an equation with the RaceChrono API
void ESP32RaceChrono::Monitor::configure_equations()
{
    // If nobody is connected yet, re-set the timer and return
    if (server->getConnectedCount() == 0)
    {
        timeout_reset(false);
        return;
    }

    // Constants and buffers
    const int max_payload = 17; // 3 bytes header, 20 bytes max
    uint8_t payload[24];

    // Iterate over all added equations
    for (unsigned eq_idx = 0; eq_idx < eqs.size(); eq_idx++)
    {
        // Vars for this equation
        auto eq = eqs[eq_idx];
        const size_t equation_len = eq.equation.length();
        uint8_t mon_config_seq_num = 0;

        // Send as many messages as necessary to finish the equation
        for (int i = 0; i < equation_len; i += max_payload)
        {
            // Determine if we can complete the equation with this payload
            size_t payload_len;
            if (equation_len - i > max_payload) // Need another message
            {
                payload[0] = 2;
                payload_len = 20;
            }
            else // Complete, last message for this equation
            {
                payload[0] = 3;
                payload_len = 3 + (equation_len - i);
            }

            // Write payload
            payload[1] = eq_idx;
            payload[2] = mon_config_seq_num;
            memcpy(payload + 3, eq.equation.substr(i).c_str(), payload_len - 3);

            // Send it
            config_ch->setValue(payload, payload_len);
            config_ch->indicate();
            mon_config_seq_num += 1;
        }
    }

    // Set a timeout for retrying to configure equations
    timeout_reset(false);
}

// Request the API send an update for all equations
void ESP32RaceChrono::Monitor::update_all()
{
    uint8_t payload[1];
    payload[0] = 4; // Update all
    config_ch->setValue(payload, sizeof(payload));
    config_ch->indicate();
}

// Request a reset of all equations
void ESP32RaceChrono::Monitor::reset()
{
    // Request RaceChrono to remove all equations if listening
    if (server->getConnectedCount() > 0)
    {
        uint8_t payload[1];
        payload[0] = 0; // Reset
        config_ch->setValue(payload, sizeof(payload));
        config_ch->indicate();
    }

    // Reset all our stored values
    for (auto eq : eqs) { eq.clear(); }

    // Reset our state to STARTED and re-configure equations
    state = impl::monitor_state_t::STARTED;
    configure_equations();
}

// Do we have valid data?
bool ESP32RaceChrono::Monitor::data_valid()
{
    return state == impl::monitor_state_t::ACTIVE ||
        state == impl::monitor_state_t::FORCED_REFRESH;
}

// Timeout occured, transition state
void ESP32RaceChrono::Monitor::timeout_state()
{
    switch(state)
    {
        case impl::monitor_state_t::STARTED:
            // Re-try adding equations
            t_state.attach_ms<ESP32RaceChrono::Monitor*>(
                TIMEOUT_INIT_MS, impl::t_state_callback, this);
            configure_equations();
            break;
        case impl::monitor_state_t::ACTIVE:
            // Request a refresh and set a reset timer
            t_state.attach_ms<ESP32RaceChrono::Monitor*>(
                TIMEOUT_RESET_MS - TIMEOUT_REFRESH_MS,
                impl::t_state_callback, this);
            state = impl::monitor_state_t::FORCED_REFRESH;
            update_all();
            break;
        case impl::monitor_state_t::FORCED_REFRESH:
            // No response from RaceChrono, reset
            reset();
            break;
    }
}

// Reset the timeout
void ESP32RaceChrono::Monitor::timeout_reset(bool update_state)
{
    switch(state)
    {
        case impl::monitor_state_t::STARTED:
        case impl::monitor_state_t::ACTIVE:
        case impl::monitor_state_t::FORCED_REFRESH:
            // Reset the refresh timer
            t_state.attach_ms<ESP32RaceChrono::Monitor*>(TIMEOUT_REFRESH_MS,
                impl::t_state_callback, this);
            if (update_state) { state = impl::monitor_state_t::ACTIVE; }
            break;
    }
}

// C-style callback for timers
void ESP32RaceChrono::impl::t_state_callback(ESP32RaceChrono::Monitor* instance)
{
    instance->timeout_state();
}

// Server callback re-starts advertising after a disconnect
void ESP32RaceChrono::impl::ServerCallbacks::onDisconnect(BLEServer* server)
{
    server->startAdvertising();
}

// Monitor Config characteristic callback
void ESP32RaceChrono::impl::MonConfigCallbacks::onWrite(BLECharacteristic* ch)
{
    // Reset timers on any successful equation registration
    if (ch->getLength() == 2 && ch->getData()[0] == 0)
    {
        mon->timeout_reset();
    }
}

// Monitor Notify characteristic callback
void ESP32RaceChrono::impl::MonNotifyCallbacks::onWrite(BLECharacteristic* ch)
{
    // Serial.print("onWrite:");
    // for (int i = 0; i < ch->getLength(); i++)
    // {
    //     Serial.print(" ");
    //     Serial.print(ch->getData()[i]);
    // }
    // Serial.println();
    uint8_t* raw = ch->getData();
    for (int i = 0; i < ch->getLength(); i += 5)
    {
        // uint8 ID, int32 value
        int monitor_id = (int) raw[i];
        int val_raw = raw[i+1]<<24 | raw[i+2]<<16 | raw[i+3]<<8 | raw[i+4];
        mon->eqs[monitor_id].update_from_raw(val_raw);
    }
    mon->timeout_reset();
}

// Spoof CAN messages to pass sensor data to RaceChrono
ESP32RaceChrono::CANSpoof::CANSpoof(BLEServer* server)
    : SERVICE_UUID((uint16_t) 0x1FF8)
    , CAN_MAIN_CHAR_UUID((uint16_t) 0x0001)
    , CAN_FILTER_CHAR_UUID((uint16_t) 0x0002)
    , server(server)
{
    // Establish service if necessary
    service = server->getServiceByUUID(SERVICE_UUID);
    if (service == nullptr)
    {
        service = server->createService(SERVICE_UUID);
    }
    server->getAdvertising()->addServiceUUID(SERVICE_UUID);
    server->startAdvertising();

    // Create the main and filter characteristic and set the server callback
    main_ch = service->createCharacteristic(
        CAN_MAIN_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY);
    filter_ch = service->createCharacteristic(
        CAN_FILTER_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    server_callbacks = new impl::ServerCallbacks();
    server->setCallbacks(server_callbacks);

    // Start the configured service
    service->start();
}

// Destructor, untested since current implementation never destroys the class
ESP32RaceChrono::CANSpoof::~CANSpoof()
{
    server->removeService(service);
    server->setCallbacks(nullptr);
    delete server_callbacks;
}

// Send RaceChrono a new sensor value
void ESP32RaceChrono::CANSpoof::update(uint32_t id, uint8_t data)
{
    // If disconnected, do nothing
    if (server->getConnectedCount() == 0) { return; }

    // 4-byte CAN ID + 1-byte data
    uint8_t payload[5];

    // ID is explicitly little endian, only 1 byte of data for now
    payload[0] = id >> 0;
    payload[1] = id >> 8;
    payload[2] = id >> 16;
    payload[3] = id >> 24;
    payload[4] = data;

    // Publish update on the main characteristic
    main_ch->setValue(payload, sizeof(payload));
    main_ch->notify();
}
