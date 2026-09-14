This is a C++ program to relay images from an ESP32 CAM to another ESP32 via esp-now.

The ESP-32 Cam takes a photo every set amount of seconds, breaks it into multiple esp-now packets, then transmits them to another esp32.
The receiving esp32 sends the received data over serial and your computer reconstructs it with Python.

Features:
Wifi free image transmission
Packet ACKs
JPG fragmentation/reassembly

ESP-32 Cam used: https://www.amazon.com/Hosyond-ESP32-CAM-Bluetooth-Development-Compatible/dp/B09TB1GJ7P/
ESP-32 receiver used: https://www.amazon.com/DORHEA-Development-Bluetooth-ESP32-S3-DevKit-ESP32-S3-WROOM/dp/B0CKXJKQ1F/
