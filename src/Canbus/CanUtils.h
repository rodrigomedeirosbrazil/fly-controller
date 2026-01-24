#ifndef CAN_UTILS_H
#define CAN_UTILS_H

#include <Arduino.h>
#include <driver/twai.h>

/**
 * Centralized CAN bus utility functions
 * Eliminates code duplication between Hobbywing, Tmotor, and Canbus classes
 */
class CanUtils {
public:
    // CAN ID parsing methods
    static inline uint8_t getPriorityFromCanId(uint32_t canId) {
        return (canId >> 24) & 0xFF;
    }

    static inline uint16_t getDataTypeIdFromCanId(uint32_t canId) {
        return (canId >> 8) & 0xFFFF;
    }

    static inline uint8_t getNodeIdFromCanId(uint32_t canId) {
        return canId & 0x7F;
    }

    static inline uint8_t getServiceTypeIdFromCanId(uint32_t canId) {
        return (canId >> 16) & 0xFF;
    }

    static inline uint8_t getDestNodeIdFromCanId(uint32_t canId) {
        return (canId >> 8) & 0x7F;
    }

    // Frame type detection
    static inline bool isServiceFrame(uint32_t canId) {
        return (canId & 0x80) >> 7 == 1;
    }

    static inline bool isRequestFrame(uint32_t canId) {
        return (canId & 0x8000) != 0;
    }

    // Tail byte utilities
    static inline uint8_t getTailByteFromPayload(uint8_t *payload, uint8_t canDlc) {
        return payload[canDlc - 1];
    }

    static inline bool isStartOfFrame(uint8_t tailByte) {
        return (tailByte & 0x80) >> 7 == 1;
    }

    static inline bool isEndOfFrame(uint8_t tailByte) {
        return (tailByte & 0x40) >> 6 == 1;
    }

    static inline bool isToggleFrame(uint8_t tailByte) {
        return (tailByte & 0x20) >> 5 == 1;
    }

    static inline uint8_t getTransferId(uint8_t tailByte) {
        return tailByte & 0x1F;
    }
};

#endif // CAN_UTILS_H

