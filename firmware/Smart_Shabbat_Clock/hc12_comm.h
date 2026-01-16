#ifndef HC12_COMM_H
#define HC12_COMM_H

// HC-12 radio helper interface; implemented in hc12_comm.cpp.

#include <stdint.h>

class String;
class HardwareSerial;

// CHANGE HERE: default ACK timeout (ms).
bool sendHC12AndWaitAck(const String& cmd, uint32_t timeoutMs = 1500);
extern HardwareSerial HC12;
extern bool hc12Ok;

#endif // HC12_COMM_H
