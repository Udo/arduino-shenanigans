/**
 * ESP8266 Web-Controlled GPIO and HTTP Client
 * 
 * This sketch implements a web server for managing GPIOs on an ESP8266 and handles
 * HTTP client requests for external notifications. Features include:
 * 
 * - Soft AP for configuration and captive portal.
 * - Web server with routes for GPIO states and Wi-Fi credentials.
 * - mDNS service for easy local network access.
 * - OTA updates for firmware management.
 * - EEPROM storage for Wi-Fi credentials and server URL persistence.
 * 
 * Commands:
 * - `/`: Root page with GPIO info and system status.
 * - `/notify?n=[URL]`: Set notification server URL.
 * - `/json`: JSON-formatted system data.
 * - `/reset`: Reboot the device.
 * 
 */
 
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <ESPping.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

// Wi-Fi Access Point credentials
const char *ssidAP = "ESP-SETUP";
const char *passwordAP = "ESP12345";

// System variables
String hostName; // Device hostname
int requestCounter = 0; // Total number of requests handled
double avgLoopTime = 0; // Average loop execution time

// DNS and Web server
const byte DNS_PORT = 53;
DNSServer dnsServer;
ESP8266WebServer server(80);

// HTTP Client variables
bool httpRequestInProgress = false; // HTTP request in progress flag
WiFiClient client;
String notifyServer = ""; // Notification server URL
String httpRequestURLWaiting = ""; // Pending HTTP request URL

// GPIO configuration and state
const int EEPROM_SIZE = 512; // Size of EEPROM for storage
int safePins[] = {0, 2, 4, 5, 12, 13, 14}; // Safe GPIO pins for monitoring
int numPins = sizeof(safePins) / sizeof(safePins[0]);
unsigned long switchTime[32]; // Last state change timestamps for GPIOs
int switchState[32]; // Current GPIO states
String eventLog[32];
int eventLogIndex = 0;

void writeStringToEEPROM(int address, String str) {
  for (int i = 0; i < str.length(); i++) {
    EEPROM.write(address + i, str[i]);
  }
  EEPROM.write(address + str.length(), '\0');
}

String readStringFromEEPROM(int address) {
  String str = "";
  char character;
  do {
    character = EEPROM.read(address++);
    if (character != '\0') {
      str += character;
    }
  } while (character != '\0');
  return str;
}

void saveNotifyServer() {
  int offset = 256;
  writeStringToEEPROM(offset, notifyServer);
  EEPROM.commit();
}

void loadNotifyServer() {
  int offset = 256;
  notifyServer = readStringFromEEPROM(offset);
  Serial.println("Loaded Notify Server: " + notifyServer);
}

void makeHTTPRequest(String URL) {
  if (httpRequestInProgress) {
    httpRequestURLWaiting = URL;
    return;
  }

  httpRequestURLWaiting = "";

  String serverPart = URL;
  String pathPart = "/";

  int protocolSeparator = URL.indexOf("://");
  if (protocolSeparator != -1) {
    serverPart = URL.substring(protocolSeparator + 3);
  }

  int pathSeparator = serverPart.indexOf("/");
  if (pathSeparator != -1) {
    pathPart = serverPart.substring(pathSeparator);
    serverPart = serverPart.substring(0, pathSeparator);
  }

  if (client.connect(serverPart.c_str(), 80)) {
    Serial.println("Connected to server");
    client.println("GET " + pathPart + " HTTP/1.1");
    client.println("Host: " + serverPart);
    client.println("Connection: close");
    client.println();
    httpRequestInProgress = true;
  } else {
    Serial.println("Failed to connect to server");
  }
}

void handleHTTPOut() {
  if (httpRequestInProgress) {
    while (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
    }
    if (!client.connected()) {
      Serial.println("Server disconnected");
      client.stop();
      httpRequestInProgress = false;
      if(httpRequestURLWaiting != "") {
        makeHTTPRequest(httpRequestURLWaiting);
      }
    }
  }
}

void updateGPIO() {
  bool updated = false;
  String states = "";
  for (int i = 0; i < numPins; i++) {
    int pin = safePins[i];
    int state = digitalRead(pin);
    if(switchState[i] != state) {
      eventLog[eventLogIndex] = "GPIO"+String(pin)+" "+String((state == 1 ? "OPEN" : "CLOSED"))+
        " AFTER t=" + String(0.001*(millis()-switchTime[i])) +
        " AT t=" + String(0.001*millis());
      eventLogIndex++;
      if(eventLogIndex >= 31) eventLogIndex = 0;
      switchState[i] = state;
      switchTime[i] = millis();
      states += "GPIO"+String(pin)+"="+String((state == 1 ? "OPEN" : "CLOSED"))+"&";
    }
  }
  if(updated) {
    if (!notifyServer.isEmpty()) {
      makeHTTPRequest(notifyServer+"?"+states);
    }
  }
}

void setupGPIO() {
  for (int i = 0; i < numPins; i++) {
    int pin = safePins[i];
    pinMode(pin, INPUT_PULLUP);  
    switchState[i] = -1;
    switchTime[i] = millis();
  }
}

String getGPIOStates() {
  String states = "";
  
  for (int i = 0; i < numPins; i++) {
    int pin = safePins[i];
    int state = digitalRead(pin);
    states += "GPIO" + String(pin) + ": " + (state == 1 ? "OPEN" : "CLOSED") + "\t";
  }
  
  return states;
}

String getGPIOStatesJSON() {
  String states = "";
  
  for (int i = 0; i < numPins; i++) {
    int pin = safePins[i];
    int state = digitalRead(pin);
    if(states != "") states += ",";
    states += "\"GPIO" + String(pin) + "\":\"" + (state == 1 ? "OPEN" : "CLOSED") + "\"";
  }
  
  return String("{")+states+String("}");
}

String getGPIOTimes() {
  String states = "";
  
  for (int i = 0; i < numPins; i++) {
    int pin = safePins[i];
    states += "GPIO" + String(pin) + ": " + 0.001*(millis()-switchTime[i]) + "\t";
  }
  
  return states;
}

String getGPIOTimesJSON() {
  String states = "";
  
  for (int i = 0; i < numPins; i++) {
    int pin = safePins[i];
    if(states != "") states += ",";
    states += "\"GPIO" + String(pin) + "\":" + 0.001*(millis()-switchTime[i]);
  }
  
  return String("{")+states+String("}");
}

int calculateSignalQuality(int32_t rssi) {
  if (rssi <= -100) {
    return 0; 
  } else if (rssi >= -50) {
    return 100; 
  } else {
    return 2 * (rssi + 100); 
  }
}

String getConnectionInfo() {
  String result = "";
  uint8_t* bssid = WiFi.BSSID();
  String macAddress = String(bssid[0], HEX) + ":" + String(bssid[1], HEX) + ":" +
                      String(bssid[2], HEX) + ":" + String(bssid[3], HEX) + ":" +
                      String(bssid[4], HEX) + ":" + String(bssid[5], HEX);
  macAddress.toUpperCase();
  int32_t rssi = WiFi.RSSI();
  int quality = calculateSignalQuality(rssi);
  result += "AP: ";
  result += macAddress;
  result += "\tMAC: ";
  result += String(WiFi.macAddress());
  result += "\tRSSI: ";
  result += String(rssi);
  result += "\tSignal: ";
  result += String(quality);
  result += "%";
  return result;
}

String getConnectionInfoJSON() {
  String result = "";
  uint8_t* bssid = WiFi.BSSID();
  String macAddress = String(bssid[0], HEX) + ":" + String(bssid[1], HEX) + ":" +
                      String(bssid[2], HEX) + ":" + String(bssid[3], HEX) + ":" +
                      String(bssid[4], HEX) + ":" + String(bssid[5], HEX);
  macAddress.toUpperCase();
  int32_t rssi = WiFi.RSSI();
  int quality = calculateSignalQuality(rssi);
  result += "\"AP\":\"";
  result += macAddress;
  result += "\",\"MAC\":\"";
  result += String(WiFi.macAddress());
  result += "\",\"RSSI\":\"";
  result += String(rssi);
  result += "\",\"Signal\":\"";
  result += String(quality);
  result += "%\"";
  return String("{")+result+String("}");
}

void handlePing() {
  static unsigned long lastPingTime = 0;
  if (millis() - lastPingTime > 1000) {
    lastPingTime = millis();
    if (Ping.ping(WiFi.gatewayIP())) {
      Serial.println("Ping "+String(Ping.averageTime())+" "+getConnectionInfo());
    } else {
      //
    }
  }
}

void loop() {
  unsigned long loopStartTime = millis();
  updateGPIO();
  dnsServer.processNextRequest();
  server.handleClient();
  ArduinoOTA.handle();
  avgLoopTime = avgLoopTime*0.9 + 0.1*(millis()-loopStartTime);
}

void startServer() {
  server.on("/", HTTP_GET, []() {
    requestCounter++;
    String gpioStates = getGPIOStates();
    String gpioTimes = getGPIOTimes();
    String conInfo = getConnectionInfo();
    server.send(200, "text/plain", String("NODE:\tHOST: ")+String(hostName)+
      "\tRC: "+String(requestCounter)+
      "\tLoad: "+String(avgLoopTime)+
      "\tUptime: "+String(millis()/1000)+"s"+
      "\tHeap: "+String(ESP.getFreeHeap())+
      "\nSWITCHES:\t"+gpioStates+"\n"+
      "SINCE:\t"+gpioTimes+"\n"+
      "CONNECTION:\t"+conInfo+"\n"+
      "COMMANDS: /, /reset, /json, /notify [URL]\n");
  });
  server.on("/notify", HTTP_GET, []() {
    if (server.hasArg("n")) {
      notifyServer = notifyServer = server.arg("n");
      saveNotifyServer();
      server.send(200, "text/plain", "Notify set: "+notifyServer+"\n");
    } else {
      server.send(200, "text/plain", "Notify: "+notifyServer+"\n");
    }
  });
  server.on("/history", HTTP_GET, []() {
    String history = "";
    
    int start = eventLogIndex;
    for (int i = 0; i < 31; i++) {
        int index = (start + i) % 31; // Calculate the correct index in the ring buffer
        if (!eventLog[index].isEmpty()) { // Only include non-empty entries
            history += "[" + String(index) + "] " + eventLog[index] + "\n";
        }
    }

    server.send(200, "text/plain", history);
  });
  server.on("/json", HTTP_GET, []() {
    requestCounter++;
    String gpioStates = getGPIOStatesJSON();
    String conInfo = getConnectionInfoJSON();
    server.send(200, "application/json", String("{\"NODE\":{\"HOST\":\"")+String(hostName)+"\",\"RC\":\""+String(requestCounter)+"\",\"Load\":\""+String(avgLoopTime)+"\",\"Uptime\":"+String(millis()/1000)+"},"+
      "\"SWITCHES\":"+gpioStates+"\"SINCE\":"+getGPIOTimesJSON()+",\"WIFI\":"+conInfo+"}\n");
  });
  server.on("/reset", HTTP_GET, []() {
    server.send(200, "text/plain", "RESET\n");
    delay(2000);
    ESP.restart();
  });
  server.begin();
  Serial.println("HTTP server started");
}

void startAP() {
  WiFi.softAP(ssidAP, passwordAP);
  Serial.print("AP started. IP address: ");
  Serial.println(WiFi.softAPIP());

  // Redirect all DNS queries to the captive portal IP
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, []() {
    String html = "<html><head><meta name=\"viewport\" content=\"width=400, initial-scale=1.0\"></head><body><h1>WiFi Configuration</h1>";
    html += "<form action=\"/save\" method=\"GET\">";
    html += "<div style=\"display:flex\"><label style=\"flex:1\">SSID:</label><input style=\"flex:3\" type=\"text\" name=\"ssid\"></div>";
    html += "<div style=\"display:flex\"><label style=\"flex:1\">Password</label><input style=\"flex:3\" type=\"text\" name=\"password\"></div>";
    html += "<div><input type=\"submit\" value=\"Save\"></div>";
    html += "</form></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/save", HTTP_GET, []() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");

    if (ssid.length() > 0 && password.length() > 0) {
      writeStringToEEPROM(0, ssid);
      writeStringToEEPROM(ssid.length() + 1, password);
      EEPROM.commit();

      server.send(200, "text/html", "<html><body><h1>Credentials Saved!</h1><p>Rebooting...</p></body></html>");
      delay(2000);
      ESP.restart();
    } else {
      server.send(200, "text/html", "<html><body><h1>Invalid Input</h1><p>Please try again.</p></body></html>");
    }
  });

  server.begin();
  Serial.println("Captive portal started");
}

void startOTA() {
  ArduinoOTA.setHostname(hostName.c_str()); // Set OTA hostname
  ArduinoOTA.setPassword("1234");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nUpdate complete");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress * 100) / total);
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });

  ArduinoOTA.begin();
  Serial.println("OTA initialized");
}

void setup() {
  setupGPIO();
  hostName = String("ESP-")+String(ESP.getChipId());
  Serial.begin(9600);
  EEPROM.begin(EEPROM_SIZE);
  
  String storedSSID = readStringFromEEPROM(0);
  String storedPassword = readStringFromEEPROM(storedSSID.length() + 1);
  loadNotifyServer(); 

  if (storedSSID.length() > 0 && storedPassword.length() > 0) {
    WiFi.hostname(hostName);
    WiFi.begin(storedSSID.c_str(), storedPassword.c_str());
    Serial.print("Connecting to Wi-Fi");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected to Wi-Fi!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      Serial.print("Host Name: ");
      Serial.println(WiFi.hostname());
      Serial.println(getConnectionInfo());

      if (MDNS.begin(hostName)) {
        Serial.println("mDNS responder started");
        Serial.print("Device accessible at: http://");
        Serial.print(hostName);
        Serial.println(".local");
      } else {
        Serial.println("Error starting mDNS responder");
      }
      startServer();
      startOTA(); 
    } else {
      Serial.println("\nFailed to connect to Wi-Fi, starting AP...");
      startAP();
    }
  } else {
    Serial.println("No stored Wi-Fi credentials found, starting AP...");
    startAP();
  }
}


