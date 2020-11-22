/**
 * Original code written by pollux labs, 2020
 * URL: https://gist.github.com/polluxlabs/1ba7824175c5e011565bd61af2fd1c6b
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <Adafruit_NeoPixel.h>
#include "secrets.h"

#define NEOPIXEL_PIN D3
#define NUM_OF_NEOPIXELS 17

long riseTime = 0;                 // Time the ISS will rise for current position
long currentTime = 0;              // Current time for GMT
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

// Current time (GMT)
bool getCurrentTime()
{
  HTTPClient http;
  http.begin(client, "http://worldtimeapi.org/api/timezone/europe/london"); //URL for getting the current time
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
    Serial.println("getCurrentTime(): Error on HTTP request");
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

    //compute time until rise
    timeUntilFlyover = riseTime - currentTime;
    return true;
  }
  else
  {
    Serial.println("getNextPass(): Error on HTTP request");
    return false;
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);    //LED Pin
  digitalWrite(LED_BUILTIN, HIGH); // invert if HIGH == ON

  pixels.begin();
  pixels.setBrightness(100);
  pixels.show();

  Serial.println("");
  Serial.println("ISS Flyover Notifier");
  Serial.println("");
}

void loop()
{
  switch (currentState)
  {
  case WIFI_INIT:
  {
    WiFi.begin(ssid, password);
    Serial.print("WiFi Connecting...");
    int waitTime = 0;
    while ((WiFi.status() != WL_CONNECTED) && (waitTime < 30))
    {
      Serial.print(".");
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(100);
      waitTime++;
    }

    digitalWrite(LED_BUILTIN, HIGH); // invert if HIGH == ON

    if (WiFi.status() == WL_CONNECTED)
    {
      success();
      Serial.println("CONNECTED!");
      currentState = START;
    }
    else
    {
      Serial.println("FAIL!");
      Serial.print("Device will restart in ");
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
    //shut down the NeoPixel until next ISS flyover
    pixels.clear();
    pixels.show();

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
    Serial.println("Getting the current time (GMT)...");
    if (!getCurrentTime())
    {
      fail();
      delay(1000);
    }
    else
    {
      success();

      Serial.print("Current time (GMT) = ");
      Serial.println(currentTime);

      currentState = GET_NEXT_PASS;
    }
    break;
  }
  case GET_NEXT_PASS:
  {
    Serial.println("Looking up next ISS flyover time...");
    if (!getNextPass())
    {
      fail();
      delay(1000);
    }
    else
    {
      success();

      Serial.print("Risetime = ");
      Serial.println(riseTime);

      Serial.print("Time until flyover (in seconds): ");
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
    timeUntilFlyover -= 1800;
    if (timeUntilFlyover > 0)
    { // while the ISS isn't overhead

      uint32_t t = timeUntilFlyover;
      int s = t % 60;
      t = (t - s) / 60;
      int m = t % 60;
      t = (t - m) / 60;
      int h = t;
      Serial.printf("Next ISS flyover in %02i:%02i:%02i\n", h, m, s);

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
    Serial.println("ISS overhead!");

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
    for (int i = 0; i < NUM_OF_NEOPIXELS; i++)
    {
      pixels.setPixelColor(i, LOW);
      pixels.show();
    }
    currentState = START;
    break;
  }
  }
}
