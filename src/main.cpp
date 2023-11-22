/**
 * Original code written by pollux labs, 2020
 * URL: https://gist.github.com/polluxlabs/1ba7824175c5e011565bd61af2fd1c6b
 */
// #define USE_OLED
#define SCREEN_WIDTH 128   // OLED display width, in pixels
#define SCREEN_HEIGHT 32   // OLED display height, in pixels
#define OLED_RESET -1      // Reset pin # (-1 if sharing Arduino reset pin or no reset pin)
#define OLED_I2C_ADDR 0x3C // Displays I2C address
#define OLED_ROTATION 0    // 0 = 0, 1 = 90, 2 = 180 or 3 = 270 (degree rotation)

#define NEOPIXEL_PIN D3
#ifndef NUM_OF_NEOPIXELS
#define NUM_OF_NEOPIXELS 4
#endif
#define STATUS_LED LED_BUILTIN
#define STATUS_LED_INVERTED true

#define OTA_HOSTNAME "ISS-Notifier"

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h> // ESP8266 WiFi support
#include <ArduinoJson.h> // JSON processor
#include <Adafruit_NeoPixel.h>
#ifdef USE_OLED
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif
#include <TimeLib.h>
#include <Timezone.h>
#include <WiFiManager.h>       // WiFi Configuration Portal
#include <ESP8266mDNS.h>       // mDNS (for OTA support)
#include <WiFiUdp.h>           // UDP (for OTA support)
#include <ArduinoOTA.h>        // OTA update support
#include <DoubleResetDetect.h> // Detect double Reset
#include <WiFiUdp.h>           // For NTP
#include <Ticker.h>

#include "secrets.h"

#ifdef USE_OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#endif

long riseTime = 0;                 // Time the ISS will rise for current position
long flyoverDuration = 0;          // Duration of ISS pass for current position
long timeUntilFlyover = 0;         // How long it will be until the next flyover
long timeUntilFlyoverComplete = 0; // How long it will be until the current flyover is complete
int heartbeatCounter = 0;          // Interval counter for periodic heartbeat flash

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;
unsigned int localPort = 2390; // local port to listen for UDP packets

const int NTP_PACKET_SIZE = 48;     // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

enum machineStates
{
  START,
  GET_TIME,
  GET_NEXT_PASS,
  WAIT_FOR_PASS,
  PASS_START,
  PASSING,
  PASS_COMPLETE
};

enum machineStates currentState = START;

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_OF_NEOPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
Ticker ticker;

#define DRD_TIMEOUT 2.0  // number of seconds between resets that counts as a double reset
#define DRD_ADDRESS 0x00 // address to the block in the RTC user memory
DoubleResetDetect drd(DRD_TIMEOUT, DRD_ADDRESS);

void success();
void fail();
bool getNextPass();
void wifiSetup();
void configModeCallback(WiFiManager *myWiFiManager);
void otaSetup();
void tick();
void heartbeat();
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);

void setup()
{
  Serial.begin(115200);
  pinMode(STATUS_LED, OUTPUT); //LED Pin
  digitalWrite(STATUS_LED, (STATUS_LED_INVERTED == true) ? HIGH : LOW);

  pixels.begin();
  pixels.setBrightness(100);
  pixels.show();

  Serial.println("");
  Serial.println(F("ISS Flyover Notifier"));
  Serial.println("");

#ifdef USE_OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR))
  {
    Serial.println(F("SSD1306 initialisation failed"));
  }
  display.setRotation(OLED_ROTATION);
  display.clearDisplay();
#endif

  // Wifi
  wifiSetup();

  // OTA init
  otaSetup();

  udp.begin(localPort);
  Serial.print(F("UDP: Running on local port "));
  Serial.println(udp.localPort());

  setSyncProvider(getNtpTime);
  setSyncInterval(28800); //every eight hours

  // entering normal runtime, clear WiFi ticker
  digitalWrite(STATUS_LED, (STATUS_LED_INVERTED == true) ? HIGH : LOW);
  ticker.attach(0.2, heartbeat);
  // If you don't want the heartbeat, do this instead of the above line
  // ticker.detach();
}

void loop()
{
  // Allow MDNS discovery
  MDNS.update();

  // Poll / handle OTA programming
  ArduinoOTA.handle();

  unsigned long loopStart = millis();
  switch (currentState)
  {
  case START:
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      currentState = GET_TIME;
    }
    else
    {
      wifiSetup();
    }

    break;
  }
  case GET_TIME:
  {
    Serial.println(F("Getting the current time..."));

#ifdef USE_OLED
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(30, 0);
    // Display static text
    display.println("Get UTC");
    display.setCursor(32, 16);
    display.println("Time");
    display.display();
#endif

    if (timeStatus() == timeNotSet)
    {
      getNtpTime();
    }

    if (timeStatus() == timeNotSet)
    {
      fail();
      delay(1000);
    }
    else
    {
      Serial.printf("Current time        : %02d:%02d:%02d %02d-%02d-%04d (UTC)\n",
                    hour(), minute(), second(),
                    day(), month(), year());

      TimeChangeRule *tcr;
      time_t t = myTz.toLocal(now(), &tcr);

      Serial.printf("                    : %02d:%02d:%02d %02d-%02d-%04d (%s)\n",
                    hour(t), minute(t), second(t), day(t), month(t), year(t), tcr->abbrev);

      currentState = GET_NEXT_PASS;
    }
    break;
  }
  case GET_NEXT_PASS:
  {
    Serial.println(F("Looking up next ISS flyover time..."));

#ifdef USE_OLED
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(30, 0);
    display.println("Get Next");
    display.setCursor(32, 16);
    display.println("ISS Pass");
    display.display();
#endif

    if (!getNextPass())
    {
      fail();
      delay(1000);
    }
    else
    {
      success();

      //compute time until rise
      timeUntilFlyover = riseTime - now();

      uint32_t t = timeUntilFlyover;
      int s = t % 60;
      t = (t - s) / 60;
      int m = t % 60;
      t = (t - m) / 60;
      int h = t;
      Serial.printf("That's %02i hours, %02i minutes and %02i seconds from now\n", h, m, s);

      t = flyoverDuration;
      s = t % 60;
      t = (t - s) / 60;
      m = t % 60;
      Serial.printf("Estimated pass duration will be %02i minutes and %02i second(s)\n", m, s);

      timeUntilFlyoverComplete = flyoverDuration;
      currentState = WAIT_FOR_PASS;
    }
    break;
  }
  case WAIT_FOR_PASS:
  {
    if (timeUntilFlyover > 0)
    { // while the ISS isn't overhead

      uint32_t t = timeUntilFlyover;
      int s = t % 60;
      t = (t - s) / 60;
      int m = t % 60;
      t = (t - m) / 60;
      int h = t;
      Serial.printf("Next ISS flyover in %02i:%02i:%02i\n", h, m, s);

#ifdef USE_OLED
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(30, 0);
      display.print("Next pass in:");
      display.setTextSize(2);
      display.setCursor(32, 16);
      display.printf("%02i:%02i:%02i", h, m, s);
      display.display();
#endif

      timeUntilFlyover--;

      unsigned long loopOverhead = millis() - loopStart;
      delay(1000 - loopOverhead);
    }
    else
    {
      currentState = PASS_START;
    }
    break;
  }
  case PASS_START:
  {
    //when ISS rises  above the horizon
    Serial.println(F("ISS overhead!"));

    uint32_t t = timeUntilFlyoverComplete;
    int s = t % 60;
    t = (t - s) / 60;
    int m = t % 60;
    Serial.printf("Estimated pass duration %02i minutes and %02i second(s)\n", m, s);

    currentState = PASSING;
    break;
  }
  case PASSING:
  {
    if (timeUntilFlyoverComplete > 0)
    {
      //map remaining flyover time on a color gradient
      int colorRed = map(timeUntilFlyoverComplete, 0, flyoverDuration, 200, 0);
      int colorBlue = map(timeUntilFlyoverComplete, 0, flyoverDuration, 0, 200);

      //show the current color on all LEDs
      pixels.fill(pixels.Color(colorRed, 0, colorBlue));
      pixels.show();

      timeUntilFlyoverComplete--;

      uint32_t t = timeUntilFlyoverComplete;
      int s = t % 60;
      t = (t - s) / 60;
      int m = t % 60;
      Serial.printf("Pass time remaining: %02i minutes and %02i second(s)\n", m, s);

#ifdef USE_OLED
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(30, 0);
      display.print("Overhead!");
      display.setTextSize(2);
      display.setCursor(32, 16);
      display.printf("%02i:%02i", m, s);
      display.display();
#endif

      unsigned long loopOverhead = millis() - loopStart;
      delay(1000 - loopOverhead);
    }
    else
    {
      currentState = PASS_COMPLETE;
    }
    break;
  }
  case PASS_COMPLETE:
  {
    //shut down the NeoPixel until next ISS flyover
    pixels.clear();
    pixels.show();

    currentState = START;
    break;
  }
  }
}

void success()
{
  pixels.fill(pixels.Color(0, 255, 0));
  pixels.show();

  delay(500);

  pixels.clear();
  pixels.show();
}

void fail()
{
  pixels.fill(pixels.Color(255, 0, 0));
  pixels.show();

  delay(500);

  pixels.clear();
  pixels.show();
}

bool getNextPass()
{
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure(); //the magic line, use with caution

  const char* apiUrl = "https://api.n2yo.com/rest/v1/satellite/visualpasses/";

  String request = "25544/";  // ISS NORAD ID
  request += String(latitude) + "/";
  request += String(longitude) + "/";
  request += String(altitude) + "/";
  request += String(daysToLookup) + "/";
  request += String(minVisibility) + "/";
  request += "&apiKey=" + String(apiKey);

  String fullUrl = apiUrl + request;

  // Serial.print("fullUrl == ");
  // Serial.println(fullUrl);

  http.begin(client, fullUrl); //URL for API call

  int httpCode = http.GET();
  if (httpCode > 0)
  {
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
    {
      String payload = http.getString(); //save response

      const size_t capacity = JSON_OBJECT_SIZE(10) + 1024;
      DynamicJsonDocument doc(capacity);
      DeserializationError error = deserializeJson(doc, payload);
      http.end();

      if (error)
      {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return false;
      }

      int passescount = doc["info"]["passescount"];

      for (int i = 0; i < passescount; i++) {
        riseTime = doc["passes"][i]["startUTC"];
        // Serial.print("Start UTC: ");
        // Serial.println(riseTime);
        // const int endUTC = doc["passes"][i]["endUTC"];
        // Serial.print("End UTC: ");
        // Serial.println(endUTC);
        flyoverDuration = doc["passes"][i]["duration"];
        // Serial.print("Duration: ");
        // Serial.println(flyoverDuration);

        if (riseTime > now()) break;
      }

      if (riseTime < now()) {
        Serial.printf("getNextPass(): No future passes, %i passescount returned\n",passescount);
        Serial.println("Response from server was");
        Serial.println(payload);
        return false;
      }

      Serial.printf("Next pass at        : %02d:%02d:%02d %02d-%02d-%04d (UTC)\n",
                    hour(riseTime), minute(riseTime), second(riseTime), day(riseTime), month(riseTime), year(riseTime));

      TimeChangeRule *tcr;
      time_t t = myTz.toLocal(riseTime, &tcr);

      Serial.printf("                    : %02d:%02d:%02d %02d-%02d-%04d (%s)\n",
                    hour(t), minute(t), second(t), day(t), month(t), year(t), tcr->abbrev);

      return true;
    }
    else
    {
      Serial.printf("getNextPass(): HTTP request failed, server response: %03i\n", httpCode);
      return false;
    }
  }
  else
  {
    Serial.printf("getNextPass(): HTTP request failed, reason: %s\n", http.errorToString(httpCode).c_str());
    return false;
  }
}

/**
 * @brief Setup WiFi connection
 */
void wifiSetup()
{
  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.5, tick);

  WiFiManager wifiManager;

  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setConfigPortalTimeout(300);

  if (drd.detect())
  {
    Serial.println(F("Double reset detected, resetting WiFi config..."));
    wifiManager.resetSettings();
  }

  if (!wifiManager.autoConnect(OTA_HOSTNAME))
  {
    Serial.println(F("Failed to connect and hit timeout"));
    Serial.print(F("Device will restart in "));
    for (int i = 5; i >= 1; i--)
    {
      Serial.print(i);
      Serial.print("...");
      fail();
      delay(500);
    }
    // reset and try again
    ESP.reset();
    delay(1000);
  }

  Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
}

/**
 * @brief WiFi config mode
 * @param myWiFiManager WifiManager object
 */
void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println(F("Entered config mode"));
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());

  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

/**
 * @brief Over The Air update
 */
void otaSetup()
{
  /// OTA Update init
  ArduinoOTA.setHostname(OTA_HOSTNAME);

  ArduinoOTA.onStart([]() {
    ticker.attach(0.1, tick);
    Serial.println("\nOTA Start");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA End");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int percentage = (progress / (total / 100));

    Serial.printf("Progress: %u%%\r", percentage);
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR)
      Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR)
      Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR)
      Serial.println(F("End Failed"));
  });

  ArduinoOTA.begin();
}

/**
 * @brief Status LED ticker
 */
void tick()
{
  //toggle state
  int state = digitalRead(STATUS_LED); // get the current state of GPIO1 pin
  digitalWrite(STATUS_LED, !state);    // set pin to the opposite state
}

void heartbeat()
{
  heartbeatCounter++;
  //toggle state
  if (digitalRead(STATUS_LED) == HIGH && heartbeatCounter > 25)
  {
    digitalWrite(STATUS_LED, LOW); // set pin to the opposite state
  }
  else if (digitalRead(STATUS_LED) == LOW && heartbeatCounter > 26)
  {
    digitalWrite(STATUS_LED, HIGH); // set pin to the opposite state
    heartbeatCounter = 0;
  }
}

/*-------- NTP code ----------*/
time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (udp.parsePacket() > 0)
    ; // discard any previously received packets
  Serial.println(F("Transmiting NTP Request"));
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500)
  {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE)
    {
      Serial.println(F("Reading NTP Response"));
      udp.read(packetBuffer, NTP_PACKET_SIZE); // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + 0 * SECS_PER_HOUR;
    }
  }
  Serial.println(F("No NTP Response :-("));
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  Serial.println(F("Sending NTP packet..."));
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // Stratum, or type of clock
  packetBuffer[2] = 6;          // Polling Interval
  packetBuffer[3] = 0xEC;       // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}
