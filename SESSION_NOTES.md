# Session notes: P1 wifi gateway firmware work

## Where we are

- Device: Wemos D1 mini (ESP8266EX, MAC 84:f3:eb:b4:07:a3) with a bare
  BC547B P1 front-end (esphome-p1reader style), powered by USB charger.
- **WORKING end-to-end**: /P1 shows live Landis+Gyr values (tariffs, 3-phase
  voltage, power, gas), 0 CRC failures, MQTT enabled and reporting; device
  stable for minutes with MQTT on (previously WDT ~11s after each boot).
- P1 wiring: meter TX -> 4k7 -> Q1 base; Q1 collector -> Wemos **D7 (GPIO13)**
  + 10k pull-up to 3V3; emitter -> GND; meter GND shared; **meter RTS tied to
  Wemos 5V** (continuous streaming, board does not drive DR).
- GPIO3 (Wemos RX pin) input is DEAD; data wire must stay on D7.

## Code changes made (uncommitted)

- `platformio.ini`
  - `env:esp8285`: added `-DDISABLE_LOGGING` and `-DSERIAL_SWAP` to
    build_flags.
  - New env `esp8285_factoryreset`: same as esp8285 plus `-DRESET_SETTINGS`
    (build_flags listed explicitly; `extends` does not append lists).
- `src/main.cpp`
  - Guarded `#define DISABLE_LOGGING` with `#ifndef` (two places).
  - `readTelegram()`: bounded to 8 lines per call (was unbounded
    `while(available)`; a flooded/corrupt RX line starved loop() -> WDT
    resets, `rst cause:4`). `Serial.setTimeout(50)` so readBytesUntil can't
    stall a second per line. readBytesUntil capped at MAXLINELENGTH-2;
    `telegram[len]='\n'` could previously write past the buffer.
  - `Serial.swap()` after `Serial.begin` under `#ifdef SERIAL_SWAP`: UART0 RX
    moves to GPIO13 (Wemos D7). Required: the onboard GPIO3 input is dead.
  - `readTelegram()` also feeds the `/Rx` diagnostic tap: `rxByteCount` plus a
    512-byte ring buffer `rxCapture` in `src/vars.h`.
  - Defaults load preserves `dataSet[2]`; the RESET_SETTINGS 'R' marker
    would otherwise be wiped from RAM and cause an infinite factory-reset
    loop (settings could never survive a reboot).
  - Added `RESET_SETTINGS` block after `EEPROM.get()`: if
    `config_data.dataSet[2] != 'R'`, forces factory defaults, writes marker
    `'R'`, commits EEPROM, calls `FST.format()`. Marker survives normal saves
    so the wipe only happens once.
- `src/MQTT.h`
  - `mqtt_connect()` rewritten: non-blocking rate-limited version. Was: up to
    3 blocking `mqtt_client.connect()` calls + 1s delays per telegram (a dead
    broker froze the loop ~45s+ per cycle, making the UI unusable). Now: at
    most one attempt, retry every 30s via `nextMQTTreconnectAttempt`.
  - `send_metric` overloads take `const String&` (was by-value copies).
  - **WDT root cause fixed**: the `const char*` overload was defined below
    `send_metric(String,String)`, which called `send_metric(name,
    metric.c_str())`. With no `const char*` overload visible at that point,
    the `const char*` arg converted to `String` and the function called
    itself: infinite recursion -> stack exhaustion -> hardware watchdog
    ~8s later, every first metric of every report. Fixed by defining the
    `const char*` overload first. Latent in the original code; only showed
    once MQTT was enabled because `equipmentID` is the first metric sent.
  - `mqtt_client.loop()` added at end of `doMQTT` (was never called).
  - Buffer overflows fixed: `char value[6]`/`sprintf %u` -> [12];
    `MQTT_Debug` `char outstr[10]`/`dtostrf width 10` -> [16];
    `doWatchDogs` `char payload[60]` too small for msg+timestamp -> larger.
  - Diagnosed via `PHASE(n)` macro (`src/vars.h`): writes a marker + millis
    to RTC user memory (survives WDT); `/Rx` shows `lastPhase` + timestamp.
- `src/wifi_functions.h`
  - `mqtt_client.setSocketTimeout(5)` after setServer, caps each blocking
    connect at ~5s.
  - Registered `/Rx` -> `handleRxDiag`: shows decode state, OEstate,
    rxByteCount, dataFailureCount and a hex+ASCII dump of the last 512 raw
    UART bytes. Auto-refreshes every 10s.
- `src/webserver.h`
  - `addHead()` gained an optional `refreshUrl` param (default "/"); the meta
    refresh URL was hardcoded to `/` so `/P1` and `/Rx` bounced to home.
    Both now pass their own path.
- Domoticz integration fully removed:
  - `src/vars.h`: dropped `domoticzIP`, `domoticzPort`, `domoticzEnergyIdx`,
    `domoticzGasIdx`, `domo`, `Json`, `LastJReport`, `prevGAS`. Settings
    struct now has 14 fields: dataSet, ssid, password, mqttTopic, mqttIP,
    mqttPort, mqttUser, mqttPass, interval, mqtt, watt, telnet, debug,
    adminPassword.
  - `src/main.cpp`: defaults initializer updated to 14 values; removed the
    `domo`->`Json` parse, the `if (Json) doJSON()` loop call and the
    `JSON.h` include.
  - `src/webserverNL.h`: removed the Domoticz fieldset from the setup form.
  - `src/webserver.h`: removed domo/domoticz* parsing; rewrote the chained
    null-terminator block as plain assignments.
  - `src/prototypes.h`, `src/p1debug.h`: removed Domoticz prototypes and
    debug prints. `send_metric` prototype updated to `const String&`.
  - `src/MQTT.h`: removed the `LastJReport` debug publish from `MQTT_Debug`.
  - `src/JSON.h`: deleted.
  - `src/lang.h`: help text updated; no more Domoticz instructions.
- `lib/PubSubClient/`: vendored copy (was registry dep) with `yield()`
  added to the CONNACK wait loop in `connect()`; it spun up to
  socketTimeout without yielding.

## The original crash (fixed)

- Exception 28 `LoadProhibited, excvaddr=0` in `Print::write(char)` from
  `Logging::printLevel` during `setup`.
- Cause: `DISABLE_LOGGING` was only defined in main.cpp's translation unit.
  `WifiManager.cpp` was compiled without it, so `Log.traceln()` in
  `WiFiManager::setup()` dereferenced the null `_logOutput` (PRINTER =
  LOG_PRINTER_NULL in the esp8285 env).
- Fix: `-DDISABLE_LOGGING` as a global build flag. Verified: no printLevel
  code remains in the esp8285 build.

## Tests

- Run with `source .venv/bin/activate && pio test -e test`.
- `env:test` is native: Unity + ArduinoFake (provides Arduino `String` and
  `millis`); `test_build_src = no` so no firmware sources are compiled.
- `test/support/WString.h` routes `<WString.h>` to ArduinoFake's copy.
- `test/test_crc/` covers CRC16 (DSMR P1 CRC-16/ARC) and CRC32.
- `test/test_decoder/` feeds a real DSMR5 telegram line by line through
  `decodeLine` from `src/decoder.h` and checks state transitions plus
  parsed `dsmrData` fields; also covers bad CRC and garbage recovery.
  Shims in the test file replace `Log`, `identifyMeter`, `FindCharInArray`.
- Wokwi is not an option: it does not support ESP8266.
- On-device alternative exists: `env:espmxdevkit` (DEBUG=2) uses
  `DSMR5Mock` as `DataSerial`, so the firmware feeds itself telegrams.

## Hardware findings

- ESP boots fine: ROM banner at **74880 baud**. App firmware runs at 115200.
- Boot modes: `(3,7)`/`(3,6)` = normal flash boot, `(1,x)` = UART download.
- Flashing is unreliable with the P1 front-end transistor connected to RX:
  - Meter connected: sync works, but data transfer fails (`result was 0107`
    or packet timeouts) because telegrams collide.
  - Meter disconnected: the unpowered transistor clamps RX; sync fails
    completely.
- Meter is a Landis+Gyr E360, SMR 5.0 (115200 8N1, CRC16).
- **GPIO3 (UART0 RX / Wemos "RX" pin) input is dead.** Proven by: clean
  test bytes sent at 115200 through the onboard CH340 (meter unplugged,
  transistor idle) produced zero bytes at the UART (`/Rx` stayed empty),
  while the pin itself toggled measurably when meter data was present.
  Likely damaged during the contention episodes when the transistor and
  CH340 drove the pin simultaneously. UART TX (GPIO1) still works.
- Fix: `Serial.swap()` moves UART0 RX to GPIO13 (Wemos D7); the transistor
  output wire was moved from RX to D7. Full telegrams decoded immediately,
  0 CRC failures. `-DSERIAL_SWAP` is now permanent in `env:esp8285`.
- RTS is hardwired to Wemos 5V so the meter streams continuously; that is
  what killed the old firmware (unbounded `while(available)` -> WDT) and
  made USB flashing impossible while wired to GPIO3.
- Working procedure (used twice successfully):
  - Disconnect the P1 transistor from RX (cut/desolder).
  - Hold GPIO0 (pin D3) to GND, press reset; verify `boot mode:(1,x)`.
  - `~/.platformio/penv/bin/esptool.py --port /dev/cu.usbserial-XX \
     --baud 115200 --before no_reset write_flash 0x0 <firmware.bin>`
- Serial port name varies: seen `cu.usbserial-110` and `cu.usbserial-10`.
- Wemos D1 mini has only a RST button; GPIO0 = D3 pin.

## Firmware versions

- `.pio/build/esp8285/firmware.bin` = "1.4 – NL <date> <time>" build
  (currently flashed): all fixes + `Serial.swap()` to D7 + `/Rx` page.
  Footer on every page shows compile timestamp.
- `.pio/build/esp8285_factoryreset/firmware.bin` = same plus one-time
  factory reset on boot. Only flash when a wipe is intended; the 'R'
  marker is already set so it would not wipe again anyway.
- OTA works: POST admin pwd to `/uploadDialog`, then POST
  `update=@firmware.bin` to `/update`.
- Old firmware known-good image:
  `Arduino IDE/firmware/oud (pre IDE 2.2)/P1WG2022.v1.1adc_NL.esp8285.bin`
  (listens on GPIO3; would WDT-loop on continuous RTS-high streaming).

## EEPROM/config notes

- `struct settings` layout changed twice vs old firmware: field sizes
  (`password` 32->64, `mqttPass` 32->64) and Domoticz fields removed.
  Any stored config is misaligned; the reset build wipes it anyway.
- `dataSet[2]=='R'` is the reset marker; never written by `/SetupSave`.
- `dataSet[0]=='j'` marks valid config; `/SetupSave` sets it and does
  `ESP.reset()` (browser timeout on save is expected).
- Defaults from code: ssid `orbi`, mqtt broker `192.168.1.123`,
  admin pwd `adminpwd`. Without valid WiFi the device starts AP `P1_Setup`
  at 192.168.4.1.

## Network facts

- Module IP on LAN: 192.168.0.145 (mDNS p1wifi.local / p1meter.local).
- MQTT broker: 192.168.0.20:1883, reachable, accepts TCP.
- MQTT only sends after a decoded telegram (`doMQTT` runs on
  datagramValid && state==DONE); no meter connection = no MQTT, by design.
- `mqtt_connect()` now makes at most one attempt per call, retried every
  30s; worst case ~5s stall per 30s.
- WiFi signal at meter location looked marginal (ping ~100-180ms on LAN).

## MQTT JSON topic

- `publishJSON()` in `src/MQTT.h` publishes one JSON per decoded telegram
  to `<mqttTopic>/json`, after `MQTT_reporter()`. Atomic snapshot for the
  Marstek dispatcher (`specs/dispatcher-requirements.md` in that repo).
- Keys: `id`, `ts`, `tariff`, `ed1`/`ed2`/`er1`/`er2` (kWh counters),
  `power_delivered`/`power_returned` (kW), `pd_l1..3`/`pr_l1..3`,
  `v_l1..3`, `c_l1..3`, `gas_m3`, `failures`, `long_failures`,
  `sags_l1..3`, `swells_l1..3`.
- `mqtt_client.setBufferSize(1024)` in `wifi_functions.h`; PubSubClient's
  default 256B packet buffer is too small for the ~700B payload.
- Not retained; string fields are appended unescaped (meter IDs and
  timestamps are safe in practice, tighten if needed).
- Verified live on the broker 2026-11-18.

## Next steps

- Optionally tidy the wiring: data wire stays on D7 (GPIO13); RX pin/GPIO3
  is unused now and could host nothing.
- If a future Wemos replacement is fitted (working GPIO3), drop
  `-DSERIAL_SWAP` from build_flags and rewire back to the RX pin.
- For the dispatcher's ~5s control loop, set the report interval low on
  /Setup; SMR5 telegrams arrive every second anyway.
