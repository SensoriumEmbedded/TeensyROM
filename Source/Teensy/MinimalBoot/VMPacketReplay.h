// Exceptional VM packet recovery. Included inside VmRuntime, after crc16.
// ABI2 gameplay clients reserve $2800..$28ef and the ready byte at $02f0.
// Only the already-pending packet is copied; it is never ACKed here.
static unsigned encodePacket(uint8_t *bytes){
    bytes[0]='M';bytes[1]='3';bytes[2]=1;bytes[3]=packet.type;bytes[4]=sequence;
    bytes[5]=packet.flags;bytes[6]=packet.length;bytes[7]=0;
    memcpy(bytes+8,packet.payload,packet.length);
    const auto crc=crc16(bytes,8+packet.length);
    bytes[8+packet.length]=crc;bytes[9+packet.length]=crc>>8;
    return 10u+packet.length;
}
static bool replayPacket(){
#if defined(FeatVMVideoDMA) && defined(Fab04_FullDMACapable)
    if((videoTiming&0xfc)!=0x80||packet.length>228)return false;
    nS_DMASetup=(videoTiming&1)?Def_nS_DMASetupNTSC:Def_nS_DMASetupPAL;
    nS_MaxAdj=(videoTiming&1)?Def_nS_MaxAdjNTSC:Def_nS_MaxAdjPAL;
    uint8_t bytes[240],ready=0xa5;const unsigned size=encodePacket(bytes);
    // Hold the CPU throughout staging and ready publication. A corrupt DMA
    // copy still cannot be used: the receiver rechecks its full CRC in RAM.
    bool started=PerformDMA(false,0x2800,bytes,size,false);
    bool okay=started&&AGIContinueDMA(false,0x02f0,&ready,1,false);
    bool closed=started&&CloseDMA();
    if(!okay||!closed){if(DMA_State!=DMA_S_DisableReady)AGIDMAEmergencyRelease();return false;}
    return true;
#else
    return false;
#endif
}
