// One scope snapshot shared by the SWD reader (main.cpp) and the logger.
#pragma once
#include <stdint.h>

constexpr int TRACE_POINTS = 300;

// Frame sent to browsers (little endian), see web_page.h decode():
//   0 'D' 'S'  2 version(2)  3 flags(bit0 = scope connected)
//   4 u16 write pos  6 u16 reserved
//   8 state[32]  40 u16 zeros[22]  84 s32 meas[6]  108 u32 psc  112 u32 arr
//   116 s16 trace[300] (screen pixels from zero)             total 716 bytes
struct __attribute__((packed)) Frame {
  char magic[2];
  uint8_t version;
  uint8_t flags;
  uint16_t pos;
  uint16_t reserved;
  uint8_t state[32];
  uint16_t zeros[22];
  int32_t meas[6];
  uint32_t psc;
  uint32_t arr;
  int16_t trace[TRACE_POINTS];
};
static_assert(sizeof(Frame) == 716, "frame layout");
