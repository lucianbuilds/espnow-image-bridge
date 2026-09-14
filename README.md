# ESPNOW Image bridge

This is a C++ program to relay images from an ESP32 CAM to another ESP32 via esp-now.

  

The ESP-32 Cam takes a photo every set amount of seconds, breaks it into multiple esp-now packets, then transmits them to another esp32.

The receiving esp32 sends the received data over serial and your computer reconstructs it with Python.

  

## Features:

 Wifi free image transmission
- Packet ACKs
- JPG fragmentation/reassembly
- Python image reconstruction

  


## Receiver setup tutorial
Plug in your ESP32 and open Arduino IDE. We need to get the ESP32 receiver's mac address before the transmitter can be programmed. Set the board as an ESP32 Dev Module or a ESP32S3 Dev module if you are using ESP32S3.

![enter image description here](https://raw.githubusercontent.com/lucianbuilds/espnow-image-bridge/refs/heads/main/photos/Adruino%20cam%20setup1.png)

Now that our receiver is correctly setup, we need to program it.
Copy and paste the [receiver source code](https://github.com/lucianbuilds/espnow-image-bridge/blob/main/source/receiver.ino) into the IDE. Press upload.

Once the code is uploaded, we need to open the serial monitor. Press Tools -> Serial monitor. Set baud rate to 921600, Both NL&CR. 

If you got a bunch of garbage in the serial monitor before changing baud rate, just unplug and replug the esp32.

![enter image description here](https://raw.githubusercontent.com/lucianbuilds/espnow-image-bridge/refs/heads/main/photos/Receiver%20asetup.png)

Copy the MAC address of the receiver as you will need it.

## Camera setup tutorial:
 
**Wiring**
Our first step is to wire up the ESP32 Camera for programming.
Take the USB-TTL dongle and connect:
| Dongle | ESP32 Cam |
|--|--|
| +5v | 5v |
| GND | GND |
| TXD | VOR |
| RXD | VOT |

Connect IOD to GND on the ESP32 Cam to enter bootloader mode(only use when programming)
This wiring will establish the Serial connection for the esp32 camera.

![Camera to cp1202 usb serial device wiring](https://raw.githubusercontent.com/lucianbuilds/espnow-image-bridge/refs/heads/main/photos/cam-usbttl-wiring.png)


**Program ESP32 Camera**
Open Arduino IDE and ensure the ESP32 library by expressif is installed. Plug in the USB-TTL dongle and set the board as an ESP32 Dev Module.

![enter image description here](https://raw.githubusercontent.com/lucianbuilds/espnow-image-bridge/refs/heads/main/photos/Adruino%20cam%20setup1.png)


Then enable PSRAM for the board.


![enter image description here](https://raw.githubusercontent.com/lucianbuilds/espnow-image-bridge/refs/heads/main/photos/Adruino%20cam%20setup2.png)


Now it's time to program. Copy & paste the [transmitter source code](https://github.com/lucianbuilds/espnow-image-bridge/blob/main/source/transmitter.ino) into the IDE. After you pasted the code, find the User Settings portion and set **mac_address** to the receiver's mac address. You can change the channel if you'd like, but it has to match the receiver's channel. In the US, channels 1/6/11 are best as they are non overlapping. I personally use channel 6 (2437MHZ).
![enter image description here](https://raw.githubusercontent.com/lucianbuilds/espnow-image-bridge/refs/heads/main/photos/Transmitter%20config.png)

Now that the config is finished, you are ready to upload your code. Press the Upload button. Once the code is uploaded, open the serial monitor and set the baud rate to 115200. Plug in your receiver then Unplug GND from IOD and unplug and replug in the usb-ttl dongle. If everything is working, you will see "Image transmission successful". 
If not, you will see "Image transmission failed" or another error. Ensure the MAC address is correct, the channels are the same, and that the camera is connected.

![enter image description here](https://raw.githubusercontent.com/lucianbuilds/espnow-image-bridge/refs/heads/main/photos/Cameraserial.png)

Now that it's working, it's time to get Python to decode these images.

## Resources
ESP-32 Cam used: https://www.amazon.com/Hosyond-ESP32-CAM-Bluetooth-Development-Compatible/dp/B09TB1GJ7P/

Requires a USB-TTL dongle: https://www.amazon.com/WWZMDiB-CP2102-USB-TTL-Programming/dp/B0BCYRFZJD/

ESP-32 receiver used: https://www.amazon.com/DORHEA-Development-Bluetooth-ESP32-S3-DevKit-ESP32-S3-WROOM/dp/B0CKXJKQ1F/
