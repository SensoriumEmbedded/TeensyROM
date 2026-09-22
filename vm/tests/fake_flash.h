// SPDX-License-Identifier: MIT
//
// A NOR flash that behaves like one: erase sets bits, programming only clears
// them.
//
// Power loss is modelled by a budget of operations; when it runs out the
// operation in flight is applied to half its bytes and the run stops.

#pragma once
#include <stdexcept>
#include <vector>
#include <stdint.h>
#include <string.h>

#include "../../Source/Teensy/MinimalBoot/Common/VMHostInstall.h"

struct PowerCut : std::exception {};

struct FakeFlash {
 std::vector<uint8_t> cells{std::vector<uint8_t>(VM_HOST_SLOT_BYTES,0xff)};
 long budget=-1;            // <0 never cuts
 long ops=0;
 uint32_t eraseFailAt=~0u,programFailAt=~0u;

 bool spend(){const long n=ops++;return budget>=0&&n>=budget;}

 bool erase(uint32_t sector){
  if(sector>=VM_HOST_SECTORS)throw std::logic_error("erase outside the slot");
  if(sector==eraseFailAt)return false;
  const bool cut=spend();
  const uint32_t n=cut?VM_HOST_SECTOR_BYTES/2:VM_HOST_SECTOR_BYTES;
  memset(&cells[sector*VM_HOST_SECTOR_BYTES],0xff,n);
  if(cut)throw PowerCut{};
  return true;}

 bool program(uint32_t offset,const uint8_t *data,uint32_t n){
  if(offset+n>VM_HOST_SLOT_BYTES)throw std::logic_error("program outside the slot");
  if(offset/VM_HOST_PAGE_BYTES!=(offset+n-1)/VM_HOST_PAGE_BYTES)throw std::logic_error("program crosses a page boundary");
  if(offset==programFailAt)return false;
  const bool cut=spend();
  const uint32_t take=cut?n/2:n;
  for(uint32_t i=0;i<take;i++)cells[offset+i]&=data[i];   // NOR: programming only clears
  if(cut)throw PowerCut{};
  return true;}

 const uint8_t *map(uint32_t offset)const{return &cells[offset];}
};

// A .TRH package in memory, read the way the installer reads an SD file.
struct FakeReader {
 const std::vector<uint8_t> *bytes;
 uint32_t pos=0;
 uint32_t failAfter=~0u;    // bytes delivered before read() starts failing

 bool seek(uint32_t off){if(off>bytes->size())return false;pos=off;return true;}
 bool read(void *dst,uint32_t n){
  if(pos+n>bytes->size())return false;
  if(pos+n>failAfter)return false;
  memcpy(dst,bytes->data()+pos,n);pos+=n;return true;}
};
