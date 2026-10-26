#include "HobbywingCan.h"
#include "../config_controller.h"
#if IS_HOBBYWING
#include <Arduino.h>
#include "../Canbus/CanUtils.h"
#include "HobbywingCanParser.h"

void HobbywingCan::parseEscMessage(twai_message_t* canMsg) {
    const uint16_t dataTypeId = CanUtils::getDataTypeIdFromCanId(canMsg->identifier);

    if (dataTypeId == HobbywingCanParser::STATUS_MSG1_DATA_TYPE_ID) {
        HobbywingCanParser::StatusMsg1 msg;
        if (!HobbywingCanParser::parseStatusMsg1(canMsg->data, canMsg->data_length_code, msg)) return;
        rpm = msg.rpm;
        directionCcw = msg.isCcw;
        lastReadStatusMsg1 = millis();
        return;
    }

    if (dataTypeId == HobbywingCanParser::STATUS_MSG2_DATA_TYPE_ID) {
        HobbywingCanParser::StatusMsg2 msg;
        if (!HobbywingCanParser::parseStatusMsg2(canMsg->data, canMsg->data_length_code, msg)) return;
        batteryCurrentMilliAmps = msg.batteryCurrentMilliAmps;
        escTemperatureCelsius = msg.escTemperatureCelsius;
        lastReadStatusMsg2 = millis();
        return;
    }
}

bool HobbywingCan::hasTelemetry() const {
    return lastReadStatusMsg2 != 0 && millis() - lastReadStatusMsg2 < TELEMETRY_FRESH_MS;
}

bool HobbywingCan::hasRpm() const {
    return lastReadStatusMsg1 != 0 && millis() - lastReadStatusMsg1 < TELEMETRY_FRESH_MS;
}
#endif
