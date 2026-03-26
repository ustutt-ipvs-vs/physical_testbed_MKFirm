#include <thread>
#include "PendulumSender.h"
#include "../Scheduling/MultiPriorityTokenBucket.h"

#include "../SerialPortScan/TeensyPortDetector.h"
#include "SenderMultiConfig.h"
#include "MPTBSubConfig.h"
#include "../Parameters/Parameters.h"
#include "../DelayHistogram/DelayHistogram.h"
#include "../Helper/MicrocontrollerResetter.cpp"

/**
 * Usage from command line:
 *
 *  Run a sequence of MPTB sub-configs from a JSON config file. An example config file can be found in
 * "exampleSenderSequenceConfig.json".
 * ./pendulum_sender s <filename>
 */

std::string device = "/dev/ttyACM0";

std::string host;
int port = 3000;

// Teensy angle bias of the raw sensor value (2400 steps per revolution)
// this value gets added to the raw sensor value before it is sent to the receiver
int teensyAngleBias;

std::string teensyInitializationString;

PendulumSender *sender;
DelayHistogram *delayHistogram;



void runMptbSequence(int argc, char *const *argv);

void sigIntHandler(int signal) {
    std::cout << "Received Signal: " << signal << std::endl;
    sender->stop();
    delete sender;
    exit(0);
}

double numberOfSamplesToBytes(double numberOfSamples) {
    return numberOfSamples * FRAME_SIZE_OF_SAMPLE;
}

double samplingPeriodToDataRate(double samplingPeriod) {
    return (1000.0 / samplingPeriod) * FRAME_SIZE_OF_SAMPLE;
}

PriorityDeterminer *getIthSubconfigMptbDeterminer(int i, SenderMultiConfig config){
    MPTBSubConfig subConfig = config.getMptbSubConfigs().at(i);
    double bAsBytes = numberOfSamplesToBytes(subConfig.getB());
    double rAsBytesPerSecond = samplingPeriodToDataRate(subConfig.getR());
    std::vector<double> thresholdsBytes;
    for (double threshold : subConfig.getThresholds()) {
        thresholdsBytes.push_back(numberOfSamplesToBytes(threshold));
    }

    return new MultiPriorityTokenBucket(bAsBytes, rAsBytesPerSecond, subConfig.getNumThresholds(),
                                              thresholdsBytes,
                                              subConfig.getCosts(), subConfig.getPrioMapping());
}

uint64_t timeSinceEpochMillisec(){
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

int main(int argc, char *argv[]) {
    signal(SIGINT, sigIntHandler);

    if(argc < 3 || argv[1][0] != 's'){
        std::cout << "This ET eval version of the pendulum only supports the sequence config mode 's'." << std::endl <<
            "Usage: ./pendulum_sender s <sender_sequence_config_file>";
        exit(1);
    }

    std::string filename = argv[2];
    std::cout << "Using sequence config file " << filename << std::endl;
    SenderMultiConfig config(filename);

    std::cout << "Config: " << config.toString();

    // Load histogram
    const std::filesystem::path filePath = config.getHistogramFilepath(); //"../DelayHistogram/downlink_histogram.json";
    std::ifstream i(filePath);
    if (not i.good()) {
        std::cout << "Error opening histogram file: " << filePath.string() << "\n";
        std::exit(2);
    }
    nlohmann::json j = nlohmann::json::parse(i);

    double reliability = config.getMptbSubConfigs().at(0).getReliability();
    int consecutiveFrameLosses = config.getMptbSubConfigs().at(0).getConsecutiveFrameLosses();
    double durationMinutesBetweenFaults = config.getMptbSubConfigs().at(0).getDurationMinutesBetweenFaults();
    delayHistogram = new DelayHistogram(j, reliability, consecutiveFrameLosses, durationMinutesBetweenFaults);
    std::cout << "Successfully loaded histogram." << std::endl;
    // For test reliability
    /*
    for(int seed = 12345; seed < 1000000; seed+=100000) {
        std::srand(seed);
        delayHistogram->test_reliability();
    }
    */

    host = config.getReceiverAddress();
    teensyAngleBias = config.getBias();
    teensyInitializationString = config.getTeensyInitializationString();

    // Reset Microcontroller so the USB cable can remain plugged in
    MicrocontrollerResetter::resetMicrocontroller();

    if(config.isAutomaticallyFindSerialDevice()){
        std::cout << "Automatically finding serial device..." << std::endl;
        device = TeensyPortDetector::findTeensySerialDevice();
    } else {
        device = config.getSerialDeviceName();
    }
    std::cout << "Using serial device " << device << std::endl;

    std::cout << std::endl;
    std::cout << config.toString() << std::endl;

    int currentConfigurationIndex = 0;
    uint64_t currentConfigurationStartTime = 0;

    auto regularCallback = [&currentConfigurationStartTime, &currentConfigurationIndex, &config]() {
        if(currentConfigurationStartTime == 0) {
            double newReliability = config.getMptbSubConfigs().at(0).getReliability();
	    int newConsecutiveFrameLosses = config.getMptbSubConfigs().at(0).getConsecutiveFrameLosses();
	    double newDurationMinutesBetweenFaults = config.getMptbSubConfigs().at(0).getDurationMinutesBetweenFaults();
            delayHistogram->update(newReliability, newConsecutiveFrameLosses, newDurationMinutesBetweenFaults);

            currentConfigurationStartTime = timeSinceEpochMillisec();
        }
        double currentConfigRuntimeMinutes = config.getMptbSubConfigs().at(currentConfigurationIndex).getDurationMinutes();
        if(timeSinceEpochMillisec() - currentConfigurationStartTime > currentConfigRuntimeMinutes * 60.0 * 1000.0){
            currentConfigurationIndex++;
            if(currentConfigurationIndex >= config.getMptbSubConfigs().size()){
                sender->sendEndSignal(config.getMptbSubConfigs().at(currentConfigurationIndex-1).getName());
                sender->stop();
                std::cout << "Finished all sequence-configs" << std::endl;
                exit(0);
            }
            double nextConfigRuntimeMinutes = config.getMptbSubConfigs().at(currentConfigurationIndex).getDurationMinutes();
            std::cout << "Starting sub-config with duration " << nextConfigRuntimeMinutes << " minutes" << std::endl;
            // Update Histogram reliability
            double newReliability = config.getMptbSubConfigs().at(currentConfigurationIndex).getReliability();
	    int newConsecutiveFrameLosses = config.getMptbSubConfigs().at(currentConfigurationIndex).getConsecutiveFrameLosses();
	    double newDurationMinutesBetweenFaults = config.getMptbSubConfigs().at(currentConfigurationIndex).getDurationMinutesBetweenFaults();
            delayHistogram->update(newReliability, newConsecutiveFrameLosses, newDurationMinutesBetweenFaults);
            std::cout << "New histogram reliability: " << newReliability << ", " << delayHistogram->printDelayBudgets() << std::endl;

            // update useET parameter in sender
            sender->setUseET(config.getMptbSubConfigs().at(currentConfigurationIndex).getUseET());
            sender->setAllowedFrameLoss(config.getMptbSubConfigs().at(currentConfigurationIndex).getAllowedFrameLoss());

            // Swap PriorityDeterminer
            PriorityDeterminer *priorityDeterminer = getIthSubconfigMptbDeterminer(currentConfigurationIndex, config);
            sender->swapPriorityDeterminer(priorityDeterminer,
                                           "pendulumsender_" + config.getMptbSubConfigs().at(currentConfigurationIndex).getName());
            sender->sendNewMptbConfigSignal(currentConfigurationIndex + 1, config.getMptbSubConfigs().at(currentConfigurationIndex-1).getName());
            currentConfigurationStartTime = timeSinceEpochMillisec();
        }
    };

    PriorityDeterminer *determiner = getIthSubconfigMptbDeterminer(0, config);
    sender = new PendulumSender(determiner, device, host, port,
                                teensyInitializationString, regularCallback,
                                "pendulumsender_" + config.getMptbSubConfigs().at(0).getName(),
                                teensyAngleBias, config.getNetworkDelaysPerPrio(), delayHistogram,
                                config.getMptbSubConfigs().at(0).getUseET(), config.getMptbSubConfigs().at(0).getAllowedFrameLoss());
    sender->start();
}




