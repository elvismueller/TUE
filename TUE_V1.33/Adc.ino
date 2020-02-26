void initAdc()
{
  // init ADC
  adcwert = analogRead(A0);   //aktuellen ADC Wert lesen
  delay(100);                 //100ms warten
  adcmittelwert = (adcwert + analogRead(A0)) / 2;
  delay(100);                 //100ms warten
  Serial.print("ADC Level = ");
  Serial.print(adcwert);
  Serial.print(" ADC Mittelwert = ");
  Serial.println(adcmittelwert);
  // check if there is an ADC
  if (adcmittelwert > adcconnectedio)
  { //ADC Wird verwendet
    Serial.println("ADC verdrahtet wird verwendet");
    ts.add(1, 60000, [&](void*) {
      UBat();
    }, nullptr, true);
    fESPrunning = true;
    if (ts.enable(1))
    {
      Serial.println("TASK 1, UBat() enabled");
    }
    if (adcmittelwert < adcstopgrenzwert)
    {
      fESPrunning = false;
      Serial.print("ADC: ESP ruht");
      LEDStatus = LEDOff;                  //LED aus
      ts.disable(0);
      ts.disable(2);
      ts.disable(3);
    }
  }
  else
  {
    Serial.println("ADC funktioniert nicht, wird nicht verwendet!");
  }
}
