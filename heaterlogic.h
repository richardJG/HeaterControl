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
  char ton[20], tr[22];
  unsigned int hr, mn, sc;
  hr = totTime / 3600;
  mn = (totTime - hr * 3600) / 60;
  sc = totTime - (hr * 3600) - (mn * 60);
  sprintf(ton, "%d:%02d:%02d", hr, mn, sc);
  return String(ton);
}


void handleNewMessages(int numNewMessages) {
  
  static bool waitforYes = false;
  String JSonCmd =  "[[\"On\", \"Off\", \"Auto\",\"Boost\"],[\"Status\",\"Help\"],[\"Temp\",\"Time\",\"Sync\",\"Hyst\"]]"; 
  
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
    if (text == "reset"){
      welcome = "Reset..Send Yes to confirm\n";
      waitforYes = true; 
    }
    
    if ((text == "help") || (text == "/start")) {
      String from_name = bot.messages[i].from_name;
      welcome = "Welcome , " + from_name + "\n";
      welcome += "Menu: \n";
      welcome += "Sync : Reset clock\n";
      welcome += "Off : Heater OFF\n";
      welcome += "On : Heater ON\n";
      welcome += "Auto : Heater AUTO\n";
      welcome += "Boost : Switch on for 1 hour\n";
      welcome += "Status : Status \n";
      welcome += "Temp : Show/Edit Zone temps \n";
      welcome += "Time : Show/Edit Time zone boundaries \n";
      welcome += "DST : toggles DST On/Off\n";
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
      welcome += "Zone: " + String(gz) + "\n";
      welcome += "Req temp: " + String(htemp[gz]) + "\n";
      welcome += "Air temp: "  + String(airTemp, 1) + "\n";
      welcome += "Heater: ";
      if (hstate) {
        welcome += "ON\n";
      }
      else {
        welcome += "OFF\n";
      }
      welcome += "DST: ";
      if (dst) {
        welcome += "ON\n";
      } else {
        welcome += "OFF\n";
      }
      if (hstate){
        // heater is on so find out how long since timeoff
        totTime += (now() - onTime);
        onTime = now();
      }

      sprintf(tr, "%d/%d/%d - %02d:%02d", day(lastReset), month(lastReset), year(lastReset), hour(lastReset), minute(lastReset));
      welcome += "Heater on: " + timeasString(totTime) + " since " + String(tr) + "\n";
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

    if (text.startsWith("temp")) {
      // processing temps for each zone
      // where temps are comma separatd list with 4 fields field = -1 if temp to be ignore
      char ch[30];
      char *token;
      int z, t;

      text.toCharArray(ch, text.length() + 1);
      token = strtok(ch, " ");
      token = strtok(NULL, ",");
      z = 0;
      t = 2;
      while (token != NULL) {
        t = -1;
        if(strlen(token) > 0){
          sscanf(token, "%d", &t);
        }
        if (t > 0) {
          htemp[z] = (uint8_t)t;
        }
        z++;
        token = strtok(NULL, ",");
      }
      welcome = "Temp settings: " + String(htemp[0]);
      for (z = 1; z < 4; z++) {
        welcome += ", " + String(htemp[z]);
      }
      welcome += "\n";
    }

    if (text.startsWith("time")) {
      // processing zone times for each zone
      // where times are comma separatd list with 4 fields field = -1 if time to be ignore
      char ch[30];
      char *token;
      int z, t;

      text.toCharArray(ch, text.length() + 1);
      token = strtok(ch, " ");
      token = strtok(NULL, ",");
      z = 0;
      t = 2;
      while (token != NULL) {
        t = gTime(token);
        if (t > 0) {
          hzone[z] = t;
        }
        z++;
        token = strtok(NULL, ",");
      }
      welcome = "Zone boundaries: " + pTime(hzone[0]);
      for (z = 1; z < 3; z++) {
        welcome += ", " + pTime(hzone[z]);
      }
      welcome += "\n";
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

//    bot.sendMessage(CHAT_ID, welcome, "");

  }
}


void sendBotMessage(String t){
  static char botMsg[50];
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
  while(1){


    
  if (now() - last10 >= TEMP_TIME) {
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

      sendBotMessage(text);
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
      sendBotMessage(xt);
    }

    last10 = now();
  }

  }
}
