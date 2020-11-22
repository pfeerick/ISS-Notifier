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

//Variables for times and duration
long riseTime = 0;
long currentTime = 0;
long duration = 0;
long timeUntilFlyover = 0; //difference

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(12, 2, NEO_GRB + NEO_KHZ800);
WiFiClient client;

void success()
{
  for (int i = 0; i < 12; i++)
  {
    pixels.setPixelColor(i, pixels.Color(0, 255, 0));
    pixels.show();
  }

  delay(500);

  for (int i = 11; i >= 0; i--)
  {
    pixels.setPixelColor(i, LOW);
    pixels.show();
  }
}

void fail()
{
  for (int i = 0; i < 12; i++)
  {
    pixels.setPixelColor(i, pixels.Color(255, 0, 0));
    pixels.show();
  }

  delay(500);

  for (int i = 11; i >= 0; i--)
  {
    pixels.setPixelColor(i, LOW);
    pixels.show();
  }
}

// Current time (GMT)
void getCurrentTime()
{
  Serial.println("Getting the current time...");

  HTTPClient http;
  http.begin(client, "http://worldtimeapi.org/api/timezone/europe/london"); //URL for getting the current time

  int httpCode = http.GET();

  if (httpCode == 200)
  { //Check for the returning code

    success();

    String payload = http.getString();

    const size_t capacity = JSON_OBJECT_SIZE(15) + 550;
    DynamicJsonDocument doc(capacity);
    DeserializationError error = deserializeJson(doc, payload);
    http.end();

    if (error)
    {
      Serial.print(F("deserializeJson() failed(current time): "));
      Serial.println(error.c_str());
      return;
    }
    currentTime = doc["unixtime"]; //save current time
    Serial.print("current time= ");
    Serial.println(currentTime);
  }
  else
  {
    Serial.println("Error on HTTP request");
    fail();
  }
}

void apiCall()
{
  Serial.println("Looking up next ISS flyover time...");
  if ((WiFi.status() == WL_CONNECTED))
  {
    getCurrentTime(); //call function for getting the current time

    HTTPClient http;
    http.begin(client, "http://api.open-notify.org/iss-pass.json?lat=" + String(latitude) + "&lon=" + String(longitude) + "&alt=" + String(altitude) + "&n=5"); //URL for API call

    int httpCode = http.GET();

    if (httpCode == 200)
    {
      success();

      String payload = http.getString(); //save response

      const size_t capacity = JSON_ARRAY_SIZE(5) + 5 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5) + 190;
      DynamicJsonDocument doc(capacity);
      DeserializationError error = deserializeJson(doc, payload);

      http.end();

      if (error)
      {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
      }

      JsonArray response = doc["response"];
      duration = response[0]["duration"]; // save duration of the next flyover
      riseTime = response[0]["risetime"]; // save start time of the next flyover

      if (riseTime < currentTime)
      { //If ISS has already passed, take the next flyover
        duration = response[1]["duration"];
        riseTime = response[1]["risetime"];
      }

      Serial.print("Risetime [0]= ");
      Serial.println(riseTime);

      //compute time until rise
      timeUntilFlyover = riseTime - currentTime;
      Serial.print("Time until flyover (in seconds): ");
      Serial.println(timeUntilFlyover);

      uint32_t t = timeUntilFlyover;
      int s = t % 60;
      t = (t - s)/60;
      int m = t % 60;
      t = (t - m)/60;
      int h = t;
      Serial.printf("That's %02i hours, %02i minutes and %02i seconds\n",h,m,s);
    }
    else
    {
      Serial.println("Error on HTTP request");
      fail();
    }
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT); //LED Pin
  digitalWrite(LED_BUILTIN, LOW); // invert if LOW == ON

  pixels.begin();
  pixels.setBrightness(100);

  Serial.println("");
  Serial.println("ISS Notifier");
  Serial.println("");

  WiFi.begin(ssid, password);
  Serial.print("WiFi Connecting...");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    Serial.print(".");
    digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));
  }

  if (WiFi.status() == WL_CONNECTED) {
    success();
    Serial.println("[CONNECTED!]");
  } else{
    fail();
    Serial.println("[FAIL!]");
  }
}

void loop()
{
  apiCall(); //API call for the next ISS flyover

  //shut down the NeoPixel until next ISS flyover
  for (int i = 0; i < 12; i++)
  {
    pixels.setPixelColor(i, LOW);
    pixels.show();
  }

  while (timeUntilFlyover > 0)
  { // while the ISS isn't overhead

    delay(1000);
    Serial.println(timeUntilFlyover);

    timeUntilFlyover--;
  }

  //when ISS rises  above the horizon
  Serial.println("ISS overhead!");
  int maxDuration = duration; //save max value of the flyover duration
  Serial.print("max duration = ");
  Serial.println(duration);

  for (duration; duration >= 0; duration--)
  {
    //map remaining flyover time on a color gradient
    int colorRed = map(duration, 0, maxDuration, 200, 0);
    int colorBlue = map(duration, 0, maxDuration, 0, 200);

    //show the current color on all LEDs
    for (int i = 0; i < 12; i++)
    {
      pixels.setPixelColor(i, pixels.Color(colorRed, 0, colorBlue));
      pixels.show();
    }
    delay(1000);
  }

  for (int i = 0; i < 12; i++)
  {
    pixels.setPixelColor(i, LOW);
    pixels.show();
  }
}
