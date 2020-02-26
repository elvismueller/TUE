void checkSaveOnLowPower()
{
  // Haben wir noch Saft?
  if (fDataSaving)
  {
    DataSaving();
    fDataSaving = false;
  }
}

void checkEspRunning()
{ //While schleife solange wir keine Betriebsspannung haben
  while (!fESPrunning)
  { //Tasks bedienen
    LEDStatus = LEDOff;   //LED aus
    yield();
    ts.update();
    Serial.print("s");
    LEDTS();
    delay(1000);
    checkSaveOnLowPower();
    // ChargePump abschalten
    digitalWrite(CHARGEPUMPENABLE, HIGH);
    //ESP abschalten
    ESP.deepSleep(10);
    delay(1000);
  }
}

void syncInternalTimeWithTochteruhr()
{ //Interne Zeit mit der Tochteruhrzeit gleich setzen, sonst läuft uns die Uhr nach einem Neustart los...
  tochter_h = atoi(c_clock_hour);
  tochter_m = atoi(c_clock_minute);
  sprintf(c_clock_hour, "%i", tochter_h);
  sprintf(c_clock_minute, "%i", tochter_m);
  TochterUhr();
}

void LED(int LEDstatus)
{
  LEDStatus = LEDstatus;       // LED Status setzen dauert 1 sec.
  yield();
  ts.update();
  delay(10);
  yield();
  ts.update();
  delay(10);
}

void printTime(time_t t)
{
  sPrintI00(hour(t));
  sPrintDigits(minute(t));
  sPrintDigits(second(t));
  Serial.print(' ');
  Serial.print(dayShortStr(weekday(t)));
  Serial.print(' ');
  sPrintI00(day(t));
  Serial.print(' ');
  Serial.print(monthShortStr(month(t)));
  Serial.print(' ');
  Serial.print(year(t));
  Serial.println(' ');
}

//Print an integer in "00" format (with leading zero).
//Input value assumed to be between 0 and 99.
void sPrintI00(int val)
{
  if (val < 10) Serial.print('0');
  Serial.print(val, DEC);
  return;
}

//Print an integer in ":00" format (with leading zero).
//Input value assumed to be between 0 and 99.
void sPrintDigits(int val)
{
  Serial.print(':');
  if (val < 10) Serial.print('0');
  Serial.print(val, DEC);
}
