#ifndef UDP_SEND_TOKEN_BUCKET_CLION_PENDULUMSENDER_H
#define UDP_SEND_TOKEN_BUCKET_CLION_PENDULUMSENDER_H


#include <string>
#include <ctime>
#include <sockpp/udp_socket.h>
#include <CppLinuxSerial/SerialPort.hpp>
#include <atomic>
#include "../Logging/PendulumLogger.h"
#include "../Scheduling/PriorityDeterminer.h"
#include "../DelayHistogram/DelayHistogram.h"

using sockpp::udp_socket;
using sockpp::inet_address;

using mn::CppLinuxSerial::SerialPort;
using mn::CppLinuxSerial::BaudRate;
using mn::CppLinuxSerial::NumDataBits;
using mn::CppLinuxSerial::Parity;
using mn::CppLinuxSerial::NumStopBits;
using sockpp::udp_socket;
using sockpp::inet_address;

class PendulumSender {
private:
    std::string serialDeviceName;
    inet_address receiverAddress;
    udp_socket senderSocket;
    PriorityDeterminer* priorityDeterminer;
    std::atomic<bool> stopSending{false};
    SerialPort serialSensor;
    PendulumLogger* logger;
    std::string serialInputBuffer;
    int angleBias;
    std::string teensyInitializationString;
    uint64_t startTime;

    DelayHistogram* delayHistogram;
    unsigned int consecutiveDropCount = 0;
    unsigned int allowedFrameLoss = 0;
    bool useET;

    unsigned long long packetCountTT = 0;
    unsigned long long reliabilityDropCount = 0;
    unsigned long long bytesSentTotal = 0;
    int packetsSentTotal = 0;
    unsigned long long feedbackPacketsCount = 0;
    uint64_t phase = 0;
    uint64_t lastSend = 0;

    std::vector<int> networkDelaysPerPrio;

    std::function<void()> regularCallback;

public:
    PendulumSender(PriorityDeterminer* priorityDeterminer, std::string serialDeviceName, std::string receiverHost,
                   int receiverPort, std::string teensyInitializationString,
                   std::function<void()> regularCallback, std::string logFilePrefix, int angleBias,
                   std::vector<int> networkDelaysPerPrio, DelayHistogram* delayHistogram, bool useET, unsigned int allowedFrameLoss);
    void start();
    void stop();
    void swapPriorityDeterminer(PriorityDeterminer* newPriorityDeterminer, std::string logFilePrefix);
    void sendNewMptbConfigSignal(int number, std::string previousConfigName);
    void sendEndSignal(std::string previousConfigName);
    void setUseET(bool useET);
    void setAllowedFrameLoss(unsigned int allowedFrameLoss);

private:
    void sendPacketET(std::string payload, Delay sampledDelay);
    void sendPacketTT(std::string payload, Delay sampledDelay);
    void sendWithTimestamp(std::string paddedPayload, Delay sampledDelay);

    void configureSocketTXTIME(std::string receiverHost, int receiverPort);
    
    void logSample(std::string payload, Delay sampledDelay);

    void handleSenderFeedback();
    static uint64_t timeSinceEpochMillisec();
    static uint64_t timeSinceEpochMicrosec();
    std::string applyAngleBias(std::string payload, Delay sampledDelay) const;
    void handleControlMessageFromTeensy();

    double calculatePoleAngle(const std::string &payload) const;
};


#endif //UDP_SEND_TOKEN_BUCKET_CLION_PENDULUMSENDER_H
