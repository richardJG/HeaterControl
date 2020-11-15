// START OF PROGRAM ********




void syncTime(void) {
  configTime(gmtOffset_sec, daylightOffset_sec, "time.google.com");
  time_t now = time(nullptr);
  while (now < SECS_YR_2000) {
    delay(100);
    now = time(nullptr);
  }
  if (dst) {
    now += 3600;
  }
  setTime(now);
}


uint8_t getZone(time_t t) {

  uint16_t dMin = hour(t) * 60 + minute();
  for (int i = 2; i >= 0 ; i--) {
    if (dMin - hzone[i] > 0) {
      return (i + 1);
    }
  }
  return (0);
}

String pTime(int T) {
  char sz[6];

  sprintf(sz, "%02d:%02d", T / 60, T % 60);
  return (String(sz));
}

int gTime(char * sz) {
  char s;
  int t1, t2;
  if (strlen(sz) < 4) {
    return (-1);
  }
  sscanf(sz, "%d %c %d", &t1, &s, &t2);
  return (t1 * 60 + t2);
}

String timeasString(unsigned long t){
  char ton[25];
  unsigned int hr, mn, sc;
  hr = t / 3600;
  mn = (t - hr * 3600) / 60;
  sc = t - (hr * 3600) - (mn * 60);
  sprintf(ton, "%d:%02d:%02d", hr, mn, sc);
  return String(ton);
}


void handleNewMessages(int numNewMessages) {
  
  static bool waitforYes = false;
  static bool tempSet = false;
  static uint8_t tempSetCount = 0;
  static bool timeSet = false;
  static uint8_t timeSetCount;
  

  String JSonCmd =  "[[\"On\", \"Off\", \"Auto\",\"Boost\"],[\"Status\",\"Settings\"],[\"Help\",\"Set Time\",\"Set Temp\"]]"; 
  
  Serial.print("Handle New Messages: ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    //    Serial.println(chat_id);
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    String welcome = "Command not recognised";

    // Print the received message
    String text = bot.messages[i].text;
    text.toLowerCase();
    Serial.println(text);

    if(waitforYes && (text == "yes")){
      welcome = "Resetting...";
      bot.sendMessage(CHAT_ID, welcome, "");
      bot.getUpdates(bot.last_message_received + 1);
      delay(1000);
      ESP.restart();
    }

    waitforYes = false;

    if(tempSet){

      char ch[30];
      char *token;
      int z, t;
      String sTo;
      
      text.toCharArray(ch, text.length() + 1);
      if(sscanf(ch, "%d", &t) > 0){
        if((t >= 5) && (t <= 30)){
          htemp[tempSetCount] = t;
        }
        text = "next";
      }

      if(text == "end"){
          tempSet = false;
          tempSetCount = 0;
          welcome = "Finished";
      } else {
        if(text == "next"){
          tempSetCount++;
          if(tempSetCount <= 3){
            (tempSetCount < 3) ? sTo = pTime(hzone[tempSetCount]) : sTo = "23:59";
            welcome = String(tempSetCount + 1) + ". " + pTime(hzone[tempSetCount - 1]) + " to " + sTo + ". Temp: " + String(htemp[tempSetCount]) + "\n";
            welcome += "Send new temp, Next or End\n";
          }
          if(tempSetCount > 3) {
            // finished last setting so reset
            tempSet = false;
            tempSetCount  = 0;
            welcome = "Finished";
          }
        } else {
          tempSet = false;
        }
        
        bot.sendMessageWithReplyKeyboard(chat_id, welcome, "", JSonCmd, true);
        return;
      }
    }
    
    if(timeSet){

      char ch[30];
      char *token;
      int z, t;
      String sTo;
      
      text.toCharArray(ch, text.length() + 1);
      t = gTime(ch);
      if((t > 0) && (t < 1460)){
        hzone[timeSetCount] = t;
        text = "next";
      }

      if(text == "end"){
          timeSet = false;
          timeSetCount = 0;
          welcome = "Finished";
      } else {
        if(text == "next"){
          timeSetCount++;
          if(timeSetCount <= 2){
            welcome = "Zone " + String(timeSetCount + 1) + ". ends at " + pTime(hzone[timeSetCount]) + "\n";
            welcome += "Send new time, Next or End\n";
          }
          if(timeSetCount > 2) {
            // finished last setting so reset
            timeSet = false;
            timeSetCount  = 0;
            welcome = "Finished";
          }
        } else {
          timeSet = false;
        }
        
        bot.sendMessageWithReplyKeyboard(chat_id, welcome, "", JSonCmd, true);
        return;
      }
    }
 
    
    if (text == "reset"){
      welcome = "Reset..Send Yes to confirm\n";
      waitforYes = true; 
    }
    
    if(text == "set temp"){
      tempSet = true;
      tempSetCount = 0;
      welcome = "1. 00:00 to " + pTime(hzone[0]) + ". Temp: " + String(htemp[0]) + "\n";
      welcome += "Send new temp, Next or End\n";
      bot.sendMessageWithReplyKeyboard(chat_id, welcome, "", JSonCmd, true);
      return;
    }
    tempSet = false;

    if(text == "set time"){
      timeSet = true;
      timeSetCount = 0;
      welcome = "Zone 1 ends at " + pTime(hzone[0]) + "\n";
      welcome += "Send new time, Next or End\n";
      bot.sendMessageWithReplyKeyboard(chat_id, welcome, "", JSonCmd, true);
      return;
    }
    timeSet = false;

    if(text == "settings"){
      welcome = "Time/Temps\n";
      welcome += "1. 00:00 to " + pTime(hzone[0]) + ". Temp: " + String(htemp[0]) + "\n2. ";
      welcome += pTime(hzone[0]) + " to " + pTime(hzone[1]) + ". Temp: " + String(htemp[1]) + "\n3. ";
      welcome += pTime(hzone[1]) + " to " + pTime(hzone[2]) + ". Temp: " + String(htemp[2]) + "\n4. ";
      welcome += pTime(hzone[2]) + " to 23:59. Temp: " + String(htemp[3]) + "\n\n";
      if(dst){
        welcome += "Daylight saving ON\n";
      } else {
        welcome += "Daylight saving OFF\n";
      }
      welcome += "Hysteresis setting: " + String(hysT) + " secs\n";
    }
        
    if ((text == "help") || (text == "/start")) {
      String from_name = bot.messages[i].from_name;
      welcome = "Welcome , " + from_name + "\n";
      welcome += "Menu: \n";
      welcome += "On : Heater ON\n";
      welcome += "Off : Heater OFF\n";
      welcome += "Auto : Heater AUTO\n";
      welcome += "Boost : Switch on for 1 hour\n";
      welcome += "Status : show current status \n";
      welcome += "Settings : Show current time/temp settings\n";
      welcome += "Help: show this help screen\n";
      welcome += "Set Time: Change time of day boundaries\n";
      welcome += "Set Temp: Change temps for each zone\n";
      welcome += "Save : Saves params to EEPROM\n";
    }

    if(text == "boost"){
      // switch on heater for one hour
      boostTime = now() + 3600;
      lastmode = hmode;
      hmode = 3;
      welcome = "Boost - heater on for 1 hour";    
    }

    if(text == "log") {
      logging = !logging;
      welcome = "Logging: ";
      if (logging) {
        welcome += "ON\n";
      }
      else {
        welcome += "OFF\n";
      }
    }

    if (text == "save") {
      EEPROM.put(eehmode, hmode);
      EEPROM.put(eehtemp, htemp);
      EEPROM.put(eehzone, hzone);
      EEPROM.put(eedst, dst);
      EEPROM.put(eehyst, hysT);
      EEPROM.commit();
      welcome = "User params saved";
    }
    
    if (text == "status") {
      char timeStr[20], tr[25];

      sprintf(timeStr, "%02d:%02d:%02d", hour(), minute(), second());
      welcome = "Status:   \n";
      welcome += "Time: " + String(timeStr) + "\n";
      welcome += "Mode: ";
      switch (hmode) {
        case 0:
          welcome += "OFF\n";
          break;
        case 1:
          welcome += "ON\n";
          break;
        case 2:
          welcome += "AUTO\n";
          break;
        case 3:
          welcome += "BOOST\n";
      }
      uint8_t gz = getZone(now());
      welcome += "Zone: " + String(gz + 1) + "\n";
      welcome += "Req temp: " + String(htemp[gz]) + "\n";
      welcome += "Air temp: "  + String(airTemp, 1) + "\n";
      welcome += "Heater: ";
      if (hstate) {
        welcome += "ON\n";
      }
      else {
        welcome += "OFF\n";
      }
      welcome += "\n";
      if (hstate){
        // heater is on so find out how long since timeoff
        totTime += (now() - onTime);
        onTime = now();
      }
      sprintf(tr, "%d/%d/%d - %02d:%02d", day(lastReset), month(lastReset), year(lastReset), hour(lastReset), minute(lastReset));
      welcome += "Heater on for: " + timeasString(totTime) + " since " + String(tr) + "\n";
    }

    if (text == "sync") {
      syncTime();
      char timeStr[20];
      sprintf(timeStr, "%02d:%02d:%02d", hour(), minute(), second());
      welcome = "Time: " + String(timeStr) + "\n";
    }

    if (text == "off") {
      hmode = 0;  
      welcome = "Mode : OFF\n";
    }
    
    if (text == "on") {
      hmode = 1;
      welcome = "Mode : ON\n";
    }
    
    if (text == "auto") {
      hmode = 2;
      welcome = "Mode : AUTO\n";
    }

    if (text.startsWith("hyst")) {
      // processing hysteresis setting
      // hyst 0 - 5
      char ch[30];
      char *token;
      int z, t;

      text.toCharArray(ch, text.length() + 1);
      token = strtok(ch, " ");
      token = strtok(NULL, ",");
      z = 0;
      t = -1;
      if (token != NULL) {
        if (strlen(token) > 0){
          sscanf(token, "%d", &t);
        }
        if((t >= 0) && (t <= 5)){
          hysT = t;
        }
      }
      welcome = "Hysteresis setting: " + String(hysT) + "\n";
    }    


    if (text == "dst") {
      (dst) ? dst = false  : dst = true;
      welcome = "DST : ";
      if (dst) {
        welcome += "ON\n";
      } else {
        welcome += "OFF\n";
      }
      syncTime();
    }

    bot.sendMessageWithReplyKeyboard(chat_id, welcome, "", JSonCmd, true);

  }
}


void sendBotMessage(String t){
  static char botMsg[60];
  t.toCharArray(botMsg, t.length() + 1);
  char *ch = botMsg;
  xQueueSend(queue, &ch, portMAX_DELAY); 
}


float getTemp(void) {

  sensors.requestTemperatures(); 
  float tempC = sensors.getTempCByIndex(0);
  return (tempC);
}

bool setHeater(bool newState) {
   unsigned long deltaOnTime;
  
  if(hstate != newState) {
    // set GPIO to control radiator on/off
    if(newState){
      onTime = now();
      offTime = onTime;
      digitalWrite(HEATER, HEATER_ON);
    }
    else {
      offTime = now();
      if(offTime > onTime){
        deltaOnTime = (offTime - onTime); 
        totTime += deltaOnTime;
        onTime = offTime;
      }
      digitalWrite(HEATER, HEATER_OFF);
      char sz[10];
      sprintf(sz, "%02d:%02d:%02d", hour(), minute(), second());
      String text = String(sz) + " Heater on for:- " + timeasString(deltaOnTime) + "\n";  

      Serial.println(text);
      sendBotMessage(text);
    }
  }
  return ( newState );
}



void process_heater( void * parameter )
{
unsigned long last, lastday;
  last = now();
  lastday = now();
  while(1){
    if(now() - lastday >= 86400){
      // ressync ESP clock once a day
      syncTime();
      lastday = now();
    }
    
    if (now() - last >= TEMP_TIME) {
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
        //TelnetStream.println(text);
        sendBotMessage(text);
      }
      last = now();
    }
  }
}
