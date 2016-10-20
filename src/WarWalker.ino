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

//declaring prototypes
void drawProgress(OLEDDisplay *display, int percentage, String label);
void updateData(OLEDDisplay *display);
void drawNetStatus(OLEDDisplay *display);


// Include the correct display library
// For a connection via I2C using Wire include
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`
// or #include "SH1106.h" alis for `#include "SH1106Wire.h"`
// For a connection via I2C using brzo_i2c (must be installed) include
// #include <brzo_i2c.h> // Only needed for Arduino 1.6.5 and earlier
// #include "SSD1306Brzo.h"
// #include "SH1106Brzo.h"
// For a connection via SPI include
// #include <SPI.h> // Only needed for Arduino 1.6.5 and earlier
// #include "SSD1306Spi.h"
// #include "SH1106SPi.h"


// Initialize the OLED display using SPI
// D5 -> CLK
// D7 -> MOSI (DOUT)
// D0 -> RES
// D2 -> DC
// D8 -> CS
// SSD1306Spi        display(D0, D2, D8);
// or
// SH1106Spi         display(D0, D2);

// Initialize the OLED display using brzo_i2c
// D3 -> SDA
// D5 -> SCL
// SSD1306Brzo display(0x3c, D3, D5);
// or
// SH1106Brzo  display(0x3c, D3, D5);

// Initialize the OLED display using Wire library
SSD1306  display(0x3c, D3, D4);
// SH1106 display(0x3c, D3, D5);

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();


  // Initialising the UI will init the display too.
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  updateData(&display);
  display.display();
}


void loop() {
  // clear the display
  display.clear();
  // draw the current demo method
  drawNetStatus(&display);

  display.display();
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
  drawProgress(display, 80, "Herding Cats...");
  delay(100);
}

void drawNetStatus(OLEDDisplay *display) {
  int x=0;
  int y=0;
  // Get scan
  std::vector<Network> netArray = scan();
  // Set defaults for text
  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);

  // Set header
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  String banner = "[=== (" + String(netArray.size()) + ") Networks ===]";
  display->drawString(64, 0, banner);

  // Draw the networks
  String lineItem = "";
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  for (int i = 0; i < 6; i++){
    String spaces;
    if (netArray[i].channel >= 10){
      spaces = "  ";
    } else {
      spaces = "    ";
    }
    lineItem = String(netArray[i].RSSI) + "  " + String(netArray[i].channel) + spaces + String(netArray[i].SSID);
    display->drawString(x, 10+y+(8*i), lineItem);
  }
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
