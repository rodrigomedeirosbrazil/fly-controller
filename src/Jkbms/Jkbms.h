#ifndef Jkbms_h
#define Jkbms_h

#include <mcp2515.h>

class Jkbms
{
    public:
        // Constructor
        Jkbms();

        // Main parsing method called from Canbus class
        void parseJkbmsMessage(struct can_frame *canMsg);

        // Data access methods
        float getTotalVoltage() const { return totalVoltage; }
        float getCurrent() const { return current; }
        float getTemperature1() const { return temperature1; }
        float getTemperature2() const { return temperature2; }
        uint8_t getSoc() const { return soc; }
        uint8_t getSoh() const { return soh; }
        uint16_t getCycleCount() const { return cycleCount; }
        bool isCharging() const { return charging; }
        bool isDischarging() const { return discharging; }

        // Cell voltage access
        uint8_t getCellCount() const { return cellCount; }
        float getCellVoltage(uint8_t cellIndex) const;
        float getMinCellVoltage() const { return minCellVoltage; }
        float getMaxCellVoltage() const { return maxCellVoltage; }

        // Status and protection flags
        bool isProtectionActive() const { return protectionActive; }
        uint16_t getProtectionFlags() const { return protectionFlags; }

        // Data freshness
        unsigned long getLastUpdateTime() const { return lastUpdateTime; }
        bool isDataFresh() const;

        // Debug methods
        void printStatus() const;

    private:
        // JK BMS CAN message IDs (based on protocol v2.0)
        static const uint32_t JK_CAN_ID_BASIC_INFO = 0x100;
        static const uint32_t JK_CAN_ID_CELL_VOLTAGES = 0x101;
        static const uint32_t JK_CAN_ID_TEMPERATURE = 0x102;
        static const uint32_t JK_CAN_ID_PROTECTION = 0x103;

        // Data storage
        float totalVoltage;           // Total battery pack voltage (V)
        float current;               // Current (A) - positive for charging, negative for discharging
        float temperature1;          // Temperature sensor 1 (°C)
        float temperature2;          // Temperature sensor 2 (°C)
        uint8_t soc;                // State of Charge (0-100%)
        uint8_t soh;                // State of Health (0-100%)
        uint16_t cycleCount;        // Battery cycle count

        // Cell voltages
        uint8_t cellCount;           // Number of cells
        float cellVoltages[24];      // Individual cell voltages (V) - max 24 cells
        float minCellVoltage;       // Minimum cell voltage (V)
        float maxCellVoltage;       // Maximum cell voltage (V)

        // Status flags
        bool charging;              // Charging status
        bool discharging;           // Discharging status
        bool protectionActive;      // Protection status
        uint16_t protectionFlags;  // Protection flags bitmap

        // Data freshness
        unsigned long lastUpdateTime;  // Last update timestamp (ms)
        static const unsigned long DATA_TIMEOUT_MS = 5000;  // 5 seconds timeout

        // Message parsing methods
        void parseBasicInfoMessage(struct can_frame *canMsg);
        void parseCellVoltageMessage(struct can_frame *canMsg);
        void parseTemperatureMessage(struct can_frame *canMsg);
        void parseProtectionMessage(struct can_frame *canMsg);

        // Utility methods
        bool isJkbmsMessage(uint32_t canId);
        void updateCellVoltageStats();
        void resetData();
};

#endif
