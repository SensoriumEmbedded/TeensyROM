#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "../../abi/vm_abi.h"
#define FLASHMEM
#define FeatVMVideoDMA
#define Fab04_FullDMACapable
enum {DMA_S_DisableReady,DMA_S_Active};
static unsigned DMA_State,nS_DMASetup,nS_MaxAdj;
static uint32_t ARM_DWT_CYCCNT;
static uint32_t clockUs;
static uint32_t micros(){return clockUs;}
static constexpr uint32_t F_CPU_ACTUAL=600000000;
static constexpr unsigned Def_nS_DMASetupNTSC=1,Def_nS_DMASetupPAL=2,Def_nS_MaxAdjNTSC=3,Def_nS_MaxAdjPAL=4;
static uint8_t c64[65536];static unsigned segments;static bool dmaFail;
static bool PerformDMA(bool,uint16_t address,uint8_t *data,uint16_t bytes,bool){
    if(dmaFail)return false;DMA_State=DMA_S_Active;memcpy(c64+address,data,bytes);segments++;return true;
}
static bool AGIContinueDMA(bool r,uint16_t a,uint8_t *d,uint16_t n,bool f){return PerformDMA(r,a,d,n,f);}
static bool CloseDMA(){DMA_State=DMA_S_DisableReady;return true;}
static void AGIDMAEmergencyRelease(){DMA_State=DMA_S_DisableReady;}
namespace VmRuntime {static uint8_t videoTiming=0x80;}
#ifndef MPE_INDEXED_HOST_HEADER
#define MPE_INDEXED_HOST_HEADER "../../../Source/Teensy/MinimalBoot/VMIndexedVideo.h"
#endif
#include MPE_INDEXED_HOST_HEADER
using namespace VmRuntime;
