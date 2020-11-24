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
#define NUM_OF_NEOPIXELS 12
#define STATUS_LED LED_BUILTIN
#define STATUS_LED_INVERTED true

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#ifdef USE_OLED
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif
#include "secrets.h"

#ifdef USE_OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#endif

long riseTime = 0;                 // Time the ISS will rise for current position
long currentTime = 0;              // Current time (UTC)
long flyoverDuration = 0;          // Duration of ISS pass for current position
long timeUntilFlyover = 0;         // How long it will be until the next flyover
long timeUntilFlyoverComplete = 0; // How long it will be until the current flyover is complete

enum machineStates
{
  WIFI_INIT,
  START,
  GET_TIME,
  GET_NEXT_PASS,
  WAIT_FOR_PASS,
  PASS_START,
  PASSING,
  PASS_COMPLETE
};

enum machineStates currentState = WIFI_INIT;

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_OF_NEOPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
WiFiClient client;

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

// Current time (UTC)
bool getCurrentTime()
{
  HTTPClient http;
  http.begin(client, "http://worldtimeapi.org/api/timezone/etc/utc"); // URL for getting the current time
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK)
  { //Check for the returning code
    String payload = http.getString();

    const size_t capacity = JSON_OBJECT_SIZE(15) + 550;
    DynamicJsonDocument doc(capacity);
    DeserializationError error = deserializeJson(doc, payload);
    http.end();

    if (error)
    {
      Serial.print(F("deserializeJson() failed(current time): "));
      Serial.println(error.c_str());
      return false;
    }
    currentTime = doc["unixtime"]; //save current time
    return true;
  }
  else
  {
    Serial.printf("getCurrentTime(): HTTP request failed, reason: %s\n", http.errorToString(httpCode).c_str());
    return false;
  }
}

bool getNextPass()
{
  HTTPClient http;
  http.begin(client, "http://api.open-notify.org/iss-pass.json?lat=" + String(latitude) + "&lon=" + String(longitude) + "&alt=" + String(altitude) + "&n=5"); //URL for API call

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK)
  {
    String payload = http.getString(); //save response

    const size_t capacity = JSON_ARRAY_SIZE(5) + 5 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5) + 190;
    DynamicJsonDocument doc(capacity);
    DeserializationError error = deserializeJson(doc, payload);

    http.end();

    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return false;
    }

    JsonArray response = doc["response"];
    flyoverDuration = response[0]["duration"]; // save duration of the next flyover
    riseTime = response[0]["risetime"];        // save start time of the next flyover

    if (riseTime < currentTime)
    { //If ISS has already passed, take the next flyover
      flyoverDuration = response[1]["duration"];
      riseTime = response[1]["risetime"];
    }

    return true;
  }
  else
  {
    Serial.printf("getNextPass(): HTTP request failed, reason: %s\n", http.errorToString(httpCode).c_str());
    return false;
  }
}

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
    Serial.println(F("SSD1306 initalisation failed"));
  }
  display.setRotation(OLED_ROTATION);
  display.clearDisplay();
#endif
}

void loop()
{
  switch (currentState)
  {
  case WIFI_INIT:
  {
    WiFi.begin(ssid, password);
    Serial.print(F("WiFi Connecting..."));

#ifdef USE_OLED
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(30, 0);
    // Display static text
    display.print("Connect");
    display.setCursor(32, 16);
    display.print("to WiFi");
    display.display();
#endif

    int waitTime = 0;
    while ((WiFi.status() != WL_CONNECTED) && (waitTime < 300))
    {
      Serial.print(".");
      digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
      delay(100);
      waitTime++;
    }

    digitalWrite(STATUS_LED, (STATUS_LED_INVERTED == true) ? HIGH : LOW);

    if (WiFi.status() == WL_CONNECTED)
    {
      success();
      Serial.println("CONNECTED!");
      currentState = START;
    }
    else
    {
      Serial.println("FAIL!");
      Serial.print(F("Device will restart in "));
      for (int i = 5; i >= 1; i--)
      {
        Serial.print(i);
        Serial.print("...");
        fail();
        delay(500);
      }
      ESP.restart();
    }
    break;
  }
  case START:
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      currentState = GET_TIME;
    }
    else
    {
      currentState = WIFI_INIT;
    }

    break;
  }
  case GET_TIME:
  {
    Serial.println(F("Getting the current time (UTC)..."));

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

    if (!getCurrentTime())
    {
      fail();
      delay(1000);
    }
    else
    {
      success();

      Serial.print(F("Current time (UTC) = "));
      Serial.println(currentTime);

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
      timeUntilFlyover = riseTime - currentTime;

      Serial.print("Risetime = ");
      Serial.println(riseTime);

      Serial.print(F("Time until flyover (in seconds): "));
      Serial.println(timeUntilFlyover);

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
      delay(1000);
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

      delay(1000);
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
