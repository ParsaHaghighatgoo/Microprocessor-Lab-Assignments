#include <Arduino.h>


void setup() {

    pinMode(A0, INPUT);
    Serial.begin(9600);

}



void loop() {
    int LDR_IN = analogRead(A0);
    Serial.println(LDR_IN);
}

