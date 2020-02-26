void initFileSystem()
{
  //clean FS, for testing
  if (clearConfigOnInit)
  {
    SPIFFS.format();
    SPIFFS.remove("/config.json");
    SPIFFS.remove("/config2.json");
  }
  //read configuration from FS json
  Serial.print("mounting FS... ");
  // Falls das Filesystem noch nicht formatiert ist, wird es hier automatisch gemacht.
  if (SPIFFS.begin())
  {
    Serial.println("FS ok!");
  }
  else
  {
    SPIFFS.format();
    Serial.println("FS formated");
  }
}

void mountFileSystemAndReadConfig()
{
  if (SPIFFS.begin())
  {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json"))
    {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        Serial.println("opened config file");
        DynamicJsonDocument json(2024);
        DeserializationError ret = deserializeJson(json, configFile);
        if (ret == DeserializationError::Ok)
        {
          Serial.println("deserialize config file...");
          serializeJson(json, Serial);
          Serial.println(" ");
          Serial.print("read config...");
          strcpy(ntpserver, json["ntp_server"]);
          strcpy(mrc_multicast, json["mrc_multicast"]);
          strcpy(mrc_port, json["mrc_port"]);
          itoa(json["clock_hour"], c_clock_hour, 10);
          itoa(json["clock_minute"], c_clock_minute, 10);
          if (json["FlagTUStellglied"] == "true")
          {
            Serial.println("FlagTUStellglied = true");
            FlagTUStellglied = true;
            digitalWrite(TAKTA, LOW);
            digitalWrite(TAKTB, LOW);
          }
          else
          {
            Serial.println("FlagTUStellglied = false");
            FlagTUStellglied = false;
            digitalWrite(TAKTA, HIGH);
            digitalWrite(TAKTB, HIGH);
          }
        }
        else
        {
          Serial.print("error parsing config: ");
          Serial.println(ret.c_str());
        }
      }
    }
  }
  else
  {
    Serial.println("failed to mount FS");
    FlagTUStellglied = true;
    digitalWrite(TAKTA, LOW);
    digitalWrite(TAKTB, LOW);
    Serial.println("failed to load json config");
  }
}

void DataSaving(void)
{
  Serial.println("Saving Data to File...");
  DynamicJsonDocument json(2024);
  json["ntp_server"] = ntpserver;
  json["mrc_multicast"] = mrc_multicast;
  json["mrc_port"] = mrc_port;
  json["clock_hour"] = tochter_h;
  json["clock_minute"] = tochter_m;
  json["FlagTUStellglied"] = FlagTUStellglied;
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing!");
  }
  else
  {
    serializeJson(json, Serial);
    size_t bytes = serializeJson(json, configFile);
    Serial.print("... Done! [");
    Serial.print(bytes, DEC);
    Serial.println(" bytes]");
  }
  configFile.close();
}

void checkConfigButton(void)
{
  if (digitalRead(CONFIGPB) == LOW)
  {
    if (LEDStatus == LEDAusVorbereitet)
    { //ESP in den Deep-Sleep versetzen
      Serial.println("Go to sleep!");
      while (FlagTUStellen)
      { //Prüfen, ob gerade eine Taktausgabe erfolgt, wenn ja dann darauf warten...
        //Tasks bedienen
        yield();
        ts.update();
        if (showWaitingInLog) Serial.print(".");
        delay(10);
      }
      DataSaving();
      //wifi_set_sleep_type(LIGHT_SLEEP_T);
      system_deep_sleep(1000000);
      //ESP.deepSleep(1000000);
      delay(100);
    }
    if (FlagButtonPressed)
    {
      if (CounterButtonPressed == 0)
      {
        Serial.println("Config Button pressed!");
        ts.remove(0);
        while (digitalRead(CONFIGPB) == LOW)
        {
          yield();
          ts.update();
          if (showWaitingInLog) Serial.print(".");
          delay(10);
          LEDStatus = LEDBlinking;      // LED blinken lassen
        }
        // jetzt Config im Flash löschen
        Serial.println("erasing");
        Serial.printf("SPIFFS.remove = %d", SPIFFS.remove("/config.json"));
        Serial.print(".");

        WiFi.persistent(true);
        delay(1000);
        WiFi.setAutoReconnect(false);
        delay(100);
        WiFi.disconnect();
        delay(100);
        wifiManager.resetSettings();
        ESP.eraseConfig();
        delay(100);
        Serial.print("..ESP Reset..");
        delay(1000);
        ESP.reset();
        //ESP.restart();
        delay(5000);
      }
      else CounterButtonPressed--;
    }
    else
    {
      FlagButtonPressed = true;
      CounterButtonPressed = 300;
      Serial.println("BP");

      LEDStatus = LEDOn;        //Blaue LED anschalten
    }
  }
  else
  {
    // Taste wird vor dem Ablaufen des CounterButtonPressed losgelassen, dann nur Daten ins Flash sichern
    if (FlagButtonPressed)
    {
      FlagButtonPressed = false;
      CounterButtonPressed = 0;
      //LEDStatus = LEDAlive;       //Blaue LED zeigt Lebenszeichen
      DataSaving();
      ts.add(0, 10, [&](void*) {
        tick();
      }, nullptr, true);
      LEDStatus = LEDAusVorbereitet;  //Jetzt kann man den TUE ausschalten
    }
  }
}

void ISRSaveData(void)
{
  Serial.println("Interrupt to save data!");
  DataSaving();
  digitalWrite(BLED, HIGH);
  Serial.println("Data Saved");
  detachInterrupt(INT);
  ESP.deepSleep(10);
}
