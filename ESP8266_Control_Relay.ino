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

void loop()
{
  WiFiClient client = server.available();
  // wait for a client (web browser) to connect
  if (client)
  {
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
            Serial.println("RELAY=OFF");
            sendRedirect(&client, "/");
          }
          else if (request.indexOf("?r=1") >= 0)
          {
            updateRelay(HIGH);
            Serial.println("RELAY=ON");
            sendRedirect(&client, "/");
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

    // close the connection:
    //    client.stop();
    Serial.println("[Client disonnected]");
  }
  digitalWrite(LED, millis() >> 11 & 1);
}

//void loop() {
//  // Check if a client has connected
//  WiFiClient client = server.available();
//  if (!client) {
//    digitalWrite(LED, millis() >> 11 & 1);
//    return;
//  }
//
//  // Wait until the client sends some data
//  Serial.println("New client");
////  while (!client.available()) {
////    delay(1);
////  }
//
//  // Read the first line of the request
//  String request = client.readStringUntil('\r');
//  Serial.println("Request: " + request);
//
//  // https://github.com/esp8266/Arduino/blob/92373a98370618dea09718010b30d311a97f3f25/libraries/ESP8266WiFi/examples/WiFiManualWebServer/WiFiManualWebServer.ino#L86
//  // read/ignore the rest of the request
//  // do not client.flush(): it is for output only, see below
//  while (client.available()) {
//    // byte by byte is not very efficient
//    client.read();
//  }
//
//  // Match the request
//  if (request.indexOf("?r=0") != -1)
//  {
//    updateRelay(LOW);
//    Serial.println("RELAY=OFF");
//    sendRedirect(&client, "/");
//  }
//  else if (request.indexOf("?r=1") != -1)
//  {
//    updateRelay(HIGH);
//    Serial.println("RELAY=ON");
//    sendRedirect(&client, "/");
//  } else if (request.indexOf("GET / ") != -1) {
//    Serial.println("PAGE");
//    sendPage(&client);
//  } else if (request.length() == 0) {
//    // Sometimes Chrome opens a connection but doesn't send any request
//    // So we dump the connection.
//    // https://github.com/esp8266/Arduino/issues/3735
//    Serial.println("DUMMY");
//    client.stop();
//    return;
//  } else {
//    Serial.println("NOT FOUND");
//    sendNotFound(&client);
//  }
//
//  delay(1);
//  Serial.println("Client disonnected");
//  Serial.println("");
//  blink(100, 100, 2);
//}

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
