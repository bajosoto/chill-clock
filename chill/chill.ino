#include <Arduino.h>
#include <TM1637Display.h>

#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library (you most likely already have this in your sketch)
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

#include <NTPClient.h>

#include <Ticker.h>

#include "SinricPro.h"
#include "SinricProLight.h"

// Module connection pins (Digital Pins)
#define CLK D6
#define DIO D5

// The amount of time (in milliseconds) between tests
#define DELAY       1000

// Mask to turn colon ON
#define COLON_ON    0b10000000
// Mask to turn colon OFF
#define COLON_OFF   0b01111111
// Digit off
#define SEG_N       0x00000000
// snake spinner steps count
#define INF_CNT     14

#define APP_KEY           "1293ebff-25b9-4e46-8103-bc4468d5d88c"      // Should look like "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET        "43b19c66-c839-4e35-8cdd-8366595baf2a-cfd6b6d9-641c-4bfb-87ca-5144a42e993c"   // Should look like "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"
#define LIGHT_ID          "5e84b2d20e29fb7f809a5aa5"    // Should look like "5dc1564130xxxxxxxxxxxxxx"
#define BAUD_RATE         9600                // Change baudrate to your need

#define BOTtoken "1241702060:AAErJRsM0pSqzILAGaVDS-tF7SK1FmvSIfo"

// Display stuff
// ================================================================

// Snake
uint8_t long_snake[INF_CNT][4] = {
  {SEG_F | SEG_A,   SEG_N,            SEG_N,    SEG_N},
  {SEG_A,           SEG_A,            SEG_N,    SEG_N},
  {SEG_N,           SEG_A | COLON_ON, SEG_N,    SEG_N},
  {SEG_N,           COLON_ON,         SEG_D,    SEG_N},
  {SEG_N,           SEG_N,            SEG_D,    SEG_D},
  {SEG_N,           SEG_N,            SEG_N,    SEG_D | SEG_C},
  {SEG_N,           SEG_N,            SEG_N,    SEG_C | SEG_B},
  {SEG_N,           SEG_N,            SEG_N,    SEG_B | SEG_A},
  {SEG_N,           SEG_N,            SEG_A,    SEG_A},
  {SEG_N,           COLON_ON,         SEG_A,    SEG_N},
  {SEG_N,           COLON_ON | SEG_D, SEG_N,    SEG_N},
  {SEG_D,           SEG_D,            SEG_N,    SEG_N},
  {SEG_E | SEG_D,   SEG_N,            SEG_N,    SEG_N},
  {SEG_F | SEG_E,   SEG_N,            SEG_N,    SEG_N},
};

// CHLL characters in 7-segment
uint8_t chill[] = {
  SEG_A | SEG_F | SEG_E | SEG_D,
  SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,
  SEG_F | SEG_E | SEG_D,
  SEG_F | SEG_E | SEG_D,
  };

uint8_t soda[] = {
  SEG_A | SEG_F | SEG_G | SEG_C | SEG_D,
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,
  SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,
  };

TM1637Display display(CLK, DIO);
Ticker ticker;
bool colon = true;

// Time stuff
// ================================================================
const long utcOffsetInSeconds = 7200;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

void tick_chill() {
  if (colon)
    chill[1] |= COLON_ON;
  else
    chill[1] &= COLON_OFF;
  display.setSegments(chill);
  colon = !colon;
}


void tick_time() {
  int time_dec = timeClient.getHours() * 100 + timeClient.getMinutes();
  if (time_dec >= 1400 && time_dec < 1405){
    if (colon)
      soda[1] |= COLON_ON;
    else
      soda[1] &= COLON_OFF;
    display.setSegments(soda);
  } else {
    display.showNumberDecEx(time_dec, colon ? 0b01000000 : 0b00000000, true); 
  }
  colon = !colon;
}


void tick_snake() {
  static uint8_t inf_idx = 0;
  display.setSegments(long_snake[inf_idx]);
  inf_idx = (inf_idx + 1) % INF_CNT;
}

// WiFi Stuff

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
}

// Bot stuff

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i=0; i<numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "") 
        from_name = "Guest";

    if (text == "/start") {
      String welcome = "Welcome to Chill-o-clock " + from_name + ".\n";
      welcome += "Here are your options...\n\n";
      welcome += "/chill : If you want to chill\n";
      welcome += "/no_chill : If you don't want to chill\n";
      bot.sendMessage(chat_id, welcome);
    } else if (text == "/chill") {
        String response = "Let's chill then :)\n";
        bot.sendMessage(chat_id, response);
        ticker.attach(1, tick_chill);
    } else if (text == "/no_chill") {
        String response = "Bruuuuh... :(\n";
        bot.sendMessage(chat_id, response);
        ticker.attach(0.5, tick_time);
    }
  }
}

// Sinric Pro Stuff

struct {
  bool powerState = false;
  int brightness = 0;
} device_state; 


bool onPowerState(const String &deviceId, bool &state) {
  Serial.printf("Device %s power turned %s \r\n", deviceId.c_str(), state?"on":"off");
  if (state)
    ticker.attach(1, tick_chill);
   else
    ticker.attach(0.5, tick_time);
  return true; // request handled properly
}


bool onBrightness(const String &deviceId, int &brightness) {
  int discrete_brightness = (brightness * 6 / 100) + 1;
  display.setBrightness(discrete_brightness);
  if (brightness == 42)
    ticker.attach(0.05, tick_snake);
  Serial.printf("Device %s brightness level changed to %d\r\n", deviceId.c_str(), brightness);
  return true;
}


void setupSinricPro() {
  // get a new Light device from SinricPro
  SinricProLight &myLight = SinricPro[LIGHT_ID];

  // set callback function to device
  myLight.onPowerState(onPowerState);
  myLight.onBrightness(onBrightness);

  // setup SinricPro
  SinricPro.onConnected([](){ Serial.printf("Connected to SinricPro\r\n"); }); 
  SinricPro.onDisconnected([](){ Serial.printf("Disconnected from SinricPro\r\n"); });
  SinricPro.begin(APP_KEY, APP_SECRET);

  myLight.sendBrightnessEvent(28);
}





void setup()
{
  Serial.begin(BAUD_RATE);

  WiFiManager wifiManager;
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  if (!wifiManager.autoConnect("Chill-o-clock")) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again
    ESP.reset();
    delay(1000);
  }

  timeClient.begin();

  setupSinricPro();

  display.setBrightness(2);

  Serial.printf("Device sewrew");

  ticker.attach(1, tick_chill);
//  ticker.attach(0.5, tick_time);
//  ticker.attach(0.05, tick_snake);

    client.setInsecure();
}

void loop()
{
//     SinricPro.handle();
//     timeClient.update();
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while(numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
//    Serial.print(timeClient.getHours());
//    Serial.print(":");
//    Serial.print(timeClient.getMinutes());
//    Serial.print(":");
//    Serial.println(timeClient.getSeconds());
    delay(DELAY);
}
