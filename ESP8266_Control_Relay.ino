#include <ESP8266WiFi.h>
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

WiFiServer server(80);

// WiFiFlientSecure for SSL/TLS support
WiFiClientSecure secureClient;
// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&secureClient, AIO_SERVER, AIO_SERVERPORT, MY_AIO_USERNAME, MY_AIO_KEY);
// io.adafruit.com SHA1 fingerprint
static const char *fingerprint PROGMEM = "59 3C 48 0A B1 8B 39 4E 0D 58 50 47 9A 13 55 60 CC A0 1D AF";
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Publish ledStatusPub = Adafruit_MQTT_Publish(&mqtt, MY_AIO_USERNAME "/f/ledStatus");
Adafruit_MQTT_Subscribe ledCommandSub = Adafruit_MQTT_Subscribe(&mqtt, MY_AIO_USERNAME "/f/ledCommand");

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

int updateRelay(int newValue) {
  if (value != newValue) {
    value = newValue;
    digitalWrite(RELAY, newValue);
    digitalWrite(LED, newValue);
    ledStatusPub.publish(newValue);
  }
  return newValue;
}

void sendPage(WiFiClient *client) {
  // Return the response
  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: text/html");
  client->println("Connection: close");
  client->println(""); //  this is a must
  client->println("<!DOCTYPE HTML>");
  client->print("<html>");
  client->print("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>ESP8266 RELAY Control</title></head><body>");

  if (value == HIGH)
  {
    client->print("<p>Relay is now: ON</p>");
    client->print("<form method=\"get\"><input type=\"hidden\" name=\"r\" value=\"0\"/><button>Turn off relay</button></form>");
  }
  else
  {
    client->print("<p>Relay is now: OFF</p>");
    client->print("<form method=\"get\"><input type=\"hidden\" name=\"r\" value=\"1\"/><button>Turn on relay</button></form>");
  }
  client->println("</body></html>");
}

void sendRedirect(WiFiClient *client, char *location) {
  client->println("HTTP/1.1 302");
  client->print("Location: ");
  client->println(location);
  client->println("Connection: close");
  client->println(""); //  this is a must
}

void sendNotFound(WiFiClient *client) {
  client->println("HTTP/1.1 404");
  client->println("Connection: close");
  client->println(""); //  this is a must
}

void handleMQTTMessage(char *payload) {

  Serial.print("received <- ");
  Serial.println(payload);

  if (strcmp(payload, "ON") == 0) {
    updateRelay(HIGH);
  } else if (strcmp(payload, "OFF") == 0) {
    updateRelay(LOW);
  } else if (strcmp(payload, "TGL") == 0) {
    updateRelay(HIGH - value);
  } else {
    return;
  }
}

void setup() {
  Serial.begin(115200); // must be same baudrate with the Serial Monitor
  pinMode(RELAY, OUTPUT);
  pinMode(LED, OUTPUT);

  Serial.println();
  Serial.println();

  updateRelay(LOW);

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

  // Start the server
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.print("Use this URL to connect: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");

  // Check the fingerprint of io.adafruit.com's SSL cert
  secureClient.setFingerprint(fingerprint);

  // Subscribe to the command topic
  mqtt.subscribe(&ledCommandSub);
}

// Heavily inspired by the loop from:
// https://arduino-esp8266.readthedocs.io/en/2.7.2/esp8266wifi/server-examples.html#put-it-together
void loop() {
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();

  WiFiClient client = server.available();
  // wait for a client (web browser) to connect
  if (client) {
    Serial.println("\n[Client connected]");
    String request = "";
    unsigned long start = millis();
    while (client.connected()) {
      // read line by line what the client (web browser) is requesting
      if (client.available())
      {
        String line = client.readStringUntil('\r');
        if (request.length() == 0) {
          request = line;
        }
        Serial.print(line);
        // wait for end of client's request, that is marked with an empty line
        if (line.length() == 1 && line[0] == '\n')
        {
          Serial.printf("Handling request: %s\n", request.c_str());
          // Match the request
          if (request.indexOf("?r=0") >= 0)
          {
            updateRelay(LOW);
            sendRedirect(&client, "/");
            Serial.println("RELAY=OFF");
            blink(100, 100, 1);
          }
          else if (request.indexOf("?r=1") >= 0)
          {
            updateRelay(HIGH);
            sendRedirect(&client, "/");
            Serial.println("RELAY=ON");
            blink(100, 100, 2);
          } else if (request.indexOf("GET / ") >= 0) {
            Serial.println("PAGE");
            sendPage(&client);
          } else {
            Serial.println("NOT FOUND");
            sendNotFound(&client);
          }
          break;
        }
      } else {
        // Dummy Chrome socket
        // https://github.com/esp8266/Arduino/issues/3735
        unsigned long elapsed = millis() - start;
        if (elapsed > 1000) {
          client.stop();
          break;
        }
      }
    }
    delay(1); // give the web browser time to receive the data

    Serial.println("[Client disonnected]");
  } else {
    Adafruit_MQTT_Subscribe *subscription;
    while ((subscription = mqtt.readSubscription(AIO_LOOP_DELAY))) {
      // Check if it is the led command feed
      if (subscription == &ledCommandSub) {
        handleMQTTMessage((char *) ledCommandSub.lastread);
      }
    }
  }
  digitalWrite(LED, millis() >> 11 & 1);
  if (!(millis() >> 10))
    //    // ping the server to keep the mqtt connection alive
    //    if (! mqtt.ping()) {
    //      mqtt.disconnect();
    //    }
    Serial.println("MQTT ping");
}
