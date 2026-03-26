cmake_minimum_required(VERSION 3.22)
project(sender-receiver-linux)

set(CMAKE_CXX_STANDARD 17)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

include(FetchContent)
FetchContent_Declare(
  sockpp
  GIT_REPOSITORY https://github.com/fpagliughi/sockpp.git
  GIT_TAG e6c4688a576d95f42dd7628cefe68092f6c5cd0f
)
FetchContent_MakeAvailable(sockpp)

FetchContent_Declare(
  cpplinuxserial
  GIT_REPOSITORY https://github.com/gbmhunter/CppLinuxSerial.git
  GIT_TAG 4bd338e378823e280c84020c0f34a4ab8dbcee96
)
FetchContent_MakeAvailable(cpplinuxserial)

add_executable(pendulum_sender
        PendulumSenderSide/PendulumSender.cpp
        PendulumSenderSide/PendulumSender.h
        Logging/PendulumLogger.cpp
        Logging/PendulumLogger.h
        Logging/LogEntries/PendulumLogEntry.cpp
        Logging/LogEntries/PendulumLogEntry.h
        Logging/Logger.cpp
        Logging/Logger.h
        Logging/LogEntries/LogEntry.cpp
        Logging/LogEntries/LogEntry.h
        nlohmann/json.hpp
        PendulumSenderSide/PendulumSenderMain.cpp
        Logging/LogEntries/SchedulingInfoEntries/SchedulingInfoEntry.h
        Logging/LogEntries/SchedulingInfoEntries/TokenBucketInfoEntry.cpp
        Logging/LogEntries/SchedulingInfoEntries/TokenBucketInfoEntry.h
        Logging/LogEntries/ActuatorLogEntry.cpp
        Logging/LogEntries/ActuatorLogEntry.h
        Logging/LogEntries/SenderFeedbackLogEntry.cpp
        Logging/LogEntries/SenderFeedbackLogEntry.h
        Scheduling/PriorityDeterminer.h
        Scheduling/PriorityDeterminer.cpp
        Scheduling/MultiPriorityTokenBucket.cpp
        Scheduling/MultiPriorityTokenBucket.h
        PendulumSenderSide/SenderConfig.cpp
        PendulumSenderSide/SenderConfig.h
        SerialPortScan/TeensyPortDetector.cpp
        SerialPortScan/TeensyPortDetector.h
        PendulumSenderSide/MPTBSubConfig.cpp
        PendulumSenderSide/MPTBSubConfig.h
        PendulumSenderSide/SenderMultiConfig.h
        PendulumSenderSide/SenderMultiConfig.cpp
        Parameters/Parameters.h
        DelayHistogram/DelayHistogram.cpp
        DelayHistogram/DelayHistogram.h
        Helper/MicrocontrollerResetter.cpp)

add_executable(pendulum_receiver
        PendulumReceiverSide/PendulumReceiverMain.cpp
        PendulumReceiverSide/PendulumReceiver.cpp
        PendulumReceiverSide/PendulumReceiver.h
        Logging/PendulumLogger.cpp
        Logging/PendulumLogger.h
        Logging/LogEntries/PendulumLogEntry.cpp
        Logging/LogEntries/PendulumLogEntry.h
        Logging/Logger.cpp
        Logging/Logger.h
        Logging/LogEntries/LogEntry.cpp
        Logging/LogEntries/LogEntry.h
        nlohmann/json.hpp
        Logging/LogEntries/ActuatorLogEntry.cpp
        Logging/LogEntries/ActuatorLogEntry.h
        Logging/LogEntries/SenderFeedbackLogEntry.cpp
        Logging/LogEntries/SenderFeedbackLogEntry.h
        Scheduling/PriorityDeterminer.h
        Scheduling/PriorityDeterminer.cpp
        Scheduling/MultiPriorityTokenBucket.cpp
        Scheduling/MultiPriorityTokenBucket.h
        Logging/LogEntries/SchedulingInfoEntries/TokenBucketInfoEntry.cpp
        Logging/LogEntries/SchedulingInfoEntries/TokenBucketInfoEntry.h
        SerialPortScan/TeensyPortDetector.cpp
        SerialPortScan/TeensyPortDetector.h
        PendulumReceiverSide/ReceiverConfig.cpp
        PendulumReceiverSide/ReceiverConfig.h
        Parameters/Parameters.h
        Helper/MicrocontrollerResetter.cpp
        )

target_link_libraries(pendulum_sender sockpp)
target_link_libraries(pendulum_sender CppLinuxSerial)

target_link_libraries(pendulum_receiver sockpp)
target_link_libraries(pendulum_receiver CppLinuxSerial)

