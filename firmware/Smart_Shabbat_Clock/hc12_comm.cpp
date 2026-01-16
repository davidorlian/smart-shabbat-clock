#include <HardwareSerial.h>
#include <ctype.h>
#include "hc12_comm.h"

// Send a command to the HC-12 module and wait for an "ACK" reply.
bool sendHC12AndWaitAck(const String& cmd, uint32_t timeoutMs) {
  hc12Ok = false;
  
  // Clear buffer: remove any stale data or noise before sending.
  Serial.print("HC-12: Clearing buffer... Content found: ");
  bool junk_found = false;
  while (HC12.available()) {
    Serial.print((char)HC12.read());
    junk_found = true;
  }
  if (!junk_found) Serial.println("None.");
  else Serial.println();
  
  // Send the command
  HC12.println(cmd);
  Serial.printf("HC-12: TX '%s'\n", cmd.c_str());

  // CHANGE HERE: delay to let the remote switch to RX mode.
  delay(50); // Wait for remote unit to switch to RX mode

  // Wait for ACK: parse incoming stream char-by-char.
  uint32_t start = millis();
  String response = "";
  
  while (millis() - start < timeoutMs) {
    if (HC12.available()) {
      char c = HC12.read();
      
      // Only append printable characters to the response string
      if (isascii(c) && isprint(static_cast<unsigned char>(c))) { 
        response += c;
      }
      
      // Check for "ACK" immediately after any character is received
      if (response.length() >= 3) {
        if (response.endsWith("ACK") || response.endsWith("ack")) { 
          Serial.println("HC-12: RX ACK SUCCESS");
          hc12Ok = true;  
          return true;
        }
      }
    }
    delay(1); // Check frequently
  }
  
  // Failure handling: ACK was not received in time.
  Serial.printf("HC-12 command '%s' FAILED ACK. Response: '%s'\n", cmd.c_str(), response.c_str());
  return false;
}
