#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "esp_camera.h"

// ============================================================
// USER SETTINGS
// ============================================================

// MAC address of the ESP-NOW receiver.
// Enter the receiver's MAC exactly like this:
// AA:BB:CC:DD:EE:FF
#define mac_address "AC:27:6E:A4:D3:38"

// ESP-NOW Wi-Fi channel
#define ESPNOW_CHANNEL 6

// Capture a new image every 5 seconds
#define CAPTURE_INTERVAL_MS 5000

// JPEG bytes carried by each ESP-NOW packet.
// 220 + 11 byte header = 231 bytes total.
#define MAX_PAYLOAD 220

// How long to wait for an ACK
#define ACK_TIMEOUT_MS 200

// Maximum number of attempts for each packet
#define MAX_RETRIES 8


// ============================================================
// AI THINKER ESP32-CAM PINS
// ============================================================

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22


// ============================================================
// GLOBALS
// ============================================================

uint8_t RECEIVER_MAC[6];

volatile bool ackReceived = false;
volatile uint32_t ackFrameID = 0;
volatile uint16_t ackSequence = 0;

uint32_t frameCounter = 0;


// ============================================================
// MAC ADDRESS PARSER
// ============================================================
//
// Converts:
//
// "AA:BB:CC:DD:EE:FF"
//
// into:
//
// {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}
//
// ============================================================

bool parseMacAddress(const char *macString, uint8_t *mac)
{
    unsigned int values[6];

    int result = sscanf(
        macString,
        "%2x:%2x:%2x:%2x:%2x:%2x",
        &values[0],
        &values[1],
        &values[2],
        &values[3],
        &values[4],
        &values[5]
    );

    if (result != 6)
    {
        return false;
    }

    for (int i = 0; i < 6; i++)
    {
        if (values[i] > 0xFF)
        {
            return false;
        }

        mac[i] = (uint8_t)values[i];
    }

    return true;
}


// ============================================================
// PRINT MAC ADDRESS
// ============================================================

void printMacAddress(const uint8_t *mac)
{
    for (int i = 0; i < 6; i++)
    {
        if (mac[i] < 0x10)
        {
            Serial.print("0");
        }

        Serial.print(mac[i], HEX);

        if (i < 5)
        {
            Serial.print(":");
        }
    }
}


// ============================================================
// ESP-NOW RECEIVE CALLBACK
// ============================================================
//
// The receiver sends back a 7-byte ACK:
//
// byte 0     = 0x02
// bytes 1-4  = frame ID
// bytes 5-6  = sequence number
//
// ============================================================

void onDataRecv(
    const esp_now_recv_info_t *info,
    const uint8_t *data,
    int len
)
{
    if (len != 7)
    {
        return;
    }

    // ACK packet type
    if (data[0] != 0x02)
    {
        return;
    }

    uint32_t frameID =
        ((uint32_t)data[1]) |
        ((uint32_t)data[2] << 8) |
        ((uint32_t)data[3] << 16) |
        ((uint32_t)data[4] << 24);

    uint16_t sequence =
        ((uint16_t)data[5]) |
        ((uint16_t)data[6] << 8);

    ackFrameID = frameID;
    ackSequence = sequence;
    ackReceived = true;
}


// ============================================================
// CRC32
// ============================================================

uint32_t calculateCRC32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < length; i++)
    {
        crc ^= data[i];

        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
            {
                crc = (crc >> 1) ^ 0xEDB88320;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}


// ============================================================
// SEND ONE IMAGE PACKET
// ============================================================
//
// Packet format:
//
// Byte 0       : packet type = 0x01
// Bytes 1-4    : frame ID
// Bytes 5-6    : sequence number
// Bytes 7-8    : total packets
// Bytes 9-10   : JPEG payload length
// Bytes 11...  : JPEG data
//
// Maximum:
// 11 + 220 = 231 bytes
//
// ============================================================

bool sendImagePacket(
    uint32_t frameID,
    uint16_t sequence,
    uint16_t totalPackets,
    const uint8_t *payload,
    uint16_t payloadLength
)
{
    uint8_t packet[11 + MAX_PAYLOAD];

    // --------------------------------------------------------
    // Build exact packet manually.
    // This avoids C++ struct-padding problems.
    // --------------------------------------------------------

    packet[0] = 0x01;

    // Frame ID
    packet[1] = frameID & 0xFF;
    packet[2] = (frameID >> 8) & 0xFF;
    packet[3] = (frameID >> 16) & 0xFF;
    packet[4] = (frameID >> 24) & 0xFF;

    // Sequence
    packet[5] = sequence & 0xFF;
    packet[6] = (sequence >> 8) & 0xFF;

    // Total packets
    packet[7] = totalPackets & 0xFF;
    packet[8] = (totalPackets >> 8) & 0xFF;

    // Payload length
    packet[9] = payloadLength & 0xFF;
    packet[10] = (payloadLength >> 8) & 0xFF;

    // JPEG payload
    memcpy(
        packet + 11,
        payload,
        payloadLength
    );

    size_t packetLength = 11 + payloadLength;


    // --------------------------------------------------------
    // Try sending this packet
    // --------------------------------------------------------

    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++)
    {
        ackReceived = false;
        ackFrameID = 0;
        ackSequence = 0;

        esp_err_t result = esp_now_send(
            RECEIVER_MAC,
            packet,
            packetLength
        );

        if (result != ESP_OK)
        {
            Serial.printf(
                "ESP-NOW send error on seq %u, attempt %d: %s\n",
                sequence,
                attempt,
                esp_err_to_name(result)
            );

            delay(20);
            continue;
        }


        // ----------------------------------------------------
        // Wait for ACK
        // ----------------------------------------------------

        unsigned long startTime = millis();

        while (millis() - startTime < ACK_TIMEOUT_MS)
        {
            if (
                ackReceived &&
                ackFrameID == frameID &&
                ackSequence == sequence
            )
            {
                return true;
            }

            delay(1);
        }

        Serial.printf(
            "No ACK for packet %u/%u, attempt %d/%d\n",
            sequence + 1,
            totalPackets,
            attempt,
            MAX_RETRIES
        );
    }

    return false;
}


// ============================================================
// SEND ENTIRE IMAGE
// ============================================================

bool sendImage(camera_fb_t *fb)
{
    uint32_t frameID = frameCounter++;

    uint16_t totalPackets =
        (fb->len + MAX_PAYLOAD - 1) / MAX_PAYLOAD;

    uint32_t crc = calculateCRC32(
        fb->buf,
        fb->len
    );

    Serial.println();
    Serial.println("================================");
    Serial.printf(
        "Sending frame %lu\n",
        (unsigned long)frameID
    );

    Serial.printf(
        "JPEG size: %u bytes\n",
        (unsigned int)fb->len
    );

    Serial.printf(
        "Packets: %u\n",
        totalPackets
    );

    Serial.printf(
        "CRC32: %08lX\n",
        (unsigned long)crc
    );

    Serial.println(
        "================================"
    );


    // --------------------------------------------------------
    // Send every JPEG chunk
    // --------------------------------------------------------

    for (
        uint16_t sequence = 0;
        sequence < totalPackets;
        sequence++
    )
    {
        size_t offset =
            (size_t)sequence * MAX_PAYLOAD;

        size_t remaining =
            fb->len - offset;

        uint16_t payloadLength =
            remaining > MAX_PAYLOAD
                ? MAX_PAYLOAD
                : remaining;

        bool success = sendImagePacket(
            frameID,
            sequence,
            totalPackets,
            fb->buf + offset,
            payloadLength
        );

        if (!success)
        {
            Serial.printf(
                "FAILED packet %u/%u\n",
                sequence + 1,
                totalPackets
            );

            return false;
        }

        Serial.printf(
            "Packet %u/%u OK\n",
            sequence + 1,
            totalPackets
        );
    }

    Serial.println();
    Serial.println(
        "Image transmission successful."
    );

    return true;
}


// ============================================================
// CAMERA INITIALIZATION
// ============================================================

bool initializeCamera()
{
    camera_config_t config;

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;

    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;

    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;

    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;

    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;

    config.xclk_freq_hz = 20000000;

    config.pixel_format = PIXFORMAT_JPEG;


    // --------------------------------------------------------
    // Use VGA when PSRAM is available.
    // Otherwise fall back to QVGA.
    // --------------------------------------------------------

    if (psramFound())
    {
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 15;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_PSRAM;

        Serial.println(
            "PSRAM detected - using VGA."
        );
    }
    else
    {
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 15;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;

        Serial.println(
            "No PSRAM - using QVGA."
        );
    }

    esp_err_t result = esp_camera_init(&config);

    if (result != ESP_OK)
    {
        Serial.printf(
            "Camera initialization failed: 0x%X\n",
            result
        );

        return false;
    }

    return true;
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println();
    Serial.println(
        "========================================"
    );
    Serial.println(
        "ESP32-CAM ESP-NOW TRANSMITTER"
    );
    Serial.println(
        "========================================"
    );


    // --------------------------------------------------------
    // Parse receiver MAC
    // --------------------------------------------------------

    Serial.print(
        "Configured receiver MAC: "
    );

    Serial.println(mac_address);

    if (!parseMacAddress(
            mac_address,
            RECEIVER_MAC
        ))
    {
        Serial.println();
        Serial.println(
            "ERROR: Invalid MAC address!"
        );

        Serial.println(
            "Expected format:"
        );

        Serial.println(
            "AA:BB:CC:DD:EE:FF"
        );

        while (true)
        {
            delay(1000);
        }
    }

    Serial.print(
        "Parsed receiver MAC: "
    );

    printMacAddress(RECEIVER_MAC);

    Serial.println();


    // --------------------------------------------------------
    // Wi-Fi / ESP-NOW
    // --------------------------------------------------------

    WiFi.mode(WIFI_STA);

    delay(100);

    // Force ESP-NOW channel.
    esp_err_t channelResult =
        esp_wifi_set_channel(
            ESPNOW_CHANNEL,
            WIFI_SECOND_CHAN_NONE
        );

    if (channelResult != ESP_OK)
    {
        Serial.printf(
            "Failed to set Wi-Fi channel: %s\n",
            esp_err_to_name(channelResult)
        );
    }

    Serial.printf(
        "ESP-NOW channel: %d\n",
        ESPNOW_CHANNEL
    );


    // --------------------------------------------------------
    // Print our own MAC
    // --------------------------------------------------------

    Serial.print(
        "Transmitter MAC: "
    );

    Serial.println(
        WiFi.macAddress()
    );


    // --------------------------------------------------------
    // Initialize ESP-NOW
    // --------------------------------------------------------

    if (esp_now_init() != ESP_OK)
    {
        Serial.println(
            "ERROR: ESP-NOW initialization failed!"
        );

        while (true)
        {
            delay(1000);
        }
    }


    // --------------------------------------------------------
    // Register receive callback
    // --------------------------------------------------------

    esp_now_register_recv_cb(
        onDataRecv
    );


    // --------------------------------------------------------
    // Add receiver as ESP-NOW peer
    // --------------------------------------------------------

    esp_now_peer_info_t peerInfo = {};

    memcpy(
        peerInfo.peer_addr,
        RECEIVER_MAC,
        6
    );

    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;

    esp_err_t peerResult =
        esp_now_add_peer(&peerInfo);

    if (peerResult != ESP_OK)
    {
        Serial.printf(
            "ERROR: Failed to add receiver peer: %s\n",
            esp_err_to_name(peerResult)
        );

        while (true)
        {
            delay(1000);
        }
    }

    Serial.println(
        "Receiver peer added successfully."
    );


    // --------------------------------------------------------
    // Initialize camera
    // --------------------------------------------------------

    if (!initializeCamera())
    {
        Serial.println(
            "ERROR: Camera initialization failed!"
        );

        while (true)
        {
            delay(1000);
        }
    }

    Serial.println(
        "Camera initialized successfully."
    );

    Serial.println();
    Serial.println(
        "Ready."
    );

    Serial.println(
        "========================================"
    );
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
    // --------------------------------------------------------
    // Capture image
    // --------------------------------------------------------

    Serial.println();
    Serial.println(
        "Capturing image..."
    );

    camera_fb_t *fb =
        esp_camera_fb_get();

    if (!fb)
    {
        Serial.println(
            "ERROR: Camera capture failed!"
        );

        delay(CAPTURE_INTERVAL_MS);

        return;
    }


    // --------------------------------------------------------
    // Validate JPEG
    // --------------------------------------------------------

    bool validJPEG = false;

    if (fb->len >= 4)
    {
        bool startsJPEG =
            fb->buf[0] == 0xFF &&
            fb->buf[1] == 0xD8;

        bool endsJPEG =
            fb->buf[fb->len - 2] == 0xFF &&
            fb->buf[fb->len - 1] == 0xD9;

        validJPEG =
            startsJPEG &&
            endsJPEG;
    }

    if (!validJPEG)
    {
        Serial.println(
            "ERROR: Captured data is not a valid JPEG!"
        );

        esp_camera_fb_return(fb);

        delay(CAPTURE_INTERVAL_MS);

        return;
    }


    // --------------------------------------------------------
    // Send image
    // --------------------------------------------------------

    bool success = sendImage(fb);

    if (!success)
    {
        Serial.println();
        Serial.println(
            "Image transmission failed."
        );
    }


    // --------------------------------------------------------
    // Return framebuffer
    // --------------------------------------------------------

    esp_camera_fb_return(fb);


    // --------------------------------------------------------
    // Wait before next image
    // --------------------------------------------------------

    delay(CAPTURE_INTERVAL_MS);
}