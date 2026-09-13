// Foreground scheduler shared verbatim by firmware and the integration test.
// Platform/bus services are supplied by VMHost.h (test stubs only replace I/O).
void VMHostPoll(){
    using namespace VmRuntime;if(!active)return;
    if(startRequested&&!started){started=true;startRequested=false;if(failure){fail(failure);return;}EZFlashRAM[0xf5]=2;}
    if(!started||failure||!module)return;
    if(inputPending){VmInput in{input.buttons,input.display,input.overflow,input.protocol};inputPending=false;
        if(in.protocol==0x90||(in.protocol==0x83&&!(indexedVideo.geometry&VM_INDEXED_SEPARATE_SELECTORS))){
            const bool cropInput=in.protocol==0x83&&(indexedVideo.geometry&VM_INDEXED_CROP_F3);
            const uint8_t display=cropInput?in.display&3:in.display;
            if(indexedVideo.configured&&display<4&&(!cropInput||!(in.display&12))){
                const uint8_t mode=display==0?indexedVideo.preferred:display;
                if(indexedVideo.capabilities&(1u<<mode)){
                    if(indexedVideo.geometry&VM_INDEXED_CROP_F3)indexedVideo.camera.input(mode,cropInput?in.display>>4:0,micros());
                    indexedVideo.requested=mode;
                }
                if(in.protocol==0x83){in.protocol=0x81;in.display=1;module->input(&in);}
            }
        }else module->input(&in);
    }
    if(pending&&EZFlashRAM[0xf6]==sequence){
        if(indexedVideo.hostPacket)indexedVideoAck();else module->ack();
        pending=false;quietRequested=false;packetReplayRequested=false;EZFlashRAM[0xf5]=2;
    }
    if(packetReplayRequested){
        if(!pending){packetReplayRequested=false;quietRequested=false;}
        else{
            // No guest work, video advance or successor packet during replay.
            // Keep the foreground quiet until the C64 validates and ACKs it.
            EZFlashRAM[0xf5]=0x12;
            if(DMA_State!=DMA_S_DisableReady)return;
            packetReplayRequested=false;
            if(!replayPacket())fail(0x19);
            return;
        }
    }
    // Consume an ACK BEFORE pumping: a module may defer input/scene changes
    // while its packet is frozen. Pump-before-ACK followed immediately by
    // packet() starves such input forever when every idle turn emits a packet.
    // An acknowledged packet must NOT consume the next emulation slice.
    // Otherwise a responsive client + recurring SID packets can starve the
    // game clock indefinitely. Publication still follows one bounded pump.
    sliceStarted=micros();
    if(quietRequested){EZFlashRAM[0xf5]=0x12;}else module->pump();
    if(failure||pending)return;
    if(quietRequested)return;
    if(indexedVideo.phase==6){if(!transferIndexedVideoSlice()){fail(0x17);return;}if(indexedVideo.phase==6)return;}
    if(indexedVideo.phase==2){if(!transferIndexedVideo()){fail(0x17);return;}indexedVideo.phase=3;}
    indexedVideo.hostPacket=indexedVideoPacket(packet);
    if(!indexedVideo.hostPacket){
        if(!module->packet(&packet)){
            indexedVideo.hostPacket=indexedVideoPacket(packet);if(!indexedVideo.hostPacket)return;
        }
    }
    if(failure)return;
    if(packet.length>228||packet.reserved||!packet.type){fail(0x15);return;}
    if(packet.type==1&&(packet.flags&0x10))indexedVideoLegacy();
    uint8_t bytes[240];sequence=sequence==255?1:sequence+1;
    const unsigned size=encodePacket(bytes);
    for(unsigned i=0;i<size;i++)EZFlashRAM[i]=bytes[i];
    pending=true;
#if defined(__arm__)
    __asm__ volatile("dmb":::"memory");
#endif
    EZFlashRAM[0xf7]=sequence;
}
