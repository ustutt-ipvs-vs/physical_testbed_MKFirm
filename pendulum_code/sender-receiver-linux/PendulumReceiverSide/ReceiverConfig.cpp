#include <fstream>
#include "ReceiverConfig.h"
#include "../nlohmann/json.hpp"

const std::string &ReceiverConfig::getReceiverAddress() const {
    return receiverAddress;
}

const std::string &ReceiverConfig::getPendulumType() const {
    return pendulumType;
}

const std::string &ReceiverConfig::getSerialDeviceName() const {
    return serialDeviceName;
}

bool ReceiverConfig::isAutomaticallyFindSerialDevice() const {
    return automaticallyFindSerialDevice;
}

std::string ReceiverConfig::toString() const {
    std::string result = "ReceiverConfig:\n";
    result += "receiverAddress: " + receiverAddress + "\n";
    result += "pendulumType: " + pendulumType + "\n";
    result += "serialDeviceName: " + serialDeviceName + "\n";
    result += "automaticallyFindSerialDevice: " + std::to_string(automaticallyFindSerialDevice) + "\n";
    result += "swingUpBehavior: " + getSwingUpBehaviorString() + "\n";
    if(swingUpBehavior != NO_SWING_UP) {
        result += "swingUpDistanceFactor: " + std::to_string(swingUpDistanceFactor) + "\n";
        result += "swingUpSpeedFactor: " + std::to_string(swingUpSpeedFactor) + "\n";
        result += "swingUpAccelerationFactor: " + std::to_string(swingUpAccelerationFactor) + "\n";
    }


    result += "controllerKVector: [";
    for(float i : controllerKVector){
        result +=  std::to_string(i) + ", ";
    }
    result += "]\n";
    result += "controllerIntegratorParam: " + std::to_string(controllerIntegratorParam) + "\n";
    result += "controlApproach: " + getControlApproachString() + "\n";

    return result;
}

ReceiverConfig::ReceiverConfig(std::string filename) {
    std::ifstream configFile(filename);
    if (!configFile.good()) {
        throw std::runtime_error("Config file does not exist");
    }

    nlohmann::json configJson = nlohmann::json::parse(configFile);

    receiverAddress = configJson["receiverAddress"];
    pendulumType = configJson["pendulumType"];
    if(configJson.contains("serialDeviceName")) {
        serialDeviceName = configJson["serialDeviceName"];
    } else {
        serialDeviceName = "auto";
    }
    automaticallyFindSerialDevice = (serialDeviceName == "auto");

    if(configJson.contains("swingUpBehavior")) {
        std::string swingUpBehaviorString = configJson["swingUpBehavior"];
        if(swingUpBehaviorString == "swingUpAtStart"){
            swingUpBehavior = SWING_UP_AT_START;
        } else if(swingUpBehaviorString == "swingUpAtNewConfigIfCrashed"){
            swingUpBehavior = SWING_UP_AT_NEW_CONFIG_IF_CRASHED;
        } else if(swingUpBehaviorString == "crashAndSwingUpAtNewConfig"){
            swingUpBehavior = CRASH_AND_SWING_UP_AT_NEW_CONFIG;
        } else if(swingUpBehaviorString == "noSwingUp"){
            swingUpBehavior = NO_SWING_UP;
        } else{
            std::string optionsString;
            for(const std::string& option : swingUpBehaviorStrings){
                optionsString += option;
                if (option != swingUpBehaviorStrings.back()){
                    optionsString += ", ";
                }
            }
            throw std::runtime_error("Unknown swingUpBehavior: " + swingUpBehaviorString
             + " The following options are available: " + optionsString);
        }
    } else {
        swingUpBehavior = NO_SWING_UP;
    }

    if(swingUpBehavior != NO_SWING_UP) {
        if(configJson.contains("swingUpDistanceFactor") &&
            configJson.contains("swingUpSpeedFactor") &&
            configJson.contains("swingUpAccelerationFactor")) {
            swingUpDistanceFactor = configJson["swingUpDistanceFactor"];
            swingUpSpeedFactor = configJson["swingUpSpeedFactor"];
            swingUpAccelerationFactor = configJson["swingUpAccelerationFactor"];
        }
        else {
            throw std::runtime_error("Swing up requires all three parameters: swingUpDistanceFactor, swingUpSpeedFactor, swingUpAccelerationFactor");
        }
    }


    if(configJson.contains("controllerKVector")) {
        controllerKVector = configJson["controllerKVector"].get<std::vector<float>>();;
    } else {
        controllerKVector = {3.6723, 13.5022, -74.6153, -19.8637};
    }

    if(configJson.contains("controllerIntegratorParam")) {
        controllerIntegratorParam = configJson["controllerIntegratorParam"];
    } else {
        controllerIntegratorParam = 5.0322;
    }

    if(configJson.contains("controlApproach")) {
        std::string controlApproachString = configJson["controlApproach"];
        if(controlApproachString == "mLQR"){
            controlApproach = MLQR;
        } else if(controlApproachString == "RobustIO"){
            controlApproach = ROBUST_IO;
        } else{
            std::string optionsString;
            for(const std::string& option : controlApproachStrings){
                optionsString += option;
                if (option != controlApproachStrings.back()){
                    optionsString += ", ";
                }
            }
            throw std::runtime_error("Unknown controlApproach: " + controlApproachString
             + " The following options are available: " + optionsString);
        }
    } else {
        controlApproach = MLQR;
    }
}

int ReceiverConfig::getMotorMaxRPM() const {
    if(pendulumType == "oldPendulum"){
        return 20 * 60;
    } else if(pendulumType == "newPendulum"){
        return 15 * 60;
    } else {
        throw std::runtime_error("Unknown pendulum type: " + pendulumType);
    }
}

double ReceiverConfig::getRevolutionsPerTrack() const {
    if(pendulumType == "oldPendulum"){
        return 20.06;
    } else if(pendulumType == "newPendulum"){
        return 15.00;
    } else{
        throw std::runtime_error("Unknown pendulum type: " + pendulumType);
    }
}

ReceiverConfig::SwingUpBehavior ReceiverConfig::getSwingUpBehavior() const {
    return swingUpBehavior;
}

std::string ReceiverConfig::getSwingUpBehaviorString() const {
    return swingUpBehaviorStrings.at(swingUpBehavior);
}


float ReceiverConfig::getSwingUpDistanceFactor() const {
    return swingUpDistanceFactor;
}

float ReceiverConfig::getSwingUpSpeedFactor() const {
    return swingUpSpeedFactor;
}

float ReceiverConfig::getSwingUpAccelerationFactor() const {
    return swingUpAccelerationFactor;
}

const std::vector<float> &ReceiverConfig::getControllerKVector() const {
    return controllerKVector;
}

float ReceiverConfig::getControllerIntegratorParam() const {
    return controllerIntegratorParam;
}

std::string ReceiverConfig::getKalmanAndControllerParameterString() {
    std::string result;
    for(float i : controllerKVector){
        result +=  std::to_string(i) + ";";
    }
    result += std::to_string(controllerIntegratorParam) + ";";
    result += std::to_string(getControlApproachInt()) + ";";
    return result;
}

ReceiverConfig::ControlApproach ReceiverConfig::getControlApproach() const {
    return controlApproach;
}

std::string ReceiverConfig::getControlApproachString() const {
    return controlApproachStrings.at(controlApproach);
}

int ReceiverConfig::getControlApproachInt() const {
    switch(controlApproach){
        case ReceiverConfig::ControlApproach::MLQR:
            return 0;
        case ReceiverConfig::ControlApproach::ROBUST_IO:
            return 1;
        default:
            throw std::runtime_error("Unknown control approach: " + getControlApproachString());
    }
}




