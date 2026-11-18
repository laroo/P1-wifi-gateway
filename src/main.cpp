/*
 * Copyright (c) 2023 Ronald Leenes
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *

 * @file P1WG_IDE_2.ino
 * @author Ronald Leenes
 *
 * @brief This file contains the main file for the P1 wifi gatewway
 *
 * @see http://esp8266thingies.nl

 * P1 wifi gateway 2022
 *
 * Deze software brengt verschillende componenten bij elkaar in een handzaam
pakket.
 *
 * De software draait op een ESP8285 of ESP8266 en heeft een bescheiden
voetafdruk.
 *
 * De data processing is gebaseerd op:
http://domoticx.com/p1-poort-slimme-meter-data-naar-domoticz-esp8266/
 * De captive portal is gebaseerd op een van de ESP8266 voorbeelden, de OTA
eenheid eveneens.
 *
 * De module levert data aan een host via MQTT messages, maar kan ook
via de webinterface van de module of via telnet worden uitgelezen
 *
 * De module zal bij opstarten eerst de buffercondensator laden.
 * Vervolgens knippert hij 2x en zal eerst proberen te koppelen aan je wifi
netwerk. Gedurende dat proces zal de LED knipperen
 * met een periode van 0.5 Hz.
 * Wanneer geen verbinding kan worden gemaakt start de modiule een Access point
onder de naam P1_Setup
 * Verbind met dit netwerk en surf naar 192.168.4.1
 *
 * Het menuscherm van de module verschijnt dan. Vul daar SSID en wachtwoord van
je wifi netwerk in.
 * Daaronder vul je gegevens van de ontvangende server in (MQTT broker IP en
poortnummer).
 * Tenslotte geef je aan per hoeveel seconde je data naar de server wilt laten
sturen.
 * DSMR4 meters hanteren een meetinterval van 10 sec. SMR5 meters leveren iedere
seconde data. Dat lijkt me wat veel en
 * kost de module ook veel stroom waardoor het maar de vraag is of hij
parasitair (door de meter gevoed) kan werken.
 * Ik raad een interval van 15 sec of hoger aan. Het interval dat je hier
invoert zal niet altijd synchroon lopen met
 * het data interval van de meter, ga er dus niet van uit dat je exact iedere x
seconden een meetwaarde ziet.
 * Tussen metingen 'slaapt' de module (de modem wordt afgeschakeld waardoor het
stroomverbruik van zo'n 70mA terugvalt naar 15 mA).
 * Dit geeft de bufferelco tijd omm op te laden voor de stroompiekjes die de
wifi zender van de module produceert
 * (en het bespaart hoe dan ook wat stroom (die weliswaar door je
energieleverancier wordt betaald, maar toch). Alle kleine
 * beetjes helpen..
 *
 *  informatie, vragen, suggesties ed richten aan romix@macuser.nl
 *
 *
 *
 *  versie: 1.2
 *  datum:  26 Nov 2023
 *  auteur: Ronald Leenes
 *

  1.3: clean-up
    1.2: serious overhaul of decoder routine.

    1.1.e: fixed passwd length related issues (adminpass, SSID, MQTT), worked on
wifi persistance, disabled wifi powermanagment
 *  1.1d: moved codebase to IDE 2.2, small fixes, added webdebug

 *  1.1c: clean-up. Telnet can now accept 10 sessions at the same time. Restart
when max is reached.
 *  1.1bf: kludge to fix empty ret value. Somehow the value is overwritten by
something. Relocating defs for actualElectricityPowerDeli and Ret seems to fix
it ofr now
 *  1.1bea: extended field length for actualElectricityPowerDeli[12] and
actualElectricityPowerRet[12];
 *  1.1be: added support for ISKRA /ISK5\2M550E-1011 (which terminates 1-0.2.7.0
and 1-0:22.7.0 without *, using kW instead)
 *  1.1bd: added 3 phase consumption in webdashboard
 *          fixe 3 phase voltage in webdashboard
 *  1.1b    cleaning up, bug fixes, cosmetic changes
 *  1.1adc
 *  1.1ad: bug fixes and graph improvements
 *  1.1aa: bug fixes
 *  1.1: implemented graphs
 *
 *
 *  ud: fixed refreshheader
 *  ua: fixed setup field issue
 *  u:
 *    password on Setup and Firmware Update
 *    made mqtt re-connect non-blocking
 *    incorporated "DSMR Reader" mqtt topics
 *    fixed hardboot daycounters reset
 *    fixed sending empty MQTT messages
 *
 *  t: improvements on powermanagement, overall (minor) cleanup
 *  ta: fix for Telnet reporting
 *
 *  s: added German localisation
 *        Added mqtt output for Swedish specific OBIS codes
 *
 *  r: speed improvements and streamlining of the parser
 *      localisations for: NL, SE
 *
 *  q: added daily values
 *  p: incorporated equipmentID in mqtt set
 *  o: fixed gas output, fixed mqtt reconnect
 *  n: empty call to SetupSave now redirects to main menu instead of resetting
settings ;-)
 *      fixed kWh/Wh inversion
 *  m: setupsave fix, relocate to p1wifi.local na 60 sec
 *      mqtt - kw/W fix
 *  l: wifireconnect after wifi loss
 *  k: fixed big BUG, softAP would not produce accessible webserver.
 *  j: raw data on port 23
 *      major code rewrite
 *      implemented data reporting methods:
 *        parsed data: json, mqtt, p1wifi.local/P1
 *        raw data: telnet on port 23, p1wifi.local/Data
 *
 *  i:  extended max line length for TD210-D meters, which have a really loong
1-0:99.97.0 line
 *  h:  extended mqtt set with instant powers, voltages, actual tarif group.
power outages, power drops
 *  g: fixed mqtt
 *
 *  Generic ESP8285 module
*   Flash Size: 2mb (FS: 128Kb, OTA: –960Kb)
*

*/

#include "Arduino.h"
#include "prototypes.h"
#include <string>
void readTelegram();

bool zapfiles = false;

String version = "1.4 – NL " __DATE__ " " __TIME__;
#define NEDERLANDS
#define HOSTNAME "p1meter"
#define FSystem 0 // 0= LittleFS 1 = SPIFFS

#define GRAPH 1
#define V3
#ifndef DEBUG
#define DEBUG 0 // 2//1//0 1 is on serial only, 2 is telnet,
#endif

#define ESMR5 1

#define OE 16 // IO16 OE on the 74AHCT1G125
#define DR 4  // IO4 is Data Request

#if DEBUG == 1
const char *host = "P1test";
#define BLED LED_BUILTIN
#define PRINTER LOG_PRINTER_SERIAL
#define DataSerial Serial
#elif DEBUG == 2
const char *host = "P1test";
#define BLED LED_BUILTIN
#define PRINTER LOG_PRINTER_TELNET
// #define PRINTER LOG_PRINTER_SERIAL
#include "DSMR5Mock.h"
DSMR5Mock dSMRMock(DR);
#define DataSerial dSMRMock
#elif DEBUG == 3
const char *host = "P1home";
#define BLED LED_BUILTIN
#ifndef DISABLE_LOGGING
#define DISABLE_LOGGING
#endif
#define PRINTER LOG_PRINTER_NULL
#define DataSerial Serial
#else
const char *host = "P1wifi";
#define BLED LED_BUILTIN
#ifndef DISABLE_LOGGING
#define DISABLE_LOGGING
#endif
#define PRINTER LOG_PRINTER_NULL
#define DataSerial Serial
#endif

#define errorHalt(msg)                                                         \
  {                                                                            \
    Log.verboseln(F(msg));                                                     \
    return;                                                                    \
  }

#include "lang.h"
#include "vars.h"
#include "led.h"

#include <ArduinoLog.h>
#include <TZ.h>
#define MYTZ TZ_Europe_Amsterdam
#include "MyAlarm.h"
#include "TimeLib.h"
#include <coredecls.h> // settimeofday_cb()

#include "CRC16.h"
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#if GRAPH == 1
//#include "lfs.h"
#if FSystem == 0
#include <LittleFS.h>
#define FST LittleFS
#elif FSystem == 1
#include "FS.h" //SPIFFS
#define FST SPIFFS
#endif
File file;
#endif

// van captive portal
/*  ============================================================================================================

    Captive Portal based on:
    https://gitlab.com/MrDIYca/code-samples/-/raw/master/esp8266_setup_portal.ino

    For more info, please watch my video at https://youtu.be/1VW-z4ibMXI
    MrDIY.ca
  ==============================================================================================================
*/
#include <EEPROM.h>

#include <ESP8266WebServer.h>
ESP8266WebServer server(80);

WiFiEventHandler stationModeGotIPHandler;
WiFiEventHandler wiFiModeChangeHandler;

#include <ESP8266mDNS.h>
#include <WiFiClient.h>

// mqtt stuff .
// https://github.com/ict-one-nl/P1-Meter-ESP8266-MQTT/blob/master/P1Meter/P1Meter.ino
#include "PubSubClient.h"
WiFiClient espClient;                // * Initiate WIFI client
PubSubClient mqtt_client(espClient); // * Initiate MQTT client

// end mqtt stuff

//// Raw data server
WiFiServer telnet(port);
WiFiClient telnetClients[MAX_SRV_CLIENTS];

ADC_MODE(ADC_VCC); // allows you to monitor the internal VCC level;

#include "WifiManager.h"
WiFiManager *wifiManager;

#include "logger.h"
LogPrinterCreator *logPrinterCreator;
Print *logPrinter;

void setup() {
  ESP.rtcUserMemoryRead(60, &lastPhase, 4);
  ESP.rtcUserMemoryRead(61, &lastPhaseMillis, 4);
  PHASE(1);
  // Configure PINS
  pinMode(OE, OUTPUT);    // IO16 OE on the 74AHCT1G125
  digitalWrite(OE, HIGH); // Put(Keep) OE in Tristate mode
  pinMode(DR, OUTPUT);    // IO4 Data Request
  digitalWrite(DR, LOW);  // DR low (only goes high when we want to receive data)
  pinMode(BLED, OUTPUT);

  // Configure Serial
  Serial.begin(115200);
#ifdef SERIAL_SWAP
  Serial.swap(); // UART0 RX on GPIO13 (Wemos D7) instead of GPIO3
#endif
  Serial.setTimeout(50); // keep readBytesUntil short; data streams continuously
  PHASE(2);

  // Configure logger
  logPrinterCreator = new LogPrinterCreator();
  logPrinter = logPrinterCreator->createLogPrinter();
  Log.begin(LOG_LEVEL_VERBOSE, logPrinter, false);

  Log.infoln("Booting");
  Log.verboseln("Serial.begin(115200);");
  Log.verboseln("Done with Cap charging … ");
  Log.verboseln("Let's go …");
  logPrinterCreator->testPrinter();

  // Wifi stuff
  Log.verboseln("Configuring WiFi");
  initWifiHandlers();

  wifiManager = new WiFiManager();
  wifiManager->setup();

  Log.verboseln("Get FlashSize");
  FlashSize = ESP.getFlashChipRealSize();

  blink(2);

  EEPROM.begin(sizeof(struct settings));
  EEPROM.get(0, config_data);

#ifdef RESET_SETTINGS
  // Factory reset: wipe settings and filesystem once. The 'R' marker in
  // dataSet[2] is never written by the normal save path, so it persists
  // and prevents re-wiping on subsequent boots.
  if (config_data.dataSet[2] != 'R') {
    config_data.dataSet[0] = 'x';
    config_data.dataSet[2] = 'R';
    EEPROM.put(0, config_data);
    EEPROM.commit();
    FST.format();
  }
#endif

  if (config_data.dataSet[0] != 'j') {
    // Preserve dataSet[2]; the RESET_SETTINGS marker lives there and would
    // otherwise be overwritten, causing a wipe on every boot.
    char resetMarker = config_data.dataSet[2];
    config_data = (settings){"n",
                             "orbi",
                             "Moesmate",
                             "sensors/power/p1meter",
                             "192.168.1.123",
                             "1883",
                             "mqtt",
                             "VerySecret",
                             "30",
                             "j",
                             "n",
                             "n",
                             "n",
                             "adminpwd"};
    config_data.dataSet[2] = resetMarker;
  }

  (config_data.watt[0] == 'j') ? reportInDecimals = false
                               : reportInDecimals = true;
  (config_data.mqtt[0] == 'j') ? Mqtt = true : Mqtt = false;
  //(config_data.telnet[0] =='j') ? Telnet = true : Telnet = false;
  Telnet = true;
  (config_data.debug[0] == 'j') ? MQTT_debug = true : MQTT_debug = false;
  if (strcmp(config_data.mqttTopic, "dsmr") ==
      0) { // auto detext need to report in 'dsmr reader' mqtt format
    mqtt_dsmr = true;
    // reportInDecimals = true;
  } else {
    mqtt_dsmr = false;
    // reportInDecimals = false;
  }

  Log.verboseln("EEprom read: done");
  PrintConfigData();

  interval = atoi(config_data.interval) * 1000;
  Log.verboseln("interval: %d", interval);

  PHASE(3);
  wifiManager->connect(config_data.ssid, config_data.password, "P1_Setup");
  PHASE(4);

  if (WiFi.status() == WL_CONNECTED) {
    // handle Files
    Log.verboseln("Mounting file system ... ");
    if (!FST.begin()) {
      Log.verboseln("FST mount failed");
      Log.verboseln("Formatting file system ... ");
      FST.format();
      if (!FST.begin()) {
        Log.verboseln("FST mount failed AGAIN");
      } else {
        Log.verboseln("done.");
        if (zapfiles)
          zapFiles();
      }
    } else
      Log.verboseln("done.");

    Log.verboseln("Reading logdata:");
    File logData = FST.open("/logData.txt", "r");
    if (logData) {
      logData.read((byte *)&log_data, sizeof(log_data) / sizeof(byte));
      logData.close();
    } else {
      Log.verboseln("Failed to open logData.txt for reading");
      needToInitLogVars = true;
      needToInitLogVarsGas = true;
    }
    if (zapfiles)
      zapFiles();

    // Log.verboseln("Something is terribly wrong, no network connection");
    timerAlarm.stopService();
    settimeofday_cb(timeIsSet_cb);
    configTime(MYTZ, "pool.ntp.org");

    Log.verboseln("All systems are go...");
    alignToTelegram();
    PHASE(5);
    nextUpdateTime = nextMQTTreconnectAttempt = EverSoOften = TenSecBeat =
        millis();

    monitoring = true; // start monitoring data
    datagram.reserve(1500);
    statusMsg.reserve(200);
  }                // WL connected
  state = WAITING; // signal that we are waiting for a valid start char (aka /)
}

void readTelegram() {
  if (DataSerial.available()) {
    // Bound work per call: a flooded or corrupt RX line would otherwise
    // block loop() until the watchdog resets.
    for (int lines = 0; lines < 8 && DataSerial.available(); lines++) {
      memset(telegram, 0, sizeof(telegram));
      int len =
          DataSerial.readBytesUntil('\n', telegram, MAXLINELENGTH - 2);
      telegram[len] = '\n';
      telegram[len + 1] = 0;
      rxByteCount += len;
      for (int i = 0; i < len; i++) {
        rxCapture[rxCapPos] = telegram[i];
        rxCapPos = (rxCapPos + 1) % 512;
      }
      ToggleLED

      Log.verbose("STATE: ");
      Log.verbose("%d", state);
      Log.verbose(" >> ");
      for (int i = 0; i < len; i++)
        Log.verbose("%c", telegram[i]);

      decodeLine(len + 1);

      Log.verbose(" << - ");
      Log.verbose("%d\n", state);

      switch (state) {
      case DISABLED: // should not occur
        break;
      case WAITING:
        currentCRC = 0;
        break;
      case READING:
        break;
      case CHECKSUM:
        break;
      case DONE:
        DataSerial.flush();
        RTS_off();
        break;
      case FAILURE:
        Log.verboseln(
            "kicked out of decode loop (invalid CRC or no CRC!)"); // if there
                                                                   // is no
                                                                   // checksum,
                                                                   // (state=Failure
                                                                   // &&
                                                                   // dataEnd)
        RTS_off();
        nextUpdateTime = millis() + interval; // set trigger for next round
        datagram = "";                        // empty datagram and
        telegram[0] = 0; // telegram (probably uncessesary beacuse
                         // DataSerial.ReadBytesUntil will clear telegram buffer)
        break;
      case FAULT:
        Log.verboseln("FAULT");
        statusMsg = ("FAULT in reading data");
        state = WAITING;
        break;
      default:
        break;
      }
    }
    LEDoff
  }
}

void loop() {
  PHASE(9);
  if ((millis() > nextUpdateTime)) { // && monitoring){
    if (!OEstate) {                  // OE is disabled  == false
      DataSerial.flush();
      state = WAITING;
      RTS_on();
    }
  } // update window
  PHASE(10);
  if (OEstate)
    readTelegram();

  if (datagramValid && (state == DONE) && (WiFi.status() == WL_CONNECTED)) {
    if (Mqtt) {
      PHASE(20);
      doMQTT();
      // if (MqttDelivered) {
      //    MqttDelivered = false;  // reset
      // }
    }
    if (Telnet) {
      PHASE(26);
      TelnetReporter();
    }
    if (MQTT_debug) {
      PHASE(27);
      MQTT_Debug();
    }
    datagramValid = false; // reset
    state = WAITING;
  }

  if ((WiFi.getMode() == WIFI_AP) || (WiFi.status() == WL_CONNECTED)) {
    PHASE(30);
    server.handleClient(); // handle web requests
    MDNS.update();
    //      if (Telnet) telnetloop();// telnet.loop();
  }

  if (millis() > FiveSecBeat) {
    if (Telnet && (WiFi.status() == WL_CONNECTED)) {
      PHASE(31);
      telnetloop();
    }
    FiveSecBeat = millis() + 5000;
  }

  if (millis() > EverSoOften) {
    PHASE(40);
    checkCounters();
    PHASE(41);
    resetFlags();
    PHASE(42);
    doWatchDogs();
    EverSoOften = millis() + 22000;
  }

  if ((state == FAILURE) &&
      (dataFailureCount ==
       10)) { // last resort if we get keeping kicked out by bad data
    PHASE(45);
    mqtt_client.disconnect();
    ESP.restart();
  }
  PHASE(50);
  timerAlarm.update();

#if PRINTER == LOG_PRINTER_TELNET
  logPrinterTelnetloop();
#endif
}

#include "MQTT.h"
#include "TELNET.h"
#include "decoder.h"
#include "functions.h"
#include "graph.h"
#include "logging.h"
#include "newMQTT.h"
#include "p1debug.h"
#include "webserver.h"
#include "webserverNL.h"
#include "wifi_functions.h"
