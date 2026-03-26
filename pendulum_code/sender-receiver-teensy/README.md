# Pendulum Sender and Receiver for Teensy Microcontrollers
This folder contains the code for following programs:
- pendulum_sender sensor-side Teensy 4.1 microcontroller
- pendulum_receiver receiver-side Teensy 3.6 microcontroller

# Setup Arduino IDE
1) Install Arduino IDE (using the AppImage on Linux). Tested Version is 2.3.2.  
Download here: https://support.arduino.cc/hc/en-us/articles/360019833020-Download-and-install-Arduino-IDE

2) Install Teensyduino Add-On in Arduino IDE (see also https://www.pjrc.com/teensy/td_download.html):  
Click File > Preferences  
Copy this link into "Additional boards manager URLs" and close window with OK: https://www.pjrc.com/teensy/package_teensy_index.json  
Click on the Boards Manager (second from the top) in the left sidebar and search for "teensy", then click "install".

3) Install TeensyStep library  
Click on the Library Manager (third from the top) in the left sidebar.  
Search for "TeensyStep", then click "install".  
Search for "BasicLinearAlgebra", then click "install".

4) Add 00-teensy.rules file to rules.d:  
Download file from "https://www.pjrc.com/teensy/00-teensy.rules"  
Copy to rules.d via  
```
sudo cp 00-teensy.rules /etc/udev/rules.d/00-teensy.rules
```

5) If not automatically detected, select the correct Teensy (3.6 or 4.1) in the "Select Board" dropdown in the top bar.