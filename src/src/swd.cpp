// Bit-banged SWD, following the bit timing used by ARM's DAPLink:
// host changes SWDIO while SWCLK is low, target samples on the rising edge;
// host samples target data while SWCLK is low, before the rising edge.
#include "swd.h"

#include <Arduino.h>
#include "soc/gpio_struct.h"

// The ESP32 exposes these GPIO registers as plain words; the RISC-V parts
// (ESP32-C3) wrap them in unions with a .val member.
#if CONFIG_IDF_TARGET_ESP32
#define REG_OUT_SET GPIO.out_w1ts
#define REG_OUT_CLR GPIO.out_w1tc
#define REG_EN_SET GPIO.enable_w1ts
#define REG_EN_CLR GPIO.enable_w1tc
#define REG_IN GPIO.in
#else
#define REG_OUT_SET GPIO.out_w1ts.val
#define REG_OUT_CLR GPIO.out_w1tc.val
#define REG_EN_SET GPIO.enable_w1ts.val
#define REG_EN_CLR GPIO.enable_w1tc.val
#define REG_IN GPIO.in.val
#endif

namespace swd {
namespace {

enum : uint8_t { ACK_OK = 1, ACK_WAIT = 2, ACK_FAULT = 4, ACK_PARITY = 8 };

// DP registers (A[3:2] << 2)
constexpr uint8_t DP_IDCODE = 0x0, DP_ABORT = 0x0, DP_CTRL_STAT = 0x4,
                  DP_SELECT = 0x8, DP_RDBUFF = 0xC;
// AHB-AP registers (bank 0)
constexpr uint8_t AP_CSW = 0x0, AP_TAR = 0x4, AP_DRW = 0xC;

constexpr uint32_t CSW_WORD_INC = 0x23000012;   // 32-bit, auto-increment
constexpr uint32_t CSW_BYTE = 0x23000000;       // 8-bit, no increment

int dioPin;
uint32_t clkMask, dioMask, halfPeriod;
uint32_t cswCache = 0;

inline void dly() {
  for (volatile uint32_t i = 0; i < halfPeriod; i++) {
  }
}
inline void clkHi() { REG_OUT_SET = clkMask; }
inline void clkLo() { REG_OUT_CLR = clkMask; }
inline void dioDrive() { REG_EN_SET = dioMask; }
inline void dioFloat() { REG_EN_CLR = dioMask; }
inline bool dioIn() { return (REG_IN >> dioPin) & 1; }

inline void writeBit(uint32_t b) {
  if (b) REG_OUT_SET = dioMask; else REG_OUT_CLR = dioMask;
  clkLo(); dly();
  clkHi(); dly();
}

inline uint32_t readBit() {
  clkLo(); dly();
  uint32_t b = dioIn();
  clkHi(); dly();
  return b;
}

inline void turnaround() {
  clkLo(); dly();
  clkHi(); dly();
}

void writeBits(uint32_t v, int n) {
  for (int i = 0; i < n; i++) writeBit((v >> i) & 1);
}

uint32_t readBits(int n) {
  uint32_t v = 0;
  for (int i = 0; i < n; i++) v |= readBit() << i;
  return v;
}

inline uint32_t parity32(uint32_t v) { return __builtin_popcount(v) & 1; }

uint8_t request(bool ap, bool read, uint8_t addr) {
  uint8_t a = (addr >> 2) & 3;
  uint8_t bits = (ap ? 1 : 0) | (read ? 2 : 0) | (a << 2);
  uint8_t par = __builtin_popcount(bits) & 1;
  return 0x81 | (bits << 1) | (par << 5);   // start, ..., stop=0, park=1
}

uint8_t transfer(bool ap, bool read, uint8_t addr, uint32_t *data) {
  uint8_t req = request(ap, read, addr);
  for (int retry = 0; retry < 200; retry++) {
    dioDrive();
    writeBits(req, 8);
    dioFloat();
    turnaround();
    uint8_t ack = readBits(3);
    if (ack == ACK_OK) {
      if (read) {
        uint32_t v = readBits(32);
        uint32_t p = readBit();
        turnaround();
        dioDrive();
        writeBits(0, 2);
        if (p != parity32(v)) return ACK_PARITY;
        *data = v;
      } else {
        turnaround();
        dioDrive();
        writeBits(*data, 32);
        writeBit(parity32(*data));
        writeBits(0, 2);
      }
      return ACK_OK;
    }
    turnaround();
    dioDrive();
    writeBits(0, 2);
    if (ack != ACK_WAIT) return ack ? ack : ACK_FAULT;
  }
  return ACK_WAIT;
}

bool dpRead(uint8_t addr, uint32_t *v) { return transfer(false, true, addr, v) == ACK_OK; }
bool dpWrite(uint8_t addr, uint32_t v) { return transfer(false, false, addr, &v) == ACK_OK; }
bool apWrite(uint8_t addr, uint32_t v) { return transfer(true, false, addr, &v) == ACK_OK; }
bool apReadPosted(uint8_t addr, uint32_t *v) { return transfer(true, true, addr, v) == ACK_OK; }

void clearErrors() {
  dpWrite(DP_ABORT, 0x1E);   // clear STICKYERR/STICKYCMP/STICKYORUN/WDERR
  cswCache = 0;
}

bool setCsw(uint32_t csw) {
  if (cswCache == csw) return true;
  if (!apWrite(AP_CSW, csw)) return false;
  cswCache = csw;
  return true;
}

void lineReset() {
  dioDrive();
  for (int i = 0; i < 56; i++) writeBit(1);
}

}  // namespace

void begin(int clkPin, int dio, uint32_t half) {
  dioPin = dio;
  clkMask = 1u << clkPin;
  dioMask = 1u << dio;
  halfPeriod = half;
  pinMode(clkPin, OUTPUT);
  pinMode(dio, INPUT_PULLUP);
  REG_OUT_SET = clkMask | dioMask;
}

bool connect(uint32_t *dpidr) {
  pinMode(__builtin_ctz(clkMask), OUTPUT);
  lineReset();
  writeBits(0xE79E, 16);     // JTAG-to-SWD
  lineReset();
  writeBits(0, 8);           // idle
  cswCache = 0;

  uint32_t id = 0;
  if (!dpRead(DP_IDCODE, &id)) return false;
  if (dpidr) *dpidr = id;
  clearErrors();
  if (!dpWrite(DP_CTRL_STAT, 0x50000000)) return false;   // CSYS/CDBG power-up
  for (int i = 0; i < 100; i++) {
    uint32_t cs = 0;
    if (dpRead(DP_CTRL_STAT, &cs) && (cs & 0xA0000000) == 0xA0000000)
      return dpWrite(DP_SELECT, 0);                       // AP 0, bank 0
    delay(1);
  }
  return false;
}

bool readWords(uint32_t addr, uint32_t *buf, size_t words) {
  while (words) {
    // TAR auto-increment is only guaranteed inside a 1 KB block
    size_t room = (0x400 - (addr & 0x3FF)) / 4;
    size_t n = words < room ? words : room;
    uint32_t v;
    if (!setCsw(CSW_WORD_INC) || !apWrite(AP_TAR, addr) ||
        !apReadPosted(AP_DRW, &v)) {          // first posted read: discard
      clearErrors();
      return false;
    }
    for (size_t i = 1; i < n; i++) {
      if (!apReadPosted(AP_DRW, &buf[i - 1])) { clearErrors(); return false; }
    }
    if (!dpRead(DP_RDBUFF, &buf[n - 1])) { clearErrors(); return false; }
    addr += n * 4;
    buf += n;
    words -= n;
  }
  return true;
}

bool readByte(uint32_t addr, uint8_t *val) {
  uint32_t w;
  if (!readWords(addr & ~3u, &w, 1)) return false;
  *val = (w >> ((addr & 3) * 8)) & 0xFF;
  return true;
}

bool writeByte(uint32_t addr, uint8_t val) {
  uint32_t lane = (uint32_t)val << ((addr & 3) * 8);
  if (!setCsw(CSW_BYTE) || !apWrite(AP_TAR, addr) || !apWrite(AP_DRW, lane)) {
    clearErrors();
    return false;
  }
  uint32_t dummy;
  return dpRead(DP_RDBUFF, &dummy);   // make sure the write completed
}

void release() {
  dioFloat();
  pinMode(__builtin_ctz(clkMask), INPUT);
}

}  // namespace swd
