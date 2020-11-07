
#include "OTA.h"


const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;

// Initialize Telegram BOT
//String BOTtoken = "1318988101:AAGbLF4R4ComYrneRV4Pn82rV1tKCRgiwG0";  // your Bot Token (Get from Botfather)
String BOTtoken = "1209654382:AAGHbjC9PmeRgkyaCKdh4E43BzkVt9KvB3g";

// Use @myidbot to find out the chat ID of an individual or a group
// Also note that you need to click "start" on a bot before it can
// message you
String CHAT_ID = "1086222001";


WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOTtoken, clientTCP);

// working variables  
uint8_t hmode;                          // 0=OFF 1 = ON  2 = AUTO
bool hstate;                            // true if heat on
uint8_t htemp[4];                       // temps for each zone
uint16_t hzone[3];                      // boundaries for each zone
bool dst;                               // add one hour if true
float airTemp;                          // current air temp
uint8_t hysT;                           // set hysteresis around switch on/off temps
unsigned long onTime, offTime, totTime; // variables used to record heater on time
bool logging = false;                   // o/p log messages if true
uint8_t pRestart;                         // = 1 if restart from ping failure, 0 otherwise
time_t lastReset; 


// EEPROM key variable storage, holds data during power off
// as long as alterations are saved 
#define eehmode  0
#define eehtemp  eehmode + sizeof(uint8_t)
#define eehzone  eehtemp + sizeof(htemp)
#define eedst  eehzone + sizeof(hzone)
#define eehyst eedst + sizeof(dst)

// storage for variables after ping reset
#define teehmode  eehyst + sizeof(dst)
#define teehtemp  teehmode + sizeof(hmode)
#define teehzone  teehtemp + sizeof(htemp)
#define teedst  teehzone + sizeof(hzone)
#define teehyst teedst + sizeof(dst)
#define teetotTime teehyst + sizeof(totTime)
#define teepRestart teetotTime + sizeof(pRestart)
#define teelastReset teepRestart + sizeof(lastReset)


#define TEMP_TIME 10
#define TELEGRAM_TIME 1000


const int HEATER =  4;    // gpio pin 4 controls heater triac
bool HEATER_ON = LOW;     // reverse logic 0 switches heater on
bool HEATER_OFF = HIGH;

// GPIO where the DS18B20 is connected to
const int oneWireBus = 16;     
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// timer for regular clock sync
unsigned long lastday;

// variables to set 10sec and 1sec timers
unsigned long last10;
unsigned long last1;

unsigned long boostTime;
uint8_t lastmode;


// START OF PROGRAM ********
#include "heaterlogic.h"


 void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  setupOTA("HeaterControl");

  // for a power on reset get all variables from EEPROM
  // otherwise variables have been stored in RTC memory
  // and should be still OK after software restart
  syncTime();
  
  EEPROM.begin(512);
  EEPROM.get(teepRestart, pRestart);
  
  if( pRestart == 0){
    Serial.println("Power on reset");
    // power on reset
    EEPROM.get(eehmode, hmode);
    EEPROM.get(eehtemp, htemp);
    EEPROM.get(eehzone, hzone);
    EEPROM.get(eedst, dst);
    EEPROM.get(eehyst, hysT);
    totTime = 0;
    logging = false;
    lastReset = now();
    
  } else {
    // restart after ping so recover saved state
    Serial.println("Reset after ping");
    bot.sendMessage(CHAT_ID, "Reset after ping failure\n", "");
    EEPROM.get(teehmode, hmode);
    EEPROM.get(teehtemp, htemp);
    EEPROM.get(teehzone, hzone);
    EEPROM.get(teedst, dst);
    EEPROM.get(teehyst, hysT);
    EEPROM.get(teetotTime, totTime);
    EEPROM.get(teelastReset, lastReset);
  }

  pRestart = 0;
  EEPROM.put(teepRestart, pRestart);  // only writes to EEPROM if pRestart has changed 
  EEPROM.commit(); 
 
  pinMode(HEATER, OUTPUT);
  digitalWrite(HEATER, HEATER_OFF);

  sensors.begin();

  // initialise 10 sec and 1 sec counters
  last10 = now();
  last1 = millis();
  lastday = now();
}


void loop() {


  if(now() - lastday >= 86400){
    syncTime();
    lastday = now();
  }

  if (now() - last10 >= TEMP_TIME) {
    // every 10 secs check if heater needs to switch on
    airTemp = getTemp();
    uint8_t zone = getZone(now());
    switch(hmode){
      case 0 :
        hstate = setHeater(false);
      break;
      case 1 :
        hstate = setHeater(true);
      break;
      case 2 :{
        if (airTemp < (float)(htemp[zone] - hysT)) {
          hstate = setHeater(true);
         }
        if (airTemp > (float)(htemp[zone] + hysT)) {
          hstate = setHeater(false);
        }
      }
      break;
      case 3:{
        if(now() > boostTime){
          hmode = lastmode;
          hstate = setHeater(false);
        } else
          hstate = setHeater(true);
        
      }
      break;
    }
    String text = "";
    if(logging){
      char sz[10];
      sprintf(sz, "%02d:%02d:%02d", hour(), minute(), second());
      text = String(sz) + " Air:- " + String(airTemp, 1);
      text += " Req: " + String(htemp[zone]);
      text += " Heater: ";  
      if (hstate) {
        text += "ON";
      } else {
        text += "OFF";
      }
      text += "\n";

      bot.sendMessage(CHAT_ID, text, "");
    }

    // check if we are still connected to internet
    if(! Ping.ping(ping_host, 1)) {
      String xt = "Ping failed -";
      if(WiFi.reconnect()){
        xt += "Reconnecting to wiFi";   
      } else {
        xt += "Restarting WiFi";
        WiFi.begin(ssid, password);
      }
      while(WiFi.waitForConnectResult() != WL_CONNECTED){
        // save current state of variables
        EEPROM.put(teehmode, hmode);
        EEPROM.put(teehtemp, htemp);
        EEPROM.put(teehzone, hzone);
        EEPROM.put(teedst, dst);
        EEPROM.put(teehyst, hysT);
        if (hstate){
          // heater is on so find out how long since timeoff 
          totTime += (now() - onTime);
        }
        EEPROM.put(teetotTime, totTime);
        pRestart = 1;
        EEPROM.put(teepRestart, pRestart);
        EEPROM.put(teelastReset, lastReset);
        EEPROM.commit();
        delay(2);
        ESP.restart();
      }
      xt += ": OK\n";
      bot.sendMessage(CHAT_ID, xt, "");
    }

    last10 = now();
  }
  if (millis() - last1 >= TELEGRAM_TIME) {  // check for new telegram commands every secon
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    
    last1 = millis();
  }
}
