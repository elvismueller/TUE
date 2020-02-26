//Lauffähig ab LP Stand 0.51
//Ein-taster auf GPIO16-RST und Ausfunktion am Config Schalter
//hoe 10.03.17
//hoe 1.6.17 V1.2 Mit NTP Funktion, sobald ein ntp server eingetragen wurde, wird versucht die aktuelle Zeit mit NTP abzufragen und die Uhr danach zu stellen.
//           wird jedoch ein MRCLOCK Telegramm empfangen, wird der NTP Empfang um die Wartezeit WaittimeNTPSync unterdrückt, MRCLOCK Empfang hat vorrang vor NTP!
//           als Zeitzone wird standardmässig MEZ mit Sommerzeit eingestellt.
//hoe 15.8.17 V1.3 Abgabeversion mit Entladespannung des Akkus bei 3.1V; Start nachdem Akku 3.6V erreicht hat. Systemwechselzeit MRClock -> NTP = 24h
//hoe 30.12.17 V1.3.1 kleinere Korrekturen (Rechtschreibung)
//hoe 10.03.18 V1.3.2 längere Taktpause beim Stellen eingebaut, damit auch grössere Uhren nachkommen. (Nach test vom Treffen Mammendorf 2017) war Taktpause war 500ms, jetzt 1000ms
//emm 21.11.19 V1.3.3 Update für ArdunioJson v6 https://arduinojson.org/v6/doc/upgrade/
//                    Fix Aufrufe für Tasks https://www.bountysource.com/issues/45704564-build-error-no-matching-function-call
//                    Ticker.h benötigt einen kleinen Fix in der Zeile 74: stumpf reinterpret_cast -> (void*)
//                    cleanup, Aufteilung in Tabs
//                    Geräteseite (Webserver) hinzugefügt

#define VERSION "V1.3.3"

#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <TickerScheduler.h>      //https://github.com/Toshik/TickerScheduler
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266WebServer.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
//needed for NTP
#include <TimeLib.h>              //https://github.com/PaulStoffregen/Time
#include <WiFiUdp.h>
#include <Timezone.h>             //https://github.com/JChristensen/Timezone

// debugging
bool showWaitingInLog = false;
bool showTimeInLog = false;
bool clearConfigOnInit = false;

// Defines for Pins
#define CHARGEPUMPENABLE 14       //SPI SCK = GPIO #14 (Charge Pump enable)
#define TAKTA            13       //SPI MOSI = GPIO #13 (TaktA)
#define TAKTB            12       //SPI MISO = GPIO #12 (TaktB)
#define CONFIGPB          5       //GPIO #5 (Input für Config Pushbutton) Bei MRC_REC = 16!
#define BLED              2       //GPIO #2 = blue LED on ESP
#define INT               4       //GPIO #4 = Interrupt für Spannungseinbruch -> Interrupt wird an der fallenden Flanke ausgelöst

// defines für Zeiten
#define TAKTTIME          10      //Wie lange wird der Stelltakt ausgegeben? 1 Einheit entspricht 10 ms Taktausgabezeit
#define TAKTWAITIME       100     //So lange darf kein Takt ausgegeben werden     
#define CHARGEPUMPTIME    2       //So lange wird auf die Chargepump gewartet
#define WaittimeNTPSync   1440000 //Zeit in ms bis von MRCLOCK Telegrammempfang auf NTP umgeschalten wird. 1440000 entspricht 24h
#define WaittimeNTP1Sync  36000   //Zeit in 10ms. Uhr stellt sich nach der Configroutine auf NTP um, sofern kein MRCLOCK Telegramm empfangen wurde. 36000 entsprechen 6 Minuten

// defines für den LED Status
#define LEDOff            0       //LED is off
#define LEDBlinking       1       //Flashing LED (Saving Time to Flash)
#define LEDAlive          2       //TUE is alive
#define LEDConfig         3       //TUE is in Config mode
#define LEDOn             5       //LED is on
#define LEDBlinkOnce      6       //LED macht einen Blinker
#define LEDAusVorbereitet 7       //LED macht langsamen Blinkrythmus

int LEDStatus = 0;                //LED Status wird durch das Programm gemäss den oben angegebenen Defines gesetzt und durch die Task LED() wird die LED angesteuert
int LEDStatusIntern = 0;          //LED Status intern für die LED Routine
int LEDStatusCounter = 0;         //LED Status Counter zum zählen bis zum nächsten Statuswechsel
int LEDPeriodCounter = 0;         //Dient als Zähler für das Aus vorbereitet....

#define LEDalivetime      1
#define LEDalivecycle     1000    //LED alive = jede 10s ein kurzer blauer blitz
#define LEDblinkingcycle  5
#define LEDAusVorbtime    2
#define LEDAusVorbcycle   5
#define LEDAusVorbperiod  1000

// define your default values here, if there are different values in config.json, they are overwritten.
char mrc_multicast[40] = "239.50.50.20";
char mrc_port[6] = "2000";

// char array for the protokollhandler
char c_clock_hour[15] = "03";
char c_clock_minute[15] = "02";
char c_clock_sek[15] = "04";
char c_time[15] = "0";
char c_speed[15] = "0";

// Strings
String esp_chipid;

// ADC Daten
int  adcwert;                   //Aktueller ADC Wert
int  adcmittelwert;             //Mittelwert
bool fESPrunning = true;        //Flag spiegelt den Zustand des ESP wieder true bedeutet genügend VCC, false = ESP ruht
bool fDataSaving = false;       //Die Daten müssen ins Flash gespeichert werden, der ESP hat zuwenig Spannung

#define adcstopgrenzwert  440         //ab diesem ADC Wert schaltet der ESP ab und zirkuliert in einer Warteschleife bis adc > adcstart 440 entspricht ca. 3.12 V
#define adcstartgrenzwert 499         //ab diesem ADC Wert schaltet der ESP wieder ein
#define adcconnectedio    300         //falls der adc beim Starten mehr als diesen Wert liest, gilt der adc als verdrahtet und wird benutzt

// Daten aus dem Protokoll
int  clock_h = 0;   // Sollzeit Stunde
int  clock_m = 0;   // Sollzeit Minute
int  clock_s = 0;   // Sollzeit Sekunde
int  clock_speed = 1; //

int  tochter_h = 0; // Istzeit auf der 12h Tochteruhr (Stunden)
int  tochter_m = 0; // Istzeit auf der 12h Tochteruhr (Minuten)

//NTP Variablen
char ntpserver[40] = "ntp.metas.ch";         // NTPServer
unsigned int localPort = 8888;               // local port to listen for UDP packets
const int timeZone = 0;                      // UTC
double dCounterToNTP;                        // if no MRCLOCK telegramm is recieved, the conter gets decressed, on ZERO the NTP time is displayed

// Timezone
// Central European Time (Frankfurt, Paris)
TimeChangeRule CEST = { "CEST", Last, Sun, Mar, 2, 120 };     // Central European Summer Time
TimeChangeRule CET = { "CET ", Last, Sun, Oct, 3, 60 };       // Central European Standard Time
Timezone CE(CEST, CET);
TimeChangeRule *tcr;        // pointer to the time change rule, use to get the TZ abbrev
time_t utc, local;
time_t prevDisplay = 0;     // when the digital clock was displayed

// Statemaschine für TochterUhrStellen Routine
int  SM = 0;        // Statemaschine Status
int  SMC = 0;       // Statemaschine Counter
bool FlagTUStellen = false;          // Flag ist gesetzt, falls die Tochteruhr einen Tick machen soll
bool FlagTUStellglied;               // Flag wiederspiegelt den Ausgang des TU Stellglieds

// Rücksetztaste
bool FlagButtonPressed = false;
int  CounterButtonPressed;

// Tickerscheduler Objekt
TickerScheduler ts(6);

// flag for saving data
bool shouldSaveConfig = false;

// flag for sleep mode
bool sleep = false;
bool flag_message_recieved = true;

// WifiUDP Class
WiFiUDP wifiUDPMRC;    //für MRClock Telegramme
WiFiUDP wifiUDPNTP;    //für NTP Telegramme

const int MRC_PACKET_SIZE = 2024; // NTP time stamp is in the first 48 bytes of the message
char packetBuffer[ MRC_PACKET_SIZE ]; //buffer to hold incoming and outgoing packets
int nLength = 0;
int time_hour = 0;
int time_minute = 0;

//WiFiManager
//Das Object wird auch noch im LOOP benötigt um die Config zu löschen....
WiFiManager wifiManager;
//Die beiden Parameter werden durch ein Callback verändert, deswegen müssen sie Global definiert sein
WiFiManagerParameter custom_time_hour();
WiFiManagerParameter custom_time_minute();

//callback notifying us of the need to save config
void saveConfigCallback(void)
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// CALLBACK wird mit dem Starten der Configurationsseite aufgerufen und stellt die Uhr um zwei Ticks weiter, damit sie mit der internen Uhr synchron läuft
void APCallback(WiFiManager *myWiFiManager)
{
  Serial.println("[APCallback()] forward two ticks!");
  forcetick();
  while (FlagTUStellen)
  {
    yield();
    ts.update();
    if (showWaitingInLog) Serial.print(".");
    delay(10);
  }
  forcetick();
  while (FlagTUStellen)
  {
    yield();
    ts.update();
    if (showWaitingInLog) Serial.print(".");
    delay(10);
  }
  WiFiManagerParameter custom_time_hour("clock_hour", "zulaessige Werte (00-11)", c_clock_hour, 15);
  WiFiManagerParameter custom_time_minute("clock_minute", "zulaessige Werte (00-59)", c_clock_minute, 15);
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();
  LED(LEDOff);               // Am Anfang ist die LED aus....

  //ChipID?
  esp_chipid = String(ESP.getChipId());
  //Identify
  Serial.print("Tochteruhrempfänger TUE ");
  Serial.print(VERSION);
  Serial.print(" ESP - ChipID = ");
  Serial.println(esp_chipid);
  //WiFi.setAutoReconnect(false);
  WiFi.persistent(true); // Wird gebraucht damit das Flash nicht zu oft beschrieben wird.
  WiFi.setAutoReconnect(true);
  WiFi.hostname(String("TUE" + esp_chipid));

  Serial.println("Configuring Pins and Interrupts");
  //Pins konfigurieren
  //SPI SCK = GPIO #14 (Charge Pump enable)
  //SPI MOSI = GPIO #13 (TaktA)
  //SPI MISO = GPIO #12 (TaktB)
  //GPIO #5 (Input für Config Pushbutton)
  //GPIO #16 for Enable and Battery Saving :-)
  //GPIO #2 = blue LED on ESP
  //GPIO #9 = Überwachung der Versorgungsspannung
  pinMode(TAKTA, OUTPUT);
  pinMode(TAKTB, OUTPUT);
  pinMode(CHARGEPUMPENABLE, OUTPUT);
  pinMode(CONFIGPB, INPUT);
  pinMode(BLED, OUTPUT);
  digitalWrite(CHARGEPUMPENABLE, HIGH);
  digitalWrite(BLED, HIGH);

  // start LED-Task
  ts.add(4, 10, [&](void*) { LEDTS(); }, nullptr, true);
  
  LED(LEDBlinkOnce);         // LED einen Flash blinken -> 1. Blinkimpuls
  
  initFileSystem();
  
  LED(LEDBlinkOnce);     // LED einen Flash blinken  -> 2. Blinkimpuls

  mountFileSystemAndReadConfig();
  syncInternalTimeWithTochteruhr();

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_text_info1("<br>Einstellmen&uuml; f&uuml;r den Tochteruhrempf&auml;nger.<br>");
  WiFiManagerParameter custom_text_info2("Empf&auml;ngt Zeitzeichentelegramme nach dem MRClock Protokoll.<br><br>");
  WiFiManagerParameter custom_text_info3("Standarteinstellung MRClock: Multicast 239.50.50.20 auf Port 2000.<br>Bitte nur die Stellung der Nebenuhr anpassen. Die anderen Einstellungen m&uuml;ssen meistens nicht ge&auml;ndert werden.<br>");
  WiFiManagerParameter custom_text_h("Stand der Nebenuhr, hier Stunden (ganze Zahl von 0 bis 11):");
  WiFiManagerParameter custom_text_m("Stand der Nebenuhr, hier Minuten (ganze Zahl von 0 bis 59):");
  WiFiManagerParameter custom_text_expert("<br>Ab hier Experteneinstellungen, nur &auml;ndern wenn wann weiss was man macht!<br>");
  WiFiManagerParameter custom_text_multicast_adress("Empfangsadresse der MRClock Telegramme:");
  WiFiManagerParameter custom_mrc_multicast("mrc_multicast", "mrc_multicast (239.50.50.20)", mrc_multicast, 40);
  WiFiManagerParameter custom_text_multicast_port("Empfangsport der MRClock Telegramme:");
  WiFiManagerParameter custom_mrc_port("mrc_port", "mrc_multicast_port (2000)", mrc_port, 5);
  WiFiManagerParameter custom_text_ntpserver("<br>Zeitserver zum Synchronisieren auf die MEZ:");
  WiFiManagerParameter custom_ntp_server("NTP_Server", "ntp.server.de", ntpserver, 40);
  WiFiManagerParameter custom_time_hour("clock_hour", "zul&auml;ssige Werte (00-11)", c_clock_hour, 15);
  WiFiManagerParameter custom_time_minute("clock_minute", "zul&auml;ssige Werte (00-59)", c_clock_minute, 15);
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  //set APCallback
  wifiManager.setAPCallback(APCallback);
  //set static ip, otherwise it will use dhcp...
  // wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  //add all your parameters here
  wifiManager.addParameter(&custom_text_info1);
  wifiManager.addParameter(&custom_text_info2);
  wifiManager.addParameter(&custom_text_info3);
  wifiManager.addParameter(&custom_text_h);
  wifiManager.addParameter(&custom_time_hour);
  wifiManager.addParameter(&custom_text_m);
  wifiManager.addParameter(&custom_time_minute);
  wifiManager.addParameter(&custom_text_expert);
  wifiManager.addParameter(&custom_text_ntpserver);
  wifiManager.addParameter(&custom_ntp_server);
  wifiManager.addParameter(&custom_text_multicast_adress);
  wifiManager.addParameter(&custom_mrc_multicast);
  wifiManager.addParameter(&custom_text_multicast_port);
  wifiManager.addParameter(&custom_mrc_port);
  //reset settings - durch das löschen der Daten, haben wir die Möglichkeit die Uhrzeit bei jedem Start neu einstellen zu können
  // wifiManager.resetSettings();
  //set minimum quality of signal so it ignores AP's under that quality defaults to 8%
  // wifiManager.setMinimumSignalQuality();
  //sets timeout until configuration portal gets turned off (useful to make it all retry or go to sleep), in seconds
  wifiManager.setTimeout(600);

  LED(LEDBlinkOnce);         // LED einen Flash blinken    -> 3. Blinkimpuls
  LED(LEDOn);                // Ab jetzt ist die LED an, falls das CAPTIVE PORTAL gestartet ist
  // Ansonsten 4. Blinkimpuls (lang)

  // Setze Istzeit = Sollzeit, sonst läuft die Uhr schon nach dem Setzen los auf irgendeine Zeit...
  clock_h = tochter_h;
  clock_m = tochter_m;

  initAdc();
  
  // starte Tasks
  ts.add(0, 10, [&](void*) { tick(); }, nullptr, true);
  ts.add(2, 10, [&](void*) { TochterUhrStellen(); }, nullptr, true);
  if (showTimeInLog)
  { // reine Anzeige interner Werte, daher nur für debugging
    ts.add(3, 1000, [&](void*) { TochterUhr(); }, nullptr, true);
  }

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration //"AutoConnectAP", "password1234"
  if (!wifiManager.autoConnect(String("TUE" + esp_chipid).c_str()))
  {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    //ESP.reset();
    //wifi_set_sleep_type(LIGHT_SLEEP_T);
    system_deep_sleep(1000000);
    //ESP.deepSleep(1000000);
    delay(100);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(ntpserver, custom_ntp_server.getValue());
  strcpy(mrc_multicast, custom_mrc_multicast.getValue());
  strcpy(mrc_port, custom_mrc_port.getValue());
  strcpy(c_clock_hour, custom_time_hour.getValue());
  strcpy(c_clock_minute, custom_time_minute.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig)
  {
    Serial.println("saving config");
    tochter_h = atoi(c_clock_hour);
    tochter_m = atoi(c_clock_minute);
    //Parameter überprüfen
    if (tochter_h < 0 || tochter_h > 23)
    {
      tochter_h = 0;
    }
    if (tochter_h >= 12)
    {
      tochter_h = tochter_h - 12;
    }
    if (tochter_m < 0 || tochter_h > 59)
    {
      tochter_m = 0;
    }
    // Setze Sollzeit = Istzeit, sonst läuft die Uhr schon nach dem Setzen los auf irgendeine Zeit...
    clock_h = tochter_h;
    clock_m = tochter_m;
    Serial.print("Uhrzeit der Tochteruhr=");
    Serial.print(tochter_h);
    Serial.print(":");
    Serial.println(tochter_m);
    DataSaving();
  }

  LED(LEDOff);           // LED aus machen

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.SSID());
  Serial.println(WiFi.hostname());
  //saving credentials only on change
  // WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  // Starting WIFI in Station Mode
  Serial.println("Starting WIFI in Station Mode, Listening to UDP Multicast");
  wifiUDPMRC;
  wifiUDPNTP;
  WiFi.mode(WIFI_STA);
  WiFi.printDiag(Serial);
  wifiUDPMRC.beginMulticast(WiFi.localIP(), IPAddress(239, 50, 50, 20), 2000);
  wifiUDPNTP.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(wifiUDPNTP.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
  delay(5000);

  dCounterToNTP = WaittimeNTP1Sync;        // Wie lange nach dem Aufstarten warten, bis die NTP Zeit übernommen wird?

  LED(LEDBlinkOnce);         // LED einen Flash blinken    -> 4. Blinkimpuls

  setupWebServer();

  LED(LEDAlive);             // Ab jetzt zeigt die LED das Leben des Empfängers an..
}

void loop()
{
  // loop checks for:
  // - MRCLOCK Telegramm, or
  // - NTP

  // MRCLOCK has priority to NTP telegramms, if there were no MRCLOCK Telegramm for WaittimeNTPSync [seconds] then, the
  // Slaveclock is syncronised to NTP Clock until the first MRCLOCK telegramm comes in...
  checkForMrClockTelegramm();
  
  // Ist der Taster gedrückt?
  checkConfigButton();
  
  // Haben wir noch Saft?
  checkSaveOnLowPower();

  // Läuft die CPU noch?
  checkEspRunning();

  // Soll auf NTP umgeschalten werden?
  checkForSwitchToNtp();

  //Tasks bedienen
  yield();
  ts.update();
  if (showWaitingInLog) Serial.print(".");
  delay(10);

  loopWebServer();
}
