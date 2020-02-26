void checkForSwitchToNtp()
{
  //Soll auf NTP-Zeitzeichenempfang umgeschaltet werden?
  if (dCounterToNTP == 0)
  {
    //jetzt wird die NTP Zeit an der Tochteruhr angezeigt...
    if (timeStatus() != timeNotSet)
    {
      if (now() != prevDisplay)
      { //update the display only if time has changed
        prevDisplay = now();
        local = CE.toLocal(now(), &tcr);
        Serial.print("Hour=");
        Serial.print(hour(local), DEC);
        Serial.print("Minute=");
        Serial.print(minute(local), DEC);
        Serial.print("Seconds=");
        Serial.println(second(local), DEC);
        clock_h = hour(local);
        clock_m = minute(local);
        clock_s = second(local);
      }
    }
  }
  else
  {
    dCounterToNTP--;
  }
}

/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBufferNTP[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address
  while (wifiUDPNTP.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpserver, ntpServerIP);
  Serial.print(ntpserver);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = wifiUDPNTP.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      wifiUDPNTP.read(packetBufferNTP, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBufferNTP[40] << 24;
      secsSince1900 |= (unsigned long)packetBufferNTP[41] << 16;
      secsSince1900 |= (unsigned long)packetBufferNTP[42] << 8;
      secsSince1900 |= (unsigned long)packetBufferNTP[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBufferNTP, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBufferNTP[0] = 0b11100011;   // LI, Version, Mode
  packetBufferNTP[1] = 0;     // Stratum, or type of clock
  packetBufferNTP[2] = 6;     // Polling Interval
  packetBufferNTP[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBufferNTP[12] = 49;
  packetBufferNTP[13] = 0x4E;
  packetBufferNTP[14] = 49;
  packetBufferNTP[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  wifiUDPNTP.beginPacket(address, 123); //NTP requests are to port 123
  wifiUDPNTP.write(packetBufferNTP, NTP_PACKET_SIZE);
  wifiUDPNTP.endPacket();
}
