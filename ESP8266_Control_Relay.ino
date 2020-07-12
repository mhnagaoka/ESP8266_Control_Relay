#include <ESP8266WiFi.h>
#include "my_network.h"

#define RELAY 0 // relay connected to  GPIO0
#define LED 2 // relay connected to  GPIO2

const char* ssid = MY_SSID; // fill in here your router or wifi SSID
const char* password = MY_PASSWORD; // fill in here your router or wifi password
WiFiServer server(80);
int value = -1;

int updateRelay(int newValue) {
  if (value != newValue) {
    value = newValue;
    digitalWrite(RELAY, newValue);
    digitalWrite(LED, newValue);
    Serial.printf("RELAY=%d\n", newValue);
  }
  return newValue;
}

void sendPage(WiFiClient *client) {
  // Return the response
  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: text/html");
  client->println(""); //  this is a must
  client->println("<!DOCTYPE HTML>");
  client->print("<html>");
  client->print("<head><title>ESP8266 RELAY Control</title></head><body>");

  if (value == HIGH)
  {
    client->print("<p>Relay is now: ON</p>");
    client->print("</p><p>Turn <a href=\"/?OFF\">OFF</a> RELAY</p>");
  }
  else
  {
    client->print("<p>Relay is now: OFF</p>");
    client->print("</p><p>Turn <a href=\"/?ON\">ON</a> RELAY</p>");
  }
  client->println("</body></html>");
}

void sendRedirect(WiFiClient *client, char *location) {
  client->println("HTTP/1.1 302");
  client->print("Location: ");
  client->println(location);
  client->println(""); //  this is a must
}

void setup()
{
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
}

void loop() {
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    digitalWrite(LED, millis() >> 11 & 1);
    return;
  }

  // Wait until the client sends some data
  Serial.println("New client");
  while (!client.available()) {
    delay(1);
  }

  // Read the first line of the request
  String request = client.readStringUntil('\r');
  Serial.println(request);
  client.flush();

  // Match the request
  if (request.indexOf("?OFF") != -1)
  {
    updateRelay(LOW);
    Serial.println("RELAY=OFF");
    sendRedirect(&client, "/");
  }
  else if (request.indexOf("?ON") != -1)
  {
    updateRelay(HIGH);
    Serial.println("RELAY=ON");
    sendRedirect(&client, "/");
  } else {
    sendPage(&client);
  }

  delay(1);
  Serial.println("Client disonnected");
  Serial.println("");
  blink(100, 100, 2);
}

void blink(int h, int l, int c) {
  for (int i = 0; i < c; i++) {
    digitalWrite(LED, LOW); // Acende o Led (ativo baixo)
    delay(h);
    digitalWrite(LED, HIGH); // Apaga o Led
    if (i < c - 1) {
      delay(l);
    }
  }
}
