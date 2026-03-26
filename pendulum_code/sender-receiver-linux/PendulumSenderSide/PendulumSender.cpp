#include "PendulumSender.h"
#include "../Parameters/Parameters.h"
#include <sstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <linux/net_tstamp.h>

#define NSEC_PER_SEC 1000000000L

static uint64_t gettime_ns(clockid_t clock)
{
    struct timespec ts;

    if (clock_gettime(clock, &ts))
        std::cout << "Error gettime" << "\n";

    return ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
}

PendulumSender::PendulumSender(PriorityDeterminer* priorityDeterminer, std::string serialDeviceName,
                               std::string receiverHost, int receiverPort, std::string teensyInitializationString,
                               std::function<void()> regularCallback, std::string logFilePrefix, int angleBias,
                               std::vector<int> networkDelaysPerPrio, DelayHistogram* delay_histogram, bool useET, unsigned int allowedFrameLoss)
                               : regularCallback(regularCallback), delayHistogram(delay_histogram) {
    this->serialDeviceName = serialDeviceName;
    receiverAddress = inet_address(receiverHost, receiverPort);

    serialSensor = SerialPort(serialDeviceName, BaudRate::B_460800, NumDataBits::EIGHT, Parity::NONE, NumStopBits::ONE);
    serialSensor.SetTimeout(-1);

    this->priorityDeterminer = priorityDeterminer;
    this->teensyInitializationString = teensyInitializationString;
    this->logger = new PendulumLogger(logFilePrefix);
    this->angleBias = angleBias;
    this->networkDelaysPerPrio = networkDelaysPerPrio;

    this->useET = useET;
    this->allowedFrameLoss = allowedFrameLoss;

    configureSocketTXTIME(receiverHost, receiverPort);
}

void PendulumSender::configureSocketTXTIME(std::string receiverHost, int receiverPort) {
    int err;
    struct addrinfo hints = {
            .ai_flags = 0,
            .ai_family = AF_UNSPEC,    // Allow IPv4 and IPv6
            .ai_socktype = SOCK_DGRAM, // unreliable communication with datagram
            .ai_protocol = 0, // socket address with any protocol can be returned
    };

    // get valid list of address structures
    struct addrinfo *res;
    err = getaddrinfo(receiverHost.data(), &std::to_string(receiverPort)[0], &hints, &res);
    if (err != 0)
        throw std::runtime_error("PendulumSender: getaddrinfo()");

    err = connect(senderSocket.handle(), res->ai_addr, res->ai_addrlen);
    if (err != 0)
        throw std::runtime_error("PendulumSender: socket.connect()");

    // activating Tx_time
    struct sock_txtime so_txtime_val = { .clockid = CLOCK_TAI };
    so_txtime_val.flags = SOF_TXTIME_REPORT_ERRORS;

    struct sock_txtime so_txtime_val_read = { 0 };
    socklen_t vallen = sizeof(so_txtime_val);

    if (setsockopt(senderSocket.handle(), SOL_SOCKET, SO_TXTIME, &so_txtime_val, vallen))
        throw std::runtime_error("PendulumSender: setsockopt txtime");

    if (getsockopt(senderSocket.handle(), SOL_SOCKET, SO_TXTIME, &so_txtime_val_read, &vallen))
        throw std::runtime_error("PendulumSender: getsockopt txtime");

    if (vallen != sizeof(so_txtime_val) || memcmp(&so_txtime_val, &so_txtime_val_read, vallen))
        throw std::runtime_error("PendulumSender: getsockopt txtime: mismatch");
}

void PendulumSender::start() {
    serialSensor.Open();

    std::string payload = "Test-Message\n";
    senderSocket.set_option(SOL_SOCKET, SO_PRIORITY, 0);
    senderSocket.send_to(payload, receiverAddress);

    // Wait for sender to be ready:
    std::cout << "Waiting for sender Teensy to send READY signal." << std::endl;
    serialSensor.Read(serialInputBuffer);
    while (serialInputBuffer.rfind("READY", 0) != 0) {
        serialInputBuffer.clear();
        serialSensor.Read(serialInputBuffer);
    }

    // Send initialization parameters to Teensy:
    serialSensor.Write(teensyInitializationString);

    // Wait for first serial values to arrive, before going into main loop:
    std::cout << "Waiting for first sensor value" << std::endl;
    serialSensor.Read(serialInputBuffer);
    std::cout << serialInputBuffer << std::endl;
    serialInputBuffer.clear();

    startTime = timeSinceEpochMillisec();
    bool swingUpGracePhase = true;

    std::cout << "StartTime: " << startTime << std::endl;

    while (!stopSending) {
        // frames are sent at time phase + sampled_delay
        // this ensures that any additional delay (e.g., by serialSensor.Read) is not included
        serialInputBuffer.clear();
        serialSensor.Read(serialInputBuffer);

        if (serialInputBuffer.rfind("FB:", 0) == 0) {
            handleSenderFeedback();
        } else if (serialInputBuffer.rfind("CT:", 0) == 0) {
            handleControlMessageFromTeensy();
            swingUpGracePhase = false;
        } else if (serialInputBuffer.rfind("Starting to send sensor values through feedback link for swing-up.", 0) == 0) {
            swingUpGracePhase = true;
            std::cout << serialInputBuffer << std::endl;
        } else if(serialInputBuffer.rfind("S:", 0) == 0){
            uint64_t now = timeSinceEpochMicrosec();
            if (((now % 75000) + 6000 < 75000) || now - lastSend < 7000) {
                continue;
            }

            lastSend = now;

            uint64_t startAtCycleBegin = 75000 - (now % 75000);
            std::this_thread::sleep_for(std::chrono::microseconds(startAtCycleBegin));
            phase = gettime_ns(CLOCK_TAI);
            assert(startAtCycleBegin <= 6000);

            // Sometimes the serial interface sends multiple samples at once (separated by '\n').
            // We need to split them up and send them individually over the network:
            std::string singleSample;
            std::istringstream sampleStream(serialInputBuffer);
            std::string mostRecentSample;
            while (std::getline(sampleStream, singleSample, '\n')) {
                singleSample += '\n';
                // Get most recent sample
                mostRecentSample = singleSample;
            }

            // Sample histogram and send as TT or ET traffic or drop
            Delay sampledDelay = delayHistogram->random_sample();
            logSample(mostRecentSample, sampledDelay);

            if(delayHistogram->is_in_reliability_range(sampledDelay)) {
                // sampledDelay is in this case equals to delayHistogram->packetDelayBudgetMax - 1
                sendPacketTT(mostRecentSample, delayHistogram->packetDelayBudgetMax - 1);
                packetCountTT++;
                consecutiveDropCount = 0;
            } else if (consecutiveDropCount >= allowedFrameLoss) {

                if(useET && delayHistogram->is_in_et_range(sampledDelay)) {
                    sendPacketET(mostRecentSample, sampledDelay);
                    consecutiveDropCount = 0;
                } else {
                    consecutiveDropCount++;
                    std::cout << "WARNING: Dropped " << consecutiveDropCount << " consecutive packets" << std::endl;
                }

                reliabilityDropCount++;
                // std::cout << "Reliability until now: " << (double) packetCountTT / (double) (reliabilityDropCount+packetCountTT) << std::endl;

            } else {
                consecutiveDropCount++;
                reliabilityDropCount++;
            }

        } else {
            std::cout << serialInputBuffer << std::endl;
        }

        if(regularCallback != nullptr && !swingUpGracePhase){
            regularCallback();
        }

    }
}

void PendulumSender::handleControlMessageFromTeensy() {
    if (serialInputBuffer.rfind("CT:GracePeriodEnded;", 0) == 0) {
        // Pendulum is starting to balance using network. Swing-up and grace period has ended.
        std::cout << "Grace period has ended. Resetting priority determiner and logger now." << std::endl;
        priorityDeterminer->resetState(); // Reset priority determiner when pendulum starts balancing
        logger->reset(); // Reset logger so that swing-up and grace period is not part of logs
    } else {
        std::cout << "Received unknown control message from sender Teensy: " << serialInputBuffer << std::endl;
    }
}

void PendulumSender::handleSenderFeedback() {
    logger->logSenderFeedback(serialInputBuffer);
    feedbackPacketsCount++;

    if(feedbackPacketsCount % 10 == 0){
        // std::cout << "Serial Feedback packet: " << serialInputBuffer;
    }
}

void PendulumSender::stop() {
    stopSending = true;
    serialSensor.Close();
    logger->saveToFile();
}

void PendulumSender::setUseET(bool useET) {
    this->useET = useET;
}

void PendulumSender::setAllowedFrameLoss(unsigned int allowedFrameLoss) {
    this->allowedFrameLoss = allowedFrameLoss;
}

void PendulumSender::logSample(std::string payload, Delay sampledDelay) {
    payload = applyAngleBias(payload, sampledDelay);
    logger->log(packetCountTT, bytesSentTotal, payload);
}

void PendulumSender::sendWithTimestamp(std::string paddedPayload, Delay sampledDelay){
    // create msghdr with cmsghdr
    char control[CMSG_SPACE(sizeof(uint64_t))];
    struct msghdr msg = {0};
    struct iovec iov = {0};
    struct cmsghdr *cm;

    iov.iov_base = paddedPayload.data();
    iov.iov_len = paddedPayload.size();

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    memset(control, 0, sizeof(control));
    msg.msg_control = &control;
    msg.msg_controllen = sizeof(control);

    cm = CMSG_FIRSTHDR(&msg);
    cm->cmsg_level = SOL_SOCKET;
    cm->cmsg_type = SCM_TXTIME;
    cm->cmsg_len = CMSG_LEN(sizeof(uint64_t));

    *((uint64_t *)CMSG_DATA(cm)) = static_cast<uint64_t>(sampledDelay) + phase;

    assert(gettime_ns(CLOCK_TAI) < static_cast<uint64_t>(sampledDelay) + phase);
    sendmsg(senderSocket.handle(), &msg, 0);
}

void PendulumSender::sendPacketET(std::string payload, Delay sampledDelay) {
    priorityDeterminer->reportPacketReadyToSend(SAMPLE_SIZE);
    payload = applyAngleBias(payload, sampledDelay);

    // Pad payload width '#' to SAMPLE_SIZE bytes and store result in paddedPayload
    std::string paddedPayload = payload;
    paddedPayload.append(SAMPLE_SIZE - payload.size(), '#');

    senderSocket.set_option(SOL_SOCKET, SO_PRIORITY, 6);
    sendWithTimestamp(paddedPayload, sampledDelay);

    bytesSentTotal += paddedPayload.size();
    packetsSentTotal += 1;
    
    std::cout << "Sent ET packet: " << priorityDeterminer->getDebugInfoString() << std::endl;
}

void PendulumSender::sendPacketTT(std::string payload, Delay sampledDelay) {
    // Refill token bucket for accurate output
    priorityDeterminer->fillBucket();

    payload = applyAngleBias(payload, sampledDelay);

    // Pad payload width '#' to SAMPLE_SIZE bytes and store result in paddedPayload
    std::string paddedPayload = payload;
    paddedPayload.append(SAMPLE_SIZE - payload.size(), '#');

    senderSocket.set_option(SOL_SOCKET, SO_PRIORITY, 5);
    sendWithTimestamp(paddedPayload, sampledDelay);

    bytesSentTotal += paddedPayload.size();
    packetsSentTotal += 1;
  
    std::cout << "Sent TT packet: " << packetsSentTotal << std::endl;
}

double PendulumSender::calculatePoleAngle(const std::string &payload) const {
    // Payload: S:encoderValue;samplingPeriodMillis;sequenceNumber;angularVelocity;currentTime;latencyMillis;\n
    size_t delimPos = payload.find(';');
    std::string currentValue = payload.substr(2,delimPos);
    int encoderValue = std::stoi(currentValue);
    encoderValue = ((encoderValue % 2400) + 2400) % 2400; // Make sure encoderValue is in [0, 2400)
    const double DEG_PER_ESTEP = 360.0 / 2400.0;
    double poleAngle = DEG_PER_ESTEP * (encoderValue-1200);
    return poleAngle;
}

uint64_t PendulumSender::timeSinceEpochMillisec(){
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

uint64_t PendulumSender::timeSinceEpochMicrosec(){
    using namespace std::chrono;
    return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
}

void PendulumSender::swapPriorityDeterminer(PriorityDeterminer *newPriorityDeterminer, std::string logFilePrefix) {
    // Save logs of old priorityDeterminer to file.
    // Save to file asynchronous so that main thread does not get blocked.
    logger->saveToFileAsync();

    // Swap priorityDeterminer
    priorityDeterminer = newPriorityDeterminer;

    // Reset statistics
    startTime = timeSinceEpochMillisec();
    packetCountTT = 0;
    reliabilityDropCount = 0;
    bytesSentTotal = 0;
    feedbackPacketsCount = 0;

    // new logger for new priorityDeterminer
    logger = new PendulumLogger(logFilePrefix);
}

/**
 * Sends a signal to the receiver to change the MPTB config to the given number.
 *
 * The payload of the signal is "NewConfig:number;previousConfigName\n" where number is the parameter of this
 * function encoded as a string and previousConfigName is the name of the previous MPTB config used for the
 * log file.
 *
 * The signal is sent with the highest priority (0).
 *
 * @throws std::runtime_error if senderSocket is not open
 * @param number
 */
void PendulumSender::sendNewMptbConfigSignal(int number, std::string previousConfigName) {
    if(!senderSocket.is_open()){
        throw std::runtime_error("PendulumSender: Cannot send new MPTB config signal because senderSocket is not open.");
    }

    std::string payload = "NewConfig:" + std::to_string(number) + ";" + previousConfigName + "\n";
    senderSocket.set_option(SOL_SOCKET, SO_PRIORITY, 0);
    senderSocket.send_to(payload, receiverAddress);
    std::cout << "PendulumSender: Sent new MPTB config signal: " << payload << std::endl;
}

/**
 * Sends a signal to the receiver to end the current run.
 *
 * The payload of the signal is "EndSignal:previousConfigName\n".
 * The signal is sent with the highest priority (0).
 *
 * @throws std::runtime_error if senderSocket is not open
 */
void PendulumSender::sendEndSignal(std::string previousConfigName) {
    if(!senderSocket.is_open()){
        throw std::runtime_error("PendulumSender: Cannot send End signal because senderSocket is not open.");
    }

    std::string payload = "EndSignal:" + previousConfigName + "\n";
    senderSocket.set_option(SOL_SOCKET, SO_PRIORITY, 0);
    senderSocket.send_to(payload, receiverAddress);

    std::cout << "PendulumSender: Sent End signal: " << payload << std::endl;
}

/**
 * Adds the given angle bias to the encoder value and appends the given network delay.
 *
 * The payload has the following form:
 * S:encoderValue;samplingPeriodMillis;sequenceNumber;angularVelocity;currentTime;0;\n
 *
 * the returned payload has the following form:
 * S:encoderValue+angleBias;samplingPeriodMillis;sequenceNumber;currentTime;angularVelocity;networkDelay;\n
 */
std::string PendulumSender::applyAngleBias(std::string payload, Delay sampledDelay) const {
    // Extract encoder value from payload:
    int pendulumSensorValue;
    int networkDelay = static_cast<int>(round((double) sampledDelay / 1e6));
    std::stringstream stringStream(payload);
    stringStream.ignore(2); // skip 'S:'
    stringStream >> pendulumSensorValue;

    // Add angle bias to encoder value:
    pendulumSensorValue += angleBias;

    // Extract sampling period from payload:
    unsigned int samplingPeriod;
    stringStream.ignore(1); // skip ';'
    stringStream >> samplingPeriod;
    // Adjust sampling period according to dropped packets and sampled Delay
    // samplingPeriod *= (consecutiveDropCount+1);
    
    // Not needed
    // Extract sequence number from payload
    int sequenceNumber;
    stringStream.ignore(1); // skip ';'
    stringStream >> sequenceNumber;

    // Extract current time from payload:
    unsigned long long currentTime;
    stringStream.ignore(1); // skip ';'
    stringStream >> currentTime;

    // Extract angular velocity from payload:
    double angularVelocity;
    stringStream.ignore(1); // skip ';'
    stringStream >> angularVelocity;

    // Reconstruct payload with new encoder value:
    std::string result = "S:"
            + std::to_string(pendulumSensorValue) + ";"
            + std::to_string(samplingPeriod) + ";"
            + std::to_string(packetsSentTotal) + ";"
            + std::to_string(currentTime) + ";"
            + std::to_string(angularVelocity) + ";"
            + std::to_string(networkDelay) + ";";

    result += '\n';
    return result;
}

