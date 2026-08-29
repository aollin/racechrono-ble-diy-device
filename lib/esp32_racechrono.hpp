// Establishes connections to the RaceChrono DIY API over ESP32 BLE

#pragma once

// Imports
#include <string>
#include <vector>
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Ticker.h>

// Namespace for RaceChrono connections via ESP32
namespace ESP32RaceChrono
{
    // Internal usage, forward declarations
    namespace impl
    {
        // Monitor states
        enum monitor_state_t
        {
            UNINITIALIZED,
            STARTED,
            ACTIVE,
            FORCED_REFRESH
        };

        // Callback classes
        class ServerCallbacks;
        class MonConfigCallbacks;
        class MonNotifyCallbacks;
    }

    // Equation for an individual monitor
    class Equation
    {
    private:
        float scale_inv;

    public:
        std::string equation;
        float value;

        // Constructor
        Equation(std::string equation, float scale=1.0f);

        // Updates stored value from the integer given by the API
        void update_from_raw(int32_t raw);

        // Clear the stored value during reset
        void clear() { value = NAN; }
    };

    // Monitor API
    class Monitor
    {
    private:
        // UUIDs, need to be set in constructor
        const BLEUUID SERVICE_UUID;
        const BLEUUID MON_CONFIG_CHAR_UUID;
        const BLEUUID MON_NOTIFY_CHAR_UUID;

        // ESP-IDF BLE objects
        BLEServer* server;
        BLEService* service;
        BLECharacteristic* config_ch;
        BLECharacteristic* notify_ch;

        // Callbacks
        impl::ServerCallbacks* server_callbacks;
        impl::MonConfigCallbacks* mon_config_callbacks;
        impl::MonNotifyCallbacks* mon_notify_callbacks;

        // State and timers
        static const unsigned TIMEOUT_REFRESH_MS = 1500;
        static const unsigned TIMEOUT_RESET_MS = 3000;
        static const unsigned TIMEOUT_INIT_MS = 1000;
        impl::monitor_state_t state;
        Ticker t_state;

        // Add all configured equations to RaceChrono monitors
        void configure_equations();

    public:
        // Requested equations
        std::vector<Equation> eqs;

        // Constructor
        Monitor(BLEServer* server);

        // Destructor
        ~Monitor();

        // Add a monitor by equation
        void add(std::string equation, float scale=1.0f);

        // Request an update of all our equations
        void update_all();

        // Reset and re-configure equations after a disconnect
        void reset();

        // Returns true if any of the equations contain valid data
        bool data_valid();

        // Called when the state timer expires, public for callback access
        void timeout_state();

        // Reset the timeout, public for callback access
        void timeout_reset(bool update_state=true);
    };

    // CAN API
    // Spoofs legitimate CAN traffic to feed sensor data to RaceChrono
    class CANSpoof
    {
    private:
        // UUIDs, need to be set in constructor
        const BLEUUID SERVICE_UUID;
        const BLEUUID CAN_MAIN_CHAR_UUID;
        const BLEUUID CAN_FILTER_CHAR_UUID;

        // ESP-IDF BLE objects
        BLEServer* server;
        BLEService* service;
        BLECharacteristic* main_ch;
        BLECharacteristic* filter_ch;

        // Callbacks
        impl::ServerCallbacks* server_callbacks;

    public:
        // Constructor
        CANSpoof(BLEServer* server);

        // Destructor
        ~CANSpoof();

        // Send a sensor update
        void update(uint32_t id, uint8_t data);
    };

    // Internal usage
    namespace impl
    {
        // C-style function for timer callbacks
        void t_state_callback(ESP32RaceChrono::Monitor* instance);

        // Server callbacks
        class ServerCallbacks : public BLEServerCallbacks
        {
        public:
            void onDisconnect(BLEServer* server);
        };


        // Config characteristic callbacks
        class MonConfigCallbacks : public BLECharacteristicCallbacks
        {
        private:
            Monitor* mon;

        public:
            MonConfigCallbacks(Monitor* mon) : mon(mon) {}
            void onWrite(BLECharacteristic* ch);
        };

        // Notify characteristic callbacks
        class MonNotifyCallbacks : public BLECharacteristicCallbacks
        {
        private:
            Monitor* mon;

        public:
            MonNotifyCallbacks(Monitor* mon) : mon(mon) {}
            void onWrite(BLECharacteristic* ch);
        };
    }
}
