#ifndef MICROCONTROLLERRESETTER_CPP
#define MICROCONTROLLERRESETTER_CPP

#include <string>
#include <iostream>
#include <thread>
#include "../Parameters/Parameters.h"
#include "../nlohmann/json.hpp"
#include "../SerialPortScan/TeensyPortDetector.h"
#include <CppLinuxSerial/SerialPort.hpp>

using mn::CppLinuxSerial::SerialPort;
using mn::CppLinuxSerial::BaudRate;
using mn::CppLinuxSerial::NumDataBits;
using mn::CppLinuxSerial::Parity;
using mn::CppLinuxSerial::NumStopBits;

class MicrocontrollerResetter {

private:
    /**
     * Extracts the last message from a string that may contain multiple messages.
     * Each message is terminated by a newline character.
     * Examples: "message1\nmessage2\nmessage3\n", "message1\n"
     * @param input The string that may contain multiple messages.
     * @return The last message of the string without the newline character at the end.
     */
    static std::string extractLastMessage(const std::string& input) {
        // Find the position of the last but one '\n'
        size_t secondLastNewlinePos = input.find_last_of('\n', input.length() - 2);

        // If no '\n' is found, return the entire string
        if (secondLastNewlinePos == std::string::npos) {
            return input;
        }

        // Extract the substring starting from the position after the second last '\n'
        return input.substr(secondLastNewlinePos + 1);
    }

public:
    static void resetMicrocontroller(){
        bool resetSuccessful = false;
        while(!resetSuccessful) {
            std::string device = TeensyPortDetector::findTeensySerialDevice();
            SerialPort serial(device, BaudRate::B_460800, NumDataBits::EIGHT, Parity::NONE, NumStopBits::ONE);
            serial.SetTimeout(1000);
            serial.Open();
            std::string temp;
            serial.Read(temp); // Clear the buffer

            serial.Write("RESET:\n");
            std::cout << "Sent reset signal to microcontroller." << std::endl;

            std::string responseString;
            serial.Read(responseString);
            std::string lastMessage = extractLastMessage(responseString);

            if(lastMessage.rfind("RESETTING MICROCONTROLLER", 0) == 0){
                // The microcontroller sends this signal immediately before resetting itself.
                resetSuccessful = true;
                std::cout << "Microcontroller was reset." << std::endl;
            }

            serial.Close();
            // Wait 2s before trying again or returning
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
};

#endif //MICROCONTROLLERRESETTER_CPP