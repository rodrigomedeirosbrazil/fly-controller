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

    /**
     * Extract Data Type ID from CAN ID (for message frames only)
     * For service frames, use getServiceTypeIdFromCanId() instead
     *
     * DroneCAN message frame format:
     * [28:26] Priority (3 bits)
     * [25:8]  Data Type ID (18 bits, but actual range is 0-32767 = 15 bits)
     * [7]     Service/Message (0 = message)
     * [6:0]   Source Node ID (7 bits)
     */
    static inline uint16_t getDataTypeIdFromCanId(uint32_t canId) {
        // Check if it's a service frame - if so, return 0 (invalid for messages)
        if (isServiceFrame(canId)) {
            return 0;  // Service frames don't have Data Type ID
        }
        // For message frames, Data Type ID is in bits [25:8] (18 bits total)
        // But Data Type ID range is 0-32767 (15 bits), so we mask to 16 bits
        return (canId >> 8) & 0xFFFF;  // Extract bits [25:8] as 16-bit value
    }

    static inline uint8_t getNodeIdFromCanId(uint32_t canId) {
        return canId & 0x7F;
    }

    /**
     * Extract Service Type ID from CAN ID (for service frames only)
     *
     * DroneCAN service frame format:
     * [28:26] Priority (3 bits)
     * [25:16] Service Type ID (10 bits)
     * [15]    Request/Response (1 bit)
     * [14:8]  Destination Node ID (7 bits)
     * [7]     Service/Message (1 = service)
     * [6:0]   Source Node ID (7 bits)
     */
    static inline uint16_t getServiceTypeIdFromCanId(uint32_t canId) {
        if (!isServiceFrame(canId)) {
            return 0;  // Message frames don't have Service Type ID
        }
        // Service Type ID is in bits [25:16] (10 bits)
        return (canId >> 16) & 0x3FF;
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

