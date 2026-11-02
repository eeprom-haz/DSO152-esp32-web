// Minimal bit-banged SWD host: connect to a Cortex-M3 and read/write its
// memory through the AHB-AP while the target keeps running.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace swd {

// Pins must be GPIO 0..31. `halfPeriod` is a busy-wait count per clock half;
// raise it if the wiring is long or flaky.
void begin(int clkPin, int dioPin, uint32_t halfPeriod);

// Line reset, JTAG-to-SWD switch, read DPIDR, power up the debug domain.
bool connect(uint32_t *dpidr);

// Word-aligned reads/writes of target memory.
bool readWords(uint32_t addr, uint32_t *buf, size_t words);
bool readByte(uint32_t addr, uint8_t *val);
bool writeByte(uint32_t addr, uint8_t val);

// Release both pins (inputs) so nothing is driven while idle.
void release();

}  // namespace swd
