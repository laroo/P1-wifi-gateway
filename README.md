# P1 wifi gateway

Firmware for an ESP8285/ESP8266 module that reads telegrams from the P1 port
of a Dutch smart meter and reports them over MQTT. Values can also be read
via the built-in web interface or raw via telnet (port 23).

This repository is a fork of
[romix123/P1-wifi-gateway](https://github.com/romix123/P1-wifi-gateway) by
Ronald Leenes. Data processing is based on
http://domoticx.com/p1-poort-slimme-meter-data-naar-domoticz-esp8266/;
the captive portal and OTA parts are based on ESP8266 examples.
See http://esp8266thingies.nl for the original hardware and documentation.

## What's different in 1.4

Bug fixes and hardening on top of the upstream 1.1e codebase:

- Fixed MQTT watchdog resets; `send_metric` recursed infinitely on the
  first metric of every report, crashing ~8s after boot whenever MQTT was
  enabled.
- Startup crash (`LoadProhibited` in `Print::write`) fixed by making
  `DISABLE_LOGGING` a global build flag.
- UART0 RX moved to GPIO13 (Wemos pin D7) via `Serial.swap()`; the stock
  GPIO3/RX input is unreliable on this board.
- Serial input bounded; a continuous or corrupt RX stream no longer
  starves the watchdog.
- MQTT reconnect is non-blocking and rate-limited; an unreachable broker
  no longer freezes the web UI.
- Domoticz integration removed; MQTT only.
- New `/Rx` diagnostics page; shows decode state, byte count, CRC
  failures, last reset reason and a hex/ASCII dump of recent RX bytes.
- Version footer shows firmware build date and time.
- Factory-reset firmware environment (`esp8285_factoryreset`) wipes
  EEPROM and LittleFS once on boot.
- Native unit tests for CRC and the DSMR decoder (`pio test -e test`).

## Setup

- On first boot the module starts AP `P1_Setup`; connect and open
  192.168.4.1.
- Enter WiFi credentials, MQTT broker IP/port and credentials, and the
  reporting interval.
- Default admin password: `adminpwd` (change it via the Setup page).
- Home Assistant "DSMR Reader" integration: use `dsmr` as MQTT prefix.

## Web endpoints

- `/` main menu, `/P1` live meter values
- `/Setup` configuration, `/update` OTA firmware upload
- `/Data` raw datagram, `/Rx` UART diagnostics

## Hardware

- Wemos D1 mini (ESP8266) or generic ESP8285 module; 2 MB flash layout
  (`eagle.flash.2m128.ld`).
- P1 front-end: meter TX through ~4k7 to a BC547 base; collector to the
  Wemos **D7** pin with a ~10k pull-up to 3V3; emitter to GND; grounds
  shared. RTS tied high makes the meter stream continuously.
- Note: this firmware expects the data wire on **D7 (GPIO13)**, not the
  RX pin, because `Serial.swap()` is enabled.

## Building

PlatformIO project; `pio` lives in `~/.platformio/penv/bin/`.

- `pio run -e esp8285` normal firmware (OTA at `/update` or esptool).
- `pio run -e esp8285_factoryreset` one-time EEPROM/LittleFS wipe image.
- `pio test -e test` native Unity tests.
