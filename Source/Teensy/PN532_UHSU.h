
#ifndef __PN532_HSU_H__
#define __PN532_HSU_H__

#include "PN532Interface.h"
#include "Arduino.h"
#include "USBHost_t36.h"


#define PN532_HSU_READ_TIMEOUT						(1000)

#define SeriaType USBSerial //USBSerial  //USBSerial_BigBuffer  HardwareSerial

class PN532_UHSU : public PN532Interface {
public:
    PN532_UHSU(SeriaType &serial);
    
    void begin();
    void wakeup();
    virtual int8_t writeCommand(const uint8_t *header, uint8_t hlen, const uint8_t *body = 0, uint8_t blen = 0);
    int16_t readResponse(uint8_t buf[], uint8_t len, uint16_t timeout);
    
private:
    SeriaType* _serial;
    uint8_t command;
    
    int8_t readAckFrame();
    
    int8_t receive(uint8_t *buf, int len, uint16_t timeout=PN532_HSU_READ_TIMEOUT);
};

#endif
