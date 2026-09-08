// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <stddef.h>

// Trusted-local ARMv7E-M hard-float modules. Not a security sandbox.
// All pointers and callbacks live until reset. Never call from an interrupt.
enum : uint32_t { VM_ABI = 2, VM_CODE_BASE = 0x18000, VM_CODE_LIMIT = 0x30000,
                  VM_DATA_BASE = 0x20014000, VM_DATA_LIMIT = 0x20044000,
                  VM_DATA_BYTES = VM_DATA_LIMIT-VM_DATA_BASE,
                  VM_RAM_BASE = 0x20200000, VM_RAM_BYTES = 512*1024 };
// Optional RAM-only profile: the upper 96 KiB of RAM2 holds initialized,
// non-executable constants; the guest receives only the lower 416 KiB.
// Legacy images keep the entire 512 KiB guest arena and the same ABI/layout.
enum : uint32_t { VM_PROFILE_LEGACY=0, VM_PROFILE_RAM2_RO96=1,
                  VM_PROFILE_RAM1_AUX=2, VM_AUX_CODE_LIMIT=0x28000,
                  VM_RAM2_RO_BYTES=96*1024, VM_RAM2_GUEST_BYTES=VM_RAM_BYTES-VM_RAM2_RO_BYTES,
                  VM_RAM2_RO_BASE=VM_RAM_BASE+VM_RAM2_GUEST_BYTES };
struct VmImageHeader {
    uint32_t magic, abi, header_bytes, code_bytes, data_bytes, bss_bytes;
    uint32_t entry, code_base, ram_base, required_services, payload_crc, header_crc;
    // [0] profile, [1] RAM2 constant payload bytes, [2..3] zero.
    uint32_t reserved[4];
};
static_assert(sizeof(VmImageHeader)==64, "MVM1 image header");
struct VmFileInfo { uint32_t bytes; uint8_t directory; char name[96]; uint8_t attributes; uint16_t date,time; };
struct VmInput { uint8_t buttons, display, overflow, protocol; };
struct VmPacket { uint8_t type, flags, length, reserved; uint8_t payload[228]; };
// Optional host-owned video transport. VIC_CELL10 is the first, deliberately
// narrow tier: the existing row-major 40x25 representation of eight bitmap
// bytes, screen byte and colour byte per cell. It transfers those three planes
// into an already-established VIC display; background must be zero and HIRES
// must match the current terminal mode. The VM then publishes its ordinary
// frame-end packet, which remains the owner of display-policy and audio commit.
// The submitted bytes remain module-owned and immutable for the duration of
// the synchronous call. Unavailable/Busy/Failed leave ownership with the VM,
// which may retry or use its ordinary packet receiver without changing state.
enum : uint32_t { VM_VIDEO_FORMAT_VIC_CELL10=1, VM_VIDEO_FLAG_HIRES=1 };
enum class VmVideoResult : uint32_t { Unavailable, Transferred, Busy, Failed };
struct VmVideoFrame {
    uint32_t bytes, format, flags, generation;
    uint16_t width, height, stride;
    uint8_t background, reserved;
    const uint8_t *pixels;
};
enum : uint32_t { VM_INDEXED_VIDEO_WORKSPACE_BYTES=24576 };
// Optional setup.reserved geometry flags. Zero preserves prior NES/DOS behavior.
enum : uint16_t { VM_INDEXED_NATIVE_HEIGHT=1, VM_INDEXED_DOUBLE_WIDTH=2 };
// Selectors arrive separately on protocol 90h. Do not interpret ordinary
// input protocol 83h as the older combined controller/video envelope.
enum : uint16_t { VM_INDEXED_SEPARATE_SELECTORS=4 };
// Generic conversion hints, shared by byte-indexed and raster producers.
// Preserve the higher foreground index when shrinking horizontally; use
// palette entry zero as multicolor background; exact CGA RGBI -> VIC colors.
enum : uint16_t { VM_INDEXED_FOREGROUND=8, VM_INDEXED_SOURCE_BACKGROUND=16,
                  VM_INDEXED_RGBI=32 };
// Optional setup hint: F5 uses a fixed half-cell split. Negotiated by
// video_configure; older hosts reject this bit and callers may retry without
// it. Other modes and existing producers retain adaptive conversion.
enum : uint16_t { VM_INDEXED_STABLE_RASTER=64 };
// Negotiated F5 profile for clients with hires sprite-plane support. Older
// hosts reject these bits; producers retry their legacy setup without them.
// SPRITE_TAGS uses bit 6 of a <=64-color index as an actor-priority hint.
// The hint never changes the composited source color, including F1/F7.
enum : uint16_t { VM_INDEXED_SPRITE_F5=128, VM_INDEXED_SPRITE_TAGS=256 };
// Negotiated F3 profile: native 160x200 crop rendered as ordinary multicolor.
// A matching client enables WASD only after receiving crop resume flag 8.
// Combined input 83h then carries W/S/A/D in display bits 4/5/6/7; bits 0..1
// retain the selector and bits 2..3 stay zero. Guest button bits are unchanged.
enum : uint16_t { VM_INDEXED_CROP_F3=512 };
// Optional compact F5: four hires color pairs in a bounded horizontal band.
// Uses VmCenterVideoSetup and 16 KiB scratch. Older hosts reject the extension.
enum : uint16_t { VM_INDEXED_CENTER_F5=1024 };
constexpr uint32_t VM_CENTER_VIDEO_WORKSPACE_BYTES=16384;
// DOS-only experiment: four hires color pairs across the 320x200 picture
// (the final four scanlines share one pair at the VIC badline boundary),
// with the native FLI left-edge artifact exposed (no covering sprites).
// Negotiates separately so existing 16 KiB center-profile modules still work.
enum : uint16_t { VM_INDEXED_FULL_F5=2048 };
constexpr uint32_t VM_FULL_VIDEO_WORKSPACE_BYTES=19456;
// Opt-in indexed service: packed RGB palette and row-major 8-bit indices.
// Modes 0 Color, 1 Auto-8, 2 Enhanced-25, 3 Sharp; capability bit = 1<<mode.
// Configuration lends an aligned, lifetime-long RAM1 workspace to firmware.
// Pixels/palette stay immutable across Busy until Transferred (including
// receiver resume ACK). Poll with the SAME generation. Never call from ISR.
// resolved_mode is output only; firmware owns hotkeys and mode selection.
struct VmIndexedVideoSetup {
    uint32_t bytes;void *workspace;uint32_t workspace_bytes;
    uint8_t default_mode,capabilities;uint16_t reserved;
};
struct VmCenterVideoSetup {
    VmIndexedVideoSetup setup; // bytes = sizeof(VmCenterVideoSetup)
    uint8_t first_row,row_count; // center: first 1..14/count 1..9; full: 0/25
    uint16_t reserved;
};
struct VmIndexedFrame {
    uint32_t bytes,generation;
    const uint8_t *pixels,*palette;
    uint32_t pixel_bytes,palette_bytes;
    uint16_t width,height,stride,colors;
    uint8_t resolved_mode;
};
// Optional synchronous raster reader (service 256). Avoids a second native
// framebuffer for banked/packed producers. Firmware calls read_pixel only
// inside video_indexed, never from an ISR or during a later DMA/ACK phase.
// Native backing may change between calls. Keep the descriptor/palette and
// generation stable after source_consumed becomes 1 until Transferred.
// Before consumption a Busy retry may refresh palette/geometry to current
// state. Firmware freezes its converted picture, not the VM's live memory.
struct VmIndexedRasterFrame {
    VmIndexedFrame frame; // bytes = sizeof(VmIndexedRasterFrame); pixels = null
    uint8_t (*read_pixel)(void *context,uint16_t x,uint16_t y);
    void *context;
    uint16_t geometry; // geometry and conversion hints, per frame
    uint8_t reserved,resolved_background;
    uint32_t source_consumed;   // output only, initialize to zero
};
// Optional dirty raster extension, negotiated by the full F5 profile. The
// bitmap uses output coordinates: 40x25 cells, low bit first, 125 bytes.
// Null requests a full conversion. A non-null map may be cleared only after
// source_consumed becomes 1; later guest writes belong to the next picture.
// Palette/geometry changes still invalidate conversion regardless of hints.
struct VmIndexedDirtyRasterFrame {
    VmIndexedRasterFrame raster; // frame.bytes = sizeof(this extension)
    const uint8_t *source_dirty;
};
enum : uint32_t { VM_OPEN_READ=1,VM_OPEN_WRITE=2,VM_OPEN_CREATE=4,VM_OPEN_EXCLUSIVE=8,VM_OPEN_TRUNCATE=16 };
enum class VmFsOp : uint32_t { Flush,Truncate,Timestamp,Close,Mkdir,Rmdir,Remove,Rename,Space };
struct VmFsRequest { VmFsOp operation; uint32_t handle,value,extra; const char *path,*destination; };
struct VmRamSpan { uint8_t *data; uint32_t bytes; };
struct VmHost {
    uint32_t abi, bytes, services;
    uint8_t *workspace; uint32_t workspace_bytes;
    const char *package_root, *content_path;
    uint32_t (*micros_now)();
    // Handles 1..24; zero is failure. read returns -1 on error.
    uint32_t (*open)(const char *path, VmFileInfo *info);
    int32_t (*read)(uint32_t handle, uint32_t offset, void *data, uint32_t count);
    int32_t (*next)(uint32_t directory, VmFileInfo *info); // 1 entry, 0 EOF, -1 error
    void (*close)(uint32_t handle);
    // RAM1 workspace follows module data/BSS. RAM2 is a separate guest arena.
    uint8_t *guest_ram; uint32_t guest_ram_bytes;
    uint32_t (*open_flags)(const char *path,uint32_t flags,VmFileInfo *info);
    int32_t (*write)(uint32_t handle,uint32_t offset,const void *data,uint32_t count);
    // 0 success, -1 failure; Space returns total/free sectors in value/extra,
    // and allocation-unit size (sectors per cluster) in handle.
    int32_t (*file_op)(VmFsRequest *request);
    // Cooperative foreground yield for pending input, ACK, retry or time slice.
    bool (*should_yield)();
    void (*fail)(uint8_t code,uint32_t detail);
    VmVideoResult (*video_present)(const VmVideoFrame *frame);
    // New center-profile hosts accept null to return a completed loan;
    // false means the frame/ACK is still pending and storage remains owned.
    bool (*video_configure)(const VmIndexedVideoSetup *setup);
    VmVideoResult (*video_indexed)(VmIndexedFrame *frame);
    // Profile RAM1_AUX only: non-executable ITCM tail and retired CRT swap
    // RAM. No live FlexRAM repartition, stack or heap borrowing.
    VmRamSpan auxiliary[2];
};
// The video callback is a tail extension. Modules which do not require it may
// still run against an ABI-2 host whose VmHost ends immediately before it.
static constexpr uint32_t VM_HOST_BASE_BYTES=offsetof(VmHost,video_present);
struct VmModule {
    uint32_t abi, bytes;
    // pump is permitted while awaiting ACK; it must not alter frozen output.
    void (*input)(const VmInput *input);
    void (*pump)();
    bool (*packet)(VmPacket *out);
    void (*ack)();
};
using VmEntry = const VmModule *(*)(const VmHost *host);
enum : uint32_t { VM_SERVICE_FILES=1, VM_SERVICE_CLOCK=2, VM_SERVICE_PACKETS=4,
                  VM_SERVICE_WRITE=8, VM_SERVICE_GUEST_RAM=16,
                  VM_SERVICE_VIDEO=32, VM_SERVICES=31,
                  VM_SERVICE_INDEXED_VIDEO=64, VM_SERVICE_RAM2_RO=128,
                  VM_SERVICE_INDEXED_RASTER=256,
                  VM_SERVICE_RAM1_AUX=512,
                  VM_HOST_SERVICES=VM_SERVICES|VM_SERVICE_VIDEO|VM_SERVICE_INDEXED_VIDEO|VM_SERVICE_RAM2_RO|VM_SERVICE_INDEXED_RASTER|VM_SERVICE_RAM1_AUX,
                  VM_KNOWN_SERVICES=VM_HOST_SERVICES, VM_IMAGE_MAGIC=0x314d564d };
static inline uint32_t vm_crc32(const void *data, uint32_t size) {
    auto p=static_cast<const uint8_t *>(data); uint32_t c=~0u;
    while(size--) { c^=*p++; for(unsigned b=0;b<8;b++) c=(c>>1)^((0u-(c&1))&0xedb88320u); }
    return ~c;
}
static inline uint32_t vm_image_ro_bytes(const VmImageHeader &h){return h.reserved[1];}
static inline uint32_t vm_image_guest_bytes(const VmImageHeader &h){return h.reserved[0]==VM_PROFILE_RAM2_RO96?uint32_t(VM_RAM2_GUEST_BYTES):uint32_t(VM_RAM_BYTES);}
static inline uint32_t vm_image_payload_bytes(const VmImageHeader &h){return h.code_bytes+h.data_bytes+vm_image_ro_bytes(h);}
static inline bool vm_valid_header(const VmImageHeader &h, uint32_t file_bytes) {
    if(h.reserved[2]||h.reserved[3])return false;
    if(h.reserved[0]==VM_PROFILE_RAM1_AUX){
        if(h.reserved[1]||!(h.required_services&VM_SERVICE_RAM1_AUX)||
           (h.required_services&VM_SERVICE_RAM2_RO)||h.code_bytes>VM_AUX_CODE_LIMIT-VM_CODE_BASE)return false;
    }else if(h.required_services&VM_SERVICE_RAM1_AUX)return false;
    else if(h.reserved[0]==VM_PROFILE_LEGACY){
        if(h.reserved[1]||(h.required_services&VM_SERVICE_RAM2_RO))return false;
    }else if(h.reserved[0]==VM_PROFILE_RAM2_RO96){
        if(!(h.required_services&VM_SERVICE_RAM2_RO)||!h.reserved[1]||h.reserved[1]>VM_RAM2_RO_BYTES)return false;
    }else return false;
    if(h.magic!=VM_IMAGE_MAGIC || h.abi!=VM_ABI || h.header_bytes!=sizeof h ||
       h.code_base!=VM_CODE_BASE || h.ram_base!=VM_DATA_BASE || !h.code_bytes ||
       h.code_bytes>VM_CODE_LIMIT-VM_CODE_BASE || h.data_bytes>VM_DATA_BYTES ||
       h.bss_bytes>VM_DATA_BYTES-h.data_bytes || (h.required_services&~VM_KNOWN_SERVICES) ||
       file_bytes!=sizeof h+vm_image_payload_bytes(h) || !(h.entry&1) ||
       (h.entry&~1u)<VM_CODE_BASE || (h.entry&~1u)>=VM_CODE_BASE+h.code_bytes) return false;
    VmImageHeader check=h; check.header_crc=0;
    return vm_crc32(&check,sizeof check)==h.header_crc;
}
