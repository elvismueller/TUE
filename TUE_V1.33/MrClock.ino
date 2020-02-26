void checkForMrClockTelegramm()
{
  // If a MRCLOCK Telegramm is recieved:
  if (int packetsize = wifiUDPMRC.parsePacket())
  {
    dCounterToNTP = WaittimeNTPSync;           // Counter updaten
    Serial.print("@.");
    // We've received a packet, read the data from it
    wifiUDPMRC.read(packetBuffer, packetsize); // read the packet into the buffer
    //Search for Message like "clock=10:32:35"
    for (int i = 0; i < sizeof(packetBuffer); i++)
    {
      if ( packetBuffer[i] == 'c' &&
           packetBuffer[i + 1] == 'l' &&
           packetBuffer[i + 2] == 'o' &&
           packetBuffer[i + 3] == 'c' &&
           packetBuffer[i + 4] == 'k' &&
           packetBuffer[i + 5] == '='
         )
      {
        c_time[0] = packetBuffer[i + 6];
        c_time[1] = packetBuffer[i + 7];
        c_time[2] = packetBuffer[i + 8];
        c_time[3] = packetBuffer[i + 9];
        c_time[4] = packetBuffer[i + 10];
        c_time[5] = packetBuffer[i + 11];
        c_time[6] = packetBuffer[i + 12];
        c_time[7] = packetBuffer[i + 13];
        c_time[8] = packetBuffer[i + 14];
        // Time Char Array zerlegen
        bool clear_h = true;
        bool clear_m = false;
        bool clear_s = false;

        for (int ii = 0; ii < 15; ii++)
        {
          if ( !clear_h && !clear_m && clear_s)
          {
            if ( c_time[ii] == 0)
            {
              clear_m = false;
              clear_s = false;
              c_clock_hour[ii] = ' ';
              c_clock_minute[ii] = ' ';
              c_clock_sek[ii] = ' ';
            }
            else
            {
              c_clock_hour[ii] = ' ';
              c_clock_minute[ii] = ' ';
              c_clock_sek[ii] = c_time[ii];
            }
          }
          if ( !clear_h && clear_m && !clear_s)
          {
            if ( c_time[ii] == ':')
            {
              clear_m = false;
              clear_s = true;
              c_clock_hour[ii] = ' ';
              c_clock_minute[ii] = ' ';
              c_clock_sek[ii] = ' ';
            }
            else
            {
              c_clock_hour[ii] = ' ';
              c_clock_minute[ii] = c_time[ii];
              c_clock_sek[ii] = ' ';
            }
          }
          if ( clear_h && !clear_m && !clear_s)
          {
            if ( c_time[ii] == ':')
            {
              clear_h = false;
              clear_m = true;
              c_clock_hour[ii] = ' ';
              c_clock_minute[ii] = ' ';
              c_clock_sek[ii] = ' ';
            }
            else
            {
              c_clock_hour[ii] = c_time[ii];
              c_clock_minute[ii] = ' ';
              c_clock_sek[ii] = ' ';
            }
          }
        }
        clock_h = atoi(c_clock_hour);
        clock_m = atoi(c_clock_minute);
        clock_s = atoi(c_clock_sek);
        Serial.print("Empfangenes Zeitzeichen: ");
        Serial.print(clock_h);
        Serial.print(":");
        Serial.print(clock_m);
        Serial.print(":");
        Serial.print(clock_s);
        Serial.print("; ");
      }
    }
    //Search for Message like "speed=5"
    for (int i = 0; i < sizeof(packetBuffer); i++)
    {
      if ( packetBuffer[i] == 's' &&
           packetBuffer[i + 1] == 'p' &&
           packetBuffer[i + 2] == 'e' &&
           packetBuffer[i + 3] == 'e' &&
           packetBuffer[i + 4] == 'd' &&
           packetBuffer[i + 5] == '='
         )
      {
        if (packetBuffer[i + 6] != 32)
        {
          c_speed[0] = packetBuffer[i + 6];
        }
        if (packetBuffer[i + 7] != 32) {
          c_speed[1] = packetBuffer[i + 7];
        }
        if (packetBuffer[i + 8] != 32) {
          c_speed[2] = packetBuffer[i + 8];
        }
        if (packetBuffer[i + 9] != 32) {
          c_speed[3] = packetBuffer[i + 9];
        }
        c_speed[4] == NULL;
        Serial.print("Speed = ");
        clock_speed = atoi(c_speed);
        Serial.print(clock_speed);
      }
    }
    // recieving one message!
    Serial.println("; Ende der Message");
    flag_message_recieved = true;
  }
}
