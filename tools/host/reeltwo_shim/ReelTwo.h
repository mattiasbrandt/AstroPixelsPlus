#pragma once
// Host shim for ReelTwo.h: just enough for core/Animation.h and core/Marcduino.h.
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#define PROGMEM
#define F(s) s
#define UNUSED_ARG(a) (void)(a);
#define strlen_P strlen
#define strncmp_P strncmp
class __FlashStringHelper;
typedef const __FlashStringHelper *PROGMEMString;
#define SizeOfArray(a) (sizeof(a) / sizeof((a)[0]))
unsigned long millis();
class Stream { public: virtual ~Stream() {} virtual int available() = 0; virtual int read() = 0; virtual size_t write(const uint8_t *, size_t) { return 0; } };
class HardwareSerial : public Stream {};
