#include <ArduinoFake.h>
#include <unity.h>
#include <string>
#include <cstring>

// No-op replacement for the ArduinoLog logger.
struct LogStub {
  template <typename... A> void verbose(A &&...) {}
  template <typename... A> void verboseln(A &&...) {}
  template <typename... A> void info(A &&...) {}
  template <typename... A> void infoln(A &&...) {}
  template <typename... A> void error(A &&...) {}
  template <typename... A> void errorln(A &&...) {}
  template <typename... A> void trace(A &&...) {}
  template <typename... A> void traceln(A &&...) {}
  template <typename... A> void warning(A &&...) {}
  template <typename... A> void warningln(A &&...) {}
  template <typename... A> void fatal(A &&...) {}
  template <typename... A> void fatalln(A &&...) {}
} Log;

#include "vars.h"
#include "CRC16.cpp"

// Shims for helpers from functions.h, which pulls in too many hardware deps.
int FindCharInArray(char array[], char c, int len) {
  for (int i = 0; i < len; i++) {
    if (array[i] == c) {
      return i;
    }
  }
  return -1;
}

void identifyMeter() {}

// Arduino builds auto-generate prototypes; declare it explicitly here.
void OBISparser(int len);

#include "decoder.h"

// DSMR5 telegram, CRC16 verified (see SmartMeterSerialMock::writeDatagram).
static const char *const TELEGRAM[] = {
    "/Ene5\\XS210 ESMR 5.0\r\n",
    "\r\n",
    "1-3:0.2.8(50)\r\n",
    "0-0:1.0.0(230629141500W)\r\n",
    "0-0:96.1.1(1234567890123456789012345678901234)\r\n",
    "1-0:1.8.1(000992.992*kWh)\r\n",
    "1-0:1.8.2(000560.157*kWh)\r\n",
    "1-0:2.8.1(000348.890*kWh)\r\n",
    "1-0:2.8.2(000859.885*kWh)\r\n",
    "0-0:96.14.0(0002)\r\n",
    "1-0:1.7.0(00.172*kW)\r\n",
    "1-0:2.7.0(00.000*kW)\r\n",
    "0-0:96.7.21(00304)\r\n",
    "0-0:96.7.9(00003)\r\n",
    "1-0:32.32.0(00003)\r\n",
    "1-0:32.36.0(00000)\r\n",
    "1-0:32.7.0(227.0*V)\r\n",
    "1-0:31.7.0(000*A)\r\n",
    "1-0:21.7.0(00.172*kW)\r\n",
    "1-0:22.7.0(00.000*kW)\r\n",
    "0-1:24.1.0(003)\r\n",
    "0-1:96.1.0(9876543210987654321098765432109876)\r\n",
    "0-1:24.2.1(230629141500W)(05446.465*m3)\r\n",
    "!2901\r\n",
};

static void feedLine(const char *line) {
  strcpy(telegram, line);
  decodeLine(strlen(line));
}

static void feedTelegram() {
  for (const char *line : TELEGRAM) {
    feedLine(line);
  }
}

void setUp() {
  state = WAITING;
  datagram = "";
  datagramValid = false;
  dataEnd = false;
  dataFailureCount = 0;
  currentCRC = 0;
  dsmrData = DSMRData();
  interval = 10;
  statusMsg = "";
}

void test_decodes_valid_telegram() {
  feedTelegram();
  TEST_ASSERT_EQUAL(DONE, state);
  TEST_ASSERT_TRUE(datagramValid);
  TEST_ASSERT_EQUAL(0, dataFailureCount);
}

void test_parses_obis_fields() {
  feedTelegram();
  TEST_ASSERT_EQUAL_STRING("50", dsmrData.p1_version.c_str());
  TEST_ASSERT_EQUAL(5, dsmrData.P1prot());
  TEST_ASSERT_EQUAL_STRING("230629141500W", dsmrData.timestamp.c_str());
  TEST_ASSERT_EQUAL_STRING("2", dsmrData.electricity_tariff.c_str());
  TEST_ASSERT_EQUAL_STRING("1234567890123456789012345678901234",
                           dsmrData.equipment_id.c_str());
  TEST_ASSERT_EQUAL_STRING("9876543210987654321098765432109876",
                           dsmrData.gas_equipment_id.c_str());
  TEST_ASSERT_EQUAL_STRING("5446.465", dsmrData.gas_delivered.c_str());

  TEST_ASSERT_FLOAT_WITHIN(0.001, 992.992,
                           dsmrData.energy_delivered_tariff1.val());
  TEST_ASSERT_FLOAT_WITHIN(0.001, 560.157,
                           dsmrData.energy_delivered_tariff2.val());
  TEST_ASSERT_FLOAT_WITHIN(0.001, 348.890,
                           dsmrData.energy_returned_tariff1.val());
  TEST_ASSERT_FLOAT_WITHIN(0.001, 859.885,
                           dsmrData.energy_returned_tariff2.val());
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.172, dsmrData.power_delivered.val());
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, dsmrData.power_returned.val());
  TEST_ASSERT_FLOAT_WITHIN(0.001, 227.0, dsmrData.voltage_l1.val());
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, dsmrData.current_l1.val());
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.172, dsmrData.power_delivered_l1.val());

  TEST_ASSERT_EQUAL_UINT32(304, dsmrData.electricity_failures);
  TEST_ASSERT_EQUAL_UINT32(3, dsmrData.electricity_long_failures);
}

void test_rejects_bad_crc() {
  // Same telegram but one digit flipped in a data line
  for (const char *line : TELEGRAM) {
    if (strcmp(line, "1-0:1.7.0(00.172*kW)\r\n") == 0) {
      feedLine("1-0:1.7.0(00.173*kW)\r\n");
    } else {
      feedLine(line);
    }
  }
  TEST_ASSERT_EQUAL(FAILURE, state);
  TEST_ASSERT_FALSE(datagramValid);
  TEST_ASSERT_EQUAL(1, dataFailureCount);
}

void test_ignores_garbage_while_waiting() {
  feedLine("garbage noise\r\n");
  feedLine("more noise\r\n");
  TEST_ASSERT_EQUAL(WAITING, state);
  TEST_ASSERT_FALSE(datagramValid);
  // Recovery: a valid telegram still parses afterwards
  feedTelegram();
  TEST_ASSERT_EQUAL(DONE, state);
  TEST_ASSERT_TRUE(datagramValid);
}

int main(int argc, char **argv) {
  using namespace fakeit;
  When(Method(ArduinoFake(), millis)).AlwaysReturn(1000);
  UNITY_BEGIN();
  RUN_TEST(test_decodes_valid_telegram);
  RUN_TEST(test_parses_obis_fields);
  RUN_TEST(test_rejects_bad_crc);
  RUN_TEST(test_ignores_garbage_while_waiting);
  return UNITY_END();
}
