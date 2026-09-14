# ESPNOW Image bridge

Relay images over esp-now protocol. The ESP-32 Cam takes a photo every few seconds, breaks it into multiple esp-now packets, then transmits them to another esp32. The receiving esp32 sends the received data over serial and a linux machine reconstructs it with Python.
  

## Features:

- Wifi free image transmission
- Packet ACKs
- JPG fragmentation/reassembly
- Python image reconstruction
- Long range capabilities

  


## Receiver setup tutorial
Plug in your ESP32 and open Arduino IDE. We need to get the ESP32 receiver's mac address before the transmitter can be programmed. Set the board as an ESP32 Dev Module or a ESP32S3 Dev module if you are using ESP32S3. I personally use an ESP32S3 because of the ipex antenna connector that will allow for better range.

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
| TXD | UOR |
| RXD | UOT |

Connect IOD to GND on the ESP32 Cam to enter bootloader mode(only use when programming)
This wiring will establish the Serial connection for the esp32 camera.

![Camera to cp1202 usb serial device wiring](https://raw.githubusercontent.com/lucianbuilds/espnow-image-bridge/refs/heads/main/photos/cam-usbttl-wiring.png)


**Program ESP32 Camera**
Open Arduino IDE and ensure the ESP32 library by expressif is installed. Plug in the USB-TTL dongle and set the board as an ESP32 Dev Module.

![enter image description here](https://raw.githubusercontent.com/lucianbuilds/espnow-image-bridge/refs/heads/main/photos/Adruino%20cam%20setup1.png)


Then enable PSRAM for the board.


![enter image description here](https://raw.githubusercontent.com/lucianbuilds/espnow-image-bridge/refs/heads/main/photos/Adruino%20cam%20setup2.png)


Now it's time to program. Copy & paste the [transmitter source code](https://github.com/lucianbuilds/espnow-image-bridge/blob/main/source/transmitter.ino) into the IDE. After you pasted the code, find the User Settings portion and set `mac_address` to the receiver's mac address. You can change the channel if you'd like, but it has to match the receiver's channel. In the US, channels 1/6/11 are best as they are non overlapping. I personally use channel 6 (2437MHZ).

![enter image description here](https://raw.githubusercontent.com/lucianbuilds/espnow-image-bridge/refs/heads/main/photos/Transmitter%20config.png)

Now that the config is finished, you are ready to upload your code. Press the Upload button. Once the code is uploaded, open the serial monitor and set the baud rate to 115200. Plug in your receiver then Unplug GND from IOD and unplug and replug in the usb-ttl dongle. If everything is working, you will see "Image transmission successful". 
If not, you will see "Image transmission failed" or another error. Ensure the MAC address is correct, the channels are the same, and that the camera is connected.

![enter image description here](https://raw.githubusercontent.com/lucianbuilds/espnow-image-bridge/refs/heads/main/photos/Cameraserial.png)

Now that it's working, it's time to get Python to decode these images.

## Python image decoding
Ensure python is installed on your machine. Download the [decoding source code](https://github.com/lucianbuilds/espnow-image-bridge/blob/main/source/serialimagereconstruction.py) and paste it into your IDE on your Linux machine. We need set the serial port of the receiver, disconnect the receiver, open terminal and do:

    ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
Nothing should pop up. Reconnect the receiver and do the command again. Something like `/dev/ttyACM0` should pop up, that is your receiver's serial port. Copy it. Go back to the IDE and paste the serial address into `SERIAL_PORT`

![enter image description here](https://github.com/lucianbuilds/espnow-image-bridge/blob/main/photos/pycode.png?raw=true)

Run the code through the IDE or terminal. You should see a camera folder appear in the same folder the python script is in. Open it and open latest.png, there is your camera output.

![enter image description here](https://github.com/lucianbuilds/espnow-image-bridge/blob/main/photos/Pythonimage.png?raw=true)
Picture is of my ceiling

## Resources
ESP-32 Cam used: https://www.amazon.com/Hosyond-ESP32-CAM-Bluetooth-Development-Compatible/dp/B09TB1GJ7P/

USB-TTL dongle: https://www.amazon.com/WWZMDiB-CP2102-USB-TTL-Programming/dp/B0BCYRFZJD/

ESP-32 receiver used: https://www.amazon.com/DORHEA-Development-Bluetooth-ESP32-S3-DevKit-ESP32-S3-WROOM/dp/B0CKXJKQ1F/
