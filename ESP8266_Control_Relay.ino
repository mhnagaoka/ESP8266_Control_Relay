#include <Espalexa.h>
#include <EspalexaDevice.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

#include "my_network.h"

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  8883               // Using port 8883 for MQTTS
#define AIO_LOOP_DELAY  250                // 250 ms
#define RELAY           0                  // relay connected to GPIO0
#define LED             2                  // relay connected to GPIO2

static const char* ssid PROGMEM = MY_SSID;
static const char* password PROGMEM = MY_PASSWORD;

ESP8266WebServer server(80);

// WiFiFlientSecure for SSL/TLS support
WiFiClientSecure secureClient;
// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&secureClient, AIO_SERVER, AIO_SERVERPORT, MY_AIO_USERNAME, MY_AIO_KEY);
// io.adafruit.com SHA1 fingerprint
static const char *fingerprint PROGMEM = "59 3C 48 0A B1 8B 39 4E 0D 58 50 47 9A 13 55 60 CC A0 1D AF";
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Publish ledStatusPub = Adafruit_MQTT_Publish(&mqtt, MY_AIO_USERNAME "/f/ledStatus");
Adafruit_MQTT_Publish ledCommandPub = Adafruit_MQTT_Publish(&mqtt, MY_AIO_USERNAME "/f/ledCommand");
Adafruit_MQTT_Subscribe ledCommandSub = Adafruit_MQTT_Subscribe(&mqtt, MY_AIO_USERNAME "/f/ledCommand");

Espalexa espalexa;
EspalexaDevice* device;

unsigned long lastMqttActivity;
int value = -1;

void blink(int l, int h, int c) {
  for (int i = 0; i < c; i++) {
    digitalWrite(LED, LOW); // Acende o Led (ativo baixo)
    delay(l);
    digitalWrite(LED, HIGH); // Apaga o Led
    delay(h);
  }
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    blink(900, 100, 5); // wait 5 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      while (1);
    }
  }

  Serial.println("MQTT Connected!");
}

int updateRelay(int newValue, int publishCommand) {
  if (value != newValue) {
    Serial.print("RELAY=");
    Serial.println(newValue);
    value = newValue;
    digitalWrite(RELAY, newValue);
    digitalWrite(LED, newValue);
    ledStatusPub.publish(newValue);
    if (publishCommand) {
      ledCommandPub.publish(newValue ? "ON" : "OFF");
    }
    lastMqttActivity = millis();
  }
  return newValue;
}

void sendRedirect(char *location) {
  server.sendHeader("Location", location);
  server.send(302);
  Serial.print("Redirect ");
  Serial.println(location);
}

void handleShowPage() {
  Serial.printf("Handling HTTP %d: %s", server.method(), server.uri().c_str());
  Serial.println();
  String page = "<!DOCTYPE HTML>";
  page += "<html>";
  page += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>ESP8266 RELAY Control</title></head><body>";

  if (value == HIGH)
  {
    page += "<p>Relay is now: ON</p>";
    page += "<form method=\"post\"><input type=\"hidden\" name=\"r\" value=\"0\"/><button>Turn off relay</button></form>";
  }
  else
  {
    page += "<p>Relay is now: OFF</p>";
    page += "<form method=\"post\"><input type=\"hidden\" name=\"r\" value=\"1\"/><button>Turn on relay</button></form>";
  }
  page += "</body></html>";
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "text/html", page);
  Serial.println("Send page");
  return;
}

void handleUpdateRelay() {
  Serial.printf("Handling HTTP %d: %s", server.method(), server.uri().c_str());
  Serial.println();
  if (server.hasArg("r")) {
    String r = server.arg("r");
    if (server.arg(0) == "1") {
      // Relay ON
      updateRelay(HIGH, 1);
      sendRedirect("/");
      blink(100, 100, 2);
    } else {
      // Relay OFF
      updateRelay(LOW, 1);
      sendRedirect("/");
      blink(100, 100, 1);
    }
  } else {
    server.send(400);
    Serial.println("Wrong arguments!");
  }
}

void handleAlexaApiOrNotFound() {
  //if you don't know the URI, ask espalexa whether it is an Alexa control request
  if (!espalexa.handleAlexaApiCall(server.uri(), server.arg(0))) {
    Serial.printf("Handling HTTP %d: %s", server.method(), server.uri().c_str());
    Serial.println();
    server.sendHeader("Connection", "close");
    server.send(404);
    Serial.println("Page not found!");
  }
}

void handleMQTTMessage(char *payload) {
  Serial.print("Handling MQTT message: ");
  Serial.println(payload);

  if (strcmp(payload, "ON") == 0) {
    updateRelay(HIGH, 0);
  } else if (strcmp(payload, "OFF") == 0) {
    updateRelay(LOW, 0);
  } else if (strcmp(payload, "TGL") == 0) {
    updateRelay(HIGH - value, 1);
  } else {
    return;
  }
}

void handleAlexaUpdateRelay(uint8_t brightness) {
  if (brightness) {
    updateRelay(HIGH, 1);
    Serial.print("Device Luz changed to ON, brightness ");
    Serial.println(brightness);
  }
  else  {
    updateRelay(LOW, 1);
    Serial.println("Device Luz changed to OFF");
  }
}

void setup() {
  Serial.begin(115200); // must be same baudrate with the Serial Monitor
  pinMode(RELAY, OUTPUT);
  pinMode(LED, OUTPUT);

  Serial.println();
  Serial.println();

  // Connect to WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
    blink(100, 100, 1);
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Check the fingerprint of io.adafruit.com's SSL cert
  secureClient.setFingerprint(fingerprint);

  // Subscribe to the command topic
  mqtt.subscribe(&ledCommandSub);

  MQTT_connect();
  updateRelay(LOW, 1);

  // Configure and start the server
  server.on("/", HTTP_GET, handleShowPage);
  server.on("/", HTTP_POST, handleUpdateRelay);
  server.onNotFound(handleAlexaApiOrNotFound);
  // server.begin(); //omit this since it will be done by espalexa.begin(&server)

  // Configure Alexa support
  device = new EspalexaDevice("Luz", handleAlexaUpdateRelay); //you can also create the Device objects yourself like here
  espalexa.addDevice(device); //and then add them
  espalexa.begin(&server);

  // Print the IP address
  Serial.println("Server started");
  Serial.print("Use this URL to connect: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
}

void loop() {
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();

  // Handle http requests
  //server.handleClient() //you can omit this line from your code since it will be called in espalexa.loop()
  espalexa.loop();

  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(AIO_LOOP_DELAY))) {
    // Check if it is the led command feed
    if (subscription == &ledCommandSub) {
      handleMQTTMessage((char *) ledCommandSub.lastread);
    }
  }

  // https://learn.adafruit.com/mqtt-adafruit-io-and-you/intro-to-adafruit-mqtt#pinging-the-server-2712941-39
  if (millis() - lastMqttActivity > 150000L) {
    // ping the server to keep the mqtt connection alive
    if (! mqtt.ping()) {
      mqtt.disconnect();
    }
    lastMqttActivity = millis();
    Serial.println("MQTT ping");
  }

  // Blink led as a health check
  digitalWrite(LED, millis() >> 11 & 1);
}
