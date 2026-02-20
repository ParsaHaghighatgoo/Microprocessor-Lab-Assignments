#include <WiFi.h>
#include <PubSubClient.h>

#define MSG_BUFFER_SIZE 64

// WiFi
const char *ssid = "Its_Meow";
const char *password = "ssss1324";

// MQTT broker
const char *mqtt_server = "192.168.12.1";
const int mqtt_port = 5653;

// Topics :D
const char *led_topic = "k11"; // phone switch publishes here
const char *ldr_topic = "k12"; // ESP publishes here for the gauge

// Pins (your pins)
const int LED_PIN = LED_BUILTIN; // D18 (LED wired to 3.3V -> LED -> GPIO18)
const int LDR_PIN = 32; // GPIO32 (ADC1)

// LED is ACTIVE-LOW because of your wiring
void setLed(bool on)
{
  digitalWrite(LED_PIN, on ? LOW : HIGH);
}

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTry = 0;
unsigned long lastLdrSend = 0;

void setup_wifi()
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *message, unsigned int length)
{
  // Safe null-terminated copy
  char msg[MSG_BUFFER_SIZE];
  unsigned int n = (length < (MSG_BUFFER_SIZE - 1)) ? length : (MSG_BUFFER_SIZE - 1);
  memcpy(msg, message, n);
  msg[n] = '\0';

  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(" | payload: ");
  Serial.println(msg);

  if (strcmp(topic, led_topic) == 0)
  {
    if (strcmp(msg, "on") == 0)
      setLed(true);
    if (strcmp(msg, "off") == 0)
      setLed(false);
  }
}

void reconnect()
{
  if (millis() - lastTry < 2000)
    return;
  lastTry = millis();

  Serial.print("Attempting MQTT connection... ");
  if (client.connect("ESP32Client"))
  {
    Serial.println("connected");
    client.subscribe(led_topic, 1); // QoS 1
  }
  else
  {
    Serial.print("failed, rc=");
    Serial.println(client.state());
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  setLed(false);

  // ADC setup (0..4095)
  analogReadResolution(12);

  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
    return;
  }

  client.loop();

  // Publish LDR value every 200 ms
  if (millis() - lastLdrSend >= 200)
  {
    lastLdrSend = millis();

    int ldr = analogRead(LDR_PIN); // 0..4095
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", ldr);

    client.publish(ldr_topic, buf, true); // retained so gauge instantly shows last value
    Serial.print("LDR = ");
    Serial.println(buf);
  }
}


