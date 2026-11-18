#include <unity.h>
#include <stdint.h>
#include <string.h>

#include "../src/CRC16.cpp"
#include "../src/CRC32.cpp"

void test_crc16_empty() {
  // CRC of zero-length input returns the initial value unchanged
  TEST_ASSERT_EQUAL_HEX16(0x0000, CRC16(0x0000, (unsigned char *)"", 0));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, CRC16(0xFFFF, (unsigned char *)"", 0));
}

void test_crc16_known_value() {
  // CRC-16/AUGMENTED check: "123456789" -> 0xBB3D for init 0x0000? No:
  // CRC-16/ARC (IBM, reflected 0xA001, init 0x0000): "123456789" -> 0xBB3D
  const char *s = "123456789";
  TEST_ASSERT_EQUAL_HEX16(0xBB3D, CRC16(0x0000, (unsigned char *)s, 9));
}

void test_crc16_incremental() {
  // Decoder feeds the CRC line by line; incremental == one-shot
  const char *part1 = "/Ene5\\XS210 ESMR 5.0\r\n\r\n1-3:0.2.8(50)\r\n";
  const char *part2 = "0-0:1.0.0(230629141500W)\r\n";
  const char *whole = "/Ene5\\XS210 ESMR 5.0\r\n\r\n1-3:0.2.8(50)\r\n"
                      "0-0:1.0.0(230629141500W)\r\n";
  unsigned int crc = CRC16(0x0000, (unsigned char *)part1, strlen(part1));
  crc = CRC16(crc, (unsigned char *)part2, strlen(part2));
  TEST_ASSERT_EQUAL_HEX16(
      CRC16(0x0000, (unsigned char *)whole, strlen(whole)), crc);
}

void test_crc32_known_value() {
  // CRC-32/MPEG-2 (non-reflected 0x04C11DB7, init 0xFFFFFFFF, no final xor):
  // "123456789" -> 0x0376E6E7
  const char *s = "123456789";
  TEST_ASSERT_EQUAL_HEX32(0x0376E6E7,
                          calculateCRC32((const uint8_t *)s, 9));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_crc16_empty);
  RUN_TEST(test_crc16_known_value);
  RUN_TEST(test_crc16_incremental);
  RUN_TEST(test_crc32_known_value);
  return UNITY_END();
}
