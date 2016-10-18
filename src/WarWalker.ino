/**The MIT License (MIT)

Copyright (c) 2016 by Daniel Eichhorn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at http://blog.squix.ch
*/

#include <Arduino.h>
#include <algorithm>
#include <vector>
#include <stdlib.h>
#include <string>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <JsonListener.h>
#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include "TimeClient.h"

//the structure
class Network {
public:
  String  SSID;
  int   RSSI;
  int   channel;
  bool encrypt;
  Network(){};
};

//the initialize function
Network constructNet(String SSID, int RSSI, int channel, bool encrypt){
  Network foo;
  foo.SSID = SSID;
  foo.RSSI = RSSI;
  foo.channel = channel;
  foo.encrypt = encrypt;
  return foo;
}

bool networkSorter(Network const& lhs, Network const& rhs) {
  return lhs.RSSI >= rhs.RSSI; // since values are negative this is reversed?
}

void printNetArray(std::vector<Network> netArray){
  int n = netArray.size();
  for (int i=0; i < n; i++){
    // String rssi = String(&netArray[i].RSSI);
    String ssid = String(netArray[i].SSID);
    String rssi = String(netArray[i].RSSI);
    String channel = String(netArray[i].channel);
    String encrypt = (netArray[i].encrypt == ENC_TYPE_NONE)?" ":"*";
    String line = " #" + String(i) + "(" + rssi + ") " + ssid + " - " + channel + " - " + encrypt;
    Serial.println(line);
  }
}

Network constructNet(String SSID, int RSSI, int channel, bool encrypt);
bool networkSorter(Network const& lhs, Network const& rhs);
std::vector<Network> scan();
void setupScan();


/***************************
 * Begin Settings
 **************************/
// Please read http://blog.squix.org/weatherstation-getting-code-adapting-it
// for setup instructions

// WIFI
const char* WIFI_SSID = "ConciergeEntertainment";
const char* WIFI_PWD = "djrebase";


// Setup
const int UPDATE_INTERVAL_SECS = 10 * 60; // Update every 10 minutes

// Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;
const int SDA_PIN = D3;
const int SDC_PIN = D4;

// TimeClient settings
const float UTC_OFFSET = -7;

// Initialize the oled display for address 0x3c
// sda-pin=14 and sdc-pin=12
SSD1306Wire     display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
OLEDDisplayUi   ui( &display );

/***************************
 * End Settings
 **************************/

// flag changed in the ticker function every 10 minutes
bool readyForWeatherUpdate = false;

String lastUpdate = "--";

Ticker ticker;

//declaring prototypes
void drawProgress(OLEDDisplay *display, int percentage, String label);
void updateData(OLEDDisplay *display);
void drawNetStatus(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void setReadyForWeatherUpdate();


// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback frames[] = { drawNetStatus };
int numberOfFrames = 1;

OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // initialize dispaly
  display.init();
  display.clear();
  display.display();


  // display.invertDisplay();
  // display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);

  // Setup to do WiFi Scan
  setupScan();

  ui.setTargetFPS(30);

  ui.setActiveSymbol(activeSymbole);
  ui.setInactiveSymbol(inactiveSymbole);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  ui.setFrames(frames, numberOfFrames);

  ui.setOverlays(overlays, numberOfOverlays);

  // Inital UI takes care of initalising the display too.
  ui.init();

  Serial.println("");

  display.flipScreenVertically();

  updateData(&display);

  ticker.attach(UPDATE_INTERVAL_SECS, setReadyForWeatherUpdate);
}

void loop() {

  if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) {
    updateData(&display);
  }

  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    delay(remainingTimeBudget);
  }

}

void drawProgress(OLEDDisplay *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}

void updateData(OLEDDisplay *display) {
  drawProgress(display, 30, "Scanning Network...");
  delay(500);
  readyForWeatherUpdate = false;
  drawProgress(display, 80, "Herding Cats...");
  delay(100);
}

void drawNetStatus(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  std::vector<Network> netArray = scan();
  String lineItem = "";
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  for (int i = 0; i < 5; i++){
    lineItem = "(" + String(netArray[i].RSSI) + ") " + String(netArray[i].SSID);
    display->drawString(x, y+(10*i), lineItem);
  }
}

void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String left = "left";
  display->drawString(0, 54, left);

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String right = "right";
  display->drawString(128, 54, right);

  display->drawHorizontalLine(0, 52, 128);
}

void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}



void setupScan(){
  Serial.begin(115200);

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("Setup done");
}

std::vector<Network> populateNetArray(int n){
  std::vector<Network> netArray(n);
  for (int i=0; i < n; i++){
    netArray[i] = constructNet(WiFi.SSID(i), WiFi.RSSI(i), WiFi.channel(i), (WiFi.encryptionType(i) == ENC_TYPE_NONE));
  }
  return netArray;
}

std::vector<Network> scan() {
  Serial.println("Starting Scan...");
  // WiFi.scanNetworks will return the number of networks found
  bool async = false;
  bool show_hidden = false;
  int n = WiFi.scanNetworks(async, show_hidden);

  serialDebugScan(n);

  std::vector<Network> netArray = populateNetArray(n);
  std::sort(netArray.begin(), netArray.end(), networkSorter);
  return(netArray);
}

void serialDebugScan(int n){
  Serial.println("scan done");
  if (n == 0){
    Serial.println("no networks found");
  } else
  {
    Serial.print(n);
    Serial.println(" networks found");
  }
  Serial.println("");
}
