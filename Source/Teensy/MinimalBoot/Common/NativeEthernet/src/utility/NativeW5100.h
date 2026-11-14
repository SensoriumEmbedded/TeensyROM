/*
 * Copyright 2018 Paul Stoffregen
 * Copyright (c) 2010 by Cristian Maglie <c.maglie@bug.st>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

// w5100.h contains private W5x00 hardware "driver" level definitions
// which are not meant to be exposed to other libraries or Arduino users

#ifndef	W5100_H_INCLUDED
#define	W5100_H_INCLUDED

#include <Arduino.h>

class SnMR {
public:
//  static const uint8_t CLOSE  = 0x00; //Unused
  static const uint8_t TCP    = 0x21;
  static const uint8_t UDP    = 0x02;
//  static const uint8_t IPRAW  = 0x03; //Unused
//  static const uint8_t MACRAW = 0x04; //Unused
//  static const uint8_t PPPOE  = 0x05; //Unused
//  static const uint8_t ND     = 0x20; //Unused
  static const uint8_t MULTI  = 0x80;
};

class SnSR {
public:
  static const uint8_t CLOSED      = 0x00;
  static const uint8_t INIT        = 0x13;
  static const uint8_t LISTEN      = 0x14;
//  static const uint8_t SYNSENT     = 0x15; //Unused
//  static const uint8_t SYNRECV     = 0x16; //Unused
  static const uint8_t ESTABLISHED = 0x17;
  static const uint8_t FIN_WAIT    = 0x18;
//  static const uint8_t CLOSING     = 0x1A; //Unused
//  static const uint8_t TIME_WAIT   = 0x1B; //Unused
  static const uint8_t CLOSE_WAIT  = 0x1C;
//  static const uint8_t LAST_ACK    = 0x1D; //Unused
//  static const uint8_t UDP         = 0x22; //Unused
//  static const uint8_t IPRAW       = 0x32; //Unused
//  static const uint8_t MACRAW      = 0x42; //Unused
//  static const uint8_t PPPOE       = 0x5F; //Unused
};

#endif
