#include <Arduino.h>

#define TXD2 17
#define RXD2 16

// Give each ESP32 a unique name
#define DEVICE_NAME "ESP32 #1"
// On the other ESP, set: #define DEVICE_NAME "ESP32 #2"

String inputBuffer = "";  // For collecting Serial input

void setup() {
  Serial.begin(9600);                                // USB serial (keyboard input)
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);         // UART2
  Serial.printf("%s is ready for UART chat!\n", DEVICE_NAME);
  Serial.println("Type a message and press ENTER to send:");
}

void loop() {
  // 1️⃣ Check for data coming from Serial Monitor (keyboard)
  if (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {  // User pressed Enter
      if (inputBuffer.length() > 0) {
        Serial2.println(inputBuffer);  // Send via UART
        Serial.printf("[%s] Sent: %s\n", DEVICE_NAME, inputBuffer.c_str());
        inputBuffer = "";  // Clear buffer
      }
    } else {
      inputBuffer += c; 
    }
  }

  // 2️⃣ Check for data coming from the other ESP32 (UART2)
  if (Serial2.available()) {
    String received = Serial2.readStringUntil('\n');
    received.trim();
    if (received.length() > 0) {
      Serial.printf("[%s] Received: %s\n", DEVICE_NAME, received.c_str());
    }
  }
}