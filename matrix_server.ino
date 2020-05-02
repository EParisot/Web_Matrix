#include <WiFi.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <LEDMatrix.h>
#include <LEDText.h>
#include <FontMatrise.h>

const int dns_port = 53;
const int http_port = 80;
const int ws_port = 1234;

#define LINES 8
#define COLUMNS 32
#define NUM_LEDS LINES * COLUMNS

#define MAX_LEN 65535
StaticJsonDocument<MAX_LEN> doc;

String html_content;
String css_content;
void read_css() {
  //open files from SPIFFS
  if(!SPIFFS.begin(true)) {
    Serial.println("FileSystem Error...");
    return;
  }
  File file = SPIFFS.open("/style.css");
  while(file.available()){
     css_content += (char)file.read();
  }
  file.close();
}

void edit_IP(IPAddress local_IP) {
  //open files from SPIFFS
  if(!SPIFFS.begin(true)) {
    Serial.println("FileSystem Error...");
    return;
  }
  File html_file = SPIFFS.open("/index.html", "r+");
  while(html_file.available()){
     html_content += (char)html_file.read();
  }
  // go to file start
  html_file.seek(0);
  // Edit IP
  String url = local_IP.toString();
  String port = "1234";
  url += ":" + port;
  String to_replace = "IP:PORT";
  html_content.replace(to_replace, url);
  // Rewrite file
  //html_file.print(html_content);
  html_file.close();
}

WebServer server(http_port);
WebSocketsServer webSocket = WebSocketsServer(ws_port);
const char* ssid = "XXXXXXXXXX";
const char* password =  "XXXXXXXXXX";
void init_wifi() {
  bool home_wifi = false;
  // Try to connect to known Wifi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  for (int i = 0; i < 3; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      home_wifi = true;
      break;
    }
    Serial.print(".");
    delay(3000);
  }
  if (home_wifi == false) {
    //Init WiFi as Station, start SmartConfig
    WiFi.mode(WIFI_AP_STA);
    WiFi.beginSmartConfig();
    //Wait for SmartConfig packet from mobile
    Serial.println("Waiting for SmartConfig.");
    while (!WiFi.smartConfigDone()) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("SmartConfig received.");
    //Wait for WiFi to connect to AP
    Serial.println("Waiting for WiFi");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
  }
  IPAddress local_IP = WiFi.localIP();
  Serial.println("WiFi Connected.");
  Serial.print("IP Address: ");
  Serial.println(local_IP);
  // Edit IP in html File
  edit_IP(local_IP);
  // On HTTP request for root, provide index.html file
  server.on("/", onIndexRequest);
  // On HTTP request for style sheet, provide style.css
  server.on("/style.css", onCSSRequest);
  // On HTTP POST JSON request
  server.on("/draw", onPostData);
  // Start web server
  server.begin();
  // Start WebSocket server and assign callback
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
}

// Callback: send homepage
void onIndexRequest() {
  server.send(200, "text/html", html_content);
}
 
// Callback: send style sheet
void onCSSRequest() {
  server.send(200, "text/css", css_content);
}

// Callback handle POST
void onPostData() {
  if (server.hasArg("plain")== false){ //Check if body received
    server.send(200, "text/plain", "Body not received");
    return;
  }
  String body = server.arg("plain");
  char *json;
  if ((json = (char*)malloc(body.length() + 1)) == NULL) {
    Serial.println("Malloc() failed");
    return;
  }
  int i = 0;
  for (i = 0; i < body.length(); ++i) {
    json[i] = body[i];
  }
  json[i] = '\0';
  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, json);
  // Test if parsing succeeds.
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }
  Serial.println("Request Recieved");
  JsonArray jsonArr = doc.as<JsonArray>();
  apply_led(jsonArr);
  free(json);
  server.send(200, "text/plain", "Done");
  Serial.println("Request Done");
}

// Callback: receiving any WebSocket message
void onWebSocketEvent(uint8_t client_num,
                      WStype_t type,
                      uint8_t * payload,
                      size_t length) {
 
  // Figure out the type of WebSocket event
  switch(type) {
 
    // Client has disconnected
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", client_num);
      break;
 
    // New client has connected
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(client_num);
        Serial.printf("[%u] Connection from ", client_num);
        Serial.println(ip.toString());
      }
      break;
 
    // Handle text messages from client
    case WStype_TEXT:
      {
        // Print out raw message
        Serial.printf("[%u] Received text: %s\n", client_num, payload);
        // Deserialize the JSON document
        DeserializationError JSONerror = deserializeJson(doc, (char *)payload);
        // Test if parsing succeeds.
        if (JSONerror) {
          webSocket.sendTXT(client_num, "deserializeJson() failed");
          Serial.print("deserializeJson() failed: ");
          Serial.println(JSONerror.c_str());
          break;
        }
        JsonArray jsonArr = doc.as<JsonArray>();
        apply_led(jsonArr);
        webSocket.sendTXT(client_num, "Request Done");
        Serial.println("Request Done");
      }
      break;
 
    // For everything else: do nothing
    case WStype_BIN:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
    default:
      break;
  }
}

// Minus LINES to revert back the text
cLEDMatrix<COLUMNS, -LINES, HORIZONTAL_ZIGZAG_MATRIX> leds;
cLEDText ScrollingMsg;
//CRGB leds[NUM_LEDS];
void init_leds() {
  FastLED.addLeds<NEOPIXEL, 33>(leds[0], NUM_LEDS);
  FastLED.clear();
  FastLED.setBrightness(5);
  FastLED.show();
  ScrollingMsg.SetFont(MatriseFontData);
  ScrollingMsg.Init(&leds, leds.Width(), ScrollingMsg.FontHeight() + 1, 0, 0);
  Serial.println("FastLed ready");  
}

char *text = NULL;
void apply_led(JsonArray jsonArr) {
  int i = 0;
  int l_s = -1;
  int l_i = -1;
  bool step_by_step = false;
  for (int j=0; j < jsonArr.size(); ++j) {
    if (jsonArr[j].containsKey("delay") || jsonArr[j].containsKey("loop")) {
      step_by_step = true;
      break;  
    }
  }
  while(i < jsonArr.size()) {
    if (jsonArr[i].containsKey("loop")) {
      if (l_i == -1)
      {
        l_s = jsonArr[i]["loop"][0].as<int>() - 1;
        l_i = jsonArr[i]["loop"][1].as<int>() - 1;
      }
      if (l_i > 0) {
        l_i -= 1;
        i = l_s;
      }
    }
    else if (jsonArr[i].containsKey("clear")) {
      FastLED.clear();
      free(text);
      text = NULL;
    }
    else if (jsonArr[i].containsKey("brightness")) {
      FastLED.setBrightness(jsonArr[i]["brightness"].as<int>());
    }
    else if (jsonArr[i].containsKey("delay")) {
      delay(jsonArr[i]["delay"].as<int>());
    }
    else if (jsonArr[i].containsKey("pos") && jsonArr[i].containsKey("color")){
      // LINES Minus pos[y] to restore flip
      leds(jsonArr[i]["pos"][0].as<int>(), LINES-1 - jsonArr[i]["pos"][1].as<int>()) = CRGB(jsonArr[i]["color"][0].as<int>(), jsonArr[i]["color"][1].as<int>(), jsonArr[i]["color"][2].as<int>());
    }
    else if (jsonArr[i].containsKey("fill")) {
      fill_solid(leds[0], NUM_LEDS, CRGB(jsonArr[i]["fill"][0].as<int>(),jsonArr[i]["fill"][1].as<int>(),jsonArr[i]["fill"][2].as<int>()));
    }
    else if (jsonArr[i].containsKey("text") && jsonArr[i].containsKey("color")){
      if ((text = (char *)malloc(strlen(jsonArr[i]["text"]) + 1)) == NULL) {
        Serial.println("Malloc() failed");
        return;  
      }
      strcpy(text, jsonArr[i]["text"]);
      ScrollingMsg.SetText((unsigned char *)text, strlen(text));
      ScrollingMsg.SetTextColrOptions(COLR_RGB | COLR_SINGLE, jsonArr[i]["color"][0].as<int>(),jsonArr[i]["color"][1].as<int>(),jsonArr[i]["color"][2].as<int>());
      ScrollingMsg.UpdateText();
    }
    ++i;
    if (step_by_step) {
      FastLED.show();
      delay(10);
    }
  }
  if (step_by_step == false) {
    FastLED.show();
    delay(10);
  }
}

void setup() { 
  Serial.begin(115200);
  //open files from SPIFFS
  if(!SPIFFS.begin(true)) {
    Serial.println("FileSystem Error...");
    return;
  }
  init_wifi(); // and read index + replace proper IP:PORT
  read_css();
  init_leds();
}

void loop() {
  // Serve web Clients
  server.handleClient();
  // Look for and handle WebSocket data
  webSocket.loop();
  // if text exists and need to be scrolled, then do it
  if (text && strlen(text) * (ScrollingMsg.FontWidth() + 1) > COLUMNS) {
    if (ScrollingMsg.UpdateText() == -1)
      ScrollingMsg.SetText((unsigned char *)text, strlen(text));
    else
      FastLED.show();
      delay(100);
  }
}
