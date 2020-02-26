//Task für den LED Status
void * LEDTS(void)
{
  if (LEDStatusIntern != LEDStatus)
  { //LED Status hat sich verändert, alle Counter reseten
    LEDStatusCounter = 0;
    LEDStatusIntern = LEDStatus;
    if (LEDStatusIntern == LEDAusVorbereitet) LEDPeriodCounter = LEDAusVorbperiod;
  }
  if (LEDStatusIntern == LEDOff)
  {
    digitalWrite(BLED,  HIGH);
  }
  if (LEDStatusIntern == LEDOn)
  {
    digitalWrite(BLED,  LOW);
  }
  if (LEDStatusIntern == LEDBlinking)
  {
    if (!LEDStatusCounter) {
      LEDStatusCounter = LEDblinkingcycle;
      if (digitalRead(BLED)) digitalWrite(BLED, LOW);
      else digitalWrite(BLED, HIGH);
    }
    else LEDStatusCounter--;
  }
  if (LEDStatusIntern == LEDAlive)
  {
    if (digitalRead(BLED))
    { //LED ist aus
      if (!LEDStatusCounter)
      {
        LEDStatusCounter = LEDalivetime;
        digitalWrite(BLED, LOW);
      }
      else LEDStatusCounter--;
    }
    else
    { //LED ist an
      if (!LEDStatusCounter)
      {
        LEDStatusCounter = LEDalivecycle;
        digitalWrite(BLED, HIGH);
      }
      else LEDStatusCounter--;
    }
  }
  if (LEDStatusIntern == LEDBlinkOnce)
  {
    digitalWrite(BLED, LOW);
    LEDStatus = LEDOff;
  }
  if (LEDStatus == LEDAusVorbereitet)
  {
    if (!LEDStatusCounter)
    {
      LEDStatusCounter = LEDAusVorbcycle;
      if (digitalRead(BLED)) digitalWrite(BLED, LOW);
      else digitalWrite(BLED, HIGH);
    }
    else LEDStatusCounter--;
    if (!LEDPeriodCounter)
    {
      LEDStatus = LEDAlive;
    }
    else LEDPeriodCounter--;
  }
}

//Task zum weiterstellen der Tochteruhr
//Wird zyklisch aufgerufen und prüft, ob die empfangene Uhrzeit mit der angezeigten Uhrzeit übereinstimmt.
//Stellt sie einen Unterschied fest, setzt sie das Flag "FlagTUStellen", dieses Flag wird durch die Task "TochterUhrStellen"
//ausgewertet, bearbeitet und zurückgestellt.
//Task verwaltet die Uhrzeit der Tochteruhr.
void * tick(void)
{
  if ( !FlagTUStellen )
  {
    // Zeitzeichen mit Tochteruhr vergleichen, die Tochteruhr ist eine 12h Uhr. Das Zeitzeichen kann auch im 24h gesendet werden.
    if ( ( clock_h == tochter_h && clock_m == tochter_m ) || ( clock_h - 12 == tochter_h && clock_m == tochter_m ) )
    {
      // Tochteruhr stimmt mit Zeitzeichen überein, nix machen
    }
    else
    {
      // Tochteruhr muss gestellt werden.
      tochter_m++;
      if (tochter_m == 60)
      { // Stundensprung
        tochter_h++;
        tochter_m = 0;
        if (tochter_h == 12)
        { // Tagesprung
          tochter_h = 0;
        }
      }
      FlagTUStellen = true;
      Serial.print("[tick()] Tochteruhr stellt auf Zeit: ");
      Serial.print(tochter_h);
      Serial.print(":");
      Serial.print(tochter_m);
      Serial.print(" Zeitzeichen: ");
      Serial.print(clock_h);
      Serial.print(":");
      Serial.print(clock_m);
      Serial.print(":");
      Serial.println(clock_s);
    }
  }
}

//Task zum gezwungenen Weiterstellen der Tochteruhr
//Wird durch das Captive Portal aufgerufen und stellt sicher, dass die angeschlossene Tochteruhr
//synchron zum internen Stellglied läuft.
//FlagTUStellen muss false sein, beim Aufruf der Routine, sonst keine Auswirkung.
void * forcetick(void)
{
  if ( !FlagTUStellen )
  {
    // Tochteruhr muss gestellt werden.
    tochter_m++;
    if (tochter_m == 60)
    { // Stundensprung
      tochter_h++;
      tochter_m = 0;
      if (tochter_h == 12)
      { // Tagesprung
        tochter_h = 0;
      }
    }
    FlagTUStellen = true;
    Serial.print("[forcetick()] Tochteruhr stellt auf Zeit: ");
    Serial.print(tochter_h);
    Serial.print(":");
    Serial.print(tochter_m);
    Serial.print(" Zeitzeichen: ");
    Serial.print(clock_h);
    Serial.print(":");
    Serial.print(clock_m);
    Serial.print(":");
    Serial.println(clock_s);
  }
}

void * TochterUhr(void)
{
  Serial.print("Tochteruhr zeigt: ");
  Serial.print(tochter_h);
  Serial.print(":");
  Serial.println(tochter_m);
}

//Task gibt den Stelltakt an die Tochteruhr raus. Sobald das Flag "FlagTUStellen" gesetzt ist, wird ein Stellbefehl ausgegeben.
//Das Flag wird nach der Ausgabe des Stellbefehls und dem Abschalten des Ausgangs wieder zurückgesetzt.
//Abarbeitung nach einer Statemaschine. Somit kann die Zeitdauer des Ausgangsimpuls kontrolliert werden.
//Sobald ein Takt ausgegeben werden soll, wird zuerst die Ladungspumpe eingeschaltet, nachher der Takt ausgeben, die
//Ausgänge abgeschaltet und die Ladungspumpe wieder abgeschaltet, eher ein neuer Stelltakt ausgegeben werden kann.
void * TochterUhrStellen(void)
{
  if (SM < 7 && SM > 0)
  {
    if (SM == 1)
    {
      // Warten auf FlagTUstellen
      if ( FlagTUStellen )
      {
        SM = 2;
        Serial.print("FlagTUStellen: ");
        Serial.println("true");
      }
    }
    if (SM == 2)
    {
      // State Chargepump init
      digitalWrite(CHARGEPUMPENABLE, LOW);
      SM = 3;
      SMC = CHARGEPUMPTIME;
    }
    if (SM == 3)
    {
      // Warten auf Spannung
      if (SMC == 0)
      {
        SM = 4;
        SMC = TAKTTIME;
        // Tick ausgeben, Ausgänge anschalten
        if (FlagTUStellglied)
        {
          digitalWrite(TAKTA, LOW);
          digitalWrite(TAKTB, HIGH);
          Serial.println("FlagTUStellglied = +");
          FlagTUStellglied = false;
        }
        else
        {
          digitalWrite(TAKTA, HIGH);
          digitalWrite(TAKTB, LOW);
          Serial.println("FlagTUStellglied = -");
          FlagTUStellglied = true;
        }
      }
      else
      {
        SMC--;
      }
    }
    if (SM == 4)
    {
      // Tick ausgeben
      if (SMC == 0)
      {
        SM = 5;
        //Tick zurückstellen
        if (!FlagTUStellglied)
        {
          digitalWrite(TAKTA, HIGH);
          digitalWrite(TAKTB, HIGH);
          Serial.println("FlagTUStellglied = -");
        }
        else
        {
          digitalWrite(TAKTA, LOW);
          digitalWrite(TAKTB, LOW);
          Serial.println("FlagTUStellglied = +");
        }
      }
      else
      {
        SMC--;
      }
    }
    if (SM == 5)
    {
      // ChargePump abschalten
      digitalWrite(CHARGEPUMPENABLE, HIGH);
      SM = 6 ;
      SMC = TAKTWAITIME;
    }
    if (SM == 6)
    {
      // Warte State nach Stelltaktausgabe
      if ( SMC == 0 )
      {
        SM = 1;
        SMC = 0;
        FlagTUStellen = false;
      }
      else
      {
        SMC--;
      }
    }
  }
  else
  { // Init Statemaschine
    SM = 1;
    SMC = 0;
  }
}

void * UBat(void)
{
  Serial.print("[UBat()] ");
  adcwert = analogRead(A0);    //aktuellen ADC Wert lesen
  adcmittelwert = (adcmittelwert + analogRead(A0)) / 2;

  if (!fESPrunning)
  {
    if (adcmittelwert > adcstartgrenzwert) {  //Wert ist über dem Startwert, ESP wieder hochfahren
      fESPrunning = true;
      Serial.print("ESP funktioniert");
      LEDStatus = LEDAlive;                   //LED alive
      ts.enable(0);
      ts.enable(2);
      ts.enable(3);
    }
  }
  if (fESPrunning)
  {
    if (adcmittelwert < adcstopgrenzwert) {
      fESPrunning = false;
      Serial.print("ESP ruht");
      fDataSaving = true;
      LEDStatus = LEDOff;                   //LED aus
      ts.disable(0);
      ts.disable(2);
      ts.disable(3);
    }
  }
  Serial.print("Level = ");
  Serial.print(adcwert);
  Serial.print(" Mittelwert = ");
  Serial.println(adcmittelwert);
}
