/**
 * Usage:
 * ./pendulum_receiver config_file.json
 */

#include <string>
#include "PendulumReceiver.h"
#include "../SerialPortScan/TeensyPortDetector.h"
#include "../Helper/MicrocontrollerResetter.cpp"

int port = 3000;

PendulumReceiver* receiver;

void sigIntHandler(int signal){
    std::cout << "Received Signal: " << signal << std::endl;
    receiver->stop();
    delete receiver;
    exit(0);
}

int main(int argc, char *argv[]){
    signal(SIGINT, sigIntHandler);

    if(argc >= 2) {
        std::string configFile = argv[1];
        ReceiverConfig config(configFile);
        std::cout << "Using config file: " << configFile << std::endl;
        std::cout << config.toString() << std::endl;

        // Reset Microcontroller so the USB cable can remain plugged in
        MicrocontrollerResetter::resetMicrocontroller();

        std::string device;
        if(config.isAutomaticallyFindSerialDevice()){
            device = TeensyPortDetector::findTeensySerialDevice();
        } else{
            device = config.getSerialDeviceName();
        }

        receiver = new PendulumReceiver(device, config.getReceiverAddress(), port,
                                        config.getMotorMaxRPM(),config.getRevolutionsPerTrack(),
                                        config.getSwingUpDistanceFactor(),config.getSwingUpSpeedFactor(), config.getSwingUpAccelerationFactor(),
                                        config.getSwingUpBehavior(), config.getKalmanAndControllerParameterString());



    } else {
        std::cout << "No config file specified. Can't start." << std::endl;
        return 1;
    }
    receiver->start();
}
